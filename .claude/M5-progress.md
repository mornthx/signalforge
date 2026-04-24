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
