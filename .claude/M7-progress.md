# M7 — Progress log

## Session metadata

- Phase 5 execution begins 2026-05-06.
- Branch: `milestone/M7` at `e2c6cd2` (understanding + plan from
  Phase 3).
- Plan: `.claude/M7-plan.md`, 11 subtasks S1–S11.
- Understanding: `.claude/M7-understanding.md`.
- Remote: `git@github.com:mornthx/signalforge.git`.

## Subtask log

Each subtask appends a start entry and a close entry.

---

### S1 — scaffolding + exprtk C++23 preflight (start)

**Goal**: per plan §2 S1, establish the M7 module foundation:

1. **Critical gate (HALT trigger #2)**: include `<exprtk.hpp>` from a
   `signalforge_expression` TU that instantiates `exprtk::parser<double>`
   and friends. Compile under all three presets (Debug, Release,
   debug-asan). If any preset fails, HALT — do NOT downgrade C++ or
   change exprtk version.
2. Create `src/expression/` directory; deliver spec §4.1 / §4.2 / §4.3
   freeze-surface headers verbatim (forward-declared types where
   possible to keep exprtk out of the public headers).
3. `src/expression/{expression,expression_engine,expression_validator}.cpp`
   — ctor/dtor stubs that compile, plus the exprtk preflight in
   `expression.cpp`'s PIMPL.
4. `src/expression/CMakeLists.txt` defining `signalforge_expression`
   STATIC library. PUBLIC deps: `Qt6::Core`, `signalforge_buffer`,
   `signalforge_decoder`. PRIVATE: observability + exprtk's include
   directory.
5. Top-level `CMakeLists.txt` adds `add_subdirectory(src/expression)`
   after `src/buffer`.
6. `tests/unit/expression/CMakeLists.txt` + a placeholder smoke test
   verifying the headers compile, default configs make sense, and
   minimal Expression / Engine / Validator construct + destruct
   cleanly.

**Acceptance**:

- All three presets build clean (HALT #2 gate).
- Doxygen on every public declaration.
- `clang-format --dry-run -Werror` clean on changed files.
- New unit test executable runs in `ctest` and reports pass.

**Freeze scope**: M2/M3/M4/M5/M6 frozen `.hpp` not modified.
Pre-commit diff check confirms.

### S1 — scaffolding + exprtk C++23 preflight (close)

**Result**: ✅ green. **HALT trigger #2 cleared**: exprtk 0.0.3
compiles and links clean under C++23 across Debug + Release +
debug-asan presets. The PIMPL pattern (exprtk's symbol_table /
expression / parser instantiated only inside `expression.cpp`)
keeps the preflight TU isolated and the public headers free of
exprtk template instantiations.

**Files added**:

- `src/expression/expression.hpp` (84 lines) — spec §4.2 verbatim.
  `Expression` class with `ExpressionOutputType` enum + `ExpressionSet`
  struct. PIMPL-hidden internals.
- `src/expression/expression.cpp` (113 lines) — exprtk preflight
  site. PIMPL contains `exprtk::symbol_table<double>` +
  `exprtk::expression<double>` instantiations. Constructor stubs
  store metadata; `evaluate()` returns a default-typed `SignalValue`
  for now (S2 wires real evaluation).
- `src/expression/expression_engine.hpp` (98 lines) — spec §4.1
  verbatim. `ExpressionEngine` QObject with `ExpressionEngineConfig`
  + nested `TickStats`. Q_DISABLE_COPY_MOVE; QTimer for tick
  cadence.
- `src/expression/expression_engine.cpp` (74 lines) — QTimer setup,
  start/stop, mutex-protected stats accessor. `onTick()` increments
  ticksTotal + emits `tickCompleted` (real evaluation in S4).
- `src/expression/expression_validator.hpp` (52 lines) — spec §4.3
  verbatim. `ExpressionValidator` with two static validate methods
  returning `std::expected<ExpressionSet, vector<…Error>>`.
- `src/expression/expression_validator.cpp` (29 lines) — stubs that
  return `std::unexpected` with a "not yet implemented" error;
  used by S1 smoke to verify the result type compiles.
- `src/expression/CMakeLists.txt` — `signalforge_expression` STATIC
  lib. PUBLIC: Qt6::Core, signalforge_buffer, signalforge_decoder.
  PRIVATE: signalforge_observability + `${exprtk_SOURCE_DIR}` (PIMPL
  isolation; consumers don't see exprtk).
- `tests/unit/expression/expression_smoke_test.cpp` (4 cases) +
  `tests/unit/expression/CMakeLists.txt` — verifies default config,
  Expression construction + accessors, ExpressionEngine zero-state,
  validator stub returns unexpected.

**Files modified**:

- `CMakeLists.txt`: `add_subdirectory(src/expression)` after
  `src/buffer`.
- `tests/unit/CMakeLists.txt`: `add_subdirectory(expression)` after
  `buffer`.

**Build verification** (local):

- `cmake --preset=debug && cmake --build --preset=debug` — clean.
- `cmake --preset=release && cmake --build --preset=release` —
  clean.
- `cmake --preset=debug-asan && cmake --build --preset=debug-asan`
  — clean.
- `clang-format --dry-run -Werror` on all changed files — clean
  (one auto-fix iteration on column alignment).

**Test verification** (local):

- `ctest --preset=debug` — 324 / 324 pass (was 320 at M6 close;
  +4 from S1 smoke).
- `ctest --preset=release` — 324 / 324 pass.
- debug-asan deferred to CI per the M5/M6 protocol.

**Frozen-file diff** vs `08a0478` (M6 merge): empty.

**Effort**: 2.5 h (plan estimate 3 h).

**HALT trigger #2 status**: ✅ cleared. exprtk's full template
instantiation (parser, expression, symbol_table) compiles under
GCC 13 / C++23 with no warnings or build-time errors.

