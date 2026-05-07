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
