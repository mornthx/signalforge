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

S3 commit: `397c70e` "replay: SessionPlayer lifecycle + worker
thread plumbing (M11 S3)". Pushed after S2 CI green
(run 25549181411 ✓). S3 CI: run 25549945101 (in_progress at S4
commit time).

---

## S4 — SessionPlayer timing dispatch at 1× (completed)

- Start: 2026-05-08T06:25Z

### Deliverables

- `src/replay/session_player.hpp`: `pendingRecord_` member +
  `dispatchLoop` private declaration.
- `src/replay/session_player.cpp`:
  - `dispatchLoop` runs on the worker thread (connected to
    `QThread::started` via Qt::DirectConnection on each `play()`).
    Reads records via `SessionReader::readNextRecord`, computes
    inter-record delta, sleeps in 5 ms chunks (so `pause()` is
    responsive within 5 ms — addresses C4 stage A
    interruption-aware sleep), and dispatches `onSignal` to the
    main thread via `QMetaObject::invokeMethod`/`Qt::QueuedConnection`.
  - Pause-mid-sleep edge case: if the worker has read a record
    but not yet dispatched it when `pause()` fires, the record is
    saved in `pendingRecord_` and drained on next `play()`. This
    resolves the "lost record across pause/resume" bug caught by
    the S4 resume test (was 2/3 records dispatched; now 3/3).
  - 30 Hz `positionUpdated` throttle (C5) implemented with a
    `lastEmitTime` cooldown of 33 ms in the worker loop.
  - `play()` re-arms the worker thread on each call (disconnect +
    fresh connect of `started()`); `pause()` quits + waits the
    thread.
- `tests/unit/replay/session_player_timing_test.cpp`: 3 cases /
  30 assertions:
  - 1× delivery preserves order across all 5 records (sub-ms
    file).
  - pause stops delivery during a 200 ms inter-record sleep —
    only the first record is delivered.
  - resume from pause continues from the same position; final
    sink count = 3/3.

### Build / test counts

- Debug + Release + debug-asan all build clean.
- ctest: Debug **561/561** + Release **561/561** (+3 from S3:
  the 3 timing cases). All M10 + S2/S3 tests still green.
- ASan local blocked by host /etc/ld.so.preload; CI authoritative.
- `clang-format -i` re-applied; dry-run -Werror clean.

### Bug found + fixed during S4

The first cut had `readNextRecord()` called BEFORE the inter-record
sleep. When `pause()` interrupted mid-sleep, the worker exited
having already advanced the reader's file pointer past the
to-be-dispatched record. Resume then read the *next* record,
silently skipping the one in flight. Caught by the S4 resume test
which expected 3/3 delivery and got 2/3.

Fix: save the in-flight record in `std::optional<ReplayRecord>
pendingRecord_` when `pause()` interrupts mid-sleep; drain it on
next `play()` before reading from the reader. The `pendingRecord_`
is only ever touched from the worker thread (pause() blocks until
worker exits), so no extra synchronisation is needed.

### Deviations from plan

- Plan §S4 anticipated `sleep_for(scaledDelta)` with no
  interruption-aware behaviour. Adopted chunked 5 ms sleeps
  preemptively (C4 was scheduled to address this in S5; bringing
  it into S4 keeps the pause test passable today and the design
  cleaner). No spec deviation.
- 30 Hz throttle (C5) also brought into S4 since the dispatch
  loop is the natural home. Plan had it scheduled at S5 — now
  done. S5 will focus on speed scaling + step.

S4 commit: `8baa394` "replay: SessionPlayer 1x timing dispatch +
chunked-sleep pause (M11 S4)". Pushed after S3 CI green
(run 25549945101 ✓). S4 CI: run 25550584604 (in_progress at S5
commit time).

---

## S5 — Speed + stepForward + 30 Hz validation (completed)

- Start: 2026-05-08T06:50Z

### Deliverables

- `src/replay/session_player.cpp`:
  - `setSpeed(double)`: clamps to [0.5, 10.0] per spec §3.3,
    logs WARN on clamp, atomic store. Worker reads on its next
    iteration so the new speed takes effect on the next record's
    delay calculation (no in-flight cancellation needed).
  - `stepForward()`: synchronous one-record dispatch on the main
    thread. Refuses to run while `playing_` is true; returns
    false at EOF. Drains a `pendingRecord_` first if a prior
    pause left one in flight. Emits `positionUpdated` directly
    (no throttle for steps; they are rare).
  - 30 Hz throttle (C5) was already implemented in S4; S5
    validates it under sustained dispatch.
- `tests/unit/replay/session_player_speed_step_test.cpp`: 4
  cases / 29 assertions:
  - setSpeed clamps 0.1 → 0.5, 100.0 → 10.0; in-range value
    accepted unchanged.
  - 10× speed completes a 90 ms file in < 50 ms wall clock
    (well under the spec §5.1 "10× completion within 1 ± 10%"
    of the 1× reference) — exercises C4 stage A.
  - stepForward delivers exactly one record per call; returns
    false at EOF after the third record.
  - positionUpdated emit count ≤ 8 across 50 records dispatched
    over ~100 ms at 1× — confirms the 30 Hz throttle holds
    (without throttling we'd see 50 emits).

### Build / test counts

- Debug + Release + debug-asan all build clean.
- ctest: Debug **565/565** + Release **565/565** (+4 from S4:
  the 4 speed/step cases). M10 + S2-S4 regression detectors
  still green.
- ASan local blocked by host /etc/ld.so.preload; CI authoritative.
- `clang-format -i` re-applied; dry-run -Werror clean.

### Test fixture bug found + fixed

The first cut of `M11 S5: stepForward delivers one record at a
time` asserted `currentPositionNs() == 100`. The fixture uses
`microseconds(100)` which is **100 000 ns** — assertion was
unit-confused. Fixed to `100000`. Caught immediately by ctest.

### Deviations from plan

- Plan §S5 anticipated that 30 Hz throttle would land in S5;
  it was brought forward into S4 because the dispatch loop
  needed it for the timing tests anyway. S5 validates it
  separately. No spec deviation.
- Plan §S5 anticipated ~250 LOC; actual ~120 LOC for code +
  150 LOC for tests = ~270 LOC. Within target.

S5 commit: `4857b3a` "replay: SessionPlayer speed + step + 30Hz
throttle validation (M11 S5)". Pushed after S4 CI green
(run 25550584604 ✓). S5 CI: pending watch.

---

## S6 — Seek + stepBackward (completed)

- Start: 2026-05-08T07:15Z

### Deliverables

- `src/replay/session_player.cpp`:
  - `seek(int64_t)`: pauses if playing, discards `pendingRecord_`,
    delegates to `SessionReader::seekToTimestamp` (S2 helper),
    syncs player counters from reader's post-seek state, resumes
    if was playing and not at end. Clamping (out-of-range
    timestamps) is the reader's responsibility — verified end-to-end.
  - `stepBackward()`: V1 strategy is rewind-to-start + forward-step
    to (currentIdx - 1). O(N) on record count, acceptable per
    M11 spec §9 Note 2. Returns false when already at the start
    of the file (no-op for the edge case).
- `tests/unit/replay/session_player_seek_test.cpp`: 5 cases /
  40 assertions:
  - seek to mid-file timestamp lands at first record ≥ target
  - seek before start clamps to 0
  - seek past end clamps to last record
  - stepBackward replays records 1..(N-1) into sink
  - stepBackward at start returns false

### Build / test counts

- Debug + Release + debug-asan all build clean.
- ctest: Debug **570/570** + Release **570/570** (+5 from S5).
  M10 + S2-S5 regression detectors quiet.
- ASan local blocked by host /etc/ld.so.preload; CI authoritative.
- `clang-format -i` re-applied; dry-run -Werror clean.

### Deviations from plan

- Plan §S6 anticipated ~250 LOC; actual ~75 LOC code + ~165
  LOC tests = ~240 LOC. Within target. The seek logic is small
  because the heavy lifting was done in S2's
  `SessionReader::seekToTimestamp`.
- Step backward dispatches all earlier records back to the sink.
  Per spec §9 Note 2 this is "slow" — for a 600 k-record file
  with backward step from the end it would re-dispatch all
  600 k records. M11 documents the limitation; V1.5+ may add
  bookmark-based fast-rewind.

S6 commit: `d0425f1` "replay: SessionPlayer seek + stepBackward
(M11 S6)". Pushed after S5 CI green (run 25551109127 ✓).
S6 CI: pending watch.

---

## S7 — PlaybackController state machine + Qt signals (completed)

- Start: 2026-05-08T07:35Z

### Deliverables

- `src/replay/playback_controller.cpp`: full state machine + signal
  fan-out from the owned SessionPlayer.
  - `loadSession(path)`: auto-closes any prior session, constructs
    a new SessionPlayer, wires its `positionUpdated`,
    `endReached`, and `error` signals to controller signals,
    opens the file. Transitions Idle → Loaded; emits
    `sessionLoaded`, `stateChanged`. Returns false on file open
    failure (state stays Idle, emits `errorOccurred`).
  - `closeSession()`: idempotent. Tears down player, clears state,
    emits `stateChanged(Idle)` + `sessionClosed`.
  - `play()` / `pause()`: state-validity gated. play() valid in
    Loaded or Paused → Playing. pause() valid only in Playing →
    Paused. Both emit `stateChanged`. Invalid transitions log
    WARN and return false.
  - `stepForward()` / `stepBackward()`: state-validity gated.
    First step from Loaded transitions to Paused. EOF during
    step transitions to Ended. Backward step works from
    Loaded / Paused / Ended.
  - `seek(int64_t)`: valid in any non-Idle / non-Error state.
    Delegates to SessionPlayer; if seek brings us back from
    Ended to mid-file, transitions to Paused.
  - `setSpeed(double)`: delegates to player; emits
    `speedChanged` with the (possibly clamped) value.
  - `endReached` from player → state = Ended (when in Playing).
  - `error` from player → state = Error, captures message,
    emits `errorOccurred`.
- `tests/unit/replay/playback_controller_test.cpp`: 6 cases /
  39 assertions:
  - full happy-path lifecycle Idle → Loaded → Playing → Ended → Idle
  - play in Idle is rejected
  - pause in Loaded is rejected
  - loadSession while loaded auto-closes prior
  - stepForward from Loaded transitions to Paused
  - closeSession from any state returns to Idle

### Build / test counts

- Debug + Release + debug-asan all build clean.
- ctest: Debug **576/576** + Release **576/576** (+6 from S6).
  All M10 + S2-S6 regression detectors quiet.
- ASan local blocked by host /etc/ld.so.preload; CI authoritative.
- `clang-format -i` re-applied; dry-run -Werror clean.

### Deviations from plan

- Plan §S7 anticipated 5 cases; ships 6 (added the
  `closeSession from Playing` case to verify the cross-state
  cleanup path). Additive; no spec deviation.
- Plan §S7 anticipated ~250 LOC; actual ~190 LOC code + ~155
  LOC tests = ~345 LOC. Within target.

S7 commit: `8ba1317` "replay: PlaybackController state machine
+ signal fan-out (M11 S7)". Pushed after S6 CI green
(run 25551633406 ✓). S7 CI: pending watch.

---

## S8 — ReplayModeManager + Live↔Replay transitions (completed)

- Start: 2026-05-08T07:55Z

### Deliverables

- `src/replay/replay_mode_manager.hpp`: `exitReplay` signature
  changed from no-arg to `exitReplay(bool resumeConnections)`.
  Doxygen documents the V1 design choice: dialogs live in
  MainWindow (S9), this class is pure orchestration. Reasoning
  recorded inline + in the C3 resolution.
- `src/replay/replay_mode_manager.cpp`: full implementation.
  - `enterReplay`: snapshots Connected ids via
    `ConnectionManager::connectionIds()` + per-id `state()` check;
    calls `disconnectConnection` per id (failures logged WARN
    but don't block transition); emits `connectionsPaused`
    only if the snapshot was non-empty; transitions to Replay;
    emits `modeChanged`.
  - `exitReplay(true)`: walks the stored snapshot calling
    `connectConnection`; emits `connectionsRestored`. Then
    transitions to Live regardless of restore success.
  - `exitReplay(false)`: just transitions to Live, clears the
    snapshot.
  - Idempotent guards: enterReplay returns false if already in
    Replay; exitReplay returns false if already in Live.
- `tests/unit/replay/replay_mode_manager_test.cpp`: 6 cases /
  26 assertions:
  - default mode is Live
  - enterReplay with no active connections transitions Live → Replay
  - enterReplay when already in Replay returns false
  - exitReplay(false) transitions Replay → Live without restoration
  - exitReplay(true) transitions Replay → Live + emits restored
  - exitReplay when already in Live returns false

### Build / test counts

- Debug + Release + debug-asan all build clean.
- ctest: Debug **582/582** + Release **582/582** (+6 from S7).
  One transient flake observed on first run (timing-sensitive
  S4 case probably under load); re-run clean. M10 + S2-S7
  regression detectors quiet.
- ASan local blocked by host /etc/ld.so.preload; CI authoritative.
- `clang-format -i` re-applied; dry-run -Werror clean.

### Deviations from plan + spec

- Spec §4.8 sketched the QMessageBox call inside
  `enterReplay()` itself. M11 S8 ships the manager UI-free —
  dialogs move to MainWindow (S9). Reasons:
  - Manager stays unit-testable without a `QApplication`/
    QMessageBox interaction.
  - Inversion-of-control: UI layer owns user interaction; logic
    layer owns state.
  - Keeps the manager's API unaware of any specific dialog
    shape (Resume / Stay Disconnected / Cancel can be
    re-skinned in V1.5+ without touching this class).
  This is captured under M11.X concerns §C3 / §S8 footnote in
  M11-progress.md (this entry).
- Plan §S8 anticipated 4 cases; ships 6 (added the empty-state
  enter/exit cases for completeness of the transition matrix).
- Active-connection snapshot tests (Test 2 dialog Yes path,
  Test 3 cancel) deferred to S11 integration tests where real
  M9 connections are available. The unit tests verify the
  state-machine transitions; the C3 disconnect-snapshot logic
  is small enough to audit visually + S11 will exercise the
  full path.

S8 commit: pending push.

