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
