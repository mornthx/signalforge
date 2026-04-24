# ADR-003 — Metric Name Validation Policy

**Status**: Accepted
**Date**: 2026-04-24
**Context**: M4 S1 HALT — M4 spec §4.6 requires per-driver metric names that embed `driverId`; `MetricsRegistry` had no validation or sanitization rule.

## Decision

`MetricsRegistry` adopts a **permissive blacklist** as its metric-name validation policy. A name is accepted iff it is non-empty and contains none of:

- whitespace (`QChar::isSpace`) or control characters (`QChar::Other_Control`)
- `"`, `'`, `\`, `<`, `>`

All other UTF-8 characters are accepted, including `:`, `/`, `.`, `-`, `@`, `[`, `]`, brackets, and Unicode letters/digits.

The validator is exposed as a free function `signalforge::observability::isValidMetricName(const QString&)` and enforced by `MetricsRegistry::getOrCreate` (invalid names → logged at ERROR + `nullptr` return; callers must null-check before invoking `add()` / `set()`).

## Alternatives considered

### Prometheus-style `[a-zA-Z_][a-zA-Z0-9_]*` (rejected)

The Prometheus exposition format requires metric names to match `[a-zA-Z_:][a-zA-Z0-9_:]*`. Community tooling often tightens this further to `[a-zA-Z_][a-zA-Z0-9_]*`.

Rejected as **overly restrictive** for SignalForge's use case. `MetricsRegistry` is internal: metric names appear only in the snapshot API (in-process consumers), log output (arbitrary strings are fine), and future M5 UI display (HTML-escaped). They are never used as identifiers in a query language. Forcing Prometheus compatibility preemptively discards useful information (e.g., `tcp:127.0.0.1:9000` as a single token) to match a format the project does not export in V1.

### Sanitization (rejected)

Rewriting invalid characters (e.g., `[/:. ]` → `_`) at registration time was considered. Rejected because:

- Information loss: `serial:/tmp/ttyV0` and `serial_tmp_ttyV0` become indistinguishable. Two drivers with IDs that differ only in separator choice would produce colliding metric keys.
- Silent behavior: a caller computing a name via string-concat would see a different name flowing through the system, complicating debugging.
- Double work: each caller would need to know about (and avoid) the same forbidden-char set as the registry's sanitizer; simpler to forbid the small set up front.

### No validation (rejected)

Accepting anything was the status quo at M2 close. Rejected because downstream tooling (log parsers, any future structured export, HTML display in M8) cannot safely consume arbitrary strings. Whitespace in particular breaks log tailing / grep workflows that are standard in SignalForge operations.

## Rationale for the specific rejection list

- **whitespace / control chars**: break log parsing and CLI tooling (`grep`, `awk`, space-delimited log formats).
- **quotes (`"`, `'`) and backslash**: break JSON / YAML serialization if the metric snapshot is ever exported (M5 performance panel may ship JSON; future observability integrations likely).
- **angle brackets (`<`, `>`)**: break HTML display in the future M8 UI unless explicitly escaped. Rejecting at registration is cheaper than escaping at every render site.

All other characters — including every driver-ID separator from the M4 plan (`:`, `/`, `.`, `-`) — are permitted. This preserves the full driverId in metric names for debugging and removes the need for a sanitization helper in `FramePipeline`.

## Consequences

### Positive

- M4 `FramePipeline` can use driver IDs verbatim in metric names. No `sanitizeForMetric` helper required.
- Full driver ID survives into logs, snapshots, and debug tooling — no lossy translation.
- The validation rule is small, auditable, and easily extended if a new forbidden character surfaces in downstream tooling.

### Negative / carry-forward

- If SignalForge later adds a Prometheus export, a dedicated sanitizer will be needed at the export boundary. This is a second-order problem deferred to that export milestone — not an M4 cost.
- Clients that construct metric names programmatically and fail validation get `nullptr` rather than an exception; forgetting the null-check produces a crash on `add()` / `set()`. Mitigated by the ERROR log at registration and by the rarity of invalid names under the permissive rule (they arise only from direct user input).

### Carry-forward docs updates

Metric-naming guidance in architecture §14 will be folded in at the next architecture revision pass (not blocking this ADR, not an M4 deliverable).

## Implementation notes

- `src/observability/metrics.hpp` declares `isValidMetricName` and documents `getOrCreate`'s `nullptr`-on-invalid contract.
- `src/observability/metrics.cpp` implements the check; `MetricsRegistry::getOrCreate` logs ERROR once and returns `nullptr` for invalid inputs.
- Unit tests in `tests/unit/observability/metrics_test.cpp` cover: the valid-name table, the rejected-name table, and the `getOrCreate` null-on-invalid contract.

## Cross-references

- M4 spec §4.6 — per-driver metric names
- M4 spec §7.2 — HALT trigger that escalated this decision
- `.claude/halt/HALT-20260424T153722Z-m4-s1-metric-sanitization.md` — the raised HALT that prompted this ADR
- M4 plan §2 S1 — preflight gate
