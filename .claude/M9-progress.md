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
