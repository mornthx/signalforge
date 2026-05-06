# M7 — Expression Engine

| Field | Value |
|---|---|
| Milestone ID | M7 |
| Sprint | 7 |
| Estimated effort | 5-7 person-days |
| Prerequisites | M6 closed (main at v0.0.7-alpha.1) |
| Next milestone | M8 (Real-time Chart UI) |
| Hard-stop type | **Interface freeze** (`ExpressionEngine` API + yaml expression schema) + **Implementation correctness** |
| Soft-HALT allowed | **No** |
| Branch | `milestone/M7` |

**Cross-reference notation**:

- `[EM §N]` — Execution Manual, section N
- `[Arch §N]` — Architecture document, section N
- `[MR]` — Milestone Roadmap
- `[CM §X]` — CLAUDE.md, section X
- `[ADR-N]` — Architecture Decision Record N
- `[M<n> §N]` — M<n> spec

---

## 1. Goal

Compute derived signals from base signals using expressions evaluated at 30 Hz batch cadence.

A user authors expressions like `power = voltage * current` or `over_temp = temperature > 80` in yaml. The Expression Engine compiles each expression at registration time, evaluates it on every 30 Hz tick by reading current values from the `SignalBufferRegistry`, and writes the result back as a new "derived" signal in the same registry. Downstream (M8 Chart UI, M10 Session Writer) sees derived signals identically to base signals.

This milestone freezes:

1. The `ExpressionEngine` C++ interface for expression registration / evaluation control.
2. The yaml expression schema (separate from M5's decode schema).

Quality philosophy from previous milestones: **explicit registration errors over silent failure**. A typo in an expression must produce a clear validation message at registration time, not a runtime silent NaN. Cycle detection rejects bad configurations before any evaluation happens.

---

## 2. Scope

### 2.1 Must deliver

1. **`ExpressionEngine`** at `src/expression/expression_engine.{hpp,cpp}`:
   - Singleton-like instance owned by `MainWindow` (or `main.cpp`)
   - Holds compiled expressions + dependency graph + 30 Hz tick timer
   - Reads from `SignalBufferRegistry` (M6); writes derived signals back to the same registry
   - Lifecycle: start (begin 30Hz ticks) / stop (halt evaluation, retain registrations)

2. **`Expression`** at `src/expression/expression.{hpp,cpp}`:
   - Wraps a single compiled exprtk expression
   - Stores its formula (for diagnostics), source signal IDs (dependency edges), result signal ID
   - `evaluate(snapshot)` reads source values, runs exprtk, returns result `SignalValue`

3. **yaml expression schema v1** at `schemas/expression_schema_v1.yaml`:
   - Top-level keys: `schema_version`, `expressions`
   - Each expression: `id`, `name`, `unit`, `formula`, optional `description`
   - JSON-Schema meta-format at `schemas/expression_schema_v1.json`

4. **`ExpressionValidator`** at `src/expression/expression_validator.{hpp,cpp}`:
   - Loads + validates yaml against the meta-schema
   - Compiles each formula (exprtk syntax check)
   - Static cycle detection: build dependency graph, reject if A → B → A
   - Type checking: reject expression if any source variable references a `QString`-typed signal in `SignalBufferRegistry`
   - Returns validated `ExpressionSet` ready for engine consumption
   - Same validator used by `expr_lint` CLI tool (§2.1-7)

5. **30 Hz batch evaluation**:
   - QTimer at 33.33 ms (30 Hz)
   - On each tick, for each expression in dependency-resolved order:
     - Query current value of each source signal via `SignalBufferRegistry::bufferFor(signalId)->queryLatestOne()`
     - Substitute into exprtk variables
     - Evaluate
     - Push result to derived signal's buffer via `SignalBufferRegistry::onSignal(now, derivedId, value)`

6. **Restricted exprtk syntax (per decision M7.1 Option B)**:
   - Allow:
     - Arithmetic: `+`, `-`, `*`, `/`, `%`, `^` (power)
     - Comparison: `<`, `>`, `==`, `!=`, `<=`, `>=`
     - Logical: `and`, `or`, `not`
     - Built-in functions: `min`, `max`, `abs`, `sqrt`, `log`, `log10`, `exp`, `sin`, `cos`, `tan`, `floor`, `ceil`, `round`
     - Literal numbers, parentheses, signal-name references
   - Reject (validator catches):
     - Control flow: `if`, `while`, `for`, `?:`
     - User-defined functions
     - String operations
     - exprtk built-ins outside the whitelist (e.g., `sgn`, `cot`, statistical functions)

7. **Type promotion rules (per decision M7.3 Option P)**:
   - Source `bool` → exprtk `0.0` / `1.0`
   - Source `int64_t` → exprtk `double` (precision warning at registration if values exceed 2^52 range observable in metadata)
   - Source `double` → exprtk `double` directly
   - Source `QString` → registration error: "expression cannot reference QString-typed signal"
   - Result always `double`. Registration may declare result `type: bool` (truthy → 1.0/true mapping) or `int64` (round-to-int with warning), defaulting to `double`.

8. **Cycle detection (per decision M7.4 Option M)**:
   - At registration: parse formula → extract referenced signal IDs → add edges to dependency graph
   - DFS / topological sort to find cycles
   - Reject the entire `ExpressionSet` on cycle (not silently drop one) with clear error: `"cycle detected: A → B → C → A"`
   - Topological order is also the evaluation order

9. **Derived signals in same registry (per decision M7.5 Option R)**:
   - At engine start, register all derived signals' metadata with `SignalBufferRegistry` via `registerDerivedSignals(...)` API
   - Result writes use the same `SignalValueSink::onSignal` interface as M5 decoders
   - M8 Chart, M10 Session Writer treat derived and base signals identically

10. **`ExpressionRegistrar`** at `src/expression/expression_registrar.{hpp,cpp}`:
    - Listens for `SignalBufferRegistry`'s "all base signals registered" event (or app-level lifecycle hook)
    - Loads yaml expression files
    - Compiles + registers expressions with `ExpressionEngine`
    - Triggers engine start

11. **Expression lint CLI tool** at `tools/expr_lint/`:
    - Mirrors `tools/schema_lint/` from M5
    - `expr_lint <file.yaml>` validates without runtime
    - Output: human-readable; `--json` flag for machine-readable
    - Exit 0 valid, 1 invalid, 2 bad CLI args

12. **Integration tests** at `tests/integration/`:
    - `test_expression_engine_basic.cpp` — register `power = v * i`, push v=12, i=2, verify power=24 derived signal
    - `test_expression_engine_dependency_chain.cpp` — A depends on B, B depends on C; verify topological evaluation order
    - `test_expression_engine_cycle_rejection.cpp` — circular dependencies rejected with clear error
    - `test_expression_engine_type_promotion.cpp` — bool / int64 sources convert correctly
    - `test_expression_engine_qstring_rejection.cpp` — QString source rejected at registration
    - `test_expression_engine_restricted_syntax.cpp` — `if`/`while`/`?:` formulas rejected

13. **Unit tests** ≥ 85% coverage on expression modules

14. **Benchmark** at `tests/benchmark/bench_expression_engine.cpp`:
    - 30 Hz tick × 100 expressions × 5 source signals each = 15,000 source reads + 100 evaluations per tick
    - Target: tick wall time < 10 ms (30% of the 33.33 ms budget)
    - Results to `tests/benchmark/results/M7-baseline.md`

15. **Example expression files** at `examples/expressions/`:
    - `power_calculations.yaml` — voltage × current = power, plus efficiency
    - `alarms.yaml` — threshold-based bool signals
    - Used in tests and user documentation

16. **Doxygen** on all public declarations

17. **`.claude/M7-done.md`** with standard completion report + freeze record

### 2.2 Must not do

1. **No modifications to M2/M3/M4/M5/M6 frozen `.hpp`**. If freeze-scope change seems needed, HALT.
2. **No control flow in expressions**. `if`/`while`/`for`/`?:` rejected at validation.
3. **No user-defined functions**. Built-in whitelist only (decision M7.1).
4. **No string/text operations**. exprtk's string capabilities not exposed.
5. **No statistical / aggregate functions over time** (`mean(signal_a, last 10s)`). V1.5+ if needed; V1 expressions are point-wise.
6. **No expression hot-reload**. Engine reads yaml at startup; reloading is manual restart for V1.
7. **No GUI expression editor**. V1 hand-authors yaml; V1.5+ adds visual editor (mirrors M5's GUI deferral).
8. **No batched evaluation per source change**. 30 Hz fixed cadence (decision M7.2). Per-source-event evaluation is V1.5+ if needed.
9. **No new top-level dependencies beyond exprtk**. exprtk is already a project dep from M0.
10. **No QObject `Expression` class**. Pure C++; engine is QObject for the QTimer.

---

## 3. Design Decisions (locked by this spec)

### 3.1 Restricted exprtk syntax

**Decision**: V1 exposes a curated subset of exprtk operators and functions. Validator rejects out-of-whitelist syntax with clear errors.

**Rationale**: exprtk is powerful (full expression language including control flow, user functions, statistical aggregates). V1's typical use is pointwise math: `power = v * i`, `over_temp = t > 80`. Allowing the full language risks:
- Performance unpredictability (a complex `if` chain can take 10× longer than arithmetic)
- User confusion (edge cases of exprtk's parsing, e.g., short-circuit semantics)
- Maintenance burden (more surface to test, document, support)

V1.5+ can expand the whitelist as real use cases emerge. Whitelist additions are non-breaking.

**Validator implementation**: parse formula, walk AST, check each node against whitelist. Reject with line + position.

### 3.2 30 Hz batch evaluation

**Decision**: Single QTimer at 33.33 ms triggers a batch evaluation of all expressions in topological order. Source signal values are read at tick time (not at source-update time).

**Rationale**: per decision M7.2 Option Y. Aligns with M8 Chart UI's 30Hz render cadence; user perceives derived values updating "in sync" with charts. Bounded CPU cost: at most 30 evaluations/sec/expression regardless of source rate.

**Cost model** (100 expressions, 5 source signals each):
- 500 `queryLatestOne` calls per tick × ~100 ns = 50 µs
- 100 expression evaluations × ~5 µs = 500 µs
- 100 `onSignal` writes × ~150 ns = 15 µs
- Total: ~600 µs per tick = 1.8% of 33 ms budget

**Latency**: derived signal lags source by up to 33 ms. Acceptable for visual chart usage. Real-time control loops (M9 future) would need a different mechanism (V1.5+).

**Implementation note**: `QTimer::setTimerType(Qt::PreciseTimer)` to keep tick jitter within ±1 ms.

### 3.3 Numeric type promotion to double

**Decision**: per decision M7.3 Option P. exprtk computes in `double`. Source variants converted on read; result mapped to declared output type at write.

**Conversion rules**:

```
Source bool      → 0.0 (false) or 1.0 (true)
Source int64_t   → static_cast<double>; if abs(value) > 2^52, log WARN once per signal
Source double    → identity
Source QString   → registration-time error (cannot evaluate)
```

**Result mapping** (from `double` to declared output type):

```
Output double → identity
Output bool   → (value != 0.0)
Output int64  → static_cast<int64_t>(std::round(value)); WARN if value out of int64 range
```

**Output type declared in yaml**:
```yaml
expressions:
  - id: power
    formula: voltage * current
    type: double  # default if omitted
  - id: alarm_active
    formula: temperature > threshold
    type: bool
```

### 3.4 Static cycle detection at registration

**Decision**: per decision M7.4 Option M. The full set of expressions is registered atomically. Validator parses each formula, builds the dependency graph (edges from result to sources), detects cycles via DFS, rejects the entire set if any cycle exists.

**Why entire set, not single expression**: a cycle involves N expressions; rejecting just one breaks user expectations. Reject the set, present the cycle path, let user fix yaml.

**Error format**:
```
Expression set validation failed: cycle detected
  power -> efficiency -> derived_voltage -> power
File: examples/expressions/bad.yaml line 12 (efficiency)
```

**Topological sort serves dual purpose**: cycle detection + evaluation order. After validation, expressions stored in topo order; engine evaluates left-to-right per tick.

**Self-loops** (`a = a + 1`): detected as cycle of length 1. Rejected.

**Forward references** (`a = b + 1` where `b = c * 2` declared later in yaml): allowed; topo sort resolves order.

### 3.5 Derived signals in same SignalBufferRegistry

**Decision**: per decision M7.5 Option R. At engine start, derived signals are registered with `SignalBufferRegistry` via the existing `onSignalsRegistered` API. Buffers allocated identically to base signals. Writes use `onSignal`.

**Rationale**: M6 already designed for multiple producer drivers. Adding the Expression Engine as another producer requires no M6 API changes.

**Driver ID convention for derived signals**: a virtual driver ID like `expression-engine`. Allows `SignalBufferRegistry::signalIdsForDriver("expression-engine")` to enumerate just derived signals if downstream needs to distinguish.

**Signal ID convention**: derived signals use plain IDs from yaml `id` field. No prefix. M8 Chart UI will display them in a "Derived" group via metadata's `description` field or a future `category` field (additive, not in M7 freeze).

### 3.6 No soft-HALT (inherits from M2-M6)

Hard HALT only.

### 3.7 Metric naming

Per the established `<module>_<metric>_<scope>` convention:

- `expression_engine_ticks_total` (counter, registry-level): total 30Hz ticks executed
- `expression_engine_tick_us` (gauge): most-recent tick wall time
- `expression_engine_evaluations_total` (counter): cumulative expression evaluations
- `expression_evaluation_us_<expressionId>` (gauge): most-recent evaluation latency for that expression
- `expression_evaluation_errors_<expressionId>` (counter): runtime evaluation errors (div-by-zero, domain error)
- `expression_compile_failures` (counter, registration-time): compile errors caught by validator

---

## 4. Key Implementation Details

### 4.1 `ExpressionEngine` class

Place at `src/expression/expression_engine.hpp`.

```cpp
// src/expression/expression_engine.hpp
#pragma once

#include "buffer/signal_buffer_registry.hpp"
#include "decoder/decoder_interface.hpp"  // For SignalValue, SignalMetadata
#include "expression/expression.hpp"

#include <QObject>
#include <QString>
#include <QTimer>
#include <chrono>
#include <memory>
#include <mutex>
#include <vector>

namespace signalforge::expression {

/// Configuration for the engine.
struct ExpressionEngineConfig {
    /// Tick interval. Default 33.33 ms (30 Hz).
    std::chrono::milliseconds tickInterval{33};

    /// If true, the timer is precise (Qt::PreciseTimer). Default true.
    bool useTimePrecision = true;

    /// Driver ID used when registering derived signals with the
    /// SignalBufferRegistry. Default "expression-engine".
    QString virtualDriverId = "expression-engine";
};

/// 30Hz batch-evaluating expression engine.
///
/// Reads source signal values from a SignalBufferRegistry, evaluates
/// each registered expression in topologically-sorted order, and
/// writes results back to the same registry.
///
/// Lifecycle:
/// - Construct with a registry reference + config
/// - Call `setExpressions(set)` with a validated ExpressionSet (from
///   ExpressionValidator)
/// - Call `start()` to begin tick evaluation
/// - Call `stop()` to halt; restart with `start()` retains registrations
///
/// Thread affinity: QObject; lives on the main thread (or whichever
/// thread the QTimer should fire on). All SignalValueSink writes happen
/// from this thread.
///
/// Freeze scope: this class is frozen at M7 close.
class ExpressionEngine : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ExpressionEngine)

public:
    explicit ExpressionEngine(
        signalforge::buffer::SignalBufferRegistry& registry,
        ExpressionEngineConfig config = {},
        QObject* parent = nullptr);
    ~ExpressionEngine() override;

    /// Register an already-validated ExpressionSet.
    /// Pre-condition: validator has done cycle detection + type checks.
    /// Post-condition: derived signals registered with the registry;
    /// engine ready for `start()`.
    /// Idempotent if called with the same set; replaces previous set
    /// otherwise (not recommended; restart preferred).
    void setExpressions(ExpressionSet expressions);

    /// Start 30Hz tick evaluation.
    void start();

    /// Stop tick evaluation. Registrations retained.
    void stop();

    /// Whether currently ticking.
    [[nodiscard]] bool isRunning() const noexcept;

    /// Number of registered expressions.
    [[nodiscard]] std::size_t expressionCount() const noexcept;

    /// Most-recent tick wall time in microseconds.
    [[nodiscard]] std::chrono::microseconds lastTickDurationUs() const noexcept;

    /// Tick statistics for diagnostics.
    struct TickStats {
        std::uint64_t ticksTotal = 0;
        std::uint64_t evaluationsTotal = 0;
        std::uint64_t evaluationErrors = 0;
        std::chrono::microseconds lastTickDurationUs{0};
        std::chrono::microseconds peakTickDurationUs{0};
    };
    [[nodiscard]] TickStats stats() const;

signals:
    void started();
    void stopped();
    void tickCompleted(std::uint64_t tickIndex);

private:
    void onTick();

    signalforge::buffer::SignalBufferRegistry* registry_;
    ExpressionEngineConfig config_;
    QTimer tickTimer_;
    ExpressionSet expressions_;  // Topologically sorted

    mutable std::mutex statsMutex_;
    TickStats stats_;
};

}  // namespace signalforge::expression
```

### 4.2 `Expression` class

Place at `src/expression/expression.hpp`.

```cpp
// src/expression/expression.hpp
#pragma once

#include "decoder/decoder_interface.hpp"  // For SignalValue, SignalType, SignalMetadata

#include <QString>
#include <chrono>
#include <memory>
#include <vector>

namespace signalforge::expression {

/// Output type tag for an expression result.
enum class ExpressionOutputType {
    Double,
    Bool,
    Int64,
};

/// One compiled expression. Pure C++ (not QObject).
class Expression {
public:
    /// Construct from validated parameters. Compiles the formula via
    /// exprtk; throws on internal compilation failure (validator
    /// should catch all user-facing errors before this).
    Expression(QString id,
               QString name,
               QString unit,
               std::optional<QString> description,
               QString formula,
               ExpressionOutputType outputType,
               std::vector<QString> sourceSignalIds);

    ~Expression();

    Expression(const Expression&) = delete;
    Expression& operator=(const Expression&) = delete;
    Expression(Expression&&) noexcept;
    Expression& operator=(Expression&&) noexcept;

    /// The expression's identifier (matches the derived signal's ID).
    [[nodiscard]] const QString& id() const noexcept;
    [[nodiscard]] const QString& name() const noexcept;
    [[nodiscard]] const QString& unit() const noexcept;
    [[nodiscard]] const std::optional<QString>& description() const noexcept;
    [[nodiscard]] const QString& formula() const noexcept;
    [[nodiscard]] ExpressionOutputType outputType() const noexcept;

    /// Source signal IDs this expression depends on. Order does not
    /// matter for evaluation (substituted by name).
    [[nodiscard]] const std::vector<QString>& sourceSignalIds() const noexcept;

    /// Evaluate with provided source values map (signalId → SignalValue).
    /// Returns the result as a SignalValue matching `outputType()`.
    /// Throws on runtime error (div-by-zero, domain error) — caller catches.
    [[nodiscard]] signalforge::decoder::SignalValue evaluate(
        const std::vector<std::pair<QString, signalforge::decoder::SignalValue>>& sources) const;

    /// Build the SignalMetadata for the derived signal this expression
    /// produces. Used during registration with SignalBufferRegistry.
    [[nodiscard]] signalforge::decoder::SignalMetadata derivedSignalMetadata() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// A validated, topologically-sorted collection of expressions.
struct ExpressionSet {
    std::vector<Expression> expressions;  // Topologically sorted
    std::vector<QString> baseSignalIds;   // Union of all sources (for prefetch)
};

}  // namespace signalforge::expression
```

### 4.3 `ExpressionValidator` class

Place at `src/expression/expression_validator.hpp`.

```cpp
// src/expression/expression_validator.hpp
#pragma once

#include "expression/expression.hpp"

#include <QString>
#include <expected>
#include <vector>

namespace signalforge::expression {

struct ExpressionValidationError {
    QString filePath;
    int lineNumber = -1;          ///< -1 if not mappable
    QString expressionId;         ///< Empty if pre-expression error
    QString message;
};

using ExpressionValidationResult = std::expected<ExpressionSet, std::vector<ExpressionValidationError>>;

/// Loads, validates, and compiles an expression yaml file.
class ExpressionValidator {
public:
    /// Validate a yaml file. Returns validated ExpressionSet on success
    /// or a list of errors. Errors include syntax errors, type errors,
    /// cycle detection, and out-of-whitelist syntax.
    ///
    /// `availableSignals` is used to validate that source signal IDs
    /// exist in the registry. Pass the result of
    /// `SignalBufferRegistry::signalIds()` after all decoders have
    /// registered.
    [[nodiscard]] static ExpressionValidationResult validateFile(
        const QString& yamlPath,
        const std::vector<signalforge::decoder::SignalMetadata>& availableSignals);

    /// Validate yaml content directly (for testing).
    [[nodiscard]] static ExpressionValidationResult validateString(
        const QString& yamlContent,
        const QString& virtualPath,
        const std::vector<signalforge::decoder::SignalMetadata>& availableSignals);
};

}  // namespace signalforge::expression
```

**Validation steps in implementation order**:

1. yaml syntax (yaml-cpp parse) → ValidationError on syntax error with line
2. Top-level keys (`schema_version`, `expressions`) present → error if missing
3. For each expression entry:
   - Required fields (`id`, `formula`) present
   - `id` unique within set
   - `id` does not collide with existing base signal ID
   - `formula` is non-empty string
4. exprtk compilation:
   - Parse formula
   - Walk AST; each operator/function checked against whitelist
   - Variable references extracted as source signal IDs
5. Source signal validation:
   - Each source ID exists in `availableSignals`
   - No source signal has type `QString` (error: cannot evaluate)
6. Output type compatibility:
   - If `type: bool` declared, formula must be a comparison/logical operation (or a numeric that could meaningfully be 0/1)
   - If `type: int64` declared, formula must be numeric
7. Cycle detection:
   - Build directed graph (expression A → its source IDs)
   - For each expression, treat its `id` as a node
   - DFS-based cycle detection
   - On cycle: report path
8. Topological sort → store sorted ExpressionSet

### 4.4 yaml expression schema v1

Example at `examples/expressions/power_calculations.yaml`:

```yaml
schema_version: 1
description: "Power and efficiency derived signals"

expressions:
  - id: power_total
    name: "Total power"
    unit: W
    formula: voltage * current
    type: double
    description: "Instantaneous power = V × I"

  - id: power_efficiency
    name: "Efficiency"
    unit: ""  # ratio
    formula: power_total / power_input
    type: double
    description: "Power efficiency ratio"

  - id: over_temperature
    name: "Over-temperature alarm"
    unit: ""
    formula: temperature > 80.0
    type: bool
    description: "Set when temperature exceeds 80°C"
```

Frozen yaml keys (M7 close):
- Top-level: `schema_version`, `description` (optional), `expressions` (required)
- Per expression: `id`, `name`, `unit`, `formula`, `type` (default `double`), `description` (optional)

### 4.5 `ExpressionRegistrar` class

Place at `src/expression/expression_registrar.hpp`.

Lifecycle hook similar to M5's `DecoderRegistrar`:

- App startup: instantiate `ExpressionEngine` and `ExpressionRegistrar`
- After M5 decoders register their base signals (via `pipelineAttached` events on `PipelineManager`), `ExpressionRegistrar` is notified
- Loads yaml expression files (configured paths, hard-coded for V1; M9 will add UI selection)
- Validates each via `ExpressionValidator::validateFile(path, registry.signalIds())`
- On success: passes ExpressionSet to engine via `setExpressions`, calls `engine.start()`
- On validation failure: logs ERROR with full validation error list; engine not started

Multi-file expression sets are merged into a single ExpressionSet (cycle detection runs across all files).

### 4.6 `expr_lint` CLI tool

Place at `tools/expr_lint/`. Mirrors `tools/schema_lint/` from M5:

```
$ expr_lint examples/expressions/power_calculations.yaml --base-signals signals.json
OK: power_calculations.yaml (3 expressions, 0 cycles)

$ expr_lint bad.yaml --base-signals signals.json
FAIL: bad.yaml

bad.yaml:8 — expression 'efficiency' formula uses unknown operator 'if'
bad.yaml:15 — cycle detected: power_total -> efficiency -> derived_voltage -> power_total
```

`--base-signals signals.json` lets the user provide an export of the signal catalog (e.g., from a previous app run). Without it, the linter only does syntax + cycle checks (skipping source signal validation).

### 4.7 Lifecycle and tick semantics

**Engine state machine**:

```
Constructed → setExpressions() → Ready
Ready → start() → Running (tick fires every 33ms)
Running → stop() → Ready (registrations retained, tick stopped)
Running → destruction → Stopped + unregistered
```

**On each tick** (per spec §3.2 cost model):

```
1. timer fires; record tick start time
2. for each expression in topo order:
     2.1 collect source values: queryLatestOne for each source
     2.2 if any source is QString: evaluation error, increment counter, skip
     2.3 if any source has no value yet (pipeline just started): use NaN as placeholder, log warn once per signal
     2.4 evaluate exprtk
     2.5 if evaluation throws: increment expression_evaluation_errors counter, log warn once per second per expression
     2.6 push result via registry.onSignal(now, derivedId, value)
3. record tick duration; update stats
```

**No-source-data handling**: if a source signal has not been pushed yet (e.g., decoder just started), `queryLatestOne` returns `nullopt`. The expression's source value is treated as NaN; exprtk evaluates accordingly (typically producing NaN result). Result is still written to the derived buffer (so M8 Chart can display NaN as a gap).

### 4.8 Topological sort handling of forward references

If yaml has:
```yaml
expressions:
  - id: power
    formula: voltage * current  # base signals; no derived deps
  - id: efficiency
    formula: power / power_input  # depends on power
```

Then the validator's dependency graph has edge `efficiency → power`. Topo sort produces `[power, efficiency]` (power evaluated first). On each tick, `power` is computed and written to the registry; then `efficiency` reads `power` (which is now present) plus `power_input`.

If user reverses yaml order, validator still produces correct topo sort (yaml order is irrelevant after validation).

**Race condition consideration**: writes during a single tick happen in order. By the time `efficiency` runs, `power` write has completed. Single-threaded engine guarantees this.

---

## 5. Test strategy

### 5.1 Coverage ≥ 85% on expression modules

- `expression.cpp`: ≥ 85%
- `expression_engine.cpp`: ≥ 85%
- `expression_validator.cpp`: ≥ 90% (validator is error-heavy)

### 5.2 Unit tests

**For `Expression`**:

- Construct with valid params; getters return correct values
- `evaluate` with bool source returns correct double
- `evaluate` with int64 source returns correct double
- `evaluate` with double source returns identity
- `derivedSignalMetadata` produces correct SignalMetadata
- Move construction works (for ExpressionSet vector)
- Runtime evaluation errors throw (caller handles)

**For `ExpressionValidator`**:

Each expected error has a fixture in `tests/integration/fixtures/invalid_expressions/`:

- `missing_version.yaml` → schema_version error
- `missing_id.yaml` → required field error
- `duplicate_id.yaml` → uniqueness error
- `id_collides_with_base.yaml` → name collision error
- `unknown_operator.yaml` → exprtk syntax error (but no `if`)
- `restricted_syntax_if.yaml` → whitelist violation
- `restricted_syntax_while.yaml` → whitelist violation
- `unknown_source.yaml` → source signal not in available set
- `qstring_source.yaml` → source signal type rejected
- `cycle_simple.yaml` → A → B → A
- `cycle_self.yaml` → A → A
- `cycle_three.yaml` → A → B → C → A
- `bool_output_with_arithmetic.yaml` → output type mismatch warning

Each test:
- Validation fails
- Error message contains specific expression ID + line + actionable text

**For `ExpressionEngine`**:

- Construction with empty ExpressionSet: ready but no work to do
- `setExpressions` with one expression: count = 1
- `start` then `stop` cycles
- `stop` while running halts ticks
- Tick fires at correct cadence (mock QTimer or wait-for-N-ticks)
- Stats counters increment

### 5.3 Integration tests

Per spec §2.1-12, six integration tests:

- `test_expression_engine_basic.cpp`: full setup with M6 registry, register one base signal, register `out = base * 2`, push base values, verify out.
- `test_expression_engine_dependency_chain.cpp`: 3 expressions with chain; verify topological evaluation order.
- `test_expression_engine_cycle_rejection.cpp`: yaml with cycle; verify rejection at validator + engine never starts.
- `test_expression_engine_type_promotion.cpp`: bool source + int64 source + double source; verify each converts to double in formula.
- `test_expression_engine_qstring_rejection.cpp`: register a QString-typed base signal; expression referencing it; verify rejection.
- `test_expression_engine_restricted_syntax.cpp`: formulas with `if`/`while`/`?:` rejected.

### 5.4 Benchmarks

`tests/benchmark/bench_expression_engine.cpp`:

**Scenario 1: Tick latency at scale**
- Pre-load registry with 500 base signals (mixed bool/int64/double)
- Register 100 expressions, each averaging 5 source signals (some shared, total 500 sources)
- Run 1000 ticks; measure p50, p95, p99 tick wall time

**Target**:
- p50 < 5 ms
- p95 < 8 ms  
- p99 < 10 ms (HALT > 15 ms)

Results to `tests/benchmark/results/M7-baseline.md`.

### 5.5 ASan / TSan clean

- ASan: zero finding on all tests (CI debug-asan)
- TSan: no data race in the registry-shared scenario where engine writes derived signals while integration test reader queries them concurrently

---

## 6. Freeze protocol

### 6.1 What freezes at M7 close

**C++ interfaces**:
- `src/expression/expression.hpp`: `Expression` class, `ExpressionSet` struct, `ExpressionOutputType` enum
- `src/expression/expression_engine.hpp`: `ExpressionEngine` class, `ExpressionEngineConfig` struct, `TickStats` struct
- `src/expression/expression_validator.hpp`: `ExpressionValidator` class, `ExpressionValidationError` struct, `ExpressionValidationResult` typedef

**yaml expression schema v1**:
- Top-level keys: `schema_version`, `description`, `expressions`
- Per-expression keys: `id`, `name`, `unit`, `formula`, `type`, `description`

Once frozen, modifications require new ADR.

### 6.2 What does NOT freeze

- exprtk whitelist contents (additive: new functions can be added without ADR)
- 30 Hz tick interval (configurable via ExpressionEngineConfig)
- Topo sort algorithm (DFS vs Kahn — implementer's choice)
- Internal `Expression::Impl` PIMPL layout
- `expr_lint` CLI tool internals

### 6.3 Freeze record format

`.claude/M7-done.md`:

```markdown
## Freezes established in this milestone

Frozen per M7 spec §6.1.

| File | sha256 |
|---|---|
| `src/expression/expression.hpp` | <...> |
| `src/expression/expression_engine.hpp` | <...> |
| `src/expression/expression_validator.hpp` | <...> |
| `schemas/expression_schema_v1.yaml` | <...> |
| `schemas/expression_schema_v1.json` | <...> |

Modifications require new ADR per M7 §6.2.
```

---

## 7. M7-specific HALT triggers

Beyond CLAUDE.md §HALT:

1. Modification to M2/M3/M4/M5/M6 frozen `.hpp` → HALT
2. exprtk version conflict (M0's pinned version doesn't compile under C++23) → HALT, propose upgrade or fork via ADR
3. Tick wall time p99 > 15 ms (50% of budget) after one optimization pass → HALT
4. Cycle detection produces false positives or misses cycles in adversarial test cases → HALT (algorithm broken)
5. Expression evaluation result has > 1 ulp drift from a hand-computed reference → HALT (numerical correctness broken)
6. Memory leak in long-running engine (1 hr × 30 Hz = 108k ticks) discovered by ASan or LSan → HALT
7. UI thread blocks > 100 ms during engine start (large yaml load) → HALT

---

## 8. Acceptance criteria

### 8.1 Build and test

- [ ] Debug, Release, debug-asan all build clean under C++23
- [ ] All unit + integration tests pass under all three presets
- [ ] Coverage ≥ 85% per §5.1
- [ ] CI green on milestone/M7 head

### 8.2 Performance

- [ ] Tick p99 < 10 ms (HALT > 15 ms)
- [ ] No tick misses observed in 1-hour soak test
- [ ] Results in `tests/benchmark/results/M7-baseline.md`

### 8.3 Correctness

- [ ] All restricted-syntax fixtures rejected
- [ ] All cycle fixtures rejected with correct path
- [ ] Type promotion rules verified per §3.3
- [ ] QString source rejection verified
- [ ] Topological evaluation order verified

### 8.4 Concurrency safety

- [ ] ASan clean
- [ ] TSan clean (or local-only block documented)
- [ ] No data race between engine writes and concurrent reader queries

### 8.5 Freeze record

- [ ] M7-done.md has Freezes section per §6.3
- [ ] Sha256s recorded for 5 files (3 hpp + 2 schema files)
- [ ] No modifications to M2/M3/M4/M5/M6 frozen files

### 8.6 Hand-off

- [ ] M7-done.md hand-off section covers:
  - For M8 Chart UI: derived signals appear in registry; no special handling needed; consider UI grouping by virtualDriverId
  - For M10 Session Writer: derived signals are persisted; replay (M11) re-derives via expression engine, not from disk
  - For M12 Performance: tick budget profile, hot expressions for potential vectorization

---

## 9. Notes for CC

- **30 Hz tick is a hard cadence guarantee, not a best-effort**. If the QTimer fires later than 33 ms (system load), don't try to "catch up" with rapid ticks — log a tick-miss metric and proceed normally. Catch-up logic creates worse latency spikes.

- **exprtk integration via existing FetchContent**. M0 already pulled exprtk. M7 just `#include <exprtk.hpp>` from `signalforge_expression` lib. Verify exprtk compiles under C++23 in S1 preflight (similar to M5's std::expected gate).

- **Validator error messages are user-facing**. A typo in yaml is the most common user error. Every error message should say: which file, which line, which expression ID, what's wrong, what to do. Bad message: "syntax error". Good: "bad.yaml:12 — expression 'power' formula uses operator 'if' which is not in V1's whitelist (allowed: arithmetic, comparison, logical, math built-ins). Use a separate expression with a comparison instead."

- **Don't pre-allocate counter-mitigations for performance**. If tick p99 is 8 ms, don't spend time optimizing to 5 ms. Spec target is 10 ms. Save effort for M8 where perf is actually a hard gate.

- **Topo sort and cycle detection are well-known algorithms**. Don't reinvent. DFS-based detection (white/gray/black coloring) is fine; Kahn's algorithm works too. Either implementation is correct; pick the one with simpler code.

---

## 10. Closing note

M7 is the first milestone where derived data is computed from base data. Quality matters in two dimensions:

1. **Validation quality** (registration time): users author yaml expressions, and clear errors prevent frustration. The lint CLI is the user's first feedback loop; invest in its message quality.

2. **Numerical correctness** (runtime): derived values feed charts and (eventually) alarms. A subtle precision loss in bool / int64 / double promotion compounds into wrong alarm decisions. Test exhaustively at the type boundaries.

When in doubt about a design choice between expressiveness (exprtk full power) and predictability (V1 whitelist), choose predictability. V1.5+ can expand; V1 cannot retract.

Performance is a third-tier concern in M7: 30Hz × 100 expressions × 5 sources is well within modern CPU budgets. The bottleneck for derived-signal latency is the tick cadence (33 ms), not the evaluation cost. Don't over-engineer.
