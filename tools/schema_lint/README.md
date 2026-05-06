# schema_lint

Standalone CLI for validating SignalForge decoder schema yaml files
against schema v1.

Build it as part of the main project:

```
cmake --preset=release
cmake --build --preset=release --target schema_lint
```

The binary lands at `build/release/tools/schema_lint/schema_lint`.

## Usage

```
schema_lint <file.yaml> [--json]
schema_lint --help
```

Exit codes:

| Code | Meaning |
|------|---------|
| 0    | The file is a valid schema v1 yaml. |
| 1    | The file is invalid; one or more errors printed. |
| 2    | Usage error (missing path, unknown flag, unreadable file). |

## Examples

### Valid schema

```
$ schema_lint examples/schemas/temperature_sensor.yaml
OK: examples/schemas/temperature_sensor.yaml (1 layout, 6 fields)
```

### Invalid schema (missing version)

```
$ schema_lint tests/integration/fixtures/invalid_schemas/missing_version.yaml
FAIL: tests/integration/fixtures/invalid_schemas/missing_version.yaml
  /abs/path/to/missing_version.yaml:2 — schema_version: 'schema_version' is required (must be 1)
```

### Invalid schema (bit-field overlap)

```
$ schema_lint tests/integration/fixtures/invalid_schemas/bit_overlap.yaml
FAIL: tests/integration/fixtures/invalid_schemas/bit_overlap.yaml
  /abs/path/to/bit_overlap.yaml:11 — layouts[0].fields[0].bit_fields: bit ranges overlap at bits [0, 2) and [1, 3)
```

### Multi-byte field with no endianness

```
$ schema_lint tests/integration/fixtures/invalid_schemas/missing_endianness.yaml
FAIL: tests/integration/fixtures/invalid_schemas/missing_endianness.yaml
  /abs/path/to/missing_endianness.yaml:6 — layouts[0].endianness: 'endianness' is required at layout level (must be 'little' or 'big')
  /abs/path/to/missing_endianness.yaml:12 — layouts[0].fields[0].endianness: multi-byte field 'pressure' requires endianness (set field-level or layout-level)
```

### JSON output (--json)

The `--json` flag emits machine-readable output suitable for piping
into editor tooling, CI checks, or downstream scripts:

```
$ schema_lint examples/schemas/temperature_sensor.yaml --json
{"errors":[],"fields":6,"layouts":1,"path":"examples/schemas/temperature_sensor.yaml","valid":true}

$ schema_lint tests/integration/fixtures/invalid_schemas/missing_version.yaml --json
{"errors":[{"field":"schema_version","line":2,"message":"'schema_version' is required (must be 1)","path":"/abs/.../missing_version.yaml"}],"path":"tests/integration/fixtures/invalid_schemas/missing_version.yaml","valid":false}
```

## When to use it

- **While authoring a schema**: run `schema_lint` after each edit
  to catch mistakes before opening the SignalForge app.
- **In CI for user projects**: run as part of a pre-commit hook or
  CI step to ensure all in-repo yaml schemas validate.
- **In the main app**: the same `SchemaValidator` produces identical
  errors at runtime, so what `schema_lint` accepts the app accepts,
  and what `schema_lint` rejects the app rejects with the same
  message.
