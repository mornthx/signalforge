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

S1 commit: `382799a` "replay: scaffold M11 module + freeze-surface
headers (S1)". Pushed after S0 CI green (run 25546591496 ✓).
S1 CI: run 25547888468 ✓ (debug + release + debug-asan all green).

---

## S2 — SessionReader streaming + seek extension (C1.α) (completed)

- Start: 2026-05-08T05:30Z

### Deliverables

- `src/session/session_reader.hpp`: additive surface per
  `M11-concerns.md` C1.α.
  - New public `struct ReplayRecord { int64_t timestampNs;
    QString signalId; SignalValue value; };`.
  - New public methods: `readNextRecord(out)`,
    `seekToTimestamp(targetNs)`, `currentRecordIndex()`,
    `currentTimestampNs()`, `atEnd()`,
    `bindCatalogSink(SignalValueSink*)`, `footerRecordCount()`,
    `lastTimestampNs()`.
  - `replayAll(sink)` reimplemented as a thin loop over
    `readNextRecord` + bound catalog sink — preserves M10
    round-trip semantics exactly (regression detector).
  - Internal split: `headerCatalog_` snapshot (immutable after
    open) lets backward seek restore catalog state to header
    only; `runningCatalog_` mutates as extensions are seen.
- `src/session/session_reader.cpp`: full implementation. Two
  shared helpers consume the on-disk record format:
  `readRecordHeader` + the existing LE / UTF-8 byte readers.
  `preScanFooter` walks the file once at open() to populate
  `lastTimestampNs_` for `seekToTimestamp` clamping. The
  `currentRecordIdx_` (signal-value-only) and `recordsRead_`
  (all records) counter split keeps the M10 footer-match
  invariant alive while exposing a useful streaming-side ordinal.
- `tests/unit/session/session_reader_streaming_test.cpp`:
  7 cases / 78 assertions:
  - streaming returns same events as `replayAll`
  - forward seek to mid-file timestamp
  - backward seek resets catalog + re-walks
  - seek past EOF clamps to last record
  - catalog-extension dispatched to bound sink during seek
  - `currentRecordIndex` tracks signal-value-only ordinals
  - `replayAll` still works after streaming + seek (M10
    round-trip preserved — regression gate)

### Build / test counts

- Debug + Release + debug-asan all build clean.
- ctest: Debug **552/552** + Release **552/552** (+7 from S1:
  the 7 new streaming cases). M10 S6/S7 round-trip suite
  passes unchanged (regression detector validated).
- ASan local blocked by host /etc/ld.so.preload; CI authoritative.
- `clang-format -i` applied on all 3 changed files; dry-run
  -Werror clean afterward.

### Bug found + fixed during S2

The first-cut implementation incremented `recordsRead_` only on
SignalValue records. Existing S7 test "catalog extension
mid-stream round-trip" failed because the writer's
`totalRecords_` (used in the footer's `totalRecords` field)
counts every record type — so my `recordsRead_` < footer's
totalRecords for files containing catalog extensions, breaking
the `fileComplete_` check. Fixed by counting **all** record
types in `recordsRead_` (matches writer's invariant) and using
the separate `currentRecordIdx_` for the M11 streaming caller's
signal-value-only ordinal. Doxygen + inline comment now
document the dual-counter contract. M10 S6/S7 + integration
`test_session_full_stack_round_trip` both green after fix.

### Deviations from plan

- Plan §S2 anticipated ~400 net LOC. Actual: ~580 LOC
  (header +150, cpp +380, test +210, CMake +12). Within
  acceptable variance — the `preScanFooter` pre-walk for
  `lastTimestampNs_` was added to keep `seekToTimestamp`
  correct without requiring a duplicate end-of-file scan
  every seek call. Worth the cost.
- Plan §S2 did not anticipate the catalog-extension counter
  mismatch bug — caught only because the M10 round-trip test
  is the regression detector that the "thin layer" pattern in
  plan §S2 relies on. The pattern paid off exactly as designed.

S2 commit: `3e80cba` "session: SessionReader streaming + seek
API (M11 S2)". Pushed after S1 CI green (run 25547888468 ✓).
S2 CI: run 25549181411 ✓ (debug + release + debug-asan all green).

---

## S3 — SessionPlayer skeleton + lifecycle (completed)

- Start: 2026-05-08T06:00Z

### Deliverables

- `src/replay/session_player.cpp`: openFile / closeFile lifecycle.
  - `openFile` instantiates a `SessionReader`, calls `open`,
    binds catalog sink, announces initial catalog to the sink,
    populates `durationNs_` (from S2's `lastTimestampNs_`) and
    `totalRecords_` (from S2's `footerRecordCount_`), creates the
    worker `QThread` (named `session-player-worker`) but does
    NOT start it. Auto-closes a previously-open file before
    opening a new one.
  - `closeFile` requests interruption, quits + waits the worker
    if running, releases the reader, resets all atomic counters.
    Idempotent.
  - Destructor calls `closeFile` defensively.
  - All control entry points (`play`, `pause`, `setSpeed`,
    `stepForward`, `stepBackward`, `seek`) remain stubs that
    log INFO + return safe defaults — S4-S6 fill them in.
- `tests/unit/replay/session_player_lifecycle_test.cpp`: 6 cases
  / 34 assertions:
  - default-constructed Idle state
  - openFile success populates counters + emits exactly one
    catalog registration
  - openFile rejects nonexistent file
  - closeFile idempotent + counters reset
  - opening a second file auto-closes the first
  - destructor cleans up while file is still open

### Build / test counts

- Debug + Release + debug-asan all build clean.
- ctest: Debug **558/558** + Release **558/558** (+6 from S2:
  the 6 new lifecycle cases). All M10 round-trip + S2 streaming
  tests still green (regression detector quiet).
- ASan local blocked by host /etc/ld.so.preload; CI authoritative.
- `clang-format -i` re-applied; dry-run -Werror clean.

### Deviations from plan

- Plan §S3 anticipated ~350 LOC; actual ~290 LOC. The dispatch
  loop carved out for S4 is the largest chunk; S3 lifecycle is
  pure plumbing. No spec deviation.
- Worker thread is created at openFile but not started —
  matches the plan ("QThread setup … no timing dispatch yet").
  S4 will connect the started() signal to a dispatch-loop slot.

S3 commit: pending push.

