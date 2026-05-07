# M7 — Plan

## 0. Execution ground rules

- Branch: `milestone/M7` (created in Phase 3 from `08a0478`; pushed
  to origin).
- Per-subtask discipline (CLAUDE.md §Required #2 + §Git operation
  protocol), identical to M5/M6:
  1. Append start entry to `.claude/M7-progress.md`.
  2. Implement per plan.
  3. Build all three presets clean (Debug, Release, debug-asan).
  4. `ctest` Debug + Release clean.
  5. `clang-format --dry-run -Werror` on changed files.
  6. Append close entry to progress.md with counts + deviations.
  7. Commit with `<module>: <imperative verb> <object>`; body states
     "Freeze scope: no M2/M3/M4/M5/M6-frozen .hpp modified."
  8. Push `milestone/M7`.
  9. Watch CI via `gh run watch`; report result before starting the
     next subtask. No silent retries.
- No new top-level dependencies (spec §2.2-9). exprtk is already
  pinned at `0.0.3` in `cmake/dependencies.cmake`.
- Locked design decisions (spec §3.1-§3.7) are reflected in
  `.claude/M7-understanding.md §5` and implemented as written; not
  re-evaluated in this plan.
- Performance HALT gates (spec §7) and the 7 spec-defined HALT
  triggers are pre-encoded in §3 below with their measurement
  points.
- Strategy: **measure first, optimize only on miss**. No
  pre-allocated counter-mitigations beyond what the spec already
  prescribes.
- Phase 1 closure follows the established M5/M6 flow (push +
  CI green + PR creation + done.md).

## 1. Subtask sequence overview

| # | Subtask | Prereqs | Effort | Commit | Notes |
|---|---|---|---|---|---|
| S1 | `src/expression/` scaffolding + exprtk preflight + `Expression` / `ExpressionEngine` / `ExpressionValidator` header skeletons + CMake wiring | — | 3 h | Yes | exprtk C++23 compile gate per HALT trigger #2; if it fails, HALT immediately (do not implement workarounds). |
| S2 | `Expression` class (PIMPL with internal exprtk symbol table + parser + expression objects) + type-promotion logic + restricted-syntax whitelist enforcement (during compile) | S1 | 5 h | Yes | Implement the M7.1 / M7.3 decisions (locked). Each compile pre-walks AST checking against whitelist. |
| S3 | `ExpressionValidator` (yaml-cpp parsing, exprtk compile, source-signal availability check, type checks, cycle detection, topological sort) | S2 | 6 h | Yes | Validator owns the heavy work. `std::expected` result type. All 13 error fixtures from spec §5.2 must produce file:line:expression-id messages. |
| S4 | `ExpressionEngine` (QTimer at 33.33 ms, tick body: collect sources via `bufferFor->queryLatestOne`, evaluate in topo order, write via `onSignal`, update tick / evaluation metrics) + `Expression::evaluate(sources)` | S3 | 5 h | Yes | Hot loop on the main thread. Stats updated under mutex; reads via accessors. |
| S5 | `ExpressionRegistrar` (yaml file load orchestration, validator → engine handoff, app-level lifecycle) | S4 | 3 h | Yes | Mirrors `DecoderRegistrar` pattern from M5. |
| S6 | yaml expression schema v1 canonical doc (`schemas/expression_schema_v1.yaml`) + JSON meta-schema (`schemas/expression_schema_v1.json`) + 2 example expression files (`examples/expressions/power_calculations.yaml`, `alarms.yaml`) | S2 | 2 h | Yes | Powers S7's lint, S8 unit tests, and S9 integration tests. |
| S7 | `expr_lint` CLI tool (`tools/expr_lint/`) | S3 + S6 | 2 h | Yes | Mirrors `tools/schema_lint/` from M5. Exit codes 0/1/2; `--json` flag. |
| S8 | Unit tests ≥ 85% coverage: `Expression` (7 cases), `ExpressionValidator` (13 fixtures from spec §5.2), `ExpressionEngine` (lifecycle, tick cadence, stats) | S2-S5 | 5 h | Yes | Each fixture in `tests/integration/fixtures/invalid_expressions/` produces a verifiable error message. |
| S9 | Integration tests (6 files per spec §5.3): basic / dependency_chain / cycle_rejection / type_promotion / qstring_rejection / restricted_syntax | S5 + S6 + S8 | 4 h | Yes | All wire through `SignalBufferRegistry` end-to-end. |
| S10 | Benchmark `bench_expression_engine.cpp` + `tests/benchmark/results/M7-baseline.md` | S4 | 3 h | Yes | 1000 ticks × 100 expressions × 5 sources. Targets p50 < 5 ms, p95 < 8 ms, p99 < 10 ms. HALT trigger #3 (p99 > 15 ms after one opt pass). |
| S11 | `.claude/M7-done.md` + freeze record + PR against main | S1-S10 green + CI green | 3 h | Yes | Mirrors M5/M6 closure flow. SHA256s for 5 files (3 hpp + 2 schema). |

**Total estimated effort**: 41 h, well within spec's 5-7 person-day
(40-56 h) budget. Slack reserved for §S3 validator error-message
polish and §S10 perf-target tuning if needed.

## 2. Subtask details

### S1 — Scaffolding + exprtk preflight + freeze-surface headers

**Deliverables**:

- `src/expression/CMakeLists.txt` adding `signalforge_expression`
  static lib. PUBLIC: Qt6::Core, signalforge_buffer, signalforge_decoder.
  PRIVATE: signalforge_observability, exprtk (header-only).
- `src/expression/expression.hpp` matching spec §4.2.
- `src/expression/expression_engine.hpp` matching spec §4.1.
- `src/expression/expression_validator.hpp` matching spec §4.3.
- `src/expression/{expression,expression_engine,expression_validator}.cpp`
  — ctor/dtor stubs that compile.
- Top-level `CMakeLists.txt` adds `add_subdirectory(src/expression)`.
- `tests/unit/expression/CMakeLists.txt` + a placeholder smoke test.

**Acceptance**:

- All three presets build clean.
- exprtk compiles under C++23 (HALT trigger #2 measurement point).
- Doxygen on every public declaration.
- `clang-format --dry-run -Werror` clean.

### S2 — `Expression` class with restricted whitelist + type promotion

**Deliverables**:

- `Expression::Impl` PIMPL containing exprtk's `symbol_table_t`,
  `expression_t`, `parser_t`, plus per-source `double` slots that
  the registry's reads bind to.
- Compile path: `parser.compile(formula, expression)`. Before
  registering with parser, walk the formula tokens (or use exprtk's
  AST visitor) to enforce the whitelist. Reject out-of-whitelist
  with descriptive errors.
- `Expression::evaluate(sources)`: writes source values into the
  bound slots; calls `expression.value()`; converts result to the
  declared `ExpressionOutputType`.
- Type-promotion helpers per spec §3.3.
- Move semantics implemented (for `ExpressionSet::expressions`
  vector storage).

**Acceptance**:

- Construct + evaluate with mixed bool/int64/double sources.
- Whitelist violations rejected at compile-time.
- `derivedSignalMetadata()` produces a valid `SignalMetadata`.

### S3 — `ExpressionValidator`

**Deliverables**:

- `ExpressionValidator::validateFile(path, availableSignals)` and
  `validateString(content, virtualPath, availableSignals)`.
- yaml-cpp parsing with `Mark()` for line numbers (mirrors M5
  `SchemaValidator`).
- Per-expression validation per spec §4.3 step list.
- Cycle detection (DFS three-color: white = unvisited, gray =
  in-progress, black = done; gray-on-gray edge = cycle).
- Topological sort produces `ExpressionSet` in evaluation order.
- All errors carry `{filePath, lineNumber, expressionId, message}`.

**Acceptance**:

- Each of the 13 error-fixture types in spec §5.2 produces an error
  with the right line + expression-id + actionable text.
- Valid yaml → `ExpressionSet` topologically sorted.

### S4 — `ExpressionEngine` (30 Hz tick + evaluation loop)

**Deliverables**:

- QObject-based engine with QTimer at config'd interval (default
  33 ms, `Qt::PreciseTimer`).
- `setExpressions(set)`: registers derived signals' metadata with
  `SignalBufferRegistry` via `onSignalsRegistered(virtualDriverId,
  derivedMetas)`. Stores the set.
- `start()` / `stop()` lifecycle; `isRunning()` query.
- `onTick()` slot:
  1. record `t0`
  2. for each expression in topo order:
     a. collect source values via `registry->bufferFor(srcId)
        ->queryLatestOne()`; if buf nullptr OR latest is nullopt,
        use NaN
     b. if any source is QString-typed: skip + increment expression
        evaluation-error counter (defensive; validator already
        rejects)
     c. `expr.evaluate(sources)` — catch exceptions, increment
        expression error counter
     d. `registry->onSignal(now, derivedId, result)`
  3. record `t1`; update `TickStats` (ticks_total++, eval_total += N,
     last_us, peak_us); set `expression_engine_tick_us` gauge.
- Stats accessor + signal emissions (`started`, `stopped`,
  `tickCompleted`).

**Acceptance**:

- Tick fires at the configured cadence (mock or wait-for-N-ticks
  unit test).
- Stats counters increment correctly.
- start/stop/start cycles cleanly.
- Tick miss handling: timer late = no catch-up (log + proceed).

### S5 — `ExpressionRegistrar`

**Deliverables**:

- `ExpressionRegistrar(SignalBufferRegistry&, ExpressionEngine&,
  std::vector<QString> yamlPaths)` constructor.
- `loadAndStart()` method:
  - For each yaml path: validate via `ExpressionValidator::validateFile`
    against `registry.signalIds()`-equivalent metadata.
  - Combine all valid sets into a single `ExpressionSet` (cycle
    detection re-runs across the union).
  - Pass to `engine.setExpressions(...)`.
  - `engine.start()`.
- Error path: log all validation errors with full context; do not
  start the engine.

**Acceptance**:

- Multi-file yaml load works (cycle detection across files).
- Bad file produces detailed log, engine remains stopped.

### S6 — Schema v1 canonical docs + example expressions

**Deliverables**:

- `schemas/expression_schema_v1.yaml` canonical example covering
  all yaml keys, signed off as the freeze artifact.
- `schemas/expression_schema_v1.json` JSON-Schema description used
  by the validator (or reference doc; spec leaves the format open).
- `examples/expressions/power_calculations.yaml` (per spec §2.1-15).
- `examples/expressions/alarms.yaml` (per spec §2.1-15).

**Acceptance**:

- Canonical yaml validates cleanly via `expr_lint`.
- Both example files validate cleanly when `availableSignals` covers
  their source IDs.

### S7 — `expr_lint` CLI tool

**Deliverables**:

- `tools/expr_lint/main.cpp` + `CMakeLists.txt` + `README.md`.
- `expr_lint <file.yaml>` validates without runtime; exit 0
  valid / 1 invalid / 2 bad CLI args.
- `--json` flag for machine-readable output.
- Optional `--base-signals signals.json` to feed available-signals
  catalog from a previous app run.

**Acceptance**:

- All M7 example files exit 0.
- All M7 invalid fixtures exit 1 with the right error in stderr.
- Smoke-tested as part of S8/S9.

### S8 — Unit tests ≥ 85% coverage

**Deliverables** (per spec §5.2):

- `tests/unit/expression/expression_test.cpp` (7 cases):
  construct + evaluate per source type, derivedSignalMetadata,
  move semantics, runtime evaluation errors.
- `tests/unit/expression/expression_validator_test.cpp`: 13 cases,
  one per fixture in
  `tests/integration/fixtures/invalid_expressions/`. Each verifies:
  validation fails, errors contain expected expression-id +
  actionable text + line number.
- `tests/unit/expression/expression_engine_test.cpp` (5 cases):
  empty set ready, single expression set count, start/stop cycles,
  tick stats increment, tick miss handling.
- `tests/integration/fixtures/invalid_expressions/` directory with
  the 13 yaml fixtures (one per error category).

**Acceptance**:

- Coverage ≥ 85% on `expression.cpp` + `expression_engine.cpp`,
  ≥ 90% on `expression_validator.cpp` (verified in CI quota
  permitting).
- All test cases pass under Debug + Release.

### S9 — Integration tests (spec §5.3)

**Deliverables** (6 files in `tests/integration/`):

- `test_expression_engine_basic.cpp` — full M6 registry +
  `power = v * i` round trip.
- `test_expression_engine_dependency_chain.cpp` — A→B→C topo order.
- `test_expression_engine_cycle_rejection.cpp` — yaml with cycle;
  validator + engine refuses to start.
- `test_expression_engine_type_promotion.cpp` — bool/int64/double
  source mix.
- `test_expression_engine_qstring_rejection.cpp` — QString source
  rejected at validation.
- `test_expression_engine_restricted_syntax.cpp` — `if`/`while`/`?:`
  rejected.

**Acceptance**:

- All 6 pass under Debug + Release.
- All 6 ASan clean in CI.

### S10 — Benchmark + M7-baseline.md

**Deliverables**:

- `tests/benchmark/bench_expression_engine.cpp`: 500 base signals
  + 100 expressions × 5 sources avg + 1000 ticks. Captures p50,
  p95, p99 wall time per tick.
- `tests/benchmark/results/M7-baseline.md` with results +
  per-expression hotspot breakdown.

**Acceptance gates** (per spec §5.4 / §7-3):

- p50 < 5 ms, p95 < 8 ms, p99 < 10 ms.
- HALT trigger: p99 > 15 ms after one optimization pass.

### S11 — Closure: M7-done.md + PR

**Deliverables**:

- `.claude/M7-done.md` per spec §6.3:
  - Deliverables checklist
  - Acceptance self-check per spec §8
  - Test count matrix
  - Benchmark summary
  - Freezes section with sha256s for 5 files (3 hpp + 2 schema)
  - Commit manifest
  - CI verification status
  - Hand-off to M8 / M10 / M11 / M12
  - HALT resolution trail
  - Deviations and concerns
- PR against `main` titled `M7: Expression Engine`.
- Stop and announce per CLAUDE.md §Phase 1 step 6:
  "M7 ready. Awaiting approval to merge M7 and begin M8 bootstrap."

## 3. Pre-encoded HALT statements (spec §7)

| # | Trigger | Measurement point | Action |
|---|---|---|---|
| 1 | Modification to M2/M3/M4/M5/M6 frozen `.hpp` | Pre-commit `git diff` against the frozen file list (extends M6's list with `src/buffer/signal_buffer.hpp` + `signal_buffer_registry.hpp`) | HALT report `.claude/halt/HALT-<ts>-frozen-modified.md`; revert change |
| 2 | exprtk version conflict (C++23 compile failure) | S1 build under all three presets | HALT report; propose ADR for upgrade or replacement; await human |
| 3 | Tick p99 > 15 ms after one optimization pass | S10 benchmark + one optimization iteration | HALT report; identify hotspot (tick body, evaluation, registry I/O) |
| 4 | Cycle detection broken | S3 + S8: 13 fixtures; adversarial 3-cycle / self-loop / mutual-recursion | HALT report; algorithm replacement |
| 5 | 1-ulp drift between computed result and hand-computed reference | S8 numerical-correctness suite | HALT report; type-promotion path bug |
| 6 | 1-hr × 30 Hz soak memory leak (ASan / LSan) | S10 long-run scenario or CI debug-asan | HALT report; lifecycle/ownership audit |
| 7 | UI thread block > 100 ms during engine start | S5/S9 measurement | HALT report; defer yaml load to a worker thread |

CLAUDE.md §HALT triggers (compile error after 3 fixes, test fail
after 3 fixes, etc.) apply at every subtask.

## 4. Risk register

| Rank | Risk | Mitigation built into the plan |
|---|---|---|
| 1 | exprtk C++23 compile failure | S1 preflight; HALT trigger #2 |
| 2 | Validator error-message quality (user-facing) | S3 + S8 each fixture verifies the error text; spec §9 emphasizes |
| 3 | Numerical drift in type promotion | S8 dedicated correctness tests; HALT trigger #5 |
| 4 | Cycle detection adversarial cases | S8 13 fixtures including self-loop, 2-cycle, 3-cycle |
| 5 | Tick p99 perf miss | S10 benchmark; only one optimization pass per HALT trigger #3 |
| 6 | Memory leak in long soak | S10 + CI debug-asan; HALT trigger #6 |
| 7 | UI block on engine start | S5/S9; HALT trigger #7 |

## 5. Performance budget breakdown

Per spec §3.2 cost model + §5.4 target:

| Operation | Per-tick cost | Frequency | Total |
|---|---|---|---|
| `queryLatestOne` × 5 sources × 100 expr | ~100 ns | once | ~50 µs |
| Expression evaluation (exprtk) × 100 | ~5 µs | once | ~500 µs |
| `onSignal` write × 100 | ~150 ns | once | ~15 µs |
| Tick body overhead (timer, stats) | ~10 µs | once | ~10 µs |
| **Total per tick** | — | — | **~575 µs** |

Spec target p50 < 5 ms = ~10× margin above the cost model.
p99 < 10 ms = ~20× margin. The S10 benchmark verifies the model.

If margin is much smaller than expected (e.g., p99 in the 8-15 ms
range), profile and optimize one of:

1. Tick body locks (registry mutex during `bufferFor`).
2. Per-evaluation `std::vector<pair>` allocation (preallocate).
3. exprtk's `value()` result conversion.

## 6. Out of scope (spec §2.2 reminder; HALT if encountered)

If any of the following becomes seemingly necessary, HALT:

- Modifications to M2/M3/M4/M5/M6 frozen `.hpp` files.
- Control flow in expressions.
- User-defined functions.
- String/text operations in expressions.
- Statistical / aggregate-over-time functions.
- Expression hot-reload.
- GUI expression editor.
- Per-source-event evaluation.
- New top-level dependencies beyond exprtk.
- `QObject` `Expression` class.

## 7. Closure flow

Per CLAUDE.md §Git operation protocol Phase 1:

1. S1-S11 complete on `milestone/M7`.
2. `git push origin milestone/M7`; report.
3. Wait for CI green; report.
4. `gh pr create` against `main`; report PR # + URL.
5. File `.claude/M7-done.md` with merge SHA placeholder + CI status.
6. Stop and announce: "M7 ready. Awaiting approval to merge M7 and
   begin M8 bootstrap."

Phase 2 (human approval) → Phase 3 (CC merges, tags
`v0.0.8-alpha.1`, bootstraps M8) follow the standard protocol. **No
tag, no merge, no M8 bootstrap is started without explicit Phase 2
approval.**
