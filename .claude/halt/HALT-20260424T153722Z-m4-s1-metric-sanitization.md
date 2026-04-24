# HALT — M4 S1: MetricsRegistry does not sanitize metric names

**UTC timestamp**: 2026-04-24T15:37:22Z
**Milestone**: M4
**Subtask**: S1 — pipeline scaffolding + FrameSink header (preflight gate)
**Trigger**: M4 spec §7.2 — "MetricsRegistry does not sanitize driver IDs with special characters, and spec §4.6 requires sanitization → HALT"
**Branch**: `milestone/M4` (head `c0f2847`)

## What happened

M4's first action per `.claude/M4-plan.md` §2 S1 was to verify that
`MetricsRegistry::getOrCreate()` sanitizes metric names containing
special characters from `driverId` (e.g., `/`, `:`, `.`, spaces in
values like `serial:/tmp/ttyV0` or `tcp:127.0.0.1:9000`).

### Evidence from `src/observability/metrics.{hpp,cpp}`

The registry is **permissive**, not **sanitizing**:

```cpp
// metrics.cpp lines 32–42
Metric* MetricsRegistry::getOrCreate(const QString& name, MetricKind kind) {
    const std::string key = name.toStdString();
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (auto it = impl_->metrics.find(key); it != impl_->metrics.end()) {
        return it->second.get();
    }
    auto metric = std::make_unique<Metric>(name, kind);
    Metric* raw = metric.get();
    impl_->metrics.emplace(key, std::move(metric));
    return raw;
}
```

The key is simply `name.toStdString()` — no filtering, no rewriting,
no rejection. Any UTF-8 string is accepted verbatim, including every
separator character an M4 pipeline `driverId` might carry.

The `MetricsRegistry` header explicitly notes that it is **not** part
of the M2 freeze surface (`metrics.hpp` line 69) — it is permitted to
evolve.

## Why this matters for M4

M4 spec §4.6 names five per-driver metrics whose names embed
`driverId`:

```
pipeline_frames_received_<driverId>
pipeline_frames_dropped_<driverId>
pipeline_ingress_watermark_<driverId>
pipeline_ingress_depth_peak_<driverId>
pipeline_errors_forwarded_<driverId>
```

With the Connection Manager's expected ID formats (plan §2 S6:
`serial:<device>`, `tcp:<host>:<port>`, `udp:<host>:<port>`,
`replay:<basename>`), these names will contain `:`, `/`, and `.`
verbatim. Downstream consumers in M5 (performance panel) and any
future text-based metric export (Prometheus-style naming rules use
`[a-zA-Z_:][a-zA-Z0-9_:]*`) would be affected.

The spec §4.6 closing sentence anticipated this divergence:

> Metric names with special characters in `driverId` (e.g., slashes in
> paths) are sanitized per existing MetricsRegistry rules (implementer
> verifies; if the registry doesn't sanitize, HALT and ask).

§7.2 reinforces: HALT on this exact condition — "Do not guess."

## Decision requested

Per plan §2 S1 and understanding §5 Rank 2, two options were
pre-drafted:

### Option A — sanitize in pipeline at registration time (recommended in understanding §5)

- Add a private helper `pipelineMetricName(const QString& driverId, const QString& base)` inside `src/pipeline/frame_pipeline.cpp`.
- Rewrite `[^A-Za-z0-9_]` → `_` on the `driverId` portion only.
- Example: `serial:/tmp/ttyV0` → `serial__tmp_ttyV0` → final metric name `pipeline_frames_received_serial__tmp_ttyV0`.
- MetricsRegistry stays unchanged.
- **Pros**: M4-local change, no impact on other subsystems, no ADR needed (MetricsRegistry is not frozen but we're not modifying it). Small, auditable.
- **Cons**: two pipelines with IDs that collide after sanitization (e.g., `a/b` and `a:b` both → `a_b`) produce duplicate metric keys. Mitigation: `PipelineManager::attach` already rejects duplicate raw driverIds; the sanitized collision is extremely unlikely in practice with the planned ID scheme (driver type prefix ensures namespace separation).
- **Scope**: stays inside M4 §2.1 deliverables; no spec deviation.

### Option B — add sanitization to `MetricsRegistry` (new lightweight ADR)

- Extend `getOrCreate(name, kind)` to call a private `sanitize(name)` helper before insertion.
- Rules: same `[^A-Za-z0-9_]` → `_` pattern.
- Document the behavior in `metrics.hpp`.
- **Pros**: single canonical place for metric name hygiene; future subsystems benefit automatically.
- **Cons**: expands M4 scope into observability; every existing M2 metric that uses `:` or `-` in its name (if any) would behave differently before/after. Requires a quick audit of existing metric usages. Since `MetricsRegistry` is *not* frozen this is **not** a freeze violation, but it does cross a module boundary that M4 otherwise avoids, and may surprise the M5 performance panel team.
- Needs a lightweight ADR recording the rename rule and the compatibility scan.

### Option C (not recommended) — no sanitization, accept raw names

- Plan proceeds as-is; metric names literally contain `:`, `/`, etc.
- Works with current MetricsRegistry (permissive).
- Risks downstream tooling friction in M5+.
- Requires spec §4.6 to be amended (strike the "are sanitized" sentence). Not an implementer decision.

## My recommendation

**Option A.** It is the narrowest change, keeps M4 self-contained, does
not require an ADR, and the collision risk is demonstrably low with
the planned ID scheme. If M5 later decides cross-subsystem sanitization
is preferable, Option A is cheap to delete (the helper migrates into
`MetricsRegistry` under Option B later).

Awaiting selection before resuming S1.

## State at HALT

- **Branch**: `milestone/M4` at `c0f2847` (planning commit).
- **Working tree**: clean (only `.claude/scheduled_tasks.lock` and
  `tools/crash_test/_deps/` untracked, pre-existing).
- **No code changes made**. Zero files modified in this subtask beyond
  this HALT report. No compilation attempted.
- **Tests**: unchanged from M3 (179 green).

## Next step after decision

- Resolve decision — user replies "Option A", "Option B", or an explicit
  alternative.
- Commit this HALT report + the chosen resolution trail (if any).
- Resume S1 from the point after the preflight gate.
