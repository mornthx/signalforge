# ADR-009 — MainWindow plumbs Connection ↔ PipelineManager

## Status

Accepted (V1.0 release blocker fix at M13 S7, 2026-05-09;
discovered during ADR-008 integration testing).

## Context

ADR-008 closed M5 §4.6's deferred wire-up: `ConnectionConfig::decoderSchemaId`
now flows through `ConnectionManager` to
`DecoderRegistrar::setSchemaForDriverType` so that when a
pipeline is attached, the registrar can pick the right
schema and construct a `SchemaDecoder`.

But integration testing of ADR-008 revealed a deeper V1.0 gap:

```
$ grep -rnE "pipelineManager_?->attach|PipelineManager.*\.attach\b" src/ tests/
src/pipeline/pipeline_manager.cpp:28:  # definition
tests/unit/pipeline/pipeline_test.cpp:466,493  # unit-test callers only
```

**Zero production code calls `PipelineManager::attach()`**. The
`Connection::connectDriver()` lifecycle reaches `Connected` cleanly,
but no `pipelineAttached` signal ever fires, so
`DecoderRegistrar::onPipelineAttached` never runs, so no
`SchemaDecoder` is ever constructed in production.

This is a multi-milestone governance gap: M9 implemented the
connection-state machine but did not wire connections into
`PipelineManager`. M10 (recording), M11 (replay), and M12
(performance) all used mocked drivers / replay-only fixtures
and never exercised the live-mode pipeline-attach path.
M13 §3.5 V hardware-verification Tests M9-1 (Serial) /
M9-2 (TCP) / M9-3 (UDP) caught the gap.

## Decision

`MainWindow` (the only place that owns both `PipelineManager` and
`ConnectionManager`) subscribes to
`ConnectionManager::connectionStateChanged` and, on the
appropriate state transitions, drives the
`PipelineManager::attach` / `detach` calls:

```cpp
// In MainWindow constructor, after pipelineManager_ +
// connectionManager_ are both constructed:
connect(connectionManager_.get(),
        &signalforge::connection::ConnectionManager::connectionStateChanged,
        this,
        [this](const QString& id,
               signalforge::connection::Connection::State state) {
            auto* conn = connectionManager_->connection(id);
            if (conn == nullptr || conn->driver() == nullptr) {
                return;
            }
            // driverId convention: `<type>:<connectionId>` —
            // matches DecoderRegistrar::driverTypeOf which splits
            // on ':' and uses the prefix as the registrar's
            // lookup key (so "udp:conn-3" → "udp" lookup).
            const QString driverId =
                driverTypeName(conn->config().driverType) +
                QStringLiteral(":") + id;

            if (state == signalforge::connection::Connection::State::Connected) {
                signalforge::pipeline::PipelineConfig cfg;
                cfg.driverId = driverId;
                (void)pipelineManager_->attach(conn->driver(), cfg);
            } else if (state == signalforge::connection::Connection::State::Idle ||
                       state == signalforge::connection::Connection::State::Error) {
                pipelineManager_->detach(driverId);
            }
        });
```

The `driverTypeName` is a small private helper in
`main_window.cpp` (file-scope; no header change) that maps
`DriverType` to the string keys the registrar expects
(`serial`, `tcp`, `udp`, `replay`). It is the same mapping as
the `driverTypeToYamlInternal` helper introduced in ADR-008's
`connection_manager.cpp` anonymous namespace.

This keeps **all changes in non-frozen files**:

- `src/app/main_window.{hpp,cpp}` — not frozen (per M11 §6.2 +
  precedent throughout V1; `MainWindow` is the V1 integration
  point, not a freeze surface).
- ADR-009 documents the wire-up.

## Rationale

Three alternatives were considered (Option α / β / γ in the
HALT report `.claude/halt/HALT-2026-05-09T17-50Z-pipeline-attach-gap.md`).

### Rejected: Option α — `Connection.cpp` takes `PipelineManager*`

Modify `Connection`'s constructor + `connectDriver()` to know
about `PipelineManager` directly.

**Rejected because**: requires modifying M9-frozen
`connection.hpp` (add `PipelineManager*` parameter to ctor)
and all `Connection` construction sites (production + tests).
Bigger blast radius than necessary; ADR-009 would also need
to authorize the M9 frozen-`.hpp` change.

### Rejected: Option β — `ConnectionManager` member `PipelineManager*`

Add `PipelineManager*` as a `ConnectionManager` ctor parameter
+ private member; do the attach/detach inside
`ConnectionManager::connectConnection / disconnectConnection`.

**Rejected because**: requires modifying M9-frozen
`connection_manager.hpp` (one new ctor parameter, one new
private member). Still a frozen-`.hpp` change. Also harder to
test in isolation — the `ConnectionManager` would now be
inseparable from a `PipelineManager`.

### Accepted: Option γ — `MainWindow` orchestrates

`MainWindow` owns both managers; subscribing to the existing
public `connectionStateChanged` signal + calling the existing
public `pipelineManager_->attach(connection->driver(), cfg)`
method involves only non-frozen files and existing public APIs.

**Accepted because**:
- No frozen `.hpp` modification (M9 freeze + everything else
  intact).
- No new ADR-required interface changes beyond ADR-009 itself.
- `Connection::driver()` is already public (verified at
  S7 implementation: `connection.hpp:149`).
- The plumbing concern (Connection ↔ PipelineManager) is an
  *application-layer* concern, which `MainWindow` is the
  natural home for.
- Smallest possible change for the V1.0 ship.

## Known limitations (V1.0 ship-as-is)

- **Reconnect after Error**: when a connection transitions
  `Error → Idle → Connecting → Connected`, the lambda re-attaches.
  If the same `driverId` already has a pipeline in `PipelineManager`,
  the second attach is refused (per
  `pipeline_manager.cpp:52` "driverId already attached"). The
  lambda explicitly detaches on `Error` / `Idle` first, but
  there's a small window during state-transition where the
  cleanup races with re-attach. V1.0 acceptable: the typical
  user-driven cycle is Disconnect (Idle, detach) → Connect
  (Connected, attach), with explicit user actions between.

- **Tests that use `ConnectionManager` without `MainWindow`** (e.g.,
  `tests/integration/test_connection_lifecycle_full_stack.cpp`)
  do not exercise this lambda; they must call
  `pipelineManager.attach()` directly to test pipeline-attach
  scenarios. The new `tests/integration/test_v1_live_mode_pipeline.cpp`
  follows this pattern: it constructs the same
  `connectionStateChanged → attach/detach` lambda inside the
  test fixture so the wire-up is exercised end-to-end without
  requiring the real `MainWindow`.

These edge cases are acceptable for V1.0 because they don't
affect the typical workflow exercised by the M13 18-test
hardware verification.

## Consequences

- **`src/app/main_window.cpp` gains the
  `connectionStateChanged → attach/detach` lambda.** No
  `main_window.hpp` change.
- **No frozen `.hpp` modification beyond ADR-008's
  `decoder_registrar.hpp`.** M2-M11 + M9 freeze surface
  remains intact.
- **V1.0 live mode functional**. The full chain
  `Connection.connectDriver → MainWindow lambda →
  PipelineManager.attach → pipelineAttached signal →
  DecoderRegistrar.onPipelineAttached → SchemaDecoder
  construction → frame decode → SignalValueSink → chart`
  finally works end-to-end.
- **The `tests/integration/test_v1_live_mode_pipeline.cpp`
  integration test (ADR-008 deliverable)** is updated to
  include the same lambda in its fixture, so the test
  exercises the full V1.0 production path including the
  ADR-009 plumbing. With both ADRs in place, the test's
  `decoderCount() >= 1` assertion passes.
- **V1.0 governance lessons** documented in M13-done.md
  §V1 Governance lessons: M9 deferred two wire-ups (schema
  + pipeline-attach); both escaped M10/M11/M12 because tests
  used mocked components; M13 §3.5 V hardware verification
  caught both, validating the blocking-release-prerequisite
  design pattern.

## Cross-references

- ADR-008 (sister fix; closes M5 §4.6 deferred schema wire-up)
- M9 spec §4 (`Connection`/`ConnectionManager` lifecycle)
- M9-done.md (gap admission)
- M13 18-test verification §M9 Tests 1-3 (gate that caught both bugs)
- M13 spec §3.5 V (release prerequisite blocking)
- `.claude/halt/HALT-2026-05-09T17-50Z-pipeline-attach-gap.md`
  (HALT report that surfaced this finding)
- `docs/v1.0-spec-list.md` §1 (sha256 for `decoder_registrar.hpp`
  updated by ADR-008; no other freeze surface changes for ADR-009)
- `docs/release-notes/v1.0.0.md` "Known limitations" (reconnect
  edge case noted)
