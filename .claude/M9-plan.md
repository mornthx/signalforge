# M9 — Plan

## 0. Execution ground rules

- Branch: `milestone/M9` (created in Phase 3 from `53d6c54`,
  fast-forwarded to `862bb73` after M9 spec landed; pushed to
  origin).
- Per-subtask discipline (CLAUDE.md §Required #2 + §Git
  operation protocol), identical to M5/M6/M7/M8:
  1. Append start entry to `.claude/M9-progress.md`.
  2. Implement per plan.
  3. Build all three presets clean (Debug, Release, debug-asan).
  4. `ctest` Debug + Release clean (debug-asan host-blocked
     per `host_asan_preload`; CI is the authoritative gate).
  5. `clang-format --dry-run -Werror` on changed files.
  6. Append close entry to progress.md with counts +
     deviations.
  7. Commit with `<module>: <imperative verb> <object>`; body
     states "Freeze scope: no M2-M8-frozen .hpp modified."
  8. Push `milestone/M9`.
  9. Watch CI via `gh run watch`; report result before starting
     the next subtask. No silent retries.
- No new top-level dependencies (spec §2.2-8). Qt SerialPort,
  Qt Network, yaml-cpp all already in tree.
- Reuse M3's `signalforge::drivers::*Config` types directly in
  `ConnectionConfig::driverConfig` variant; do not duplicate
  the structs in `signalforge::connection`. Anticipated as
  M9-concerns.md C1 (per §11 of understanding).
- Strategy: **measure first, optimize only on miss**. M9 is
  primarily a UI + persistence milestone; perf gates are
  light (spec §5).
- Phase 1 closure follows the established M5/M6/M7/M8 flow
  (push + CI green + PR creation + done.md).

## 1. Subtask sequence overview

| # | Subtask | Prereqs | Effort | Commit | Notes |
|---|---|---|---|---|---|
| S1 | `src/connection/` scaffolding + freeze-surface headers + CMake wiring | — | 4 h | Yes | `Connection` + `ConnectionManager` headers per spec §4.1 / §4.2; reuse M3 driver_configs (document as C1). Stub .cpp files compile clean. Top-level CMake adds `add_subdirectory(src/connection)`. |
| S2 | `Connection` lifecycle state machine + driver dispatch + tests | S1 | 5 h | Yes | Idle → Connecting → Connected → Disconnecting → Idle (or → Error). Driver factory: switch-on-`DriverType` constructs the right M3 driver. Wire driver state callbacks → `Connection::stateChanged`. Unit tests for state transitions. |
| S3 | `ConnectionManager` CRUD + connection lookup + Qt signals + unit tests | S2 | 4 h | Yes | add / edit / remove / connect / disconnect / connectAll / disconnectAll / lookup. `connectionAdded` / `Removed` / `StateChanged` signals. Persistence stubs (filled in S4). |
| S4 | yaml save/load via yaml-cpp + canonical schema + persistence test | S3 | 5 h | Yes | `connections_schema_v1.yaml` + `.json` documentation form. Auto-save on every change. Round-trip test (incl. binary `payload` with NUL/CRLF). Missing/invalid yaml → graceful empty state. |
| S5 | `ConnectionDialog` modal + dynamic `QStackedWidget` per driver type + validation | S2 | 6 h | Yes | Driver type combo → swap pages. Per-type validation (spec §2.1-11). Common fields: display name, decoder schema combo (queries `DecoderRegistrar::availableSchemas()`), auto-connect toggle, auto-connect commands list editor. |
| S5s | Inherited M8 1-hour soak verification (deferred from M8) | (independent) | 2 h impl + 1 h run | Yes | Add `--soak <seconds>` and `--memory-snapshot <interval>` flags to `bench_chart`. Run 1 hour locally. Append result to `tests/benchmark/results/M8-baseline.md` + update `M8-done.md` §8.2. Acceptance: Vmrss < 10% growth, dropped frames < 50/108 k. HALT if fail. Per Phase 3 continuation prompt § "INHERITED CONCERN". |
| S6 | `ConnectionListWidget` + `ConnectionStatusWidget` + integration with M8 status bar | S3 | 4 h | Yes | List view (per-row icon + name + status). Buttons / context menu: Connect / Disconnect / Edit / Remove / Add. Status widget extends M8 status bar; click → opens manager. |
| S7 | `AutoConnectCommandSequence` + bytes round-trip + timeout/expected handling | S2 | 4 h | Yes | Sequential commands; failures log + continue per spec §3.5. Test with payload containing NUL + CRLF. Metric counters. |
| S8 | `ReplayDriver` completion (fill in M3 skeleton) + replay-driver integration test | S1 | 5 h | Yes | Reads frame log; plays at `playbackSpeed`; loops if configured; supports pause/resume via DriverInterface (M11 will use). Stays strictly in driver scope; replay UX is M11. |
| S9 | MainWindow integration: replace M3 preview ConnectionManager + add Connections menu / toolbar / list panel + status widget | S5 + S6 | 5 h | Yes | Remove `src/app/connection_manager.{hpp,cpp}` (M3 preview). Wire new `signalforge::connection::ConnectionManager` into `MainWindow`. Connect to existing `DecoderRegistrar` + `PipelineManager` from M5/M4. |
| S10 | 7 integration tests at `tests/integration/` + manual hardware verification protocol doc | S9 | 6 h | Yes | Per spec §2.1-14: lifecycle / persistence / dialog_validation / auto_connect_commands / status_widget / replay_full_cycle / serial_loopback. `docs/m9-hardware-verification.md` authored alongside. |
| S11 | `.claude/M9-done.md` + freeze record + manual hardware verification execution + PR | S5s + S10 green + CI green | 4 h | Yes | Mirrors M8 closure flow. SHA256s for 4 frozen files (2 hpp + 2 schema). Hardware verification results documented in done.md. |

**Total estimated effort**: ~52 h (within spec's 40-56 h
budget). Slack reserved for §S5 dialog UI polish and §S10
hardware-verification documentation if needed.

S5s ("inherited soak") is scheduled between S5 and S6 per the
Phase 3 continuation prompt's "after M9 S3 or before M9
closure" suggestion. S5 is the natural pause point — yaml
persistence is solid, dialog is functional, but UI integration
hasn't yet started so the chart subsystem is the only moving
piece in CI for the soak window.

## 2. Subtask details

### S1 — Scaffolding + freeze-surface headers + CMake wiring

**Deliverables**:

- `src/connection/CMakeLists.txt` adding `signalforge_connection`
  static lib. PUBLIC: Qt6::Core, Qt6::Widgets, signalforge_drivers,
  signalforge_decoder, signalforge_frame, signalforge_pipeline.
  PRIVATE: signalforge_observability, yaml-cpp::yaml-cpp.
  AUTOMOC ON.
- `src/connection/connection.hpp` per spec §4.2 with M3
  `signalforge::drivers::*Config` reused as the variant payload
  (rather than declaring new structs per spec literal text;
  documented as C1). Includes `Connection` class, `State` enum,
  `DriverType` enum, `AutoConnectCommand` struct,
  `ConnectionConfig` struct.
- `src/connection/connection_manager.hpp` per spec §4.1.
- `src/connection/connection_dialog.hpp` (forward declaration of
  the QDialog subclass).
- `src/connection/connection_list_widget.hpp` (forward).
- `src/connection/connection_status_widget.hpp` (forward).
- `src/connection/{connection, connection_manager,
  connection_dialog, connection_list_widget,
  connection_status_widget}.cpp` — ctor/dtor stubs.
- Top-level `CMakeLists.txt` adds
  `add_subdirectory(src/connection)`.
- `tests/unit/connection/CMakeLists.txt` + placeholder smoke
  test.
- `.claude/M9-concerns.md` written with C1 (driver-config struct
  reuse) + C2 (M3 preview ConnectionManager removal in S9).

**Acceptance**:

- All three presets build clean.
- Doxygen on every public declaration.
- `clang-format --dry-run -Werror` clean.

### S2 — `Connection` lifecycle + driver dispatch

**Deliverables**:

- `Connection` impl with the 5-state state machine.
- `connectDriver()` initiates async driver `open()` + `start()`,
  transitions Idle → Connecting; on success → Connected (and
  triggers AutoConnectCommandSequence via S7's plumbing); on
  failure → Error.
- `disconnectDriver()` initiates Connected → Disconnecting →
  Idle. Error → Idle is allowed via `disconnectDriver()`.
- Driver factory in ctor:
  ```cpp
  switch (config_.driverType) {
  case DriverType::Serial:
      driver_ = std::make_unique<drivers::SerialDriver>(
          std::get<drivers::SerialConfig>(config_.driverConfig));
      break;
  // ... Tcp / Udp / Replay
  }
  ```
- Wire driver state callbacks → `Connection::stateChanged`
  emission.

**Tests** at `tests/unit/connection/connection_test.cpp`:

- State machine transitions (each edge); illegal transitions
  rejected.
- Construction with each `DriverType` succeeds.
- `state()` / `lastError()` / `config()` accessors.
- `setConfig(newConfig)` only works in Idle (returns false
  otherwise).

### S3 — `ConnectionManager` CRUD + Qt signals

**Deliverables**:

- `ConnectionManager(DecoderRegistrar&, QObject*)` ctor.
- `addConnection(config)` returns generated id (or honors
  `config.id` if free); emits `connectionAdded`.
- `editConnection(id, newConfig)` only when Idle; returns false
  otherwise.
- `removeConnection(id)` only when Idle; returns false
  otherwise; emits `connectionRemoved`.
- `connectConnection(id)` / `disconnectConnection(id)` /
  `connectAll()` / `disconnectAll()` delegating to
  `Connection::connectDriver` / `disconnectDriver`.
- Lookup: `connection(id)`, `connectionIds()`,
  `connectionCount()`, `connectedCount()`, `erroredCount()`.
- Forward per-Connection `stateChanged` → `connectionStateChanged
  (id, newState)`.
- Persistence stubs (filled in S4).

**Tests** at `tests/unit/connection/connection_manager_test.cpp`:

- add / edit / remove round trip; activeId behavior.
- editConnection on connected → returns false.
- removeConnection on connected → returns false.
- Forwarded state-change signals fire correctly.

### S4 — yaml save/load + canonical schema + persistence test

**Deliverables**:

- `ConnectionManager::saveConfigFile(path)` /
  `loadConfigFile(path)` using yaml-cpp.
- `connections_schema_v1.yaml` canonical example + JSON-Schema
  documentation form.
- Schema written so the M3 driver-config field names (e.g.
  Serial's `device`) match the yaml keys.
- `defaultConfigPath()` helper using `QStandardPaths`.
- Auto-save in `addConnection` / `editConnection` /
  `removeConnection`.
- Missing file → empty manager, returns false.
- Invalid yaml → log ERROR, returns false, leaves manager
  empty.

**Tests** at `tests/unit/connection/connection_persistence_test.cpp`:

- yaml round-trip on a 3-connection config (one of each driver
  type plus Replay).
- Round-trip preserves binary AutoConnectCommand `payload`
  with NUL + CRLF.
- Missing file → false + empty manager.
- Invalid yaml (deliberate parse error) → false + ERROR
  logged.
- Schema version != 1 → rejected.

### S5 — `ConnectionDialog` modal + dynamic widget

**Deliverables**:

- `ConnectionDialog` modal `QDialog` per spec §3.1 / §4.7.
- `QStackedWidget` with one page per driver type.
- Driver Type combo → page swap.
- Per-driver widgets:
  - Serial: device combo (auto-populated via
    `QSerialPortInfo::availablePorts()`), baud spinbox,
    dataBits combo, parity combo, stopBits combo, flowControl
    combo.
  - TCP: host line edit, port spinbox, connectTimeout
    spinbox.
  - UDP: bind port + remote host/port + buffer size.
  - Replay: file picker + speed spinbox + loop checkbox.
- Common fields: display name, decoder schema combo
  (populated via `DecoderRegistrar::availableSchemas()`),
  autoConnectOnStartup checkbox (with tooltip noting "V1.5+:
  currently has no effect"), auto-connect commands list
  editor.
- "Test Connection" button (V1: opens & closes; surfaces error
  if any).
- Per-type config validation; OK button disabled when invalid.

**Tests** at `tests/unit/connection/connection_dialog_test.cpp`
(headless via `QApplication`):

- Dialog constructs, populates combos.
- Setting Driver Type swaps pages.
- Validation rejects invalid configs (port == 0, host empty,
  etc.).
- Filling fields → produces correct `ConnectionConfig` via
  the dialog's getter.

### S5s — Inherited M8 1-hour soak

**Deliverables**:

- Add `--soak <seconds>` and `--memory-snapshot <interval>`
  flags to `tests/benchmark/bench_chart.cpp`.
- Soak mode: real-clock QTimer (33 ms redraw + 1 ms inject),
  60 signals × 1 chart, runs for the requested seconds, logs
  Vmrss snapshots from `/proc/self/status` every interval
  seconds, reports final stats (total ticks, dropped frames,
  Vmrss growth %).
- Run locally for 3600 seconds.
- Acceptance: Vmrss growth < 10%, dropped frames < 50,
  ASan/LSan clean (CI debug-asan path).
- Append `## 1-hour soak (S5s)` section to
  `tests/benchmark/results/M8-baseline.md`.
- Update `.claude/M8-done.md` §8.2 marking the soak ✅ with
  measured numbers (and replace the "pending" note in §1-hour
  soak result).

**HALT triggers**:
- Vmrss growth > 10% → HALT (M8 spec §7 trigger #6).
- Dropped frames > 50 → HALT (correctness).
- ASan finding → HALT (memory safety).

If HALT fires: file `.claude/halt/HALT-<ts>-m8-soak.md` and
stop. Otherwise commit + push.

### S6 — `ConnectionListWidget` + `ConnectionStatusWidget`

**Deliverables**:

- `ConnectionListWidget` (`QListView`-based) showing one row
  per connection: icon (driver-type), name, status indicator
  (Idle / Connected / Error).
- Buttons / context menu: Connect, Disconnect, Edit, Remove,
  Add. Double-click → Edit dialog.
- `ConnectionStatusWidget` (small `QWidget` for status bar)
  showing "X/N connected" + error count. Click → opens main
  dialog.
- Wire the widgets to `ConnectionManager`'s
  `connectionAdded` / `Removed` / `StateChanged` signals so
  rebuilds happen automatically.

**Tests** at `tests/unit/connection/connection_list_widget_test.cpp`:

- Initial population from a manager with N connections.
- After `connectionAdded`, list grows.
- After `connectionRemoved`, list shrinks.
- Clicking Connect button → manager.connectConnection invoked.

### S7 — `AutoConnectCommandSequence`

**Deliverables**:

- Sequential command sender invoked on `Connection::stateChanged
  → Connected`.
- For each `AutoConnectCommand`:
  1. Wait `delayBefore`.
  2. Driver write `payload`.
  3. If `expected` set: wait for matching response or `timeout`.
  4. Continue to next command on success / WARN-and-continue on
     timeout / ERROR-and-abort on driver write failure.
- Metric counters:
  - `connection_auto_command_timeouts`
  - `connection_auto_command_failures`
- Emit `Connection::autoConnectCommandSent(name)` and
  `autoConnectCompleted(success)`.
- Per spec §3.5: errors don't disconnect — connection stays
  Connected with "auto-connect failed" status.

**Tests** at `tests/integration/test_auto_connect_commands.cpp`:

- Connection with 3 commands; verify all 3 sent in order
  with `delayBefore` honored.
- Command with `expected` matching → continues.
- Command with `expected` not matching → WARN + continue.
- Driver write failure mid-sequence → ERROR + abort, but
  Connection stays Connected.

### S8 — `ReplayDriver` completion

**Deliverables**:

- Fill in the M3 skeleton at
  `src/drivers/replay_driver.{hpp,cpp}`.
- Read frame log file (format: M2 / M3-defined; verify
  format spec or extend if needed).
- Emit frames to FramePipeline at `playbackSpeed × original-rate`.
- Loop if `loop == true`.
- Support pause / resume via DriverInterface API (M11
  consumer).
- M9 doesn't add UX; just driver functionality.

**Tests** at `tests/integration/test_replay_driver_full_cycle.cpp`:

- Play a known frame log; verify frame count + first/last
  timestamps.
- `playbackSpeed = 0.5` → playback wall-time ≈ 2× source
  duration (within 5% per spec §8.3 manual verification).
- `loop = true` → after EOF, restarts from beginning.

### S9 — MainWindow integration + remove M3 preview

**Deliverables**:

- Remove `src/app/connection_manager.{hpp,cpp}` (M3 preview).
- Update `src/app/main_window.{hpp,cpp}`:
  - Replace `connectionManager_` (M3 preview) with the new
    `signalforge::connection::ConnectionManager`.
  - Add `signalSelector_->refresh()` call after
    `connection->connectDriver()` succeeds (so newly-arriving
    decoder signals appear in the chart's signal selector).
  - Add "Connections" menu (Add / Edit / Remove / Connect All
    / Disconnect All).
  - Add toolbar "Add Connection" button.
  - Add ConnectionListWidget panel (left of M8 chart area, as
    a `QDockWidget` or in the existing splitter).
  - Add ConnectionStatusWidget to the M8 status bar (alongside
    the existing FPS / dropped / throttled labels).
- Update `src/app/CMakeLists.txt` link list:
  + signalforge_connection.

**Tests**: covered by S10 integration tests.

### S10 — Integration tests + hardware verification protocol

**Deliverables**:

7 integration tests at `tests/integration/`:

- `test_connection_manager_lifecycle.cpp` — add Replay
  connection (most testable without hardware), connect,
  disconnect, remove. Verify state transitions + Qt signal
  emissions.
- `test_connection_persistence.cpp` — add 2 connections, save
  yaml, reload from yaml, verify both present + bit-identical
  config.
- `test_connection_dialog_validation.cpp` — invalid configs
  (port 0, empty host, missing file) rejected; valid configs
  produce correct `ConnectionConfig`.
- `test_auto_connect_commands.cpp` — Replay-driver-based test;
  3 commands sent in order with timing within tolerance.
- `test_connection_status_widget.cpp` — status widget label
  reflects Connection state changes.
- `test_replay_driver_full_cycle.cpp` — full Replay driver
  cycle through ConnectionManager.
- `test_serial_loopback.cpp` — Serial driver via socat-fixture
  loopback (existing M3 fixture).

`docs/m9-hardware-verification.md`:

- Step-by-step protocol for verifying Serial (real device or
  socat), TCP (echo server), UDP (bind + sender), Replay
  (recorded log).
- Acceptance criteria per driver type.
- Where to record results in M9-done.md.

### S11 — `.claude/M9-done.md` + freeze record + manual hardware verification + PR

**Deliverables**:

- Run the manual hardware verification protocol (S10 deliverable).
  Document results in M9-done.md.
- `.claude/M9-done.md` per the M5/M6/M7/M8 pattern:
  - Deliverables checklist (vs spec §2.1).
  - Acceptance self-check per spec §8.
  - Test count matrix.
  - Manual hardware verification result.
  - Inherited concerns: M8 1-hour soak ✅ (resolved in S5s).
  - Freezes section with sha256 of:
    - `src/connection/connection.hpp`
    - `src/connection/connection_manager.hpp`
    - `schemas/connections_schema_v1.yaml`
    - `schemas/connections_schema_v1.json`
  - Commit manifest.
  - CI verification status.
  - Hand-off to M10 (Session Writer) / M11 (Replay) / M12
    (Performance) / M13 (Packaging).
  - HALT resolution trail (none expected).
  - Deviations and concerns.
- PR against `main`, title "M9: Connection Manager (Full
  Features)". Body summarizes scope + frozen artifacts +
  manual hardware verification results.
- **Stop and announce** per CLAUDE.md §Phase 1 step 6:
  "M9 ready. Awaiting approval to merge M9 and begin M10
  bootstrap."

## 3. Pre-encoded HALT statements (spec §7)

| # | Trigger | Measurement point | Action |
|---|---|---|---|
| 1 | Modification to M2-M8 frozen `.hpp` | Pre-commit `git diff` against the inherited freeze list | HALT report `.claude/halt/HALT-<ts>-frozen-modified.md`; revert change |
| 2 | M3 driver interface needs non-additive change | S2 driver dispatch + S8 ReplayDriver completion | HALT report; consider ADR for M3 amendment |
| 3 | yaml schema breaking change required mid-implementation | S4 persistence test | HALT report; re-evaluate decision M9.2 |
| 4 | Connection dialog can't validate driver config (e.g., port enumeration hangs) | S5 dialog implementation | HALT report; mitigation proposal |
| 5 | Persistence yaml round-trip not bit-identical | S4 persistence test | HALT report; yaml-cpp formatting investigation |
| 6 | Qt SerialPort not available on target platform | S1 build | HALT report; document hardware limitation |
| 7 | Auto-connect command cannot reliably wait for response | S7 + S10 `test_auto_connect_commands.cpp` | HALT report; simplify or re-design (e.g., drop expected-response feature in V1) |

Plus the inherited M8 spec §7 trigger #6 (1-hour soak memory >
10% growth or dropped frames > 50) → HALT during S5s.

CLAUDE.md §HALT triggers (compile error after 3 fixes, test
fail after 3 fixes, etc.) apply at every subtask.

## 4. Risk register

(See `.claude/M9-understanding.md` §8 for the 7-row register.)

## 5. Dependencies (no new top-level)

Per spec §2.2-8, no new top-level dependencies. Existing in-
tree modules + already-used Qt modules are sufficient. See
`.claude/M9-understanding.md` §9 for the full list.

## 6. What's deferred to V1.5+ / V2

(Per spec §2.2 + scattered §3 notes.)

V1.5+:
- Per-connection auto-connect-on-startup toggle effect (currently
  forward-compat only).
- Connection grouping / folders.
- Connection sharing / export.
- Remote connection management.
- Advanced auto-connect command DSL (conditionals, loops,
  response parsing).
- Multi-modal config dialog with tabs (Option B).
- Tray icon for connection status (Option T).
- Drag-drop reordering in connection list.
- "Test Connection" button (advanced — V1 just opens & closes).

V2:
- Connection-level encryption / TLS / authentication.
- Network discovery (mDNS / SSDP for IoT devices).
- Plugin-architecture for custom driver types.

## 7. Inherited from M8 (S5s)

The 1-hour soak from M8 spec §5.6 + plan §S11 runs as S5s,
between S5 and S6. Per the Phase 3 continuation prompt:

- Vmrss growth < 10% across 1 hour
- Dropped frames < 50 across ~108 k frames
- ASan / LSan clean

If pass: append result to M8-baseline.md + update
M8-done.md §8.2 marking the soak ✅. If fail: HALT and
assess M8 hotfix vs M9-introduced regression.
