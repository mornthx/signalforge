# M5 — Decoder Layer

| Field | Value |
|---|---|
| Milestone ID | M5 |
| Sprint | 5 |
| Estimated effort | 8-10 person-days |
| Prerequisites | M4 closed (main at v0.0.5-alpha.1) |
| Next milestone | M6 (Signal Buffer) |
| Hard-stop type | **Schema freeze** (yaml v1) + **Interface freeze** (`DecoderInterface`) + **Implementation correctness** |
| Soft-HALT allowed | **No** |
| Branch | `milestone/M5` |

**Cross-reference notation**:

- `[EM §N]` — Execution Manual, section N
- `[Arch §N]` — Architecture document, section N
- `[MR]` — Milestone Roadmap
- `[CM §X]` — CLAUDE.md, section X
- `[ADR-N]` — Architecture Decision Record N
- `[M<n> §N]` — M<n> spec

---

## 1. Goal

Parse `RawFrame` byte payloads into typed `SignalValue` instances using yaml schemas. M5 is the **first milestone where user-facing data format matters** — users author yaml files to describe their device's protocol; the decoder reads those files and turns raw bytes into named signals with units.

Two freeze surfaces:

1. **yaml schema v1 format**: once frozen, V1 user-authored yaml files using v1 must continue to work across V1's lifetime. Extensions bump to v2.
2. **`DecoderInterface` (C++)**: implements `FrameSink` from M4. M6 Signal Buffer and M8 Chart UI consume decoder output.

Quality philosophy from previous milestones: **user experience over raw speed**. A schema error with file:line:field pointer is more valuable than 5% faster parse. A malformed frame that's logged with specific byte-level detail is more valuable than one that's silently dropped.

---

## 2. Scope

### 2.1 Must deliver

1. **yaml schema v1 format specification** at `schemas/decoder_schema_v1.yaml`:
   - Example schema showing all supported features
   - Inline comments documenting each field
   - JSON-Schema at `schemas/decoder_schema_v1.json` describing the meta-schema (used by the validator)

2. **`DecoderInterface`** at `src/decoder/decoder_interface.hpp`:
   - Pure abstract class inheriting `signalforge::pipeline::FrameSink`
   - Additional pure virtuals beyond FrameSink: `schemaId()`, `signalMetadata()`, `signalValueSink()` (the destination for decoded values)
   - `SignalValue` = `std::variant<bool, int64_t, double, QString>`
   - `SignalMetadata` struct: name, unit, data-type tag, optional description/scale/offset

3. **`SchemaDecoder`** concrete decoder at `src/decoder/schema_decoder.{hpp,cpp}`:
   - Implements `DecoderInterface`
   - Loads a schema from yaml at construction time
   - Fixed-layout byte frame parsing (V1 only; variable-length is deferred to V1.5)
   - Per-frame: match against schema, extract fields, emit to SignalValueSink
   - Endianness (explicit per-field or per-layout), bit-range extraction, scale/offset transforms

4. **`SchemaValidator`** at `src/decoder/schema_validator.{hpp,cpp}`:
   - Loads yaml, validates against `decoder_schema_v1.json`
   - Precise error messages: file path + yaml line number + field name + specific issue
   - Returns a parsed, validated `Schema` struct (ready for SchemaDecoder to consume)
   - Same validator is used by the CLI lint tool (§2.1-6) and by SchemaDecoder at load time

5. **`SignalValueSink`** at `src/decoder/signal_value_sink.hpp`:
   - Pure abstract: downstream consumer of decoded signals (M6 Signal Buffer will implement this)
   - `onSignal(timestamp, signalId, value)` method
   - In M5, a `LoggingSignalValueSink` stub lets decoder output be visible for testing

6. **Schema lint CLI tool** at `tools/schema_lint/`:
   - Standalone `schema_lint <file.yaml>` executable
   - Uses same `SchemaValidator`
   - Exit code 0 on valid, non-zero on invalid
   - Output: human-readable validation result; `--json` flag for machine-readable output
   - User runs this while authoring schemas to catch errors offline

7. **Decoder registry integration**:
   - M4's `PipelineManager::pipelineAttached` signal triggers decoder registration
   - `DecoderRegistrar` class (new) listens for pipeline attachment, constructs appropriate `SchemaDecoder` based on configured schema for that driver, registers it as a sink on the pipeline
   - For M5, schema selection is hard-coded per driver type in a config map (full schema selection UI is M9's domain)

8. **Integration tests** at `tests/integration/`:
   - `test_schema_decoder_basic.cpp` — end-to-end: RawFrame → decoder → SignalValueSink
   - `test_schema_decoder_bit_fields.cpp` — bit-range extraction
   - `test_schema_decoder_endianness.cpp` — little + big endian
   - `test_schema_decoder_unmatched.cpp` — unknown frames logged + counted + discarded
   - `test_schema_validator_errors.cpp` — invalid schemas rejected with precise errors

9. **Unit tests** ≥ 85% coverage on decoder modules

10. **Benchmark**: `bench_decoder_throughput.cpp`:
    - Target: ≥ 100 kHz frames/sec sustained for a typical 16-byte frame
    - Methodology: pre-built RawFrames fed through decoder, measure signals/sec emitted
    - Results appended to `tests/benchmark/results/M5-baseline.md`

11. **Example schemas** at `examples/schemas/`:
    - `temperature_sensor.yaml` — simple 16-byte frame, 3 fields
    - `modbus_style.yaml` — more complex, multiple frame types, bit flags
    - Used in both tests and user documentation

12. **Doxygen** on all public declarations

13. **`.claude/M5-done.md`** with standard completion report + schema version record + sha256 of frozen headers + schema v1 freeze declaration

### 2.2 Must not do

1. **No modifications to M2/M3/M4 frozen files**. If any freeze-scope change seems needed, HALT.
2. **No signal buffer / time series storage**. Decoders emit to `SignalValueSink`; M6 implements the actual storage.
3. **No expression evaluation** (M7 territory).
4. **No GUI schema editor**. V1 users write yaml by hand; V1.5 might add a visual editor.
5. **No variable-length frame support**. V1 schema v1 handles fixed-layout only. Variable-length (e.g., Modbus with length byte) is deferred to schema v2 or custom decoders.
6. **No CRC verification**. Fields can be named `crc` but the decoder does not verify them in M5. Adding CRC verification is a schema v1.1 amendment if needed.
7. **No `UdpDecoder`, `SerialDecoder`, etc. as concrete classes**. `SchemaDecoder` is the one concrete decoder in M5; protocol-specific decoders (as Plugin classes) are V1.5+.
8. **No multi-decoder per driver**. One driver gets one decoder. Fanout to multiple consumers happens at the `SignalValueSink` level (multiple sinks) in M6.
9. **No performance panel UI changes**. M8 owns UI.

---

## 3. Design Decisions (locked by this spec)

Decisions confirmed in pre-M5 planning.

### 3.1 Schema granularity: one yaml file per driver

**Decision**: Each user-authored yaml file describes one driver's complete frame-parsing rules. The file may contain multiple frame layouts (distinguished by discriminator fields like magic bytes), but one driver → one file.

**Rationale**: Real protocols often share framing conventions (header bytes, length prefix). Single-file-per-driver lets related frame types sit together. Single file simplifies config management.

### 3.2 Schema validation: strict at load + CLI lint tool

**Decision**: `SchemaValidator` rejects any schema error at load time with a clear error message (file + line + field + specific issue). `SchemaDecoder` construction fails if validation fails. Additionally, a standalone `schema_lint` CLI tool lets users validate offline during authoring.

**Rationale**: Silent-fail schemas are debugging hell. Early rejection with specific errors is the norm in every mature format parser (yq, python yaml.safe_load with schemas, etc.). Lint tool means users get feedback while editing, not after opening the app.

### 3.3 Unknown frames: warn + count + discard

**Decision**: A frame whose bytes don't match any layout in the active schema produces:
- `SF_LOG_WARN` with driver id + first N bytes in hex
- `frames_unmatched_<driverId>` counter incremented
- Frame discarded (not forwarded to SignalValueSink)

**Rationale**: Unmatched frames are not fatal — could be protocol variants, junk data, or user's schema gaps. But they must be visible so users can debug. Counter lets M8 performance panel show "5% of frames unmatched — check schema".

### 3.4 Bit extraction: range-based with optional description

**Decision**: yaml syntax for bit fields uses `bit_start` + `bit_count`. `bit_count: 1` produces bool `SignalValue`; `bit_count > 1` produces int64 (0..2^count - 1). Optional `description: "..."` field on each bit field for documentation.

**Example**:
```yaml
fields:
  - name: status
    offset: 10
    type: uint8
    bit_fields:
      - { name: alarm, bit_start: 0, bit_count: 1, description: "Temperature alarm latch" }
      - { name: sensor_mode, bit_start: 1, bit_count: 2 }   # 2 bits → 0..3
      - { name: reserved, bit_start: 3, bit_count: 5 }
```

**Rationale**: Uniform syntax for single-bit and multi-bit. `description` is optional so simple schemas stay terse; complex schemas with state machines can document.

### 3.5 Endianness: explicit per-field or per-layout, never implicit

**Decision**: yaml schemas must declare endianness:
- Per-layout default: `endianness: little` or `endianness: big` at layout level
- Per-field override: a field may have its own `endianness` key
- Schema with multi-byte integer fields and no endianness declaration → validation error

**Rationale**: Endianness bugs are silent and devastating (wrong values look plausible until reviewed in detail). Forcing explicit declaration catches errors at schema authoring, not in production.

### 3.6 SignalValue = variant<bool, int64_t, double, QString>

**Decision**: Per pre-M4 planning (roadmap v2.1 §M5). Four variants:
- `bool`: single-bit flags
- `int64_t`: integers (unsigned integers fit in int64 for V1 purposes; native 64-bit unsigned is deferred to V1.5)
- `double`: floats (float32 and float64 source types both decode to double)
- `QString`: text fields (null-terminated or length-prefixed bytes, extracted as QString)

**Rationale**: 4 variants cover V1 needs. Storage efficiency and per-type optimization is M6 Signal Buffer's internal concern, not M5's.

### 3.7 One decoder per driver

**Decision**: Each `FramePipeline` gets one `SchemaDecoder` registered as its sink. The decoder handles all frames for that driver. Fanout to multiple downstream consumers (buffer + session writer + live display) happens on the `SignalValueSink` side, not on the decoder side.

**Rationale**: Simplifies lifecycle. Decoder lifetime = pipeline lifetime. M6 Signal Buffer can fan out signals to M8 Chart + M10 Session Writer; that's M6's design concern.

### 3.8 No soft-HALT

Same as M2/M3/M4 — hard-HALT only.

### 3.9 Schema version in file

**Decision**: Every user yaml file must declare `schema_version: 1` at top level. Mismatched versions (e.g., `schema_version: 2` when this codebase only supports v1) → validation error with message "this codebase supports schema versions: [1]; file declares: 2".

**Rationale**: Future V1.5+ introduces schema v2 (variable-length, CRC, etc.). Version declaration allows same codebase to support multiple versions as SignalForge evolves.

### 3.10 Decoder metrics namespace

Per M4's `decoder_<metric>_<driverId>` convention:

- `decoder_frames_decoded_<driverId>` (counter): successfully decoded frames
- `decoder_frames_unmatched_<driverId>` (counter): frames with no matching layout
- `decoder_frames_malformed_<driverId>` (counter): frames matching a layout but field extraction failed (e.g., bit_start out of payload bounds)
- `decoder_signals_emitted_<driverId>` (counter): signals sent to SignalValueSink (one frame can emit multiple)
- `decoder_last_decode_us_<driverId>` (gauge): latency of most recent decode, µs

---

## 4. Key Implementation Details

### 4.1 `DecoderInterface` and `SignalValue`

Place at `src/decoder/decoder_interface.hpp`.

```cpp
// src/decoder/decoder_interface.hpp
#pragma once

#include "pipeline/frame_sink.hpp"
#include "frame/raw_frame.hpp"

#include <QString>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

namespace signalforge::decoder {

/// Value type of a decoded signal. M6 stores these; M7 evaluates
/// expressions over numeric variants; M8 displays them in charts.
///
/// Storage guidance for M6:
/// - bool: single bit (can pack 64 per uint64)
/// - int64_t: 8 bytes
/// - double: 8 bytes
/// - QString: pointer to shared storage (QString is ref-counted)
///
/// Expression semantics for M7:
/// - bool auto-converts to double (0.0 / 1.0)
/// - int64_t auto-converts to double (mantissa is 52 bits; values > 2^52
///   lose precision — M7 logs warn at registration if an expression uses
///   an int64 source whose current/recent values exceed 2^52)
/// - QString variables cannot be used in expressions — M7 rejects at
///   registration time with a clear error.
using SignalValue = std::variant<bool, std::int64_t, double, QString>;

/// Which variant the schema declares a signal to be.
enum class SignalType {
    Bool,
    Int64,
    Double,
    String,
};

/// Per-signal metadata. One SignalMetadata per signal emitted by this
/// decoder. Content is stable across the decoder's lifetime.
struct SignalMetadata {
    QString id;                       ///< Unique identifier, stable across app run. "<driverId>/<fieldName>" convention.
    QString name;                     ///< Human-readable; from yaml `name` field.
    QString unit;                     ///< SI or engineering unit; from yaml `unit` field.
    SignalType type;                  ///< Which variant this signal uses.
    std::optional<QString> description;  ///< Optional from yaml.
    std::optional<double> scale;      ///< Optional linear transform: raw * scale + offset = value.
    std::optional<double> offset;     ///< Optional.
};

/// Downstream consumer of decoded signals.
/// M5 provides LoggingSignalValueSink for test/stub use.
/// M6 provides the real SignalBuffer-backed implementation.
class SignalValueSink {
public:
    virtual ~SignalValueSink() = default;

    /// Called for each decoded signal value.
    /// `timestamp` is the RawFrame.recvAt from the origin frame.
    /// `signalId` matches SignalMetadata.id of a registered signal.
    /// `value` is the variant with data.
    ///
    /// Thread affinity: called on the pipeline's thread (same thread
    /// as the Decoder's FrameSink::onFrame that produced this value).
    virtual void onSignal(std::chrono::steady_clock::time_point timestamp,
                          const QString& signalId,
                          const SignalValue& value) = 0;

    /// Called once per decoder at registration time to publish the
    /// decoder's signal catalog. Consumer may use this to pre-allocate
    /// buffers, create UI bindings, etc.
    virtual void onSignalsRegistered(const QString& driverId,
                                     const std::vector<SignalMetadata>& signals) {
        (void)driverId;
        (void)signals;
    }

    /// Called when the decoder is detaching (pipeline destroyed).
    /// Consumer should mark signals as "stream ended".
    virtual void onSignalsUnregistered(const QString& driverId) {
        (void)driverId;
    }
};

/// Abstract decoder: a FrameSink that translates RawFrames into SignalValues.
///
/// A decoder is registered with a FramePipeline via addSink(). On each
/// incoming frame, the decoder's onFrame (inherited from FrameSink)
/// parses the frame and calls setSignalSink's onSignal for each field.
///
/// Thread affinity: onFrame runs on the pipeline's thread; onSignal
/// callbacks happen on the same thread.
///
/// Freeze scope: this class + SignalValueSink + SignalValue + SignalMetadata
/// + SignalType are frozen at M5 close. Modifications post-freeze require ADR.
class DecoderInterface : public signalforge::pipeline::FrameSink {
public:
    /// Schema identifier for this decoder. Stable across decoder lifetime.
    /// Convention: relative path from schema root, e.g., "temp_sensor.yaml".
    [[nodiscard]] virtual QString schemaId() const = 0;

    /// Catalog of signals this decoder may emit. Called once after
    /// construction; consumer caches. Stable across decoder lifetime.
    [[nodiscard]] virtual std::vector<SignalMetadata> signalMetadata() const = 0;

    /// Attach a downstream consumer. Idempotent: attaching the same sink
    /// twice is a no-op (logs warning). Thread-safe; may be called before
    /// or after frames start flowing.
    virtual void setSignalSink(std::shared_ptr<SignalValueSink> sink) = 0;
};

}  // namespace signalforge::decoder
```

### 4.2 `SchemaDecoder` implementation

Place at `src/decoder/schema_decoder.{hpp,cpp}`.

```cpp
// src/decoder/schema_decoder.hpp
#pragma once

#include "decoder/decoder_interface.hpp"
#include "decoder/schema.hpp"

#include <QString>
#include <memory>
#include <mutex>
#include <vector>

namespace signalforge::decoder {

/// Yaml-schema-driven decoder. Loads a schema at construction; parses
/// incoming RawFrames per schema and emits SignalValues to the
/// attached SignalValueSink.
class SchemaDecoder : public DecoderInterface {
public:
    /// Construct with a pre-validated schema (typically from SchemaValidator).
    /// driverId is used for metric namespacing and log context.
    SchemaDecoder(Schema schema, QString driverId);
    ~SchemaDecoder() override;

    // DecoderInterface overrides
    QString schemaId() const override;
    std::vector<SignalMetadata> signalMetadata() const override;
    void setSignalSink(std::shared_ptr<SignalValueSink> sink) override;

    // FrameSink overrides (inherited via DecoderInterface)
    void onFrame(const signalforge::frame::RawFrame& frame) override;
    QString sinkName() const override;
    // onError and onLifecycle use default no-op implementations from FrameSink.

private:
    Schema schema_;
    QString driverId_;
    mutable std::mutex sinkMutex_;
    std::shared_ptr<SignalValueSink> sink_;  // nullable until setSignalSink

    // Cached metric pointers (populated in constructor after registration)
    // ...

    /// Try each layout in schema_; first to match wins. Returns true if matched.
    [[nodiscard]] bool tryDecodeFrame(const signalforge::frame::RawFrame& frame);

    /// Extract a single field from the payload.
    [[nodiscard]] std::optional<SignalValue> extractField(
        const FieldDef& field,
        const QByteArray& payload,
        Endianness layoutEndianness);
};

}  // namespace signalforge::decoder
```

### 4.3 Schema data structures

Place at `src/decoder/schema.hpp`.

```cpp
// src/decoder/schema.hpp
#pragma once

#include <QString>
#include <cstdint>
#include <optional>
#include <vector>

namespace signalforge::decoder {

enum class Endianness { Little, Big };

enum class FieldEncoding {
    Int8, Int16, Int32, Int64,
    Uint8, Uint16, Uint32, Uint64,
    Float32, Float64,
    Bool,           // single bit; requires bit_start
    BitField,       // multi-bit integer; uses bit_start + bit_count
    FixedString,    // fixed-length UTF-8 string, length via bit_count or size
};

struct BitFieldDef {
    QString name;
    int bitStart = 0;
    int bitCount = 1;
    std::optional<QString> description;
};

struct FieldDef {
    QString name;
    int offset = 0;               ///< Byte offset within payload
    FieldEncoding encoding = FieldEncoding::Uint8;
    int sizeBytes = 0;            ///< Byte size for multi-byte encodings
    std::optional<Endianness> endianness;  ///< Overrides layout-level default
    std::optional<double> scale;
    std::optional<double> offset;
    QString unit;
    std::optional<QString> description;
    
    /// For BitField encoding: multiple named bits or bit ranges within
    /// this field's byte(s). Each bit_fields entry becomes its own signal.
    std::vector<BitFieldDef> bitFields;
};

struct LayoutMatch {
    int offset = 0;
    std::vector<std::uint8_t> bytes;  ///< Expected bytes at offset for match
};

struct Layout {
    QString name;
    Endianness endianness = Endianness::Little;  ///< Explicit; validator rejects missing
    LayoutMatch match;               ///< How to identify this layout (magic bytes, etc.)
    int minPayloadBytes = 0;         ///< Minimum frame size for this layout
    std::vector<FieldDef> fields;
};

struct Schema {
    int schemaVersion = 1;
    QString id;                      ///< File path relative to schema root
    QString description;             ///< Optional from yaml
    std::vector<Layout> layouts;     ///< One schema can have multiple layouts
};

}  // namespace signalforge::decoder
```

### 4.4 `SchemaValidator`

Place at `src/decoder/schema_validator.{hpp,cpp}`.

```cpp
// src/decoder/schema_validator.hpp
#pragma once

#include "decoder/schema.hpp"

#include <QString>
#include <expected>  // C++23; fallback to std::variant or tl::expected if unavailable
#include <vector>

namespace signalforge::decoder {

struct ValidationError {
    QString filePath;
    int lineNumber = -1;          ///< -1 if not mappable to a line
    QString fieldPath;            ///< e.g., "layouts[0].fields[3].bit_fields[1]"
    QString message;              ///< Human-readable; actionable
};

using ValidationResult = std::expected<Schema, std::vector<ValidationError>>;
// Fallback if C++23 expected unavailable:
//   struct ValidationResult {
//       std::optional<Schema> schema;
//       std::vector<ValidationError> errors;
//   };

class SchemaValidator {
public:
    /// Load and validate a yaml file.
    /// Returns validated Schema on success, list of errors on failure.
    [[nodiscard]] static ValidationResult validateFile(const QString& yamlPath);

    /// Validate yaml content already loaded as a string.
    /// `virtualPath` is used only for error messages (doesn't need to exist).
    [[nodiscard]] static ValidationResult validateString(
        const QString& yamlContent,
        const QString& virtualPath);
};

}  // namespace signalforge::decoder
```

**Implementation uses yaml-cpp** (already a project dependency). Validation sequence:

1. Parse yaml; catch syntax errors → ValidationError with line number
2. Check required top-level keys (`schema_version`, `layouts`) → error with line if missing
3. Validate each layout:
   - `endianness` present (reject if missing) → specific error
   - `match` present with at least one byte pattern
   - `fields` non-empty
4. Validate each field:
   - `name` non-empty, unique within layout
   - `offset` non-negative, `< minPayloadBytes`
   - `encoding` valid enum value
   - `sizeBytes` matches encoding (e.g., uint16 → sizeBytes=2)
   - Multi-byte fields: `endianness` present (either field-level or inherited from layout)
   - BitField: `bit_fields` non-empty, ranges don't overlap, all fit within `sizeBytes * 8` bits
5. Build `Schema` struct and return

### 4.5 `LoggingSignalValueSink` (M5 stub)

Place at `src/decoder/logging_signal_value_sink.{hpp,cpp}`.

Simple implementation for tests and early validation:

```cpp
class LoggingSignalValueSink : public SignalValueSink {
public:
    void onSignal(std::chrono::steady_clock::time_point timestamp,
                  const QString& signalId,
                  const SignalValue& value) override {
        // Log at INFO with formatted value
        // Count in an atomic counter for test introspection
    }
    
    // Expose counters for tests
    [[nodiscard]] std::uint64_t signalsReceived() const;
    [[nodiscard]] std::uint64_t signalsByType(SignalType t) const;
};
```

Not for production — M6's Signal Buffer replaces this. But M5 tests use it to verify decoder correctness end-to-end.

### 4.6 `DecoderRegistrar`

Place at `src/decoder/decoder_registrar.{hpp,cpp}`.

Listens for `PipelineManager::pipelineAttached`, constructs a `SchemaDecoder` for the driver (based on configured schema path), and registers it via `pipeline->addSink()`.

M5's `DecoderRegistrar` uses a hard-coded config map:

```cpp
// src/app/main_window.cpp or similar
std::unordered_map<QString, QString> driverTypeToSchemaPath = {
    {"serial", "examples/schemas/temperature_sensor.yaml"},
    {"tcp", ""},  // no schema for TCP by default in M5
    {"udp", ""},
    {"replay", ""},
};
```

M9 Connection Manager replaces this with per-connection user selection.

### 4.7 Schema lint CLI tool

Place at `tools/schema_lint/`.

Structure matches `tools/crash_test/` pattern (standalone CMake, not wired into main build):

```
tools/schema_lint/
├── CMakeLists.txt
├── main.cpp
└── README.md
```

`main.cpp`:

```cpp
int main(int argc, char** argv) {
    // CLI: schema_lint <file.yaml> [--json]
    // Uses SchemaValidator::validateFile
    // Output: human-readable (default) or JSON (with --json flag)
    // Exit code: 0 valid, 1 invalid, 2 bad CLI args
}
```

`README.md` explains usage:

```
$ schema_lint examples/schemas/temperature_sensor.yaml
OK: temperature_sensor.yaml (1 layout, 3 fields, 2 bit fields)

$ schema_lint broken.yaml
FAIL: broken.yaml

broken.yaml:8 — layouts[0].fields[1].endianness is required for multi-byte field 'pressure'
broken.yaml:15 — layouts[0].fields[2].bit_fields[0].bit_start (10) exceeds field size (8 bits total)
```

Link against `signalforge_decoder` (contains `SchemaValidator`).

### 4.8 Example schema

Place at `examples/schemas/temperature_sensor.yaml`:

```yaml
schema_version: 1
description: "Example: simple temperature sensor with 16-byte frames"

layouts:
  - name: telemetry
    endianness: little
    match:
      offset: 0
      bytes: [0xAA]         # magic byte identifying this layout
    min_payload_bytes: 16
    fields:
      - name: timestamp_ms
        offset: 1
        encoding: uint32
        size_bytes: 4
        unit: ms
        description: "Device-local monotonic millisecond counter"

      - name: temperature
        offset: 5
        encoding: int16
        size_bytes: 2
        scale: 0.01
        unit: "°C"
        description: "Measured temperature"

      - name: pressure
        offset: 7
        encoding: uint16
        size_bytes: 2
        scale: 0.1
        unit: kPa

      - name: status
        offset: 9
        encoding: bitfield
        size_bytes: 1
        bit_fields:
          - { name: alarm, bit_start: 0, bit_count: 1, description: "Temperature exceeded alarm threshold" }
          - { name: calibration_active, bit_start: 1, bit_count: 1 }
          - { name: sensor_mode, bit_start: 2, bit_count: 2, description: "0=normal 1=high-precision 2=fast 3=reserved" }
          - { name: reserved, bit_start: 4, bit_count: 4 }

      - name: crc
        offset: 10
        encoding: uint16
        size_bytes: 2
        description: "XMODEM CRC16 of bytes 0-9 (not verified by V1 decoder)"

      - name: padding
        offset: 12
        encoding: uint32
        size_bytes: 4
```

---

## 5. Test strategy

### 5.1 Coverage ≥ 85% on decoder modules

- `schema_decoder.cpp`: ≥ 85%
- `schema_validator.cpp`: ≥ 90% (validator is error-heavy; high coverage ensures all error paths tested)
- `decoder_registrar.cpp`: ≥ 80%

### 5.2 Unit tests

**For `SchemaDecoder`**:

- Valid schema loads; `signalMetadata()` returns expected signals
- First frame byte matches layout magic → correctly decoded
- Byte doesn't match any layout → unmatched counter increments
- Byte matches but payload too short → malformed counter, frame discarded, no signal emitted
- Multi-layout schema: different magic bytes route to different layouts
- Per-field endianness overrides layout-level
- Bit field extraction: single bits → bool, multi-bit → int64
- Scale/offset applied to numeric fields
- String field: fixed-length extracted correctly, null terminator honored

**For `SchemaValidator`**:

- Minimal valid schema accepted
- Missing `schema_version` → ValidationError with clear message
- Wrong `schema_version` (e.g., 2) → error
- Missing `endianness` on layout with multi-byte field → error
- Invalid `encoding` enum value → error
- `sizeBytes` inconsistent with encoding (e.g., uint16 with sizeBytes=4) → error
- Duplicate field names within layout → error
- Bit fields overlap (0-1 and 1-2) → error
- Bit field exceeds byte boundary (bit_start=5, bit_count=5 on uint8) → error
- Line numbers in errors are actual yaml lines (or -1 if yaml parser doesn't provide)

**For `LoggingSignalValueSink`** (M5-only):

- `onSignal` called with correct args
- Counters increment correctly
- Log output contains signalId and value

### 5.3 Integration tests

**`test_schema_decoder_basic.cpp`**:

1. Load `examples/schemas/temperature_sensor.yaml` into SchemaValidator
2. Construct SchemaDecoder
3. Attach LoggingSignalValueSink
4. Feed RawFrame with known bytes
5. Verify 7 signals emitted (timestamp_ms, temperature, pressure, 4 bit fields from status, padding) with expected values

**`test_schema_decoder_bit_fields.cpp`**:

Specific tests for bit field extraction:
- bit_count=1 → bool
- bit_count=2 → int64 with values 0..3
- bit_count=8 (whole byte) → int64
- Cross-byte bit field (bit_start=6, bit_count=4) → int64 extracted from bits 6-9

**`test_schema_decoder_endianness.cpp`**:

1. Same schema with `endianness: little` → decode expected little-endian values
2. Change to `endianness: big` → decode expected big-endian values
3. Per-field endianness override: verify field's own endianness takes precedence

**`test_schema_decoder_unmatched.cpp`**:

1. Schema with single layout (magic=0xAA)
2. Feed frames with magic=0xBB, 0xCC, etc.
3. Verify `frames_unmatched` counter increments
4. Verify SignalValueSink not called
5. Verify log messages emitted

**`test_schema_validator_errors.cpp`**:

Various malformed schemas → verify specific error messages. Each schema file under `tests/integration/fixtures/invalid_schemas/`:

- `missing_version.yaml` — no schema_version
- `missing_endianness.yaml` — layout with multi-byte field, no endianness
- `invalid_encoding.yaml` — encoding: "not_a_real_type"
- `bit_overlap.yaml` — two bit fields overlap
- `bit_overflow.yaml` — bit field extends past byte boundary
- `duplicate_field.yaml` — two fields with same name

Each test asserts:
- Validation fails
- Error message contains specific field path and line number

### 5.4 Benchmarks

**`bench_decoder_throughput.cpp`**:

Pre-build 1000 RawFrames with realistic payloads. Feed through SchemaDecoder in a tight loop. Measure frames/sec.

Threshold:
- Simple schema (4-6 numeric fields): ≥ 100k frames/sec (10 µs per decode)
- Complex schema (with bit fields, strings): ≥ 50k frames/sec

Results to `tests/benchmark/results/M5-baseline.md`.

### 5.5 Schema lint CLI tests

`tools/schema_lint/` has its own `tests/` subdirectory (or use Catch2 integration into main suite):

- Valid schema: exit 0, "OK" in output
- Invalid schema (each fixture from §5.3): exit 1, specific error
- `--json` output is parseable JSON

---

## 6. Freeze protocol

### 6.1 What freezes at M5 close

**Schema format v1**:
- Top-level keys: `schema_version`, `description`, `layouts`
- Layout keys: `name`, `endianness`, `match`, `min_payload_bytes`, `fields`
- Field keys: `name`, `offset`, `encoding`, `size_bytes`, `endianness`, `scale`, `offset`, `unit`, `description`, `bit_fields`
- BitField keys: `name`, `bit_start`, `bit_count`, `description`
- Encoding enum values: `int8`, `int16`, `int32`, `int64`, `uint8`, `uint16`, `uint32`, `uint64`, `float32`, `float64`, `bool`, `bitfield`, `fixed_string`

Once merged, user yaml files using v1 format must continue to work across V1's lifetime. Extensions (e.g., variable-length frames, CRC verification) bump to v2.

**C++ interfaces**:
- `SignalValue`, `SignalType`, `SignalMetadata` — type signatures
- `DecoderInterface` — all pure virtuals and inheritance from FrameSink
- `SignalValueSink` — all virtuals
- `SchemaValidator` public API — `validateFile`, `validateString`, `ValidationResult`, `ValidationError`

### 6.2 What does NOT freeze

- `Schema` / `Layout` / `FieldDef` / `BitFieldDef` struct layouts (internal representation; may change to optimize)
- `SchemaDecoder` implementation details (private members, helpers in .cpp)
- `LoggingSignalValueSink` (M5-only stub; replaced by M6)
- `DecoderRegistrar` internals
- Metric names that are additive extensions
- Tool internals of `tools/schema_lint/`

### 6.3 Freeze record format

`.claude/M5-done.md` includes:

```markdown
## Freezes established in this milestone

The following are frozen per M5 spec §6.1:

**Schema format v1**:
- File: `schemas/decoder_schema_v1.yaml` (sha256: <...>)
- JSON-Schema meta-format: `schemas/decoder_schema_v1.json` (sha256: <...>)
- Example: `examples/schemas/temperature_sensor.yaml` (sha256: <...>)

**C++ interfaces**:
- `src/decoder/decoder_interface.hpp` (sha256: <...>)
- `src/decoder/schema_validator.hpp` (sha256: <...>)

User-facing contract: any yaml file with `schema_version: 1` authored after M5 close must be accepted by all future V1 validators.

C++ contract: modifications require new ADR per M5 §6.2.
```

---

## 7. M5-specific HALT triggers

Beyond CLAUDE.md §HALT:

1. **Any modification to M2/M3/M4 frozen `.hpp`** → HALT.
2. **yaml-cpp's line number reporting is unavailable or inaccurate** for error messages → HALT, propose alternative (custom parser wrapper, or accept -1 as "not available").
3. **C++23 `std::expected` unavailable on GCC 13** → HALT (probable since GCC 13 added expected in preview; verify). If unavailable, propose `std::variant<Schema, errors>` or add tl::expected as dependency (which would need ADR).
4. **Benchmark throughput < 50k frames/sec** even after fix attempts → HALT with bottleneck analysis.
5. **Schema validator error messages lack line numbers for a significant fraction of cases** (e.g., > 20% of errors have line -1) → HALT, propose remediation.
6. **UI thread blocks during decoder registration** (DecoderRegistrar's pipelineAttached handler blocks main thread > 100ms) → HALT.

---

## 8. Acceptance criteria

### 8.1 Build and test

- [ ] Debug, Release, debug-asan all build clean, zero warnings
- [ ] All unit + integration tests pass under Debug + Release
- [ ] Coverage ≥ 85% on decoder modules per §5.1
- [ ] `tools/schema_lint/` builds independently and test examples pass

### 8.2 Schema format

- [ ] `schemas/decoder_schema_v1.yaml` (or `.json`) documents the frozen format
- [ ] `examples/schemas/temperature_sensor.yaml` and `modbus_style.yaml` validate successfully
- [ ] All invalid-schema fixtures produce expected errors with line numbers

### 8.3 Benchmarks

- [ ] Simple schema ≥ 100k frames/sec
- [ ] Complex schema ≥ 50k frames/sec
- [ ] Results in `tests/benchmark/results/M5-baseline.md`

### 8.4 Integration

- [ ] DecoderRegistrar listens for `pipelineAttached`, creates decoder, registers as sink
- [ ] End-to-end: driver emits frame → pipeline → decoder → LoggingSignalValueSink shows expected signals
- [ ] Unknown frames produce log warnings and increment counter
- [ ] `tools/schema_lint` catches all invalid-schema fixtures with useful error output

### 8.5 Freeze record

- [ ] `.claude/M5-done.md` has Freezes section per §6.3
- [ ] Sha256sums recorded
- [ ] No modifications to M2/M3/M4 frozen files (verify via git diff against merge base)

### 8.6 Hand-off to M6

- [ ] `.claude/M5-done.md` hand-off section covers:
  - How M6 implements SignalValueSink
  - Signal catalog publication via `onSignalsRegistered`
  - Thread affinity expectations for M6 (decoder thread = pipeline thread; M6 buffer can be same-thread or offload)
  - Baseline throughput to maintain

---

## 9. Notes for CC

- **Schema validator error messages are a core user experience feature.** Every error should tell the user exactly what's wrong and where. "Invalid schema at layouts[0].fields[2]" is insufficient; "line 15: field 'status' requires bit_fields when encoding=bitfield" is correct. Budget time for good error messages.

- **Don't add encoding types speculatively.** If the spec lists 13 encodings (int8..float64 plus bool/bitfield/fixed_string), implement exactly those. Additions post-freeze require ADR.

- **yaml-cpp line numbers vary by error type.** Some errors have accurate lines; some have -1. Document which.

- **Bit field extraction is error-prone.** Test exhaustively: single bits, multi-bit, cross-byte (if supported in M5 — clarify before implementation whether cross-byte bit fields are required, otherwise keep bit fields within a single byte for M5).

- **The lint CLI tool is user-facing.** Its output format will be copied into error reports, pasted into bug tickets. Prioritize clarity over brevity.

- **Schema v1 is frozen for the duration of V1.** This is a strong commitment. Any user's yaml file working today must work after V1 ships and throughout V1.x patches. Test this mental model: if in doubt about a schema design choice, lean toward the most conservative interpretation.

---

## 10. Closing note

M5 is the first milestone where user-authored artifacts (yaml files) become contract-level outputs. User schema files live outside the repo — they're in user project directories, shared via email, copied between machines. They outlast the codebase in some sense: if a user writes a schema in 2026, they expect it to still work when they reopen their project in 2027 with V1.x.

This makes yaml schema v1 the most permanent output of V1. The C++ interfaces can be refactored internally; the schema format must be stable.

When in doubt about the yaml format, choose the more explicit and more permissive option (accept explicit declarations; require them for anything ambiguity-prone like endianness). User experience over internal elegance.
