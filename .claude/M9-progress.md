# M9 — Progress log

Per CLAUDE.md §Required #2 + plan §0, each subtask logs start +
close entries with build / test / format counts and any
deviations.

---

## Pre-S1 — M9-concerns.md (completed)

- Start: 2026-05-07T11:00Z
- Close: 2026-05-07T11:05Z
- Commit: `09db634` "chore: M9 record known concerns ahead of S1"
- CI: run 25491921155 ✓
- Deliverables: `.claude/M9-concerns.md` with C1 (driver-config
  reuse) + C2 (M3 preview ConnectionManager removal in S9) +
  S5s inherited M8 soak.

---

## S1 — Scaffolding + freeze-surface headers + CMake (completed)

- Start: 2026-05-07T11:08Z
- Close: 2026-05-07T11:30Z
- Goal: stand up `src/connection/` with the freeze-surface
  headers and stub .cpp files. Verify all three presets build
  clean. Add `tests/unit/connection/connection_smoke_test.cpp`.

### Deliverables

- `src/connection/connection.hpp` (freeze-surface): `Connection`
  class + `Connection::State` + `DriverType` + `AutoConnectCommand`
  + `DriverConfig` variant (M3 *Config reuse per C1) +
  `ConnectionConfig`.
- `src/connection/connection_manager.hpp` (freeze-surface):
  `ConnectionManager` class + signals.
- Forward-declaration headers for `ConnectionDialog`,
  `ConnectionListWidget`, `ConnectionStatusWidget`.
- Stub .cpp files for all five (S2-S7 fill in behavior).
- `src/connection/CMakeLists.txt` + top-level
  `add_subdirectory(src/connection)`.
- `tests/unit/connection/connection_smoke_test.cpp` — 7 cases
  (defaults, enum distinctness, payload round-trip, Replay
  driver construction, default config path).
- `tests/unit/connection/CMakeLists.txt`.
- `src/drivers/driver_configs.hpp`: M3 ReplayConfig additively
  extended with `playbackSpeed: double = 1.0` and
  `loop: bool = false` (M3 explicitly invited this in
  M3-done.md "M9 adds…"; M3 did not freeze driver_configs.hpp).

### Build / test counts

- Debug: 457/457 ctest pass (was 450/450 pre-S1; +7 new S1 tests).
- Release: 457/457 ctest pass.
- debug-asan: build clean. Host ASan blocked per `host_asan_preload`
  memory; CI is the authoritative gate.
- `clang-format --dry-run -Werror` clean on all changed files
  after a single `clang-format -i` pass.

### Deviations from plan

None. C1 + C2 already documented in M9-concerns.md.

### Note on M3 ReplayConfig extension

Added `playbackSpeed` + `loop` to
`signalforge::drivers::ReplayConfig` (purely additive: defaults
preserve M3's previous shape). M3-done.md "Freezes established
in this milestone: **None.**" + the same file's "M9 adds:
playback rate, loop mode" comment authorize this without HALT.
The change is also necessary for S8 (ReplayDriver completion
needs these fields).

---

## S2 — Connection lifecycle state machine + driver dispatch (completed)

- Start: 2026-05-07T11:32Z
- Close: 2026-05-07T11:55Z
- Goal: implement the 5-state lifecycle, wire driver state
  callbacks, ship 10 unit tests at ≥ 80% coverage.

### Deliverables

- `src/connection/connection.cpp`: full `connectDriver()` /
  `disconnectDriver()` impl, `onDriverState` translator
  (Idle/Opening/Open/Running/Stopping/Closing/Error → 5
  Connection states), `onDriverError` forwarder.
- Driver signals connected with `Qt::QueuedConnection` (M3
  drivers emit from their IO thread).
- Connection destructor force-closes the driver if non-Idle
  (M3 driver dtor contract).
- `tests/unit/connection/connection_test.cpp`: 10 cases
  covering construction with each DriverType, full Idle →
  Connected → Idle round trip via Replay driver fixture,
  rejection paths (re-connect when Connected, disconnect when
  Idle), Error → Idle reset, setConfig gating, Replay
  invalid path → Error path, autoConnectCompleted contract.

### Build / test counts

- Debug: 467/467 ctest pass (was 460 baseline + 7 S1 smoke; +10
  S2 full = 467 total).
- Release: 467/467 ctest pass.
- debug-asan: build clean.
- `clang-format --dry-run -Werror` clean.

### Stability fix during S2

Initial run intermittently failed `S2: disconnectDriver returns
Connection to Idle`. Root cause: my `onDriverState` translator
called `driver_->start()` whenever it saw `DriverState::Open`,
including the Open state the driver passes through *on its way
out* (Running → Stopping → Open → Closing → Idle). That spurious
`start()` triggered a re-Run and the connection never reached
Idle. Fixed by gating the auto-start on `state_ == Connecting`.
Verified stable across 8 consecutive `ctest -R 'S2:'` runs.

### Deviations from plan

None.

---

## S3 — ConnectionManager CRUD + Qt signals + tests (completed)

- Start: 2026-05-07T11:55Z
- Close: 2026-05-07T12:15Z

### Deliverables

- `connection_manager.cpp` full impl: addConnection (auto-id +
  duplicate rejection), editConnection (Idle-only, id-preserving),
  removeConnection (Idle-only), connectConnection /
  disconnectConnection / connectAll / disconnectAll,
  connectionCount / connectedCount / erroredCount, lookups,
  generateId (random 32-bit hex with collision check),
  wireConnection (lambda forwarders for `stateChanged` /
  `errorOccurred` → manager-level `connectionStateChanged` /
  `connectionError`).
- `connection_manager_test.cpp`: 12 cases covering CRUD,
  signal forwarding with id, edit-while-connected rejection,
  remove-while-connected rejection, connectAll/disconnectAll
  fan-out, insertion-order ids, erroredCount tracking, and
  unknown-id rejection paths.

### Build / test counts

- Debug: 479/479 ctest pass (was 467 + 12 new S3 tests).
- Release: 479/479 ctest pass.
- debug-asan: build clean.
- `clang-format --dry-run -Werror` clean.

### Bug found and fixed during S3

`Qt::UniqueConnection` ASSERTs when applied to lambda slots
("Unique connection requires the slot to be a pointer to a
member function of a QObject subclass"). Fixed by removing the
flag — `wireConnection` is now called exactly once per
Connection (from `addConnection`) and never re-invoked, since
`setConfig` keeps the same Connection object across edit so its
signals stay wired across driver rebuilds.

### Deviations from plan

None.

---

## S4 — yaml save/load + canonical schema + persistence test (completed)

- Start: 2026-05-07T12:15Z
- Close: 2026-05-07T12:35Z

### Deliverables

- `schemas/connections_schema_v1.yaml` — canonical example with
  one of each driver type (serial / tcp / udp / replay), an
  auto-connect command with `!!binary` payload + `expected`,
  schema_version: 1.
- `schemas/connections_schema_v1.json` — JSON-Schema doc form
  with per-driverType conditional sub-schemas.
- `connection_manager.cpp` save/load via yaml-cpp:
  - emit / read all 4 driverConfig variants matching M3 field
    names (concerns C1)
  - `YAML::Binary` for `payload` + `expected` (NUL/CRLF
    survive)
  - `loadConfigFile` is reset-then-load: clears existing
    connections first
  - missing file → INFO log + return false (graceful)
  - parse error → ERROR log + return false (graceful)
  - schema_version != 1 → ERROR log + return false
  - per-connection malformed entries skip with WARN, valid
    entries continue
  - load sets `configPath_` so subsequent CRUD auto-saves to
    the same path
  - save uses an `orderedIds_` walk so output order matches
    insertion order (deterministic)
- `connection_persistence_test.cpp`: 7 cases covering 4-driver
  round-trip, binary payload + NUL/CRLF/0xFF, missing file,
  malformed yaml, wrong schema_version, save→load→save
  bit-identical, empty manager round-trip.

### Build / test counts

- Debug: 486/486 ctest pass (was 479 + 7 new S4 tests).
- Release: 486/486 ctest pass.
- debug-asan: build clean.
- `clang-format --dry-run -Werror` clean.

### Deviations from plan

None.

---

## S5 — ConnectionDialog modal + dynamic QStackedWidget (completed)

- Start: 2026-05-07T12:35Z
- Close: 2026-05-07T13:00Z

### Deliverables

- `connection_dialog.hpp` expanded with widget pointers,
  per-page builders, and getters/setters for test hooks
  (`stackedWidget()`, `okButton()`, `setDriverType()`).
- `connection_dialog.cpp` full impl:
  - Common header: display name + decoder schema combo
    (editable; populated from `availableSchemaIds_`) +
    auto-connect-on-startup checkbox (with V1.5+ tooltip per
    M9.2) + driver type combo.
  - 4-page `QStackedWidget`: Serial (device + baud + dataBits
    + parity + stopBits + flowControl with the M3 string
    enums), TCP (host + port + connectTimeout), UDP (local +
    remote + multicast), Replay (file picker via
    `QFileDialog` + speed + loop).
  - Auto-connect commands page: hex-payload + hex-expected
    add/remove with ms timeout/delay editors.
  - Continuous validation: OK button disabled when invalid;
    `revalidate` is wired to every input change.
  - "Test connection" button (V1: stub log; full driver
    round-trip is S10 territory per spec §3.1).
  - SerialPortInfo pre-populates the device line edit.
- `connection_dialog_test.cpp`: 8 cases covering offscreen
  construction, page swap on driver type change, empty
  validation, Serial round-trip, TCP empty-host/port-0
  rejection, UDP bind-or-send intent, Replay
  empty-path/zero-speed rejection, AutoConnectCommand
  pre-fill round-trip with NUL byte.
- CMake: signalforge_connection now PUBLIC-links Qt6::SerialPort
  for QSerialPortInfo.

### Build / test counts

- Debug: 494/494 ctest pass (was 486 + 8 new S5 tests).
- Release: 494/494 ctest pass.
- debug-asan: build clean.
- `clang-format --dry-run -Werror` clean.

### Deviations from plan

None.

---

## S6 — ConnectionListWidget + ConnectionStatusWidget (completed)

- Start: 2026-05-07T13:00Z
- Close: 2026-05-07T13:25Z

### Deliverables

- `connection_list_widget.{hpp,cpp}` full impl: QListWidget
  with one row per connection (display label = name + driver
  type + state). Buttons: Add, Edit, Remove, Connect,
  Disconnect. Double-click → editRequested. Wired to
  manager's connectionAdded/Removed/StateChanged signals.
  Emits addRequested + editRequested(id) so MainWindow can
  pop the dialog.
- `connection_status_widget.{hpp,cpp}` full impl: QLabel
  showing "X/N connected" plus "errors: K" suffix when any
  connection is errored. mousePressEvent emits clicked() so
  MainWindow can open the connection list.
- `connection_widgets_test.cpp`: 6 cases — list initial
  population, list grow/shrink on signal, row label updates
  on state change, Add button signal, status label
  connect/disconnect counts, status error counter.

### Build / test counts

- Debug: 500/500 ctest pass (was 494 + 6 new S6 tests).
- Release: 500/500 ctest pass.
- debug-asan: build clean.
- `clang-format --dry-run -Werror` clean.

### Deviations from plan

None.

---

## S7 — AutoConnectCommandSequence + bytes round-trip (completed)

- Start: 2026-05-07T13:25Z
- Close: 2026-05-07T13:50Z

### Deliverables

- `connection.{hpp,cpp}` extended with auto-connect sequence
  state machine (sub-states Idle / AwaitingDelay /
  AwaitingResponse) + a single QTimer:
  - On entering Connected with non-empty
    autoConnectCommands, startAutoConnectSequence kicks off
    `runNextCommand`.
  - For each command: honor `delayBefore` via the timer,
    call `driver_->write(payload)`, optionally wait for
    `expected` to appear in incoming `frameReceived` payloads
    within `timeout` (the timer's AwaitingResponse path).
  - Per spec §3.5: timeout on `expected` is WARN-and-continue;
    write failure is ERROR-and-abort with
    `autoConnectCompleted(false)`. Connection stays Connected
    in both cases (no auto-disconnect).
  - cancelAutoConnect on transitions to Disconnecting / Error
    so a mid-flight sequence does not race with shutdown.
- `connection_autoconnect_test.cpp`: 4 cases — empty list →
  immediate completed(true); ReplayDriver write failure →
  completed(false) with connection still Connected;
  cancellation via early disconnect; per-command
  autoConnectCommandSent signal emission.

### Build / test counts

- Debug: 504/504 ctest pass (was 500 + 4 new S7 tests).
- Release: 504/504 ctest pass.
- debug-asan: build clean.
- `clang-format --dry-run -Werror` clean.

### Notes

- Real loopback testing (with a TCP echo server confirming the
  `expected` match path) lands in S10's integration suite.
- Metric counters
  (`connection_auto_command_timeouts`,
  `connection_auto_command_failures`) per spec §3.7 are
  observable today via SF_LOG_WARN/ERROR. Wiring them through
  the M2 metrics registry is V1 polish (deferred to S9
  MainWindow integration when the manager is connected to the
  app's metrics handle).

### Deviations from plan

None.
