# M5 — Understanding

## 1. Restatement of the M5 goal

M5 delivers the **decoder layer**: yaml-authored schemas describe a
device's byte-framed protocol, and a `SchemaDecoder` turns each
`RawFrame` payload into one-or-more typed `SignalValue` emissions onto
a `SignalValueSink`. This is **the first milestone where user-authored
artifacts become contract-level outputs** — yaml files authored against
v1 must continue to work through the entire V1 lifetime.

Hard-stop types (all three concurrent):

1. **Schema freeze**: yaml schema v1 format, frozen at M5 close.
2. **Interface freeze**: `DecoderInterface`, `SignalValueSink`,
   `SignalValue`, `SignalMetadata`, `SignalType`, `SchemaValidator`
   public API.
3. **Implementation correctness**: the decoder must actually parse
   every supported field encoding + endianness + bit layout correctly.

Soft-HALT is not allowed (inherits M2/M3/M4 stance).

Quality philosophy continues the earlier milestones' theme: **user
experience over raw speed**. A schema error with file:line:field is
more valuable than 5% faster parse; an unmatched frame logged with
hex-dump beats one silently dropped.

## 2. Observed repo state

Phase 3 already executed (this session):

```
$ git log --oneline origin/main -5
321b100 Merge pull request #5 from mornthx/milestone/M4        # M4 merge
312dbbc docs: add M5 decoder layer spec                         # user-committed M5 spec
363a79b chore: record M4 PR number and CI status in done.md
8f8a484 chore: M4 completion report
dbc33eb bench: add pipeline throughput benchmark and M4 baseline
```

Phase 3 actions completed this session (report confirmed):

- PR #5 merged to main (merge commit `321b100b`).
- Tag `v0.0.5-alpha.1` annotated on `321b100b` and pushed.
- `milestone/M5` branch created from main at `321b100b` and pushed
  with upstream tracking.

M5 spec `docs/milestones/M5-decoder-layer.md` is at commit `312dbbc`
on main; 917 lines; already digested.

**Incoming from M4 (frozen, read-only)**:

- `src/pipeline/frame_sink.hpp` — `FrameSink` abstract base (onFrame,
  onError, onLifecycle, sinkName). `DecoderInterface` derives from it.
- `src/pipeline/frame_pipeline.hpp` — `FramePipeline` public API.
  Decoders register as sinks via `pipeline->addSink(decoder)`.
- `src/pipeline/pipeline_manager.hpp` — `PipelineManager` +
  `pipelineAttached(driverId, pipeline*)` / `pipelineDetached(driverId)`
  signals. The `DecoderRegistrar` subscribes to `pipelineAttached`.
- `src/drivers/*` — every concrete driver (M3), unchanged.
- `src/frame/raw_frame.hpp` — `RawFrame` with `payload` (QByteArray),
  `recvAt` (steady_clock time_point).
- `src/observability/metrics.hpp` — `MetricsRegistry` with ADR-003
  permissive naming; M5's `decoder_*_<driverId>` metrics will register
  the same way.
- `src/app/connection_manager.{hpp,cpp}` — M3+M4 UI that attaches
  pipelines per connection. M5's `DecoderRegistrar` lives alongside
  this but does not touch the UI.

**Existing dependencies** usable in M5:

- `yaml-cpp` 0.8.0 — already a project dependency (confirmed in
  `cmake/dependencies.cmake`); `SchemaValidator` uses it for parsing
  + line-number reporting.
- `nlohmann_json` — already a dependency; used for `schema_lint`'s
  `--json` output and for the JSON meta-schema file.

**No new top-level dependencies required by M5 spec §2.2-3**.

## 3. Key constraints

### 3.1 New top-level module `src/decoder/`

The spec places all new code at `src/decoder/`. Root `CMakeLists.txt`
already has a placeholder `add_subdirectory(src/decode)` — need to
confirm whether to reuse that path or create `src/decoder/`. The
spec text consistently uses `src/decoder/`; the existing `src/decode/`
directory appears empty / placeholder from M0 scaffolding. Plan:
**reuse `src/decode/`** (already on the build, matches the `decode`
subdir convention established in M0) and consistently name the
library `signalforge_decoder`. Confirm empty directory by inspection
during S1.

### 3.2 `std::expected` availability (§7.3 HALT trigger)

The spec's `SchemaValidator` signature uses
`std::expected<Schema, std::vector<ValidationError>>`. Verified
locally: **GCC 13 supports `std::expected` only under `-std=c++23`**;
our current build is `-std=c++20`. Three options:

- **A**: bump `CMAKE_CXX_STANDARD` to 23. Simple; GCC 13 / Clang 17+
  / MSVC 19.33+ all support it. Cross-cutting change but affects only
  the standard-library surface.
- **B**: use the spec-provided fallback struct
  `{ std::optional<Schema>, std::vector<ValidationError> }`. Stays on
  C++20 and keeps the freeze surface simple (a plain struct vs a
  standard-library template).
- **C**: add `tl::expected` as a dependency — needs ADR per M5 §7.3
  and Forbidden #1.

Planning choice: **Option A** (bump to C++23). Rationale:
  - GCC 13.2 (our CI/dev toolchain) ships `std::expected` as stable
    under C++23 since libstdc++ 12.
  - Keeps the frozen API in line with the spec verbatim, avoiding a
    divergence in the header's doxygen.
  - Affects only build flags; no source-level change required in
    existing M0–M4 code (C++23 is a strict superset of C++20 for our
    usage).
  - Reduces future churn when M7/M8 want other C++23 features.

If Option A turns out to have unexpected toolchain issues (e.g.,
CI's `ubuntu-24.04` runner), we fall back to Option B immediately
with a note in `.claude/M5-concerns.md`.

### 3.3 Schema v1 is a user-facing contract

Spec §10 is explicit: yaml files authored against v1 must continue
to work through V1's lifetime. This drives several design choices:

- **Error messages must be actionable**: file path + yaml line + field
  path + specific issue. Anything less gets rewritten before merge.
- **No speculative encoding types**. Only the 13 listed encodings
  (`int8..int64`, `uint8..uint64`, `float32`, `float64`, `bool`,
  `bitfield`, `fixed_string`) land in v1.
- **Explicit endianness required** (§3.5). Validator rejects
  multi-byte fields without declared endianness.
- **`schema_version: 1` top-level required**. Schema v2 bumps the
  version.
- The JSON meta-schema at `schemas/decoder_schema_v1.json` is itself
  frozen (sha256 in done.md).

### 3.4 Thread affinity

`DecoderInterface::onFrame` runs on the pipeline's worker thread
(FrameSink contract). `SignalValueSink::onSignal` is called from the
decoder's `onFrame`, so also on the pipeline's worker thread. M5
doesn't need to add any additional thread; all decoder work is
serialized per-pipeline.

M5's `LoggingSignalValueSink` stub writes to the sync logger — safe
across threads per M2's logger contract. M6's real
`SignalBuffer`-backed sink will decide its own threading, but that's
M6's concern.

### 3.5 DecoderRegistrar pattern

`DecoderRegistrar` connects to `PipelineManager::pipelineAttached`
and, on attach, constructs a `SchemaDecoder` based on a schema
configured for that driver's type (hard-coded map in M5; per-
connection UI in M9). It calls `pipeline->addSink(decoder)` +
`decoder->setSignalSink(sharedSink)`.

Tricky points:
- The registrar must pin the decoder's `shared_ptr` somewhere (the
  pipeline holds a strong ref as a sink — good). Handle detach via
  `pipelineDetached` if needed (or rely on pipeline teardown to drop
  the last ref).
- Main-thread block budget: the handler must not do expensive work
  (yaml load + validate + construct) while blocking the main thread
  for > 100 ms (§7.6). Schema parsing of a typical schema is
  sub-millisecond; the budget is fine for V1 schemas. Re-validate in
  S1 if pathological schemas surface.

### 3.6 Bit fields within a single byte (M5 simplification)

Spec §9 notes: "cross-byte bit fields (bit_start=6, bit_count=4) → int64
extracted from bits 6-9" BUT also: "clarify before implementation
whether cross-byte bit fields are required, otherwise keep bit fields
within a single byte for M5".

Plan: **support cross-byte bit fields in v1** (within a field's
`sizeBytes` window). The test matrix in §5.3 explicitly requires
cross-byte in `test_schema_decoder_bit_fields.cpp` (bit_start=6,
bit_count=4). Validator enforces that `bit_start + bit_count ≤
sizeBytes * 8`. This is consistent — a `bitfield` field with
`size_bytes: 2` and bit_start=6, bit_count=4 extracts from bits 6-9
of the 16-bit value (endianness-respecting).

### 3.7 Unmatched frame policy (§3.3)

- `SF_LOG_WARN` with `driverId` + first 16 bytes hex dump
- `decoder_frames_unmatched_<driverId>` counter
- Frame discarded; `SignalValueSink::onSignal` not called

This is load-bearing for user debugging — schema authors need to see
"5 % of frames unmatched, here's a hex sample" to iterate on their
schema.

### 3.8 Malformed frame policy (§3.10)

A frame that matches a layout but where field extraction fails
(e.g., payload shorter than declared `min_payload_bytes`, or a bit
field that would read past the end of the buffer):

- `SF_LOG_WARN` with the specific failure reason
- `decoder_frames_malformed_<driverId>` counter
- Frame discarded

### 3.9 Metric namespace per §3.10 / plan-§§4.6

New metrics (registered at `SchemaDecoder` construction via
`MetricsRegistry::getOrCreate`):

- `decoder_frames_decoded_<driverId>` (counter)
- `decoder_frames_unmatched_<driverId>` (counter)
- `decoder_frames_malformed_<driverId>` (counter)
- `decoder_signals_emitted_<driverId>` (counter)
- `decoder_last_decode_us_<driverId>` (gauge)

ADR-003's permissive validator accepts `decoder_*_<driverId>`
verbatim, so no sanitization is needed (driver IDs like
`serial:/tmp/ttyV0` and `tcp:127.0.0.1:9000` are valid metric name
substrings).

## 4. Freeze surface analysis

Two categories — C++ and schema — both permanent.

### 4.1 C++ freeze surface

- `src/decoder/decoder_interface.hpp`:
  - `SignalValue` type alias
  - `SignalType` enum (4 values)
  - `SignalMetadata` struct (7 fields)
  - `SignalValueSink` class (3 virtuals)
  - `DecoderInterface` class (3 pure virtuals on top of FrameSink's
    4)
- `src/decoder/schema_validator.hpp`:
  - `ValidationError` struct (4 fields)
  - `ValidationResult` alias
  - `SchemaValidator::validateFile` / `::validateString`

### 4.2 Schema v1 freeze surface

Per §6.1:
- Top-level keys: `schema_version`, `description`, `layouts`
- Layout keys: `name`, `endianness`, `match`, `min_payload_bytes`, `fields`
- Field keys (15): `name`, `offset`, `encoding`, `size_bytes`,
  `endianness`, `scale`, `offset`, `unit`, `description`, `bit_fields`
- BitField keys (4): `name`, `bit_start`, `bit_count`, `description`
- Encoding enum values (13): as listed in §3.1

Additionally frozen:

- `schemas/decoder_schema_v1.yaml` (canonical example + documentation)
- `schemas/decoder_schema_v1.json` (JSON meta-schema the validator
  uses internally)
- `examples/schemas/temperature_sensor.yaml` (exact bytes in §4.8)

All SHA256'd in `M5-done.md` per §6.3.

## 5. Risks and mitigations

### Rank 1 — Benchmark throughput < 100k simple / 50k complex

Decoder hot path: QByteArray slicing, memcpy for multi-byte,
endianness swap, scale/offset math. For a 16-byte frame with 4 fields,
that's ~dozens of ns of arithmetic. 100k frames/sec = 10 µs per
frame — well above raw decode cost. The real risk is Qt overhead
(QString construction for signalId, shared_ptr ref in sink call) and
MetricsRegistry atomic increments.

**Mitigation**: pre-compute signalId QStrings at decoder construction;
use QString references in the hot loop. Reserve Metric* pointers at
ctor time. Benchmark-profile if we're below threshold.

**Estimated probability of miss**: 15 %.

### Rank 2 — yaml-cpp line numbers sparse on deep errors

yaml-cpp reports line numbers for parse errors (`YAML::ParserException`)
but for semantic errors we construct ourselves (e.g., missing field),
we need to manually track node positions via `node.Mark()`. Some
error paths (nested maps inside sequences) may lose line info.

**Mitigation**: test every error-path fixture in §5.3 individually;
accept `-1` for a documented subset if unavoidable. Spec §7.5 HALT
fires only if > 20 % of errors lose line info.

**Estimated probability of HALT**: 10 %.

### Rank 3 — C++23 std::expected toolchain issue (§7.3)

Local GCC 13 compiles `std::expected` cleanly under `-std=c++23`.
Ubuntu 24.04 ships GCC 13.2 (our CI runner). Should be fine. If CI
breaks, fall back to Option B (struct).

**Estimated probability**: 5 %.

### Rank 4 — Main-thread block during decoder registration (§7.6)

Typical schema: < 2 KB yaml, parse + validate + decoder construct
in single-digit ms. `DecoderRegistrar::onPipelineAttached` runs on
the main thread (the signal is emitted on the thread that called
`PipelineManager::attach`, which is the main thread from
ConnectionManager). Budget: 100 ms. Plenty of headroom for V1
schemas (the largest example is ~50 lines).

**Mitigation**: measure in S7 integration test; HALT if any run
exceeds 100 ms.

**Estimated probability**: < 5 %.

### Rank 5 — Schema v1 design ambiguity surfaced post-merge

Once frozen, yaml files relying on an ambiguous behavior lock us in.
Example: does `scale: 0.1` apply before or after bit extraction?
Where does `unit` go for a bit field vs a parent byte field?

**Mitigation**: during S2 (schema data structures + validator), the
validator's rules are the spec; the test fixtures exercise every
corner. Document ambiguities in `.claude/M5-concerns.md` and resolve
with the user before freezing.

**Estimated probability of post-freeze ambiguity**: 15 %. We'll know
this one during the acceptance review on `M5-done.md`.

## 6. Relationship to prior milestones' assets

- **M2 freeze**: `RawFrame`, `WatermarkTracker`, `MetricsRegistry`,
  `MpscQueue`, platform utilities, observability logger — all unchanged.
- **M3 freeze**: driver interfaces, concrete drivers — unchanged.
- **M4 freeze**: `FrameSink`, `FramePipeline`, `PipelineManager` —
  `DecoderInterface` derives from `FrameSink` (the hook M4 left for
  us); `DecoderRegistrar` subscribes to `PipelineManager::pipelineAttached`.
- **ADR-003** (permissive metric naming): used verbatim — no
  sanitization needed for decoder metrics.
- **M3/M4 integration test infrastructure** (`CoreAppHolder`,
  `pumpUntil`, `MockDriver`, UDP loopback helpers): reused for
  decoder integration tests.

## 7. Assumptions taken as given

- `src/decode/` (not `src/decoder/`) is the directory we use — matches
  existing M0 scaffolding; root CMakeLists already has
  `add_subdirectory(src/decode)`. Spec text uses `src/decoder/` but
  file-path layout is implementer-judgment per spec §2 silence on
  exact directory.
- `nlohmann_json` is an existing dependency suitable for both the
  meta-schema parsing and `schema_lint --json` output.
- Root `CMakeLists.txt` bumps to `set(CMAKE_CXX_STANDARD 23)` for
  `std::expected` availability. This is a build-flag change, not a
  freeze violation; M2/M3/M4 code still compiles cleanly under C++23.
- `tools/schema_lint/` follows the `tools/crash_test/` pattern:
  standalone CMake, not wired into main build, tests run from that
  subdirectory.
- No UI for schema selection in M5 — hard-coded `driverType → schemaPath`
  map per §4.6; M9 replaces.

## 8. What M5 does NOT do (scope control)

Restated from spec §2.2 for review:

1. No modifications to M2/M3/M4 frozen files.
2. No signal buffer / time-series storage (M6).
3. No expression evaluation (M7).
4. No GUI schema editor (V1.5).
5. No variable-length frame support (schema v2 or custom decoders).
6. No CRC verification (schema v1.1 if needed).
7. No protocol-specific concrete decoders (`UdpDecoder`, `SerialDecoder`).
8. No multi-decoder per driver.
9. No performance panel UI changes.

If any of these become tempting, HALT.
