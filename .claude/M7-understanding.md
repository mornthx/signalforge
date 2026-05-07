# M7 — Understanding

## 1. Restatement of the M7 goal

M7 delivers the **expression engine**: a 30 Hz batch evaluator that
reads base signals from the M6 `SignalBufferRegistry`, computes
user-authored derived signals via a restricted-exprtk formula
language, and writes the results back into the same registry as
new derived signals. Downstream M8 Chart UI / M10 Session Writer
treat derived and base signals identically.

This is **the second user-authored artifact tier in V1**: yaml
expression files become contract-level outputs, mirroring M5's
yaml schema v1.

Hard-stop types (concurrent):

1. **Interface freeze**: `Expression`, `ExpressionEngine`,
   `ExpressionValidator` public APIs + the `ExpressionSet` /
   `ExpressionEngineConfig` / `ExpressionValidationError` struct
   layouts.
2. **yaml expression schema v1 freeze**: top-level + per-expression
   keys.
3. **Implementation correctness**: all 13 validator error fixtures
   reject with actionable messages; type-promotion rules verified;
   topological evaluation order verified; 1-ulp numerical drift
   ceiling.

**Soft-HALT is not allowed** (inherits M2-M6 stance).

Quality philosophy carried forward: **explicit registration errors
over silent failure** (per spec §1). yaml typos must produce
file:line:expression-id errors at registration time, not silent NaN
runtime values.

## 2. Observed repo state

```
$ git log --oneline -3
08a04785 Merge pull request #8 from mornthx/milestone/M6
eb86f0bd Merge pull request #9 from mornthx/docs/m7-spec
7b32fbcd docs: add M7 expression engine spec
```

Phase 3 actions completed this session (report confirmed):

- PR #8 merged at `08a04784cb6ba85986ef017513b11425c5dac1a9`.
- Tag `v0.0.7-alpha.1` pushed, pointing at the M6 merge.
- `milestone/M7` branch created from `08a0478` and pushed.

`docs/milestones/M7-expression-engine.md` (868 lines) is on
origin/main via PR #9 and visible on `milestone/M7`.

`src/expression/` does not yet exist; the M7 implementation creates
it. `exprtk` is already a project dep via FetchContent (pinned at
`0.0.3`, `cmake/dependencies.cmake`).

## 3. Scope reminder

### Must deliver (spec §2.1)

- `Expression` (`src/expression/expression.{hpp,cpp}`): one compiled
  exprtk expression with type-promotion rules.
- `ExpressionEngine` (`src/expression/expression_engine.{hpp,cpp}`):
  QObject-based 30 Hz tick evaluator. Reads from
  `SignalBufferRegistry`; writes derived signals back to the same
  registry.
- `ExpressionValidator`
  (`src/expression/expression_validator.{hpp,cpp}`): yaml + cycle +
  type + restricted-syntax checks. Returns `std::expected<…>`.
- `ExpressionRegistrar`
  (`src/expression/expression_registrar.{hpp,cpp}`): app-startup
  hook that loads yaml, validates, hands off to engine.
- yaml schema v1 (`schemas/expression_schema_v1.yaml` +
  `…_v1.json`).
- `expr_lint` CLI tool (`tools/expr_lint/`).
- 6 integration tests (per spec §5.3).
- Unit tests ≥ 85% on expression modules (validator ≥ 90%).
- Benchmark + `tests/benchmark/results/M7-baseline.md`.
- 2 example expression files (`examples/expressions/`).
- Doxygen on public declarations.
- `.claude/M7-done.md` + freeze record.

### Must not do (spec §2.2)

- No modifications to M2/M3/M4/M5/M6 frozen `.hpp` (HALT trigger).
- No control flow in expressions (`if`/`while`/`for`/`?:`).
- No user-defined functions; no exprtk built-ins outside the
  whitelist.
- No string/text operations.
- No statistical / aggregate-over-time functions.
- No expression hot-reload; no GUI editor; no per-source-event
  evaluation (V1.5+ deferrals).
- No new top-level dependencies beyond exprtk (already pinned).
- No QObject `Expression` class — only `ExpressionEngine` is QObject
  (for the QTimer).

## 4. M7 freeze surface (locks at M7 close)

Per spec §6.1:

- `src/expression/expression.hpp`:
  - `Expression` class — public method signatures
  - `ExpressionSet` struct layout
  - `ExpressionOutputType` enum
- `src/expression/expression_engine.hpp`:
  - `ExpressionEngine` class — public method signatures (Q_OBJECT
    macros included)
  - `ExpressionEngineConfig` struct layout
  - `TickStats` struct layout
- `src/expression/expression_validator.hpp`:
  - `ExpressionValidator` class — public static methods
  - `ExpressionValidationError` struct layout
  - `ExpressionValidationResult` typedef
- yaml expression schema v1:
  - Top-level: `schema_version`, `description`, `expressions`
  - Per-expression: `id`, `name`, `unit`, `formula`, `type`,
    `description`

What does **not** freeze (spec §6.2): exprtk whitelist contents
(additive), 30 Hz tick interval (configurable), topo-sort algorithm
choice, internal `Expression::Impl` PIMPL layout, `expr_lint`
internals.

## 5. Locked design decisions (spec §3.1-§3.7, reflected verbatim)

These decisions confirmed in pre-M7 planning. M7 implements as
written; does not re-evaluate.

### 5.1 Restricted exprtk syntax (§3.1)

V1 exposes a curated subset of exprtk: arithmetic + comparison +
logical + a built-in math whitelist (`min`, `max`, `abs`, `sqrt`,
`log`, `log10`, `exp`, `sin`, `cos`, `tan`, `floor`, `ceil`,
`round`). Validator parses formula → walks AST → rejects nodes
outside whitelist with file:line + actionable text. V1.5+ may
expand non-breakingly.

### 5.2 30 Hz batch evaluation (§3.2)

Single QTimer at 33.33 ms (`Qt::PreciseTimer`) triggers a
topologically-ordered batch over all expressions. Source values
read at tick time (not source-update time). Aligns with M8's 30 Hz
chart cadence; bounded CPU cost.

Cost model (100 expressions × 5 sources): 600 µs per tick = 1.8%
of budget. Latency floor: derived signal lags source by ≤ 33 ms.

### 5.3 Numeric type promotion to double (§3.3)

exprtk computes in `double`. Source variants converted on read:
`bool` → 0.0/1.0; `int64_t` → static_cast<double> with WARN once
when |value| > 2^52; `double` → identity; `QString` → registration
error. Result mapped to declared output type (`double` default,
`bool` via `!= 0`, `int64` via `round` with WARN on out-of-range).

Output type declared in yaml (`type:` key); defaults to `double`.

### 5.4 Static cycle detection at registration (§3.4)

Validator parses each formula → extracts source signal IDs → builds
directed graph. DFS-based cycle detection (or Kahn's algorithm —
implementer choice). On cycle: reject the **entire ExpressionSet**
(not individual expressions) with the cycle path as error message
(`"power → efficiency → derived_voltage → power"`). Topological
sort doubles as evaluation order.

Self-loops detected as cycles of length 1; forward references
allowed (yaml order is irrelevant after validation).

### 5.5 Derived signals in same SignalBufferRegistry (§3.5)

Engine registers derived signals' metadata via the existing M6
`SignalValueSink::onSignalsRegistered` API at engine start.
Buffers allocated by M6 identically to base signals. Writes use
`onSignal`. Driver-id convention for derived signals:
`expression-engine` (a virtual driver). Signal IDs: plain (no
prefix) per yaml `id` field.

### 5.6 No soft-HALT (§3.6)

Inherits M2-M6 stance.

### 5.7 Metric naming (§3.7)

Per the established `<module>_<metric>_<scope>` convention:

- `expression_engine_ticks_total` (counter)
- `expression_engine_tick_us` (gauge)
- `expression_engine_evaluations_total` (counter)
- `expression_evaluation_us_<expressionId>` (gauge)
- `expression_evaluation_errors_<expressionId>` (counter)
- `expression_compile_failures` (counter)

## 6. Performance targets (spec §5.4)

Tick latency at scale (500 base signals, 100 expressions averaging
5 sources each, 1000 ticks):

| Percentile | Target | HALT |
|---|---|---|
| p50 | < 5 ms | — |
| p95 | < 8 ms | — |
| p99 | < 10 ms | > 15 ms |

Plan §S10 measures these. The plan does **not** pre-allocate
counter-mitigations; "measure first, optimize only on miss" per
session-prompt convention from M6.

## 7. M7 HALT triggers (spec §7, beyond CLAUDE.md §HALT)

These are encoded into plan §3 with measurement points.

1. Modification to M2/M3/M4/M5/M6 frozen `.hpp` → HALT.
2. exprtk version conflict (M0's pinned 0.0.3 doesn't compile under
   C++23) → HALT, propose ADR for upgrade or replacement. Mitigation
   gate at S1 preflight.
3. Tick p99 > 15 ms after one optimization pass → HALT (S10
   measurement point).
4. Cycle detection produces false positives or misses adversarial
   cases → HALT (algorithm broken).
5. 1-ulp drift between computed result and hand-computed reference
   → HALT (numerical correctness broken).
6. Memory leak in 1-hr × 30 Hz = 108 k-tick soak (ASan / LSan) →
   HALT.
7. UI thread blocks > 100 ms during engine start (large yaml load)
   → HALT.

## 8. Risks and design notes

1. **exprtk C++23 compatibility**. M0 pinned `0.0.3`. M5 already
   compiles under C++23 with various Qt + spdlog deps; exprtk should
   too, but S1 preflight verifies. Mitigation: HALT trigger #2
   guards this.
2. **Cycle detection correctness**. DFS three-color (white/gray/
   black) is the cleanest implementation; the algorithm is textbook
   but adversarial fixtures (3-cycles, self-loops, mutual recursion)
   will exercise edges. S2 unit tests cover all 13 error fixtures.
3. **Type promotion edge cases**. `int64_t` → `double` loses
   precision above 2^52. The WARN-once-per-signal mechanism keeps
   logs sane. Tests in S8 cover the boundary.
4. **NaN propagation when source has no data**. `queryLatestOne`
   returns nullopt before any push. Engine treats this as NaN
   source; result is NaN; pushed to derived buffer. M8 chart
   renders gap. Documented in spec §4.7.
5. **Tick miss handling**. If QTimer fires late under system load,
   don't catch up — log a tick-miss metric, proceed. Catch-up
   creates worse spikes. Spec §9 explicit on this.
6. **30 Hz tick is hard cadence, not best-effort**. Spec §9 reminder.
7. **Validator messages are user-facing**. Every validation failure
   gets file:line:expression-id + actionable text. Bad: "syntax
   error". Good: "bad.yaml:12 — expression 'power' formula uses
   operator 'if' which is not in V1's whitelist (allowed:
   arithmetic, comparison, logical, math built-ins). Use a separate
   expression with a comparison instead." Spec §9 emphasizes this.
8. **Local ASan blocked**. Per memory `host_asan_preload`, the host's
   `/etc/ld.so.preload` (`AppProtection.so`) prevents ASan-instrumented
   binaries from running locally. CI is the authoritative ASan/UBSan
   gate. Same protocol as M5/M6.

## 9. Hand-off to downstream milestones

- **M8 (Chart UI)**: derived signals appear in the registry; no
  special handling needed. M8 may render a "Derived" group via
  `signalIdsForDriver("expression-engine")` — does not require an
  M7 freeze-surface change.
- **M10 (Session Writer)**: persists derived signals identically to
  base signals.
- **M11 (Replay)**: derived signals are re-derived by the engine
  during replay (fed by replayed base signals), not loaded from
  disk. M10's persistence captures derived snapshots so M11 can
  cross-check.
- **M12 (Performance Optimization)**: tick budget profile + hot
  expressions for potential vectorization. Inherits the same M6
  storage-overhead reduction goal.

## 10. Open ambiguities (none blocking)

None known at this time. M7 spec is more concrete than M5/M6's
were at this stage — design decisions §3.1-§3.7 are explicit and
locked, the cost model is quantified (§3.2), the validator's
error format is specified (§3.4), and the freeze surface is
fully enumerated.

If discoveries during implementation surface ambiguities, they will
be logged in `.claude/M7-concerns.md` and (per CLAUDE.md
§Ambiguity) resolved without HALT when additive, HALTed otherwise.

## 11. Definition of Done

A subtask is "done" when CLAUDE.md §Definition of Done is
satisfied: clean build under Debug + Release + debug-asan, all
relevant tests pass under all three (debug-asan via CI per the
M5/M6 protocol), coverage target met, Doxygen on public
declarations, clang-format / clang-tidy clean, conforming commit,
`.claude/M7-progress.md` updated.

The milestone is "done" when spec §8 acceptance criteria are
satisfied, the freeze record in `.claude/M7-done.md` is filed with
sha256s for 5 files (3 hpp + 2 schema), and a PR is opened against
main.
