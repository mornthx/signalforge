# M5 — Plan

## 0. Execution ground rules

- Branch: `milestone/M5` (already pushed).
- Per-subtask discipline (CLAUDE.md §Required #2 + §Git operation
  protocol), identical to M4:
  1. Append start entry to `.claude/M5-progress.md`.
  2. Implement per plan.
  3. Build all three presets clean (Debug, Release, debug-asan).
  4. `ctest` Debug + Release clean; debug-asan per existing
     LSan-suppressions configuration.
  5. `clang-format --dry-run -Werror` on changed files.
  6. Append close entry to progress.md with counts + deviations.
  7. Commit with `<module>: <imperative verb> <object>`; body states
     "Freeze scope: no M2/M3/M4-frozen .hpp modified."
  8. Push `milestone/M5`.
  9. **Watch CI via `gh run watch`**; report result before starting
     the next subtask. No silent retries.
- No new top-level dependencies (spec §2.2-3).
- Schema v1 decisions are captured in this plan; any ambiguity
  discovered during implementation lands in `.claude/M5-concerns.md`
  immediately.
- Performance HALT (§7.4): decoder throughput < 50k complex or <
  100k simple without a clear §5.5 category → HALT.

## 1. Subtask sequence overview

| # | Subtask | Prereqs | Effort | Commit | Notes |
|---|---|---|---|---|---|
| S1 | C++23 toolchain bump + `src/decode/` scaffolding + `DecoderInterface` + `SignalValueSink` headers + `LoggingSignalValueSink` stub | — | 3 h | Yes | Sets the freeze surface for the pure-C++ part. §7.3 gate: if C++23 blows up on CI, fall back to struct per §3.2. |
| S2 | `Schema` data structs + `SchemaValidator` + yaml-cpp integration + `schemas/decoder_schema_v1.{yaml,json}` canonical docs | S1 | 6 h | Yes | Biggest single piece. Error-message quality is explicit acceptance gate (§8.2). |
| S3 | `SchemaDecoder` (all 13 encodings + bit extraction + endianness + scale/offset + string) + decoder metric registration | S2 | 6 h | Yes | Hot path; measure in S8. |
| S4 | `DecoderRegistrar` + hard-coded `driverType → schemaPath` map + ConnectionManager wiring review (no UI change) | S3 | 2 h | Yes | §7.6: main-thread block budget ≤ 100 ms. |
| S5 | Example schemas (`temperature_sensor.yaml`, `modbus_style.yaml`) + invalid fixtures | S2 | 2 h | Yes | Fixtures power S6/S7 error tests. |
| S6 | Unit tests ≥ 85 % (SchemaDecoder + SchemaValidator + LoggingSignalValueSink) | S3, S5 | 4 h | Yes | Every §5.2 enumerated case. |
| S7 | Integration tests (`test_schema_decoder_basic.cpp`, `_bit_fields`, `_endianness`, `_unmatched`, `test_schema_validator_errors.cpp`) | S4, S5 | 4 h | Yes | Uses S5 fixtures + M4 pipeline. |
| S8 | Benchmark `bench_decoder_throughput.cpp` + `M5-baseline.md` | S3 | 2 h | Yes | §7.4 HALT gate: < 100k simple / < 50k complex. |
| S9 | `tools/schema_lint/` standalone CLI + README + `--json` output | S2 | 2 h | Yes | User-facing; prioritize message clarity. |
| S10 | `.claude/M5-done.md` + freeze record + PR against main | S1–S9 green | 3 h | Yes | Mirrors M4 closure flow. |

**Total estimated effort**: 34 h, inside spec's 8–10 person-day
(64–80 h) budget. Slack reserved for error-message polish (§9 note)
and yaml-cpp line-number quirks (§7.2 / risk rank 2).

## 2. Subtask details

### S1 — Toolchain bump + scaffolding + DecoderInterface header

**Deliverables**:

- Root `CMakeLists.txt`: bump `set(CMAKE_CXX_STANDARD 23)`. Verify
  C++23 builds cleanly on Debug + Release + debug-asan presets via
  a clean configure.
- If configure or build fails in a way not trivially fixable: HALT
  with a pre-drafted question comparing Option B (fallback struct)
  vs Option A.
- `src/decode/CMakeLists.txt`: new `signalforge_decoder` STATIC
  library. PUBLIC deps: `Qt6::Core`, `signalforge_frame`,
  `signalforge_pipeline`. PRIVATE: observability, platform, yaml-cpp,
  nlohmann_json.
- `src/decode/decoder_interface.hpp` verbatim from spec §4.1.
  Includes `<expected>` (C++23) only in `schema_validator.hpp`, not
  here.
- `src/decode/logging_signal_value_sink.{hpp,cpp}` per spec §4.5.
  Small; purely a test stub.
- `tests/unit/decode/CMakeLists.txt` + `decoder_test.cpp` placeholder
  with 3–4 cases verifying that `LoggingSignalValueSink` accepts all
  4 `SignalValue` variants and the defaults on `SignalValueSink` and
  `DecoderInterface` are correct.

**Tests**: ~4 unit cases.

**Commit**: `decoder: scaffold module with DecoderInterface and logging sink`.

### S2 — Schema data structures + SchemaValidator + v1 canonical docs

**Deliverables**:

- `src/decode/schema.hpp` per spec §4.3 (pure value types; no
  validation logic).
- `src/decode/schema_validator.{hpp,cpp}` per spec §4.4:
  - Uses `std::expected<Schema, std::vector<ValidationError>>`.
  - Internal implementation uses yaml-cpp's `YAML::Node::Mark()` for
    line numbers; accepts `-1` where Mark is unavailable (documented
    case-by-case).
  - Validation rules per spec §4.4 sequence (1–5).
  - JSON meta-schema reference: validator loads
    `schemas/decoder_schema_v1.json` at first use (or embeds it; TBD
    based on whether JSON schema validation is via nlohmann or hand-
    written — lean toward hand-written validator driven by the spec
    itself, using the JSON only as a human-readable canonical
    description).
- `schemas/decoder_schema_v1.yaml` — canonical example showing every
  encoding + bit field + endianness variant with inline comments.
- `schemas/decoder_schema_v1.json` — JSON description of the meta-
  schema (what yaml files must contain), matching what the validator
  enforces. Used for documentation + `schema_lint --json` if relevant.

**Tests** (deferred to S6; S2 unit tests are minimal structural
checks only).

**Commit**: `decoder: add SchemaValidator and yaml schema v1 canonical docs`.

### S3 — SchemaDecoder

**Deliverables**:

- `src/decode/schema_decoder.{hpp,cpp}` per spec §4.2.
- Field extraction for every encoding in spec's 13-value enum.
  Single-byte extraction is trivial; multi-byte uses `qFromLittleEndian`
  / `qFromBigEndian` per endianness. Float32/64: `memcpy` + endianness.
- Bit field extraction: `qFromLittleEndian` the containing bytes,
  shift, mask. Cross-byte range within the field's `sizeBytes` window
  (see understanding §3.6).
- Scale/offset: `raw * scale + offset` applied only to numeric
  signals; bit fields ignore scale/offset (bit fields are always raw
  int64 / bool per §3.4).
- `tryDecodeFrame`: iterate layouts, first whose `match.bytes` found
  at `match.offset` wins. Dispatch to field loop for that layout.
- Metric registration at ctor (5 metrics per understanding §3.9).
  `_last_decode_us_` gauge updated at end of each successful decode.
- Unmatched frames: log WARN with first 16 hex bytes + increment
  `decoder_frames_unmatched_<driverId>`.
- Malformed frames: log WARN with reason + increment
  `decoder_frames_malformed_<driverId>`.
- `setSignalSink` is thread-safe (mutex); idempotent (log warn on
  duplicate).

**Tests**: 10 unit cases deferred to S6; S3 spot-check via one
hard-coded schema in the existing `decoder_test.cpp` scaffold.

**Commit**: `decoder: add SchemaDecoder with field + bit + endian extraction`.

### S4 — DecoderRegistrar + pipeline wiring

**Deliverables**:

- `src/decode/decoder_registrar.{hpp,cpp}` per spec §4.6.
  - Constructor takes `PipelineManager*` and a
    `std::unordered_map<QString, QString>` driver-type → schema-path
    map (hard-coded for M5; M9 rewrites).
  - `connect` on `PipelineManager::pipelineAttached`: extract driver
    type from driverId prefix (`serial:`, `tcp:`, `udp:`, `replay:`),
    look up schema path, call `SchemaValidator::validateFile`,
    construct `SchemaDecoder`, call `pipeline->addSink`.
  - If schema path is empty (e.g., TCP/UDP defaults in M5) or
    validation fails: log INFO "no decoder for driverId …"; skip.
  - `DecoderRegistrar` also sets the decoder's `SignalValueSink` to
    a process-wide `LoggingSignalValueSink` for M5 (M6 provides the
    real sink).
- `src/app/main_window.{hpp,cpp}` minor tweak: instantiate
  `DecoderRegistrar` after `PipelineManager`, pass both into the
  dialog if needed. No UI change.
- Integration-test hook: `DecoderRegistrar` is constructible without
  `MainWindow` for test purposes (so S7 can build it directly).

**Tests**: 3 unit cases (constructor accepts null map; attach with
unknown type → skip with log; attach with valid type → decoder
registered).

**Commit**: `decoder: add DecoderRegistrar listening for pipeline attach`.

### S5 — Example + invalid fixtures

**Deliverables**:

- `examples/schemas/temperature_sensor.yaml` per spec §4.8, verbatim.
- `examples/schemas/modbus_style.yaml`: a 2-layout schema with
  discriminator (function code), integer and bit fields, multi-byte
  big-endian for Modbus register semantics. Size ~80 lines.
- `tests/integration/fixtures/invalid_schemas/` (new):
  - `missing_version.yaml`
  - `missing_endianness.yaml`
  - `invalid_encoding.yaml`
  - `bit_overlap.yaml`
  - `bit_overflow.yaml`
  - `duplicate_field.yaml`
- `tests/integration/fixtures/valid_schemas/` (new):
  - symlink or copy of the two examples above

**No tests in S5** (fixtures power S6 and S7).

**Commit**: `decoder: add example schemas and invalid fixtures`.

### S6 — Unit tests ≥ 85 %

**Deliverables** — `tests/unit/decode/decoder_test.cpp` expanded to
cover every case enumerated in spec §5.2:

- **SchemaDecoder** (~9 cases):
  - Valid schema construct; signalMetadata matches.
  - Frame matches layout magic → expected signals emitted.
  - Unknown magic → unmatched counter increments, no signal.
  - Short payload → malformed counter, no signal.
  - Multi-layout schema: two magics routed correctly.
  - Per-field endianness override vs layout default.
  - Bit field single bit → bool; multi-bit → int64.
  - Scale/offset applied correctly.
  - `fixed_string` extraction (null-terminated byte range → QString).

- **SchemaValidator** (~10 cases): every error type from §5.2.

- **LoggingSignalValueSink** (~3 cases): all four SignalValue variants
  logged + counted; `signalsByType` returns correct counts.

**Tests**: ~22 new unit cases.

**Commit**: `decoder: expand unit tests to cover validator + decoder paths`.

### S7 — Integration tests

**Deliverables** (under `tests/integration/`):

- `test_schema_decoder_basic.cpp` — end-to-end per spec §5.3.
  Validates schema, constructs decoder, feeds one handcrafted RawFrame
  via a `MockDriver`, asserts 7 signals emit with expected values.
- `test_schema_decoder_bit_fields.cpp` — bit_count=1, 2, 8, cross-byte.
- `test_schema_decoder_endianness.cpp` — little + big + per-field
  override.
- `test_schema_decoder_unmatched.cpp` — unknown magic → counter +
  log (capture via SF_LOG harness from M2).
- `test_schema_validator_errors.cpp` — each fixture under
  `invalid_schemas/` produces the expected error.

**Tests**: ~10 integration cases.

**Commit**: `tests: add decoder integration and schema validation tests`.

### S8 — Benchmark

**Deliverables**:

- `tests/benchmark/bench_decoder_throughput.cpp`:
  - Scenario A: simple schema (4-6 numeric fields) — target ≥ 100k
    frames/s.
  - Scenario B: complex schema (bit fields + string + scale/offset)
    — target ≥ 50k frames/s.
  - Methodology: pre-build 1000 RawFrames; loop through decoder
    multiple times in a tight inline call (no QThread hop; measures
    pure decode cost).
- `tests/benchmark/CMakeLists.txt` adds the new binary.
- `tests/benchmark/results/M5-baseline.md`: raw JSON + threshold
  evaluation.

**§7.4 HALT gate**: if below target AND no §5.5 category applies,
HALT. Typical suspects: QString construction overhead (Category 2),
MetricsRegistry atomic increments (Category 1 / CC code).

**Commit**: `bench: add decoder throughput benchmark and M5 baseline`.

### S9 — schema_lint CLI

**Deliverables**:

- `tools/schema_lint/CMakeLists.txt` — standalone CMake project,
  matches `tools/crash_test/` pattern.
- `tools/schema_lint/main.cpp`: CLI per spec §4.7. Exit codes 0 (valid)
  / 1 (invalid) / 2 (usage error).
- `tools/schema_lint/README.md`: usage examples, sample output for
  valid + invalid cases.
- Links against `signalforge_decoder` (contains `SchemaValidator`).
- `--json` output uses nlohmann/json; human-readable output carefully
  formatted (§9 note).

**Tests**: minimal — run binary against S5 fixtures in a shell-level
test (not Catch2, just bash in the `tools/schema_lint/` dir's README
verified as documentation).

**Commit**: `tools: add schema_lint CLI`.

### S10 — M5-done.md + PR

Mirrors M4's S10:

1. `.claude/M5-done.md` per execution-manual §6.2:
   - Timing + commit manifest.
   - Deliverables checklist vs spec §2.1 (all 13 items).
   - Acceptance self-check vs §8.1–§8.6.
   - Test count matrix.
   - Benchmark summary + §5.5 classifications.
   - **Freeze record**: C++ headers' SHA256 + schema canonical
     files' SHA256 per §6.3.
   - Hand-off to M6 (SignalValueSink is the contract M6 implements;
     decoder thread-affinity; baseline throughput).
   - HALT resolution trail if any.
   - CI run manifest.
2. Commit `chore: M5 completion report`.
3. Push, watch CI green, `gh pr create --base main --head milestone/M5`
   with authorization per the Phase 5 session prompt.
4. Fill PR number into done.md; push the one-liner update.
5. Stop + announce "M5 ready. Awaiting approval to merge M5 and
   begin M6 bootstrap."

## 3. HALT triggers specific to M5 (rehearsed)

Pre-drafted statements per spec §7:

- **§7.1** (modification to M2/M3/M4 frozen .hpp): never occurs by
  design. If a compile error suggests otherwise, HALT with
  "M5 §2.2-1 violation: [header]:[line] required change is [what];
  options are [A] wrap via adapter, [B] ADR + new method".
- **§7.2** (yaml-cpp line numbers unavailable): HALT with
  "yaml-cpp Mark() returned -1 for [N]% of error paths during S6
  testing, exceeding the 20% threshold. Sample failures: [examples].
  Options: [A] accept and document; [B] wrap yaml-cpp with a custom
  parser that tracks positions manually."
- **§7.3** (C++23 `std::expected` unavailable): verified locally
  works; if CI breaks, HALT with "CI GCC 13 rejects `std::expected`
  under -std=c++23. Options: [A] fallback struct per M5 §3.2 Option
  B; [B] upgrade GCC 14 on CI (ADR)."
- **§7.4** (benchmark throughput below target): HALT with
  "simple schema measured X / 100k kfps; complex measured Y / 50k.
  Profile: [hot path breakdown]. Suspected Category: [...]."
- **§7.5** (validator error messages lack line numbers for > 20% of
  cases): HALT with error-fixture-to-line-number matrix.
- **§7.6** (UI thread block > 100 ms in DecoderRegistrar): HALT with
  "`onPipelineAttached` measured X ms on [schema path]. Threshold
  100 ms. Options: [A] move validate + construct to a worker thread;
  [B] pre-validate schemas at app startup."

Standard CLAUDE.md HALT triggers (3 fix attempts, new dependency,
frozen-file modification) also apply.

## 4. Risk-ranked substitution plan

Standard fallbacks:

- C++23 `std::expected` fails → struct fallback (spec-documented).
- yaml-cpp line numbers partially missing → document per-error-class;
  HALT only if > 20 % of errors lose line info.
- Benchmark below target with Category 2 (Qt framework) or Category
  1 (CC code fixable with single optimization pass) → fix and retry;
  if still below after one pass, HALT.

## 5. Timing and discipline

- Target: 34 h focused implementation across 4–5 calendar days
  (matches M4 pacing, spec is fuller).
- Every push followed by CI watch. No silent retries (reinforced
  again, per Git operation protocol).
- Progress log entries per subtask (start + close). Concerns logged
  immediately.

## 6. Closing alignment

The plan structure mirrors M4's. The two freeze surfaces (C++ and
schema v1) get equal weight. Schema v1 correctness at M5 close is
the strongest commitment — every user file authored after this
milestone must continue to work. When in doubt about a yaml design
choice, document in `.claude/M5-concerns.md` and resolve before
merging rather than after.
