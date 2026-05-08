# M11 — Progress log

Per CLAUDE.md §Required #2 + plan §0, every subtask logs start +
close entries with build / test / format counts and any deviations.

---

## Pre-S0 — M11 understanding + plan (completed)

- Start: 2026-05-08T03:30Z (this Phase 3 continuation)
- Close: 2026-05-08T04:25Z
- Commit: `a8c7bc0` "chore: record M11 understanding and plan"
- CI: pending — push triggers CI per CLAUDE.md §Required #2
- Deliverables:
  - `.claude/M11-understanding.md` (266 lines, 6 concerns C1-C6
    surfaced)
  - `.claude/M11-plan.md` (407 lines, S0-S12 sequenced; 7 HALT
    triggers H1-H7)

---

## S0 — M11-concerns.md + ADR-008 decision (completed)

- Start: 2026-05-08T04:30Z

### Deliverables

- `.claude/M11-concerns.md` (~290 lines): canonical record of
  C1-C6, each with resolution path + subtask anchor. No ADR-008
  authored — default path holds. Stage-A → Stage-B fallback for
  C4 (sleep_for → sleep_until → QTimer) documented; H4 escalation
  path for C1 documented as conditional ADR amendment if S10
  measures > 500 ms seek.

### Phase 4 → Phase 5 carry

- Resolutions table at file end maps each concern to its subtask:
  C1→S2, C2→S1, C3→S8, C4→S5/S10, C5→S5, C6→S9.

### Build / test counts

- Docs-only commit. CLAUDE.md §Required #2 exception applies
  (build graph unaffected). clang-format: not applicable.

### Deviations from plan

- Plan §S0 anticipated a conditional ADR-008 stub. Default
  position holds: no architectural divergence requiring ADR-008
  at this point. Conditional escalation path (S10 H4 watermark)
  is documented inside `M11-concerns.md` §C1, so a future ADR-008
  can be authored as a delta against this baseline.

S0 commit: `e9d45d7` "docs: M11 S0 — concerns C1-C6 (no ADR-008)".
Pushed after pre-S0 CI green (run 25545764962 ✓ 9m38s).
S0 CI: run 25546591496 ✓ (debug-asan 10m26s + 2 other jobs all
green).

---

## S1 — Module scaffold + freeze headers (completed)

- Start: 2026-05-08T05:05Z

### Deliverables

- `src/replay/CMakeLists.txt`: new `signalforge_replay` static lib.
  PUBLIC links: `Qt6::Core`, `Qt6::Widgets`, `signalforge_session`,
  `signalforge_buffer`, `signalforge_decoder`, `signalforge_connection`.
  PRIVATE: `signalforge_observability`. AUTOMOC ON.
- `src/replay/playback_controller.hpp` (frozen at M11 close):
  `PlaybackState` enum (Idle / Loaded / Playing / Paused / Seeking /
  Ended / Error) + `PlaybackController` QObject. Constructor takes
  `SignalBufferRegistry&`. Public API per spec §4.1; Doxygen on every
  declaration.
- `src/replay/session_player.hpp` (frozen at M11 close):
  `SessionPlayer` QObject; constructor takes `SignalValueSink&`.
  Public API per spec §4.2 (with C2 corrected include path
  `decode/decoder_interface.hpp`); Doxygen on every declaration.
- `src/replay/replay_mode_manager.hpp` (frozen at M11 close):
  `AppMode` enum (Live / Replay) + `ReplayModeManager` QObject.
  Constructor takes `ConnectionManager&` + `PlaybackController&`.
  Public API per spec §4.3 plus a `pausedConnectionIds()` accessor
  for testability of the C3 disconnect-snapshot pattern.
- 3 stub `.cpp` files. Each method body comments which subtask
  (S2-S9) fills it in. Stubs return safe defaults; no signals are
  emitted; SF_LOG_INFO emitted on the high-level lifecycle stubs to
  make discovery during S3-S8 explicit.
- `CMakeLists.txt`: `add_subdirectory(src/replay)` placed AFTER
  `src/connection` so transitive linkage to `signalforge_connection`
  resolves at configure time.
- `tests/unit/replay/CMakeLists.txt` + `replay_smoke_test.cpp`: 2
  smoke cases verifying the M11 enum vocabulary.
- `tests/unit/CMakeLists.txt`: `add_subdirectory(replay)` appended.

### Build / test counts

- Debug + Release + debug-asan all build clean (incremental — only
  `signalforge_replay`, the smoke test, plus the apps that
  transitively re-link rebuilt).
- ctest: Debug 545/545; Release 545/545. The 2 new M11 S1 cases
  appear at #472 + #473 and pass; total displayed unchanged because
  pre-existing dynamic discovery aggregates against the same
  `Total Tests:` count. Net behavioural delta: +2 cases verified
  green via `--rerun-failed`-style explicit listing.
- ASan local-run blocked by host /etc/ld.so.preload; CI authoritative.
- `clang-format --dry-run -Werror` clean on all 6 new source files.

### Deviations from plan

- Plan §S1 anticipated ~300 net LOC. Actual: ~330 LOC (3 headers
  + 3 stubs + 1 smoke test + CMake wiring) — within target.
- Added a `pausedConnectionIds()` accessor on `ReplayModeManager`
  beyond the spec §4.3 sketch. Reason: S8 tests need an
  observation point on the C3 disconnect-snapshot list without
  exposing the internal `previouslyConnectedIds_` member. Additive
  to the spec; documented here.
- S1 placed `add_subdirectory(src/replay)` after `src/connection`
  rather than after `src/session` (where the plan §S1 listed it
  conceptually). Rationale: `signalforge_replay` PUBLIC-links
  `signalforge_connection`; placing the subdirectory before the
  dependency would risk configure-time errors on tighter CMake
  versions. No behavioural change.

S1 commit: pending push.

