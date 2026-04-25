# M5 — Progress log

## Session metadata

- Phase 5 execution begins 2026-04-25.
- Branch: `milestone/M5` at `78a715c` (understanding + plan from Phase 3).
- Plan: `.claude/M5-plan.md`, 10 subtasks S1–S10.
- Understanding: `.claude/M5-understanding.md`.
- Remote: `git@github.com:mornthx/signalforge.git`.

## Subtask log

Each subtask appends a start entry and a close entry.

---

### S1 — C++23 toolchain bump + src/decode/ scaffolding + DecoderInterface + LoggingSignalValueSink (start)

**Goal**: per plan §2 S1, establish the decoder module foundation:

1. **Critical gate**: bump `CMAKE_CXX_STANDARD` from 20 to 23 in the
   root `CMakeLists.txt`. Verify clean build on all three presets
   (Debug / Release / debug-asan). If any preset breaks in a
   non-trivially-fixable way, HALT with plan §3 pre-drafted Option
   A / Option B comparison.
2. Create `src/decode/` (reusing the M0 empty placeholder; root
   CMakeLists already has `add_subdirectory(src/decode)`).
3. Deliver M5 spec §4.1 verbatim:
   - `src/decode/decoder_interface.hpp` — freeze-surface header with
     `SignalValue` alias, `SignalType` enum, `SignalMetadata` struct,
     `SignalValueSink` abstract, `DecoderInterface` abstract
     (inherits `FrameSink` from M4).
4. Deliver `LoggingSignalValueSink.{hpp,cpp}` per spec §4.5 — test
   stub writing each signal at INFO + atomic counters for
   introspection.
5. `src/decode/CMakeLists.txt` defining `signalforge_decoder` STATIC
   library. PUBLIC deps: `Qt6::Core`, `signalforge_frame`,
   `signalforge_pipeline`. PRIVATE: observability, platform.
6. `tests/unit/decode/` with placeholder `decoder_test.cpp` (3–4
   unit cases verifying the stub's behaviour and that
   `DecoderInterface`'s FrameSink defaults work).

No M2/M3/M4-frozen .hpp touched. C++23 bump is a build-flag change —
no source-level change required in existing code (C++23 is a strict
superset of C++20 for our usage).

### S1 — C++23 toolchain bump + src/decode/ scaffolding + DecoderInterface + LoggingSignalValueSink (close)

**Files delivered**:
- Root `CMakeLists.txt`: `CMAKE_CXX_STANDARD` 20 → 23 (one-line change).
- `src/decode/CMakeLists.txt`: renamed target `signalforge_decode` →
  `signalforge_decoder` (matches spec namespace); PUBLIC deps
  Qt6::Core + signalforge_frame + signalforge_pipeline; PRIVATE
  observability + platform.
- `src/decode/decoder_interface.hpp` — frozen-at-M5-close:
  * `SignalValue = variant<bool, int64_t, double, QString>`
  * `SignalType` enum (4 values)
  * `SignalMetadata` struct (7 fields, per spec §4.1)
  * `SignalValueSink` abstract (3 virtuals)
  * `DecoderInterface` abstract (3 pure virtuals + inherits FrameSink)
- `src/decode/logging_signal_value_sink.{hpp,cpp}` — M5 test stub.
  Per-type atomic counters; logs at SF_LOG_INFO.
- Removed M0 placeholder: `src/decode/placeholder.{cpp,hpp}` (the
  `signalforge::decode` namespace + `signalforge_decode` target) —
  confirmed unused by any other TU or CMake target.
- `tests/unit/decode/` new subdirectory + 4 unit cases for
  `LoggingSignalValueSink`.
- `.claude/M5-concerns.md` created with the first deviation entry:
  renamed `SignalValueSink::onSignalsRegistered` parameter `signals`
  → `signalsList` to avoid Qt macro collision (ABI-identical;
  parameter names are not part of the signature).

**Deviation log**: see `.claude/M5-concerns.md` entry #1.

**Build verification**:
- Debug (C++23): clean.
- Release (C++23): clean.
- debug-asan (C++23): clean.
- `std::expected` available (verified pre-flight before the commit;
  further S2 use will exercise it).

**Test verification**:
- Debug: 221/221 tests pass (+ 4 new LoggingSignalValueSink cases).
- Release: 221/221 tests pass.
- debug-asan: runtime still blocked locally by `/etc/ld.so.preload`;
  CI authoritative.

**Format**: clang-format clean on all new files.

**Freeze scope**: no M2/M3/M4-frozen .hpp modified. The CXX_STANDARD
bump is a build-flag change; C++23 is a strict superset of C++20 for
our usage. `DecoderInterface` + `SignalValueSink` + `SignalValue` +
`SignalType` + `SignalMetadata` enter the M5 freeze at M5 close.

**§7.3 gate**: PASSED. `std::expected` compiles cleanly under the
repo's new C++23 standard on the local GCC 13 toolchain; CI will
confirm on the Ubuntu 24.04 matrix.

**Time**: ~1 h (under 3 h plan estimate; the `signals` Qt-macro fix
cost ~10 min).

---

## CI verification status

### S1 push 2026-04-25 (initial)

S1 commit `d5e9317` pushed to `milestone/M5`. CI run 24908582887
canceled in 3s with billing block: "The job was not started because
recent account payments have failed or your spending limit needs to
be increased." All three matrix jobs (Debug, Release, debug-asan)
canceled.

Local verification on the push:
- Debug + Release + debug-asan: build clean.
- Debug + Release: 221/221 tests pass.
- debug-asan: runtime blocked by host `/etc/ld.so.preload`; CI is
  the authoritative gate.
- `clang-format --dry-run -Werror` clean on changed files.

Per session instructions: stop and await user direction.

### Update 2026-04-25 (continued)

Repository made public during this session. GitHub Actions usage on
the account had already crossed the 2000-minute monthly free tier
(1200 minutes consumed before signalforge transitioned to public;
specific source breakdown not investigated). Public-repo unlimited-
minutes policy applies prospectively but does NOT retroactively
clear the current month's quota at the account level.

Decision: continue M5 with local-only verification through the end
of this billing month. Quota resets on the 1st of the next month, at
which point CI will resume automatically on the next push.

License: MIT (commit `8183ff5` is on `main`, not `milestone/M5`; it
does not affect M5 work).

All M5 commits during this period will be tagged
`[ci-skip-watch: billing-blocked]` in the commit body, and S10 close
will enumerate them in `M5-done.md`.

---

### S2 — Schema data structures + SchemaValidator + yaml schema v1 canonical docs (start)

**Goal**: per plan §2 S2, deliver:

1. `src/decode/schema.hpp` — pure value types (Endianness,
   FieldEncoding, BitFieldDef, FieldDef, LayoutMatch, Layout, Schema)
   per spec §4.3.
2. `src/decode/schema_validator.{hpp,cpp}` per spec §4.4:
   - `std::expected<Schema, std::vector<ValidationError>>` result type
     (C++23 `<expected>` — verified available in S1).
   - yaml-cpp-driven parsing using `YAML::Node::Mark()` for line
     numbers (-1 fallback documented per error class).
   - Validation rules per spec §4.4 sequence (1–5).
3. `schemas/decoder_schema_v1.yaml` — canonical example exercising
   every encoding + endianness + bit fields with inline comments.
4. `schemas/decoder_schema_v1.json` — JSON description of the meta-
   schema (used as documentation; validator is hand-written, driven
   by the spec).
5. `src/decode/CMakeLists.txt`: link `yaml-cpp` PRIVATE; add new
   sources.
6. Minimal structural unit tests in `tests/unit/decode/decoder_test.cpp`
   (2–3 cases verifying validator round-trip on the canonical example
   + a basic invalid input rejection); the full §5.2 enumerated
   coverage lands in S6.

No M2/M3/M4-frozen .hpp touched. yaml-cpp is already a project
dependency (no new top-level dep per CLAUDE.md §Forbidden #1).

### S2 — Schema data structures + SchemaValidator + yaml schema v1 canonical docs (close)

**Files delivered**:
- `src/decode/schema.hpp` — `Endianness`, `FieldEncoding` (13
  values frozen at M5 close), `BitFieldDef`, `FieldDef`, `LayoutMatch`,
  `Layout`, `Schema`. `FieldDef::offsetTransform` chosen over the
  spec's duplicate `offset` member name (deviation #2).
- `src/decode/schema_validator.{hpp,cpp}` — `std::expected<Schema,
  std::vector<ValidationError>>` result type. yaml-cpp `Mark()` line
  numbers (1-based, -1 fallback). Validation pipeline:
  - top-level `schema_version` (== 1) + `layouts` (non-empty seq)
  - per layout: `name`, `endianness` (required), `match.offset`,
    `match.bytes` (in [0,255]), `min_payload_bytes`, non-empty `fields`
  - per field: `name` (unique within layout), `offset` (>=0), `encoding`
    (one of 13), `size_bytes` (canonical for numeric, {1,2,4,8} for
    bitfield, any positive for fixed_string), optional `endianness`
    override, optional `scale` / `offset_transform` / `unit` /
    `description`. Multi-byte numeric requires resolvable endianness.
    `bool` rejected as top-level encoding (use `bit_count: 1` inside a
    `bitfield`). BitField parents require non-empty, non-overlapping,
    in-range `bit_fields` with unique names.
  - duplicate detection at every list (layouts, fields, bit_fields).
- `schemas/decoder_schema_v1.yaml` — canonical example exercising all
  13 encodings, both endiannesses (per-layout default + per-field
  override), single-bit/multi-bit/cross-byte bit fields, scale +
  offset_transform, fixed_string, multi-layout dispatch.
- `schemas/decoder_schema_v1.json` — JSON-Schema description of the
  meta-schema (used as documentation; validator is hand-written).
- `src/decode/CMakeLists.txt` — added new sources; linked
  `yaml-cpp::yaml-cpp` PRIVATE.
- `tests/unit/decode/CMakeLists.txt` — `SIGNALFORGE_REPO_ROOT`
  compile-time path so the canonical schema test can locate the
  source-tree yaml file.
- `tests/unit/decode/decoder_test.cpp` — added 4 SchemaValidator
  cases (rejected empty content; minimal valid round-trip; missing
  layout endianness flagged; canonical v1 schema validates cleanly).
- `.claude/M5-concerns.md` entries #2 (`offset` rename) and #3 (`bool`
  reserved for bit-field children only).

**Build verification**:
- Debug (C++23): clean.
- Release (C++23): clean.
- debug-asan (C++23): clean.

**Test verification**:
- Debug: 225/225 tests pass (+ 4 new SchemaValidator cases).
- Release: 225/225 tests pass.
- debug-asan: runtime still blocked locally by `/etc/ld.so.preload`;
  CI authoritative when quota resets.

**Format**: `clang-format --dry-run -Werror` clean on changed files.

**Freeze scope**: no M2/M3/M4-frozen .hpp modified. The `Schema` struct
is internal representation (not frozen per spec §6.2). Schema v1 yaml
format and `SchemaValidator` public API + `ValidationResult` /
`ValidationError` enter the M5 freeze at M5 close.

**§7.5 (line numbers) preliminary**: yaml-cpp's `Mark()` returns valid
positions for all syntax errors observed in S2 unit tests; -1 only
appears in synthetic missing-key cases where the parent map's mark is
substituted (which is the documented behavior). S6 will exercise the
full §5.2 error matrix and verify the < 20% threshold.

**Time**: ~2 h (under 6 h plan estimate; the spec's duplicate `offset`
member required the deviation entry but the resolution was
straightforward).

---

### S3 — SchemaDecoder (start)

**Goal**: per plan §2 S3, deliver:

1. `src/decode/schema_decoder.{hpp,cpp}` per spec §4.2:
   - Implements `DecoderInterface` (and therefore `FrameSink`).
   - Holds a pre-validated `Schema` + `driverId`.
   - `tryDecodeFrame`: iterates layouts; first whose `match.bytes`
     equal the payload at `match.offset` wins.
   - `extractField`: dispatch on `FieldEncoding`:
     - `Int8/16/32/64`, `Uint8/16/32/64` — endianness-aware byte
       assembly into `int64_t`. Sign-extend signed types. Apply
       `scale` / `offset_transform` linear transform → emit `double`
       when transform present, otherwise `int64_t`.
     - `Float32/Float64` — endianness-aware byte assembly + memcpy to
       float/double; cast/promote to `double`. Apply transform if
       present.
     - `BitField` — read container as little-endian/big-endian uint64,
       shift right by `bit_start`, mask `bit_count` bits. Emit `bool`
       for `bit_count == 1`, else `int64_t`.
     - `FixedString` — extract byte range, honor null terminator,
       decode as UTF-8 → `QString`.
   - Metric registration at ctor (5 metrics per spec §3.10):
     - `decoder_frames_decoded_<driverId>` (counter)
     - `decoder_frames_unmatched_<driverId>` (counter)
     - `decoder_frames_malformed_<driverId>` (counter)
     - `decoder_signals_emitted_<driverId>` (counter)
     - `decoder_last_decode_us_<driverId>` (gauge)
   - Unmatched frames: SF_LOG_WARN with first 16 hex bytes; counter
     incremented; no signal emitted.
   - Malformed frames: SF_LOG_WARN with reason; counter incremented.
   - `setSignalSink` is mutex-protected; idempotent (warn on duplicate).
2. Update `src/decode/CMakeLists.txt`.
3. 1–2 spot-check unit tests in `tests/unit/decode/decoder_test.cpp`
   (full §5.2 coverage deferred to S6).

No M2/M3/M4-frozen .hpp touched.

### S3 — SchemaDecoder (close)

**Files delivered**:
- `src/decode/schema_decoder.hpp` — `SchemaDecoder` concrete decoder
  inheriting `DecoderInterface`. Pre-computed `CachedLayout` /
  `CachedField` structures map field declarations to metadata indices
  (one per signal; one per bit slice for BitField parents) so that
  `onFrame` does no per-frame metadata-construction work.
- `src/decode/schema_decoder.cpp`:
  - Constructor: registers 5 metrics under namespace
    `decoder_<metric>_<driverId>` per spec §3.10. Walks the schema
    once to build `metadataCatalog_` + the `layoutCache_`.
  - `setSignalSink`: mutex-guarded; idempotent on duplicate pointer
    (warns and ignores). On a real change, unregisters the previous
    sink and registers the new one.
  - `onFrame`: dispatches to `tryDecodeFrame`. Updates
    `decoder_last_decode_us_<driverId>` gauge each call. Logs WARN
    + increments `decoder_frames_unmatched_<driverId>` when no layout
    matched.
  - `tryDecodeFrame`: walks layouts in order; first whose magic
    matches wins. Honors `min_payload_bytes` + per-field bounds
    checks (both report through the malformed counter and a structured
    SF_LOG_WARN). Per-field dispatch:
    - **Numeric integers** (int8..uint64): byte assembly via
      endianness-aware `readUnsigned`, sign-extension when signed,
      linear transform `raw * scale + offset` applied when either is
      present (output type promoted to `double`).
    - **Floats** (float32/float64): byte-swap + memcpy into a stack
      `float`/`double`, promoted/cast to `double`. Linear transform
      applied if present.
    - **BitField**: read sizeBytes-byte container with endianness;
      shift + mask per slice. `bit_count == 1` → `bool` signal;
      `bit_count > 1` → `int64_t` signal.
    - **FixedString**: byte slice, null-terminator-honored UTF-8
      decode → `QString` signal.
  - Sink callbacks wrapped in try/catch; sink-thrown exceptions are
    logged at ERROR but never propagated to the pipeline.
  - Destructor: unregisters from the current sink (if any) safely.
- `src/decode/CMakeLists.txt`: added new sources.
- `tests/unit/decode/decoder_test.cpp`: added 3 spot-check cases:
  - `signalMetadata` enumerates every field + bit field with correct
    types (Int64 / Double / Bool inferred per the rules above).
  - Matched frame produces all expected signals with correct counts
    per type (alarm bool, mode/code int64, magic/counter int64,
    temperature double).
  - Unmatched frame emits no signals.

**Build verification**:
- Debug (C++23): clean.
- Release (C++23): clean.
- debug-asan (C++23): clean.

**Test verification**:
- Debug: 228/228 tests pass (+ 3 SchemaDecoder spot-checks).
- Release: 228/228 tests pass.
- debug-asan: runtime still blocked locally by `/etc/ld.so.preload`;
  CI authoritative when quota resets.

**Format**: `clang-format --dry-run -Werror` clean on changed files.

**Freeze scope**: no M2/M3/M4-frozen .hpp modified. `DecoderInterface`
(frozen at S1 + M5 close) is implemented but not changed. `SchemaDecoder`
itself is not frozen per spec §6.2.

**Notes**:
- `SignalMetadata::offset` (frozen field) is populated from the
  schema's `offsetTransform` (the linear-transform offset, named
  per deviation #2). This bridge is internal to the decoder and
  invisible to consumers.
- One sink-call try/catch per signal is intentional: spec §FrameSink
  and pipeline §error-handling guarantee that a faulty sink does not
  crash the producer; the same discipline applies to a SignalValueSink.

**Time**: ~2 h (under 6 h plan estimate).

---

### S4 — DecoderRegistrar (start)

**Goal**: per plan §2 S4 + spec §4.6, deliver:

1. `src/decode/decoder_registrar.{hpp,cpp}`:
   - QObject listening on `PipelineManager::pipelineAttached`.
   - Constructor takes `PipelineManager*` and a
     `std::unordered_map<QString, QString>` driver-type → schema-path.
   - Hard-coded for M5 (M9 replaces with UI-driven selection).
   - Driver type extracted from driverId prefix before the `:`.
   - On a recognized type, validates the schema file via
     `SchemaValidator::validateFile`; on success constructs a
     `SchemaDecoder` (shared_ptr) and registers via `pipeline->addSink`.
   - On `pipelineDetached`, releases the decoder for that driverId.
   - Optional process-wide `LoggingSignalValueSink` wired as the
     default for M5; M6 will replace.
   - Spec §7.6: validate + construct must complete < 100 ms on the
     calling (UI / event-loop) thread; for typical schemas this is
     milliseconds.
2. 3 unit tests per plan: empty map → no decoders attached;
   unrecognized type → skip with INFO log; valid map → decoder
   registered + sinkCount increments.

No M2/M3/M4-frozen .hpp touched. PipelineManager and FramePipeline
are M4-frozen and consumed via their public API only.

### S4 — DecoderRegistrar (close)

**Files delivered**:
- `src/decode/decoder_registrar.{hpp,cpp}`:
  - `DecoderRegistrar` is a `QObject` taking `PipelineManager*`, a
    `std::unordered_map<QString, QString>` driver-type → schema-path,
    and an optional `std::shared_ptr<SignalValueSink>` default sink.
  - Connects to `pipelineAttached` and `pipelineDetached` signals at
    construction (auto-disconnects via `QObject` parent-child cleanup
    when destroyed).
  - `onPipelineAttached`: extracts the type prefix via `driverTypeOf`,
    looks up the schema path, runs `SchemaValidator::validateFile`,
    constructs `SchemaDecoder`, attaches the default sink (if any),
    registers via `pipeline->addSink`, and tracks the decoder under
    its `driverId`.
  - `onPipelineDetached`: releases the decoder for that `driverId`;
    `SchemaDecoder` destructor unregisters the sink cleanly.
  - Unknown types or empty/missing schema paths log INFO and skip
    silently (per spec §4.6 — not every driver has a schema in M5).
  - Validation failures log a per-error ERROR including filePath +
    line + fieldPath + message; the pipeline is not blocked.
- `src/decode/CMakeLists.txt`: added `decoder_registrar.{cpp,hpp}` to
  sources, set `AUTOMOC ON` (the registrar is the first QObject in
  this static lib).
- `tests/unit/decode/decoder_test.cpp`: added 4 cases:
  - `driverTypeOf` splits on the first `:` (handles `tcp:host:port`,
    plain `nocolon`, etc.).
  - Empty schema map yields no decoders (no signal slots fire).
  - Unknown driver type is skipped (no decoder created even when
    the slot is invoked manually with a bogus `driverId`).
  - Valid map + canonical schema: `onPipelineAttached` invocation
    creates a decoder, registers it as a sink (so `sinkCount` rises
    to 1), and `onPipelineDetached` cleans it up.
- `tests/unit/decode/decoder_test.cpp` also gained a `CoreAppHolder`
  fixture (mirrors the pipeline test) to provide a `QCoreApplication`
  + register Qt metatypes for the FramePipeline path.

**Build verification**:
- Debug (C++23): clean.
- Release (C++23): clean.
- debug-asan (C++23): clean.

**Test verification**:
- Debug: 232/232 tests pass (+ 4 DecoderRegistrar cases).
- Release: 232/232 tests pass.
- debug-asan: runtime still blocked locally by `/etc/ld.so.preload`.

**Format**: `clang-format --dry-run -Werror` clean on changed files.

**Spec §7.6 (UI thread block ≤ 100 ms)**: validate + construct
measured in microseconds for the canonical example (the Debug build
runs the full test in ~13 ms wall-clock for the registrar TC, which
includes pipeline + manager + decoder construction and signal
delivery). No HALT trigger.

**Freeze scope**: no M2/M3/M4-frozen .hpp modified. The registrar is
not part of the M5 freeze surface (see spec §6.2).

**Time**: ~1 h (under 2 h plan estimate; the slot-invocation test
pattern via `QMetaObject::invokeMethod` keeps the tests
deterministic without needing an event-loop spin).

---

### S5 — Example schemas + invalid fixtures (start + close)

**Goal**: per plan §2 S5, deliver:

1. `examples/schemas/temperature_sensor.yaml` — verbatim from spec
   §4.8 (16-byte telemetry frame with timestamp + temperature +
   pressure + status bit field + crc + padding).
2. `examples/schemas/modbus_style.yaml` — 2-layout schema with magic
   byte at offset 1 (function code dispatch), big-endian, integer +
   bit fields, including a cross-byte 8-bit `device_id` slice and a
   16-bit bitfield container.
3. `tests/integration/fixtures/valid_schemas/` — copies of the two
   examples (consumed by S7 integration tests).
4. `tests/integration/fixtures/invalid_schemas/` — six fixtures
   exercising distinct §5.2 error categories:
   - `missing_version.yaml`
   - `missing_endianness.yaml`
   - `invalid_encoding.yaml`
   - `bit_overlap.yaml`
   - `bit_overflow.yaml`
   - `duplicate_field.yaml`

**Verification**: all 8 fixtures were validated against the S2
`SchemaValidator`; expected pass / fail outcomes matched, and every
invalid fixture produced an actionable error message with a 1-based
line number and the dotted field path:

```
OK   examples/schemas/temperature_sensor.yaml (valid)
OK   examples/schemas/modbus_style.yaml (valid)
OK   invalid_schemas/missing_version.yaml (invalid)
    schema_version:2 — 'schema_version' is required (must be 1)
OK   invalid_schemas/missing_endianness.yaml (invalid)
    layouts[0].endianness:6 — 'endianness' is required at layout level
    layouts[0].fields[0].endianness:12 — multi-byte field 'pressure' requires endianness
OK   invalid_schemas/invalid_encoding.yaml (invalid)
    layouts[0].fields[0].encoding:13 — invalid encoding 'not_a_real_type'
OK   invalid_schemas/bit_overlap.yaml (invalid)
    layouts[0].fields[0].bit_fields:11 — bit ranges overlap at bits [0, 2) and [1, 3)
OK   invalid_schemas/bit_overflow.yaml (invalid)
    layouts[0].fields[0].bit_fields[0]:16 — bit range [5, 10) does not fit in 8-bit container
OK   invalid_schemas/duplicate_field.yaml (invalid)
    layouts[0].fields[1].name:15 — duplicate field name 'counter' within layout
```

This is the §7.5 line-number coverage check: 6/6 invalid fixtures
have valid 1-based line numbers (well above the 80% threshold).

**No new tests in S5** — fixtures power S6 (unit) and S7 (integration).

**Build verification**: not exercised. Per CLAUDE.md §Required #2
exception, fixture-only commits do not require a rebuild when the
build graph is unaffected. Build and test counts unchanged from S4.

**Format**: yaml/json fixtures are user-authored format examples; not
subject to clang-format.

**Freeze scope**: yaml fixtures use schema v1 syntax exclusively.
Examples and the canonical schema lock the v1 contract together at
M5 close.

**Time**: ~30 min.

---

### S6 — Unit tests ≥ 85 % (start)

**Goal**: per plan §2 S6 + spec §5.2, expand
`tests/unit/decode/decoder_test.cpp` to cover every enumerated case:

- **SchemaValidator** (~10 cases): minimal valid; missing
  `schema_version`; wrong `schema_version` (e.g., 2); missing layout
  `endianness` with multi-byte field; invalid `encoding` enum;
  `size_bytes` inconsistent with encoding; duplicate field names;
  bit-field overlap; bit-field overflow; bit-field zero count.
- **SchemaDecoder** (~9 cases): construct + signalMetadata; magic
  match → expected signals; unknown magic → unmatched counter;
  short payload → malformed counter, no signals; multi-layout
  dispatch (two magics → two layouts); per-field endianness override
  vs layout default; bit_count == 1 → bool, bit_count > 1 → int64;
  scale/offset applied to numeric; `fixed_string` extraction (null
  terminator honored).
- **LoggingSignalValueSink**: existing 4 cases already cover the §5.2
  list; no expansion needed.

S6 also runs the existing fixtures from S5 against the validator to
exercise the file-loading path (vs in-memory yaml), giving line-number
coverage from real files.

No M2/M3/M4-frozen .hpp touched.

### S6 — Unit tests ≥ 85 % (close)

**Files delivered**:
- `tests/unit/decode/decoder_test.cpp` expanded with 19 new cases:

  **SchemaValidator** (12 cases — exceeds the ~10 plan estimate):
  - schema_version != 1 reports "supports versions: [1]" with the
    declared version inline.
  - Invalid encoding string surfaces both the offending value and
    the full allowed-list in the message.
  - size_bytes mismatched with encoding reports the encoding's
    canonical size in the message ("expected 2" for uint16).
  - Duplicate field names within a layout are flagged.
  - Bit-field range overlap is detected and reported with the
    overlapping bit ranges in the message.
  - Bit-field overflow is detected with the offending range.
  - bit_count == 0 is rejected at the bit_count fieldPath.
  - `bool` as top-level encoding is rejected with a message
    pointing the user to `bit_count: 1` inside a `bitfield`.
  - Validator + invalid_encoding fixture reports a valid 1-based
    line number.
  - Every invalid_schemas/* fixture validates to a non-empty error
    list (smoke test for the full §5.2 fixture set).
  - examples/temperature_sensor.yaml validates cleanly with 6 fields.
  - examples/modbus_style.yaml validates cleanly with 2 layouts.

  **SchemaDecoder** (7 cases — covers the §5.2 list):
  - Short-payload frame (matched magic but below min_payload_bytes)
    is malformed; emits no signals.
  - Multi-layout dispatch: two distinct magics route to two layouts;
    consecutive frames produce one signal each.
  - Per-field endianness override vs layout default: little-endian
    layout with one big-endian field; both decode correctly.
  - Scale + offset_transform applied to int16 produces the expected
    double signal (raw 2000 * 0.01 - 10.0 = 10.0).
  - fixed_string honors the null terminator: `"fw-42\0"` + garbage
    decodes to `"fw-42"`.
  - Cross-byte bit field (size 2 container with bits 0..7 + 4..11
    + 12..15 slices) extracts correctly via little-endian assembly.
  - float32 little-endian decode (1.5 → 0x3FC00000 byte sequence).

  **LoggingSignalValueSink**: existing 4 cases already cover §5.2.

**Validator change**: bumped `bool` encoding rejection to fire before
the numeric-canonical-size branch (it had been unreachable because
`bool` has `sizeBytes == 1` and was being absorbed by the numeric
path). The new ordering checks for `Bool` explicitly first. This is
not a freeze deviation — `bool` was always intended as a bitfield-
child-only encoding per spec §3.4 and concerns.md #3.

**Build verification**:
- Debug (C++23): clean.
- Release (C++23): clean.
- debug-asan (C++23): clean.

**Test verification**:
- Debug: 251/251 tests pass (+ 19 new).
- Release: 251/251 tests pass.
- debug-asan: runtime still blocked locally by `/etc/ld.so.preload`.

**Coverage**: not measured numerically (no llvm-cov / gcov harness in
this build). The §5.2 enumeration is exhaustively covered by the 19
new tests + 4 pre-existing sink tests + 4 spot-check decoder tests
from S3 = 27 cases on the decoder modules. All branches of the
validator's encoding switch and BitField sub-validation are touched
by at least one test. Quantitative coverage will be the CI gate when
quota resets.

**Format**: clean.

**Freeze scope**: no M2/M3/M4-frozen .hpp modified. The validator
implementation tweak (Bool ordering) is in
`schema_validator.cpp` (not frozen per spec §6.2).

**Time**: ~1.5 h (under 4 h plan estimate).
