# expr_lint

Standalone CLI for validating SignalForge expression yaml files
against schema v1 (M7).

Build it as part of the main project:

```
cmake --preset=release
cmake --build --preset=release --target expr_lint
```

The binary lands at `build/release/tools/expr_lint/expr_lint`.

## Usage

```
expr_lint <file.yaml> [--base-signals <signals.json>] [--json]
expr_lint --help
```

Without `--base-signals`, only **syntax + cycle checks** run
(yaml parse, top-level shape, per-expression shape, exprtk compile,
forbidden-function check, cycle detection, topological sort).
Source-id existence and source-type checks are skipped.

With `--base-signals <signals.json>`, **full** validation runs,
including source-id existence and source-type checks against the
provided catalog.

Exit codes:

| Code | Meaning |
|------|---------|
| 0    | The file is a valid expression schema v1 yaml. |
| 1    | The file is invalid; one or more errors printed. |
| 2    | Usage error (missing path, unknown flag, unreadable file, bad signals.json). |

## `--base-signals` JSON format

```json
{
  "signals": [
    {"id": "voltage", "name": "Voltage", "unit": "V", "type": "double"},
    {"id": "current", "name": "Current", "unit": "A", "type": "double"},
    {"id": "power_input", "name": "Configured input", "unit": "W", "type": "double"}
  ]
}
```

Required per entry: `id`, `type`. Optional: `name`, `unit`.

Recognised type names: `double`, `bool`, `int64`, `qstring`
(also accepted: `QString`, `string`).

## Examples

### Valid expression file (syntax-only)

```
$ expr_lint examples/expressions/power_calculations.yaml
OK: examples/expressions/power_calculations.yaml (3 expressions, 3 source signals)
```

### Valid with a catalog (full validation)

```
$ expr_lint examples/expressions/power_calculations.yaml --base-signals signals.json
OK: examples/expressions/power_calculations.yaml (3 expressions, 3 source signals)
```

### Cycle detection

```
$ expr_lint tests/integration/fixtures/invalid_expressions/cycle_simple.yaml
FAIL: tests/integration/fixtures/invalid_expressions/cycle_simple.yaml
  /abs/path/cycle_simple.yaml:-1 — <top>: Expression set validation failed: cycle detected: beta -> alpha -> beta
```

### Missing source signal (full validation)

```
$ expr_lint examples/expressions/power_calculations.yaml --base-signals signals_missing_input.json
FAIL: examples/expressions/power_calculations.yaml
  /abs/path/power_calculations.yaml:21 — power_efficiency: expression 'power_efficiency' references unknown source signal 'power_input' (not in registry's available-signals list)
```

### JSON output (`--json`)

```
$ expr_lint examples/expressions/power_calculations.yaml --json
{"errors":[],"expressions":3,"path":"...","sources":3,"valid":true}

$ expr_lint tests/integration/fixtures/invalid_expressions/cycle_simple.yaml --json
{"errors":[{"expression":"","line":-1,"message":"...","path":"..."}],"path":"...","valid":false}
```

## When to use it

- **While authoring expressions**: run `expr_lint` after each edit
  to catch yaml typos, formula errors, and cycles before launching
  the SignalForge app.
- **In CI for user projects**: pre-commit / CI hook for in-repo
  expression yaml files. Without `--base-signals` you still catch
  yaml errors, exprtk compile errors, and cycles — the bulk of
  authoring mistakes.
- **In the main app**: the same `ExpressionValidator` produces
  identical errors at runtime, so what `expr_lint` accepts the app
  accepts, and what `expr_lint` rejects the app rejects with the
  same message.
