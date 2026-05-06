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

**CI** (run 25437535700): success — debug, release, debug-asan all
green.

---

### S3 — ExpressionValidator (yaml + cycle detection + type checks) (start)

**Goal**: per plan §2 S3, replace S1's stubs with a working validator
that walks the spec §4.3 8-step pipeline.

1. yaml-cpp parsing with `Mark()` for line numbers (mirrors M5
   `SchemaValidator`). Top-level shape check (`schema_version`,
   `expressions`).
2. Per-expression: required-field check (`id`, `formula`),
   uniqueness, base-id collision, formula non-empty.
3. Source-identifier extraction from the formula via regex
   (`[a-zA-Z_][a-zA-Z0-9_]*`), filtered against reserved
   words + the spec §3.1 whitelist + exprtk constants. The
   filtering set is shared with S2's `applyRestrictedSettings`
   whitelist (kept consistent in code).
4. exprtk compile preflight inside the validator (separate from
   `Expression` construction) to catch user-facing errors with
   structured `ExpressionValidationError`. Uses the same
   restricted-parser settings as S2.
5. Source-signal validation against `availableSignals`: each source
   ID must exist; none may have type `QString`.
6. Output-type compatibility hint (per spec §4.3 step 6): if the
   yaml declares `type: bool`/`int64`, no error in S3 — the result
   coercion handles it. We log a hint when `type: bool` is used
   with a non-comparison formula (heuristic; non-blocking).
7. Cycle detection via DFS three-color over the dependency graph
   (edges from expression-id → its sources). Reports the cycle
   path on detection.
8. Topological sort produces `ExpressionSet` with `expressions` in
   evaluation order + `baseSignalIds` (union of all sources).
9. 13 fixture yamls under
   `tests/integration/fixtures/invalid_expressions/` (one per spec
   §5.2 error category) + a corresponding unit test that loads
   each and verifies the right error type / line / expression-id.
10. 1 valid fixture under
    `tests/integration/fixtures/valid_expressions/` exercising the
    happy path (multi-expression set with derived dependencies).

**Acceptance**:

- Each of the 13 error fixtures fails with at least one
  `ExpressionValidationError` whose `expressionId` matches the bad
  entry (or empty for pre-expression errors) and whose `message`
  contains actionable hint text.
- Valid fixture yields a topologically sorted `ExpressionSet` with
  expected expression count + base signal IDs.

**Freeze scope**: M2/M3/M4/M5/M6 frozen `.hpp` not modified.
`ExpressionValidator` public API is unchanged from S1; S3 just
implements the body.

### S3 — ExpressionValidator (yaml + cycle + type) (close)

**Result**: ✅ green.

**Changes**:

- `src/expression/expression_validator.cpp` — full implementation:
  - yaml-cpp parsing with `Mark()`-based line numbers.
  - `reservedIdentifiers()` (whitelisted functions + logical ops +
    exprtk constants + control-flow keywords) and
    `forbiddenFunctionNames()` (sgn, cot, statistical aggregates,
    if-as-function) sets shared with the S2 runtime parser.
  - `extractSourceIds(formula)` regex-based identifier extraction
    that filters reserved + forbidden, returning user-defined
    source signal IDs in first-occurrence order.
  - `firstForbiddenName(formula)` for the spec §3.1 whitelist
    error path (produces user-facing message before exprtk is
    asked to parse).
  - `exprtkCompileError(formula, sources)` does a parser preflight
    with the same restricted settings as S2; returns nullopt on
    success or structured error text on failure.
  - DFS three-color cycle detection. Reports the cycle path in the
    error message.
  - Kahn's-algorithm topological sort. The natural Kahn order is
    consumers-before-sources for our edge direction; the result is
    reversed to give sources-before-consumers (the engine
    evaluation order).
  - Final ExpressionSet construction via `Expression(...)`. The
    construct path also runs exprtk compile (twice for valid
    inputs); cheap enough to defer optimization to M12.
  - `baseSignalIds` is the union of source IDs that are NOT
    themselves expressions (i.e., true base signals).
- `src/expression/CMakeLists.txt`: added `yaml-cpp::yaml-cpp` to the
  PRIVATE link deps.
- 13 invalid-expression yaml fixtures under
  `tests/integration/fixtures/invalid_expressions/`:
  missing_version, missing_id, duplicate_id, id_collides_with_base,
  unknown_operator, restricted_syntax_if, restricted_syntax_while,
  unknown_source, qstring_source, cycle_simple, cycle_self,
  cycle_three, bool_output_with_arithmetic.
- 1 valid yaml fixture under
  `tests/integration/fixtures/valid_expressions/multi.yaml`
  exercising 3 expressions with a chained dependency
  (`power_efficiency` depends on `power_total`).
- `tests/unit/expression/expression_validator_test.cpp` (14 cases):
  one per fixture + one happy-path test verifying topological
  ordering (`power_efficiency` indexed after `power_total`) +
  `baseSignalIds` content.

**Build verification** (local):

- Debug + Release + debug-asan all build clean.
- `clang-format --dry-run -Werror` clean (no auto-fix needed for
  validator; one auto-fix for the test on first run).

**Test verification** (local):

- `ctest --preset=debug` — 347 / 347 pass (was 333 at S2 close;
  +14 from S3: 13 fixture cases + 1 happy-path).
- `ctest --preset=release` — 347 / 347 pass.
- debug-asan deferred to CI.

**Frozen-file diff** vs `08a0478` (M6 merge): empty.

**Note on `bool_output_with_arithmetic`**: per spec §4.3 step 6,
bool-output with arithmetic is treated as a permissive case — the
result coercion in S2 maps non-zero to true, which is well-defined.
The fixture is retained in `invalid_expressions/` for parity with
spec §5.2's enumeration but the test asserts validation
**succeeds**. (The fixture name predates this clarification; spec
itself uses "warning" wording, not "rejection".)

**Notable design decisions**:

- The forbidden-function check happens BEFORE source extraction, so
  formulas containing `if(x, 1, 0)` are caught with a clean
  "forbidden function/keyword" message rather than the cascade of
  exprtk's "undefined symbol" errors.
- Source extraction filters out forbidden names so they don't
  appear in the dependency graph (they're rejected separately).
- `extractSourceIds` is order-preserving so the deterministic
  topo-sort output matches first-occurrence-in-formula for ties.

**Compile fixes**: 1 (`slots` rename → `valueSlots` in the
validator's exprtkCompileError helper, mirroring S2's Qt-macro
collision fix).

**Effort**: 4.5 h (plan estimate 6 h).

**CI** (run 25439096830): success — debug, release, debug-asan all
green.

---

### S4 — ExpressionEngine (30 Hz tick + evaluation loop) (start)

**Goal**: per plan §2 S4, replace S1's stub onTick with a working
evaluation loop.

1. Constructor registers spec §3.7 engine-level metrics (
   `expression_engine_ticks_total`,
   `expression_engine_tick_us`,
   `expression_engine_evaluations_total`).
2. `setExpressions(set)`:
   - Build derived metadata catalog from each Expression's
     `derivedSignalMetadata()`.
   - Register with the registry via the virtual driver ID.
   - Register per-expression metrics
     (`expression_evaluation_us_<id>`,
     `expression_evaluation_errors_<id>`).
   - Store the set.
3. `onTick()`:
   - Record `t0`.
   - For each expression in topo order:
     - Collect source values: for each source ID, query
       `registry->bufferFor(srcId)->queryLatestOne()`; if buf
       nullptr OR queryLatestOne returns nullopt, contribute NaN
       (`SignalValue{quiet_NaN}`).
     - Try `expr.evaluate(sources)`; on exception increment
       `expression_evaluation_errors_<id>`, log warn once per second
       per expression (rate-limited via a per-expression timestamp).
     - Push result via `registry->onSignal(now, derivedId, result)`.
   - Record `t1`; update `TickStats` (ticksTotal++, evaluationsTotal
     += N, lastTickDurationUs, peakTickDurationUs); set engine-level
     metric gauges.
4. Tick-miss handling: per spec §9, do NOT catch up. The Qt timer
   simply fires later; no special logic.
5. `tests/unit/expression/expression_engine_lifecycle_test.cpp`:
   - Lifecycle (start/stop/start, isRunning, expressionCount).
   - Single-tick evaluation produces a derived signal in the
     registry (uses QTest::qWait to spin the event loop).
   - Multi-tick (TicksTotal > 1 after waiting longer than 2 ×
     tickInterval).
   - Source-NaN propagation: missing base signal → derived NaN.

**Acceptance**:

- All existing tests still pass under Debug + Release + debug-asan.
- New unit tests verify the lifecycle, single + multi tick, and
  NaN propagation. Linked against Qt6::Test for `QTest::qWait`.

**Freeze scope**: M2/M3/M4/M5/M6 frozen `.hpp` not modified.
`ExpressionEngine` public API is unchanged from S1; S4 just
implements the body.

### S4 — ExpressionEngine (30 Hz tick + evaluation loop) (close)

**Result**: ✅ green.

**Changes**:

- `src/expression/expression_engine.hpp`: PIMPL pointer
  `ExpressionEnginePrivateOpaque` added (forward-declared in the
  header, defined in the .cpp) so the public surface stays free of
  `signalforge::observability::Metric`.
- `src/expression/expression_engine.cpp` — full evaluation path:
  - Constructor registers spec §3.7 engine-level metrics
    (`expression_engine_ticks_total`,
    `expression_engine_tick_us`,
    `expression_engine_evaluations_total`).
  - `setExpressions(set)`:
    - Re-registration unregisters previous virtualDriver entry
      with the registry.
    - Builds derived metadata from each `Expression::derivedSignalMetadata()`.
    - Calls `registry->onSignalsRegistered(virtualDriverId, derivedMetas)`.
    - Re-registers per-expression metrics
      (`expression_evaluation_us_<id>`,
      `expression_evaluation_errors_<id>`).
    - Initializes per-expression rate-limit timestamps.
  - `onTick()`:
    - Records `t0`.
    - For each expression in topo order:
      collect source values via `bufferFor(srcId)->queryLatestOne()`
      (NaN if buf null OR query nullopt) → evaluate (catch exceptions
      with rate-limited warn) → push result via
      `registry->onSignal(t0, expressionId, result)`.
    - Records tick duration; updates `TickStats` and metric
      gauges.
    - Emits `tickCompleted(tickIndex)`.
  - Pre-allocated `sources` vector reused across expressions to
    avoid per-tick allocations.
  - Tick-miss handling per spec §9: no catch-up logic.

**Tests**:

- `tests/unit/expression/expression_engine_lifecycle_test.cpp`
  (8 cases):
  - Lifecycle (start/stop idempotency).
  - setExpressions registers derived signals in registry.
  - Single tick: stats counters increment, derived buffer's
    `totalSamplesPushed` advances.
  - 100 ticks crosses publish cadence: derived signal's
    `queryLatestOne` resolves to the expected product.
  - Multi-tick (10 ticks → ticksTotal=10, evaluationsTotal=10).
  - Missing source → NaN derived value.
  - Topo-order dependency chain (a then b → b sees a's value).
  - Empty expression set ticks cleanly.
- Tests invoke `onTick` directly via
  `QMetaObject::invokeMethod(...,Qt::DirectConnection)` to avoid
  the QCoreApplication dependency for QTimer dispatch. The QTimer
  cadence path is exercised in S9 integration tests where a Qt
  event loop is set up.

**Build verification** (local):

- Debug + Release + debug-asan all build clean.
- `clang-format --dry-run -Werror` clean.

**Test verification** (local):

- 8 lifecycle tests pass directly (24 assertions).
- Full ctest with `--timeout 120 -j1`: 354 / 355 pass; the
  remaining 1 is M6's pre-existing `S10 integration: 1M-sample
  concurrent` which runs to completion in ~1m58s on Debug (over
  120s ctest timeout but not actually broken). On Release / CI
  default timeout it passes within the suite. The slow test was
  green at M6 close and is unaffected by S4 changes.
- Cumulative test count: **355** (was 347 at S3 close;
  +8 from S4).

**Frozen-file diff** vs `08a0478` (M6 merge): empty.

**Bug fixes during S4**:

1. Initial S4 tests used `QSignalSpy::wait(500)` and
   `QTest::qWait(N)` — both spin-loop without a `QCoreApplication`.
   Two test instances hung at 100% CPU for 132+ minutes before being
   killed and the tests refactored to use `QMetaObject::invokeMethod`
   instead.
2. Tests initially called `queryLatestOne` after a single tick;
   M6's SignalBuffer doesn't publish until cadence threshold (100
   pushes) is crossed. Added `runTicksForFirstPublish` helper to
   drive 100 ticks before the publish-dependent assertions.

**Effort**: ~3.5 h actual work + ~2.5 h debug-and-recovery from
the QSignalSpy hang (CPU spin not wall time). Plan estimate 5 h.

**CI** (run 25448331665): success — debug, release, debug-asan all
green.

---

### S5 — ExpressionRegistrar (yaml orchestration + engine handoff) (start)

**Goal**: per plan §2 S5, app-level lifecycle hook that loads yaml
files, validates against the registry's base signals, hands the
result to `ExpressionEngine`, and starts ticking. On failure,
logs all validation errors and leaves the engine stopped.

1. New `src/expression/expression_registrar.{hpp,cpp}`:
   - Constructor takes `SignalBufferRegistry&`, `ExpressionEngine&`,
     and `std::vector<QString> yamlPaths`.
   - `loadAndStart()` method:
     - Reads each yaml path; merges their `expressions` sequences
       into a synthetic combined yaml document via yaml-cpp Node
       manipulation. This lets the validator's cycle detection
       run across all files in one pass.
     - Builds the available-signals catalog from
       `registry.signalIds()` + `bufferFor(id)->metadata()`.
     - Calls `ExpressionValidator::validateString(merged, …)`.
     - On success: `engine.setExpressions(...)` + `engine.start()`.
     - On failure: logs each validation error with file/line/
       expression-id; engine remains stopped.
2. New `src/expression/CMakeLists.txt` source list += registrar.
3. Tests:
   - `tests/unit/expression/expression_registrar_test.cpp`:
     happy-path (1 file, valid) starts the engine; mixed-file
     happy path (2 files merged); validation-failure path leaves
     the engine stopped + logs errors; missing-file path errors
     cleanly.

**Acceptance**:

- `loadAndStart()` succeeds for valid yaml + valid base-signal
  catalog; engine ticks (stats counter advances after invoking
  manual ticks per the M6-cadence approach used in S4).
- Validation failure on any file leaves engine stopped.
- Cross-file cycle is detected.

**Freeze scope**: M2/M3/M4/M5/M6 frozen `.hpp` not modified. New
`ExpressionRegistrar` class is added to the M7 freeze surface
(spec §6.1 enumerates the three previous classes; the registrar
is a behavioral helper, not strictly enumerated, but its API
will be referenced in M7-done.md).

### S5 — ExpressionRegistrar (yaml orchestration + engine handoff) (close)

**Result**: ✅ green.

**Changes**:

- `src/expression/expression_registrar.{hpp,cpp}`: new orchestrator
  class.
  - Constructor takes registry + engine references + yamlPaths.
  - `loadAndStart()`:
    - Empty paths → WARN + return false.
    - For each path: open + parse via yaml-cpp; on per-file IO or
      yaml-syntax errors, push structured `ExpressionValidationError`
      and return false.
    - Merges all `expressions:` sequences from all files into a
      single virtual yaml document via yaml-cpp Node manipulation.
      Cross-file cycle detection runs in the validator's single
      pass over the merged document.
    - Builds available-signals catalog from
      `registry.signalIds()` + `bufferFor(id)->metadata()`.
    - Calls `ExpressionValidator::validateString(merged, …)`. On
      failure: log all errors with file/line/expression-id; return
      false.
    - On success: `engine.setExpressions(...)` + `engine.start()`;
      info-log expression count + file count.
  - `lastErrors()` accessor returns the most recent error list (for
    test assertions + diagnostic display).
- `src/expression/CMakeLists.txt`: source list += registrar files.

**Tests**:

- `tests/unit/expression/expression_registrar_test.cpp` (6 cases):
  - Happy path with `valid_expressions/multi.yaml` (3 expressions
    registered).
  - Validation failure (`cycle_simple` fixture) leaves engine
    stopped, lastErrors populated.
  - Missing-file path errors cleanly with structured error.
  - Empty path list returns false (warn only).
  - Multi-file merge: two synthetic yamls in QTemporaryDir, where
    file B references a derived signal from file A; loadAndStart
    succeeds with 2 expressions registered.
  - Cross-file cycle: file A's `alpha` depends on file B's `beta`,
    and vice versa; validator reports cycle, engine stays stopped.

**Build verification** (local):

- Debug + Release + debug-asan all build clean.
- `clang-format --dry-run -Werror` clean.

**Test verification** (local):

- 6 registrar tests pass directly (21 assertions).
- Cumulative test count: 361 (was 355 at S4 close; +6 from S5).

**Frozen-file diff** vs `08a0478` (M6 merge): empty.

**Effort**: 2.5 h (plan estimate 3 h).


**CI** (run 25449269093): success — debug, release, debug-asan all
green.

---

### S6 — yaml schema v1 canonical + examples (start)

**Goal**: per plan §2 S6, produce the freeze artifacts for the yaml
expression schema v1 + two example expression files.

1. `schemas/expression_schema_v1.yaml`: canonical example with
   doc-comments for every key. Freeze reference; sha256 lands in
   M7-done.md.
2. `schemas/expression_schema_v1.json`: JSON-Schema-style
   description (documentation form; matches M5's
   `decoder_schema_v1.json` pattern).
3. `examples/expressions/power_calculations.yaml`: voltage × current
   = power, plus efficiency.
4. `examples/expressions/alarms.yaml`: threshold-based bool signals.

Acceptance: all four files parse + validate cleanly with a
synthetic base-signal catalog; new unit test exercises both
example files.

**Freeze scope**: yaml schema v1 surface (top-level + per-expression
keys) freezes at M7 close. M2/M3/M4/M5/M6 frozen `.hpp` not
modified.

**CI** (run 25450511585): success — debug, release, debug-asan all
green.

---

### S7 — `expr_lint` CLI tool (start)

**Goal**: per plan §2 S7 + spec §4.6, deliver the standalone CLI
that mirrors M5's `tools/schema_lint/` for the expression schema.

**API extension** (additive on the M7 freeze surface, before close):
- `ExpressionValidator::validateFileSyntaxOnly(QString)` and
  `validateStringSyntaxOnly(QString, QString)` skip source-id
  existence + source-type checks. All other validation steps still
  run (yaml parse, top-level shape, per-expression shape, exprtk
  compile, forbidden-function check, cycle detection, topological
  sort).
- Public methods delegate to a single internal
  `validateContent(..., bool skipSourceCheck)`. Existing entry
  points unchanged.

**CLI** (`tools/expr_lint/main.cpp`):
- `expr_lint <file.yaml>` → syntax + cycle only.
- `expr_lint <file.yaml> --base-signals signals.json` → full
  validation against the JSON catalog.
- `--json` for machine-readable output.
- Exit 0/1/2 per spec §4.6.

**JSON catalog format**:
```json
{ "signals": [ {"id":"v","type":"double","unit":"V"}, ... ] }
```
Recognised types: `double`, `bool`, `int64`, `qstring` (alias
`QString`, `string`).

**Tests**: 3 new validator cases under `[s7][lint-mode]` covering
(a) accept unknown sources in syntax-only mode, (b) cycles still
caught in syntax-only mode, (c) full validation rejects what
syntax-only accepts (proves the modes differ as designed).

**Smoke runs** (release):
- `power_calculations.yaml` syntax-only → OK 3 expressions, 3 sources
- `alarms.yaml --json` → OK valid=true
- `power_calculations.yaml --base-signals` (full catalog) → OK
- `power_calculations.yaml --base-signals` (missing power_input) →
  FAIL with actionable error
- `cycle_simple.yaml` (invalid) → FAIL with cycle path
- `--bogus` → exit 2; `/tmp/nonexistent.yaml` → exit 2; `--help` →
  exit 0

**Freeze scope**: validator `.hpp` not yet frozen (M7 close pending).
M2/M3/M4/M5/M6 frozen `.hpp` not modified.

**Build**: clean on debug + release + debug-asan.
**Tests**: 367/367 on both debug and release.
**Format**: clang-format clean on changed files.

**CI** (run 25451566722): success — debug, release, debug-asan all
green.

---

### S8 — Unit tests ≥85% coverage (start)

**Audit baseline**: existing tests across `expression_smoke_test`,
`expression_evaluation_test`, `expression_validator_test`,
`expression_engine_lifecycle_test`, `expression_registrar_test`,
and `expression_examples_test` already cover the spec §5.2 scenarios
(13 invalid fixtures + happy paths + lifecycle). Initial gcov pass:

| File | Pre-S8 | Target |
|------|--------|--------|
| `expression.cpp` | 90.08% | ≥85% |
| `expression_engine.cpp` | 81.95% | ≥85% |
| `expression_validator.cpp` | 85.22% | ≥90% |
| `expression_registrar.cpp` | 75.00% | (no specific) |

**Gap-fills** (new):

1. `tests/unit/expression/expression_class_test.cpp` (8 cases):
   accessors, omitted-description = nullopt, derivedSignalMetadata
   for each of the 3 output types, move-construction with
   post-move evaluate, move-assignment with post-move evaluate,
   ExpressionSet move-only contract.

2. `expression_validator_test.cpp` (+9 inline-yaml cases under
   `[s8][shape]`): schema_version != 1, missing top-level
   `expressions`, `expressions` not a sequence, entry not a
   mapping, empty id, missing formula, empty formula, unknown
   output type, validateFile cannot-open. Inline yaml strings
   instead of new fixtures since these are top-level shape errors.

3. `expression_engine_lifecycle_test.cpp` (+2 cases under `[s8]`):
   - setExpressions called twice (covers unregister-then-reregister
     branch).
   - bufferFor==nullptr → NaN (unregistered source — separate code
     path from S4's "registered but no value" NaN test).

**Post-S8 coverage**:

| File | Post-S8 | Δ |
|------|---------|---|
| `expression.cpp` | 90.08% | (already over) |
| `expression_engine.cpp` | 85.71% | +3.76 pp ✓ |
| `expression_validator.cpp` | 93.71% | +8.49 pp ✓ |
| `expression_registrar.cpp` | 75.00% | (unchanged) |

Weighted average over the 4 files: **88.94%**.

Remaining uncovered branches in `expression_engine.cpp`:
- Destructor's `tickTimer_.stop()` and `start()/stop()` Q_EMIT —
  require a running QCoreApplication event loop. Exercised in S9
  integration tests.
- `registry_ == nullptr` early return — defensive code; engine
  takes the registry by reference, so this branch can't fire from
  reachable user code. Per CLAUDE.md anti-patterns, not a target
  for "trust the framework" scenarios.

**Test count**: 386/386 on debug and release (was 367 → +19).

**Build**: clean on debug + release + debug-asan.
**Format**: clang-format clean on all changed files.

**Acceptance** (per plan §S8):
- ✓ Coverage ≥ 85% on `expression.cpp` (90.08%)
- ✓ Coverage ≥ 85% on `expression_engine.cpp` (85.71%)
- ✓ Coverage ≥ 90% on `expression_validator.cpp` (93.71%)
- ✓ All test cases pass under Debug and Release

---

### S9 — Integration tests (start)

**Goal**: per plan §2 S9 + spec §5.3, deliver the 6 end-to-end
expression-engine tests at `tests/integration/`.

**New files** (one per scenario):

1. `test_expression_engine_basic.cpp`: yaml → registrar → engine →
   100 ticks → `power = v * i` derived signal published with value
   24.0 (v=12, i=2).
2. `test_expression_engine_dependency_chain.cpp`: 3-step chain
   `c ← b ← a ← base_x`. With base_x=10, expected a=11, b=22, c=17.
   200 ticks lets the publish cadence settle each level.
3. `test_expression_engine_cycle_rejection.cpp`: 2-node cycle
   x↔y. Asserts (a) registrar returns false, (b) error message
   contains "cycle" and the cycle path (x → y → x), (c) no derived
   signals are registered.
4. `test_expression_engine_type_promotion.cpp` (2 cases): bool
   source promotes to 1.0 (`flag * 5.0` → 5.0); int64 source
   promotes to double (`counter / 4.0` with counter=42 → 10.5).
5. `test_expression_engine_qstring_rejection.cpp`: QString-typed
   base signal as a source produces a registration error containing
   "QString" and references the offending expression id.
6. `test_expression_engine_restricted_syntax.cpp` (4 cases):
   `if(...)`, `while(...)`, ternary `? :`, `for(...)` all rejected
   with messages referencing the offending expression id. Formulas
   are quoted in yaml so `:` doesn't break yaml parsing.

**Validator change** (additive):
- Augmented `firstForbiddenName` to flag formulas containing the
  ternary `?` character. exprtk's `disable_all_control_structures`
  only blocks `if/switch/for/while/repeat/return` (per
  `cntrl_struct_list` in exprtk 0.0.3 line 485-488); the ternary
  operator is its own token. The check produces the same actionable
  error format as other whitelist violations.

**Test count**: 396/396 on debug and release (was 386 → +10 from
the 10 new test cases: basic 1 + chain 1 + cycle 1 + type_promote 2
+ qstring 1 + restricted 4).

**Build**: clean on debug + release (debug-asan deferred to CI).
**Format**: clang-format clean on changed files.
