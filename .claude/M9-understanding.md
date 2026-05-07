# M9 — Understanding

## 1. Restatement of the M9 goal

M9 delivers the **full Connection Manager UI** plus persistence
of user-configured connections (Serial / TCP / UDP / Replay).
M3 left a preview `ConnectionManager` dialog that builds one
driver at a time without persistence; M9 replaces it with a
production-grade subsystem.

This is the **first daily-use complete loop** of V1 (spec §1):
after M9, the user can launch the app, configure a connection,
connect to a device, see decoded signals chart in real time
(M5 → M6 → M7 → M8), disconnect, quit, and on next start the
connection list is preserved (manual reconnect).

Hard-stop types (concurrent, per spec):

1. **Interface freeze**: `ConnectionManager`, `Connection`,
   `ConnectionConfig`, `Connection::State` enum, `DriverType`
   enum, `AutoConnectCommand`, `DriverConfig` variant, plus the
   four per-driver config structs.
2. **yaml connection schema v1 freeze**: top-level keys +
   per-connection / per-driverConfig / per-command keys.
3. **Hardware loop verification**: Serial / TCP / UDP / Replay
   all reconnect cleanly across an app restart, documented in
   `docs/m9-hardware-verification.md`.

**Soft-HALT is not allowed** (inherits M2-M8 stance).

Quality philosophy carried forward from M8: **fail visibly +
document edge cases**. Connection failures (port not found, host
unreachable, file permission denied) must produce clear,
actionable error messages (spec §10: "Cannot connect" is bad,
"TCP connection to 192.168.1.100:9090 timed out after 5 seconds.
Verify host is reachable." is good). Missing or invalid yaml
must degrade gracefully — empty connection list is better than
crash.

## 2. Observed repo state

```
$ git log --oneline origin/main -5
862bb73 Merge pull request #14 from mornthx/docs/m9-spec
e85ecfc docs: add M9 connection manager spec
53d6c54 Merge pull request #13 from mornthx/milestone/M8
101dd52 chore: record M8 PR number in done.md
6f3aaba chore: M8 completion report (S11)
```

Phase 3 actions completed this session (report confirmed):

- PR #13 (M8 chart UI) merged at `53d6c54`.
- Tag `v0.0.9-alpha.1` pushed, pointing at the M8 merge.
- `milestone/M9` created from `53d6c54`, then fast-forwarded to
  `862bb73` (which added the M9 spec).

`docs/milestones/M9-connection-manager.md` (854 lines) is on
`origin/main` via PR #14.

`src/connection/` does not yet exist; M9 creates it.

**Existing M3 surface relevant to M9**:

- `src/drivers/driver_configs.hpp` — already defines
  `signalforge::drivers::SerialConfig` / `TcpConfig` /
  `UdpConfig` / `ReplayConfig` as value types. Driver
  constructors (`SerialDriver(SerialConfig)`, etc.) take these
  by value. **M9 will reuse these structs as the variant
  payloads in `ConnectionConfig::driverConfig`** rather than
  duplicating shapes — see §11 anticipated deviation.
- `src/drivers/driver_interface.hpp` — frozen at M3 close;
  `DriverInterface` abstract API unchanged in M9.
- `src/drivers/replay_driver.{hpp,cpp}` — M3 left a partial
  `ReplayDriver` (the M9 spec §4.8 / §2.1 explicitly tasks M9
  with completing it). Already takes `ReplayConfig`; M9 fills
  in playback / loop / pause-resume implementation.
- `src/app/connection_manager.{hpp,cpp}` — the M3 **preview**
  dialog (`QDialog` subclass, single-driver, non-persistent).
  M9 replaces it with the new
  `signalforge::connection::ConnectionManager` + dialog. The
  M3 preview is removed once MainWindow is rewired (S9).

**Existing M5/M6/M7/M8 surface relevant to M9**:

- `DecoderRegistrar` (M5) — `availableSchemas()` for the
  `decoderSchemaId` combo in `ConnectionDialog`.
- `SignalBufferRegistry` (M6) — Connection lifecycle drives
  signal flow into this; M9 doesn't directly modify the
  registry.
- `ChartManager` / `SignalSelector` (M8) — already wired in
  MainWindow (S8). Connection status bar widget extends M8's
  status bar.

## 3. Scope reminder

### Must deliver (spec §2.1)

- `ConnectionManager` (`src/connection/connection_manager.{hpp,cpp}`)
  replacing M3's preview dialog. Owns N Connections, lifecycle
  coordination, persistence.
- `Connection` (`src/connection/connection.{hpp,cpp}`) wrapping
  one driver + config + state machine (Idle / Connecting /
  Connected / Disconnecting / Error).
- `ConnectionConfig` struct (frozen): id, displayName,
  driverType, driverConfig variant, decoderSchemaId,
  autoConnectOnStartup (V1 reads as false; forward-compat),
  autoConnectCommands.
- `ConnectionDialog` (modal `QDialog`) — driver-type combo →
  `QStackedWidget` with per-driver-type config widget + common
  fields (display name, decoder schema, auto-connect toggle,
  auto-connect commands list).
- `ConnectionListWidget` — list view of all connections with
  per-row status (Idle / Connected / Error) + Connect /
  Disconnect / Edit / Remove / Add buttons.
- `ConnectionStatusWidget` — extends M8 status bar with
  "X/N connected" + "errors" indicator; click → opens manager
  dialog.
- `AutoConnectCommandSequence` — sequential post-connect
  commands (raw bytes + optional expected response + timeout +
  delayBefore); failures log WARN/ERROR but don't disconnect.
- yaml connection schema v1 (`schemas/connections_schema_v1.yaml`
  + `…_v1.json`).
- Persistence: `~/.config/signalforge/connections.yaml` (or
  platform equivalent); auto-save on every change; load on
  startup (graceful degradation on missing/invalid).
- MainWindow integration: "Connections" menu, toolbar button,
  ConnectionListWidget panel, ConnectionStatusWidget in status
  bar, replace M3 preview manager.
- Driver-specific config validation (per-type rules).
- Reconnect-on-startup behavior (manual; banner displays
  "X connections loaded").
- Connection lifecycle Qt signals.
- 7 integration tests at `tests/integration/`.
- Unit tests ≥ 80% coverage on connection modules.
- Manual hardware verification protocol at
  `docs/m9-hardware-verification.md`.
- Doxygen on all public declarations.
- `.claude/M9-done.md` + freeze record.

### Must NOT do (spec §2.2)

- No modification to M2-M8 frozen `.hpp`.
- No always-auto-reconnect mode (decision M9.2; manual).
- No multi-modal config dialog with tabs (decision M9.3;
  dynamic `QStackedWidget`).
- No connection sharing / export to other users.
- No connection groups / folders (V1 flat list).
- No remote connection management.
- No advanced auto-connect command DSL (sequential only).
- No new top-level dependencies.
- No driver-internal changes (M3's `DriverInterface` etc.
  remain frozen).
- No connection editor for the underlying decoder schema.
- No connection status remoting (no syslog / Prometheus /
  email).

## 4. Locked design decisions (spec §3)

1. **M9.1 (Modal dialog)** — `ConnectionDialog` is a modal
   `QDialog`; user must complete or cancel before continuing.
2. **M9.2 (Manual reconnect on startup)** — yaml loads
   connections in Idle state; user clicks Connect (or "Connect
   All"). `ConnectionConfig::autoConnectOnStartup` exists for
   forward compatibility but V1 reconnect logic ignores it.
3. **M9.3 (Dynamic driver-specific widget)** — single
   `QStackedWidget` with one page per driver type; combo
   selection swaps current index.
4. **M9.4 (Status bar)** — `ConnectionStatusWidget` extends M8
   status bar (already has FPS / dropped / throttled labels).
5. **M9.5 (yaml auto-connect commands)** — sequential commands
   (raw bytes + optional expected response + timeout +
   delayBefore); errors log + continue, don't disconnect.
6. **M9.6 (No soft-HALT)** — inherits M2-M8.
7. **M9.7 (Metric naming)** — `connection_*` prefix per spec
   §3.7.

## 5. Performance gates (spec §5)

M9 is a UI / persistence milestone; performance is not the
primary concern. Verified by integration tests:

- Connection list yaml load < 100 ms for 100 connections.
- Connection state UI update within 50 ms of state change.
- No memory growth in idle state (100 connections × 10 min
  test).
- Auto-connect commands sent within 100 ms of Connected state
  (excluding `delayBefore`).

These are not bench-gated; integration tests verify.

## 6. Freeze surface (spec §6.1)

- `src/connection/connection_manager.hpp`: `ConnectionManager`
  class.
- `src/connection/connection.hpp`: `Connection` class +
  `Connection::State` enum + `ConnectionConfig` +
  `DriverType` enum + `AutoConnectCommand` + `DriverConfig`
  variant.
- `schemas/connections_schema_v1.yaml`: canonical example.
- `schemas/connections_schema_v1.json`: documentation form.

Modifications require a new ADR.

## 7. M9-specific HALT triggers (spec §7)

1. Modification to M2-M8 frozen `.hpp` → HALT.
2. M3 driver interface needs non-additive change to support
   per-driver configuration → HALT (consider ADR for M3
   amendment).
3. yaml schema breaking change required mid-implementation
   → HALT (re-evaluate decision M9.2).
4. Connection dialog can't validate driver config for some
   driver type (e.g., serial port enumeration hangs) → HALT,
   propose mitigation.
5. Persistence yaml round-trip not bit-identical when saving/
   loading without changes → HALT (yaml-cpp formatting issue).
6. Qt SerialPort not available on target platform (extremely
   unlikely on Ubuntu 24.04) → HALT, document hardware
   limitation.
7. Auto-connect command cannot reliably wait for response
   (timing flakiness) → HALT, simplify or re-design.

CLAUDE.md §HALT triggers (compile error after 3 fixes, test
fail after 3 fixes, etc.) apply at every subtask.

## 8. Risk register

| Rank | Risk | Mitigation |
|---|---|---|
| 1 | Spec §4.2 *Config structs vs M3's existing structs (different namespaces, slightly different shape) | Reuse M3's `signalforge::drivers::*Config` directly in `ConnectionConfig::driverConfig` variant. Documented as M9-concerns.md C1 (anticipated). Avoids HALT trigger #2. |
| 2 | yaml-cpp binary round-trip for AutoConnectCommand `payload` (bytes with NUL / trailing \n must survive) | Spec §9 explicitly notes this; integration test will round-trip a known-tricky payload (NUL + CR/LF). |
| 3 | ReplayDriver completion intersects with M11 (Replay UX) | Per spec §2.2-9 + §4.8: M9 delivers the driver only; M11 adds the timeline scrubber + playback controls. Stay strictly in driver scope. |
| 4 | Manual hardware verification on CI (no real hardware) | Per spec §8.3 + §9 note: documented protocol in `docs/m9-hardware-verification.md`; manual run before close; results in M9-done.md. CI cannot replace this gate. |
| 5 | Auto-connect command timing flakiness (HALT trigger #7) | Use generous default timeouts (1000 ms); document the expected-response feature as best-effort (timeout + log WARN, not failure). |
| 6 | Multi-driver state coherence (e.g., two TCP connections fighting over port) | OS-level conflict; let driver `open()` return ConfigInvalid / ResourceBusy; surface error to user. |
| 7 | yaml `autoConnectOnStartup: true` written by V1 user but ignored by V1 reconnect logic | Forward-compat: schema accepts it; behavior deferred to V1.5+ per decision M9.2. Document in dialog tooltip. |

## 9. Dependencies

Existing in-tree:
- `signalforge_drivers` (M3, frozen) — `DriverInterface` +
  `SerialDriver` / `TcpDriver` / `UdpDriver` / `ReplayDriver`
  (skeleton; M9 completes it). Reuses `driver_configs.hpp`
  structs.
- `signalforge_decoder` (M5, frozen) — `DecoderRegistrar`'s
  `availableSchemas()` for the dialog combo.
- `signalforge_pipeline` (M4, frozen) — `FramePipeline` /
  `PipelineManager` for driver-to-decoder fan-out.
- `signalforge_buffer` (M6, frozen + ADR-005 chunked storage).
- `signalforge_chart` (M8, frozen) — `ChartManager` for the
  MainWindow integration.
- `signalforge_observability` (M2, frozen) — metrics naming
  per spec §3.7.

External (unchanged):
- Qt 6.10.2 modules: Core, Widgets, SerialPort, Network — all
  present.
- yaml-cpp — already pinned via FetchContent for M5/M7/M8.

No new top-level dependencies (spec §2.2-8).

## 10. Test strategy

- **Unit tests** at `tests/unit/connection/` (≥ 80% coverage):
  - Connection state machine transitions.
  - ConnectionConfig variant + per-driver-type config
    construction.
  - AutoConnectCommand payload encode/decode round-trip.
  - ConnectionManager add/edit/remove + activeChartId-style
    indexing.
  - yaml save/load round-trip + missing-file/invalid-yaml
    error paths.
- **Integration tests** at `tests/integration/` (7 files per
  spec §2.1-14):
  - `test_connection_manager_lifecycle.cpp`
  - `test_connection_persistence.cpp`
  - `test_connection_dialog_validation.cpp`
  - `test_auto_connect_commands.cpp`
  - `test_connection_status_widget.cpp`
  - `test_replay_driver_full_cycle.cpp`
  - `test_serial_loopback.cpp` (mock or skipped on CI; manual
    hardware path)
- **Manual hardware verification** at
  `docs/m9-hardware-verification.md` — protocol authored
  alongside implementation. Covers the 4 driver types against
  real hardware (or mocks where unavoidable). Run before M9
  close; results documented in M9-done.md.

## 11. Anticipated deviations / inherited concerns

### Inherited from M8: 1-hour soak follow-up (per Phase 3
continuation prompt)

> Pending from M8: 1-hour soak verification (60 charts × 1 kHz ×
> 30 Hz × 1 hour) per M8 spec §5.6 + plan §S11.
> To be executed during M9 implementation at a convenient point
> (suggested: after M9 S3 or before M9 closure).
> Acceptance: Vmrss growth < 10%, dropped frames < 50,
> ASan/LSan clean.
> If soak fails, file HALT and assess M8 hotfix vs
> M9-introduced regression.

The soak does not block M9 progression but **must be completed
before M9 closure**. The plan schedules the soak as subtask
S11s (between S11 and S12 / done.md).

The soak harness was started but uncommitted at the M8 → M9
transition; the discarded skeleton (bench_chart `--soak`/
`--memory-snapshot` flags) is rebuilt as part of S11s.

### Anticipated C1: M9 spec §4.2 *Config structs vs M3 existing structs

**Spec text** (§4.2) defines new `SerialConfig` / `TcpConfig` /
`UdpConfig` / `ReplayConfig` in the `signalforge::connection`
namespace.

**Reality**: M3 already defines these structs in
`signalforge::drivers` (`src/drivers/driver_configs.hpp`).
Driver constructors (`SerialDriver(SerialConfig)`, etc.) take
these existing types by value.

**Resolution**: M9's `ConnectionConfig::driverConfig` variant
uses the existing `signalforge::drivers::*Config` types
directly. No new struct definitions; no M3 driver-constructor
changes (HALT trigger #2 not fired); no namespace duplication.

The spec's *Config struct field names (e.g., `portName` vs M3's
`device`, `parity` as `int` vs M3's `QString`) are reconciled
by adopting the M3 types as the canonical V1 form. The spec is
the design intent; M3 is the implementation truth — and M3's
shape is already in production from M3 close.

Documented as `M9-concerns.md` C1 once implementation begins
(S1).

### Anticipated C2: M3 preview ConnectionManager removal

`src/app/connection_manager.{hpp,cpp}` is the M3 preview dialog.
S9 (MainWindow integration) removes it from the build and
deletes the source files. This is **not** a frozen-`.hpp`
change — `connection_manager.hpp` lives at `src/app/`, not in
the M3 freeze list. Per CLAUDE.md §Forbidden #2 the freeze list
is the M3-specific freeze record (driver_interface.hpp etc.),
not every file M3 created.

The replacement `signalforge::connection::ConnectionManager` is
at `src/connection/connection_manager.hpp`. Different namespace,
different file path, distinct class — no symbol collision.

## 12. Plan link

`.claude/M9-plan.md` lays out the 12-subtask plan with effort
estimates totaling ~52 h (within spec's 5-7 person-day estimate
of 40-56 h). Subtasks ordered to land an integrable slice early
(S1: scaffold + freeze headers + Connection skeleton) so later
subtasks can build on a moving foundation. The M8 1-hour soak
runs as S11s after S5 (typical "convenient point" suggested in
the Phase 3 continuation prompt).
