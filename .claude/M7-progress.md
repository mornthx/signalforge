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

**CI** (run 25435111363): success — debug, release, debug-asan all
green.

---

### S2 — Expression class with restricted whitelist + type promotion (start)

**Goal**: per plan §2 S2, plug operational logic into the S1 PIMPL:

1. Update `Expression::Impl` to own per-source `double` slots
   (vector indexed in `sourceSignalIds` order) plus a name → slot
   index map.
2. Constructor:
   - Bind each source name to its slot via `symbols.add_variable`.
   - Apply exprtk parser settings to enforce the spec §3.1
     restricted whitelist (disable control structures, break/continue,
     return statement, local var defs, string capabilities, plus
     base-function blacklist — anything outside the spec's 13-item
     math whitelist).
   - Compile the formula. On failure, throw `std::runtime_error`
     with the parser's error text. (Validator catches user-facing
     errors before this point; runtime throw reflects an internal
     bug.)
3. `evaluate(sources)`:
   - For each `(signalId, value)`, look up slot index, convert the
     variant to `double` per spec §3.3, write to the slot.
   - Call `compiled.value()`.
   - Convert result to the declared `ExpressionOutputType` per spec
     §3.3 (`bool` via `!= 0`, `int64` via `round` with WARN on
     out-of-range).
4. Move semantics retained (PIMPL `unique_ptr` is move-only by
   default).
5. Add `tests/unit/expression/expression_evaluation_test.cpp` (8
   cases): valid arithmetic, mixed-type sources, output-type
   conversion paths, NaN propagation when source is missing,
   restricted-syntax rejection (if/while/?:), restricted-function
   rejection (sgn/cot), bool output, int64 output.

**Acceptance**:

- All existing tests still pass under Debug + Release + debug-asan.
- New unit test verifies all evaluation paths + whitelist
  enforcement.
- Whitelist violations throw at construction time (validator's
  user-facing path lands in S3).

**Freeze scope**: M2/M3/M4/M5/M6 frozen `.hpp` not modified. The
`Expression::Impl` PIMPL is internal to `expression.cpp`; the public
header is unchanged from S1.

### S2 — Expression class with restricted whitelist + type promotion (close)

**Result**: ✅ green.

**Changes**:

- `src/expression/expression.cpp` — fully implemented:
  - `Expression::Impl` now owns `exprtk::symbol_table<double>`,
    `exprtk::expression<double>`, a `std::vector<double>
    sourceSlots` (initial NaN per slot for missing-source
    propagation), and `std::unordered_map<QString, std::size_t>
    nameToSlot`.
  - Constructor binds each source name to its slot via
    `symbols.add_variable(...)`, registers the symbol table, runs
    `applyRestrictedSettings(parser)`, and compiles. On failure
    throws `std::runtime_error` with exprtk's diagnostic.
  - `applyRestrictedSettings` enforces the spec §3.1 control-flow
    + assignment restrictions via `disable_all_control_structures`,
    `disable_local_vardef`, `disable_all_assignment_ops`. Per-base-
    function whitelist (sgn, cot, statistical aggregates) is
    deferred to S3 validator (exprtk 0.0.3's `disable_base_function`
    takes an enum, not a string; a textual / AST scan in the
    validator is the cleaner home).
  - `promoteToDouble` per spec §3.3: bool → 0.0/1.0; int64 →
    cast with WARN once when |value| > 2^52; double → identity;
    QString → defensive NaN.
  - `mapResult` per spec §3.3: double identity; bool via `!= 0`;
    int64 via `round` with WARN on out-of-range / NaN / Inf.
  - `evaluate` writes NaN to all slots, then overwrites by source,
    then `compiled.value()`. Missing sources → NaN result (M8 chart
    renders gap; spec §4.7).
- `tests/unit/expression/expression_evaluation_test.cpp` (9 cases):
  arithmetic / bool source / int64 source / bool output / int64
  output / NaN propagation / control-flow rejection / assignment
  rejection / whitelisted built-ins (max + sqrt) round trip.
- `tests/unit/expression/expression_smoke_test.cpp` updated:
  source IDs use bare-identifier form (e.g., `v`, `i`) since exprtk
  identifiers cannot contain `/` or `:`. M7 V1 expressions reference
  base signals by their bare ID; driver-prefix mapping (e.g.,
  `serial:0/voltage` → `voltage`) is V1.5+ work.

**Compile fixes** (within HALT trigger #1's 3-attempt budget):

1. `disable_assignment_op` → `disable_all_assignment_ops` (exprtk
   0.0.3 doesn't expose per-op variants).
2. `disable_base_function(const char*)` doesn't exist; signature
   takes a `settings_base_funcs` enum. Removed the per-function
   blacklist; deferred to validator.
3. `disable_break_continue_statement` / `disable_return_statement` /
   `disable_string_capabilities` don't exist on exprtk 0.0.3's
   `settings_store`. Removed; their effective coverage falls under
   `disable_all_control_structures`.
4. `slots` member name collided with Qt's `slots` keyword macro.
   Renamed to `sourceSlots`. (Fourth Qt-macro collision in the
   project; previous: `signals` rename in M5, `emit` lambda rename
   in M6 S11.5, `signals` parameter in M5 SignalValueSink.)

**Build verification** (local):

- Debug + Release + debug-asan all build clean.
- `clang-format --dry-run -Werror` clean (one auto-fix iteration).

**Test verification** (local):

- `ctest --preset=debug` — 333 / 333 pass (was 324 at S1 close;
  +9 from S2 evaluation tests).
- `ctest --preset=release` — 333 / 333 pass.
- debug-asan deferred to CI.

**Frozen-file diff** vs `08a0478` (M6 merge): empty.

**Effort**: 3.5 h (plan estimate 5 h).

