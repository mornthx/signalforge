# M11 — Plan

Pairs with `.claude/M11-understanding.md`. Source of truth:
`docs/milestones/M11-replay-ux.md` at `d96bb5b`. CLAUDE.md governs.

---

## 0. Methodology

- One subtask = one logical commit (CLAUDE.md §Required #3 subject
  format `<module>: <imperative verb> <object>`, ≤ 72 char).
- Each commit carries `M<n>-progress.md` updates, build / test
  counts, and any spec deviation.
- Build gate before every code commit: Debug + Release + debug-asan
  build clean; ctest Debug + Release green; clang-format dry-run
  clean. Debug-asan local execution may be blocked by host
  `/etc/ld.so.preload`; CI is the authoritative ASan gate (recorded
  in memory).
- Documentation-only commits use the CLAUDE.md §Required #2
  exception ("non-code-only" → no rebuild required).
- HALT triggers in §3 fire **immediately** when their watermark is
  crossed; no recovery attempts beyond the documented limits.
- After every git push, watch CI (`gh run watch`); proceed to next
  subtask only after green.

## 1. Subtask sequence

| ID | Title | Net LOC est. | Output | Notes |
|---|---|---:|---|---|
| S0 | M11-concerns.md (C1-C6) + ADR-008 stub | ~150 | docs | If C1.α holds (additive SessionReader extension), no ADR needed; this slot reserves space if perf escapes the gate at S10 |
| S1 | Module scaffold + freeze headers + CMake | ~300 | code | `src/replay/CMakeLists.txt`, `playback_controller.hpp`, `session_player.hpp`, `replay_mode_manager.hpp`, empty `.cpp`s |
| S2 | SessionReader streaming + seek extension | ~400 | code + tests | Adds `readNextRecord` / `seekToTimestamp` / `currentRecordIndex` / `currentTimestampNs` / `atEnd` / `ReplayRecord` struct. Backward-compat: `replayAll` becomes a thin loop |
| S3 | SessionPlayer skeleton + worker thread + load/close | ~350 | code + tests | `openFile` / `closeFile` lifecycle, `QThread` worker, no timing yet |
| S4 | SessionPlayer timing dispatch (1×) | ~200 | code + tests | `sleep_for`-based loop; 1× delivery; no speed scaling |
| S5 | SessionPlayer pause / resume / step / speed | ~250 | code + tests | Atomic `playing_` + `speedFactor_`; step forward; 30 Hz position throttling |
| S6 | SessionPlayer seek + step backward | ~250 | code + tests | Sequential scan via S2 API; backward step = `seek(currentTimestampNs - 1) → stepForward` |
| S7 | PlaybackController state machine + Qt signals | ~250 | code + tests | Idle → Loaded → Playing ↔ Paused → Ended / Error; signal fan-out from SessionPlayer |
| S8 | ReplayModeManager + Live↔Replay transitions | ~300 | code + tests | Connection snapshot/restore; mode dialogs (QMessageBox); buffer clear hook |
| S9 | MainWindow integration | ~400 | code | File → Open Session…; replay toolbar; status-bar wiring; C6 mode-gating; rebuild chart container on mode switch |
| S10 | Bench + 60 k-record fixture + perf gate | ~250 | code + bench | `bench_replay.cpp`; M11-baseline.md; runs §5.1 + §5.2 metrics |
| S11 | Integration tests (7 per spec §2.1-12) | ~600 | tests | See §4.S11 below for the 7-test mapping |
| S12 | M11-done.md + freeze records + hardware-verification protocol | ~400 | docs | Mirrors M10-done.md shape; sha256 of 3 frozen headers; 6-test manual protocol |

Subtask LOC totals to ~3 850 net. CLAUDE.md §Required #4 caps any
PR at ≤ 800 net lines added — M11 will land as a sequence of
12 commits to a single milestone branch, merged to main as a
single PR (the M2-M10 pattern). The 800-line cap applies per merge
to main; one PR for the whole branch is the M9 / M10 precedent and
matches the §G milestone closure flow in CLAUDE.md.

## 2. Time budget

Spec target 5-7 person-days. Aggressive estimate aligned with the
M10 actual (the M10 plan budgeted 4-6 days; actual was ≈ 90 min
for 12 subtasks owing to the additive nature of M10 on top of
M9). M11's bigger surface (UI rebuild + worker timing) may take
proportionally longer.

## 3. HALT triggers (M11-specific, on top of CLAUDE.md §HALT)

Each trigger has a measurement point and an immediate action.

| # | Trigger | Measurement point | Action |
|---|---|---|---|
| H1 | Modification to any M2-M10 frozen `.hpp` (SessionReader excluded — see C1) | `git diff` against M2-M10 freeze list before every commit | HALT; revert; document |
| H2 | Replay timing error > 20 % at 1× over 10 s | `bench_replay --realtime 10` (S10) | HALT; investigate `sleep_for` vs `QTimer`; if `QTimer` also misses, HALT and report |
| H3 | Memory growth > 10 % across full file replay | `bench_replay --memory-soak 1800` (S10 / hand-off) | HALT; profile leak; do not paper over with periodic free |
| H4 | M10 SessionReader extension cannot meet seek < 500 ms on 600 k-record file | `bench_replay --seek-test` (S10) | HALT; propose indexed-seek hotfix in `M11-concerns.md` (parallel to M6 ADR-005) |
| H5 | Live ↔ Replay leaves charts in inconsistent state | Integration test (S11) | HALT; investigate registry clear path |
| H6 | Step backward state mismatch | Integration test (S11) — same record index → identical signal value | HALT; verify catalog-extension replay on backward seek |
| H7 | Seek to invalid timestamp crashes / hangs | Fuzz integration test (S11) | HALT; clamp boundaries |

Plus CLAUDE.md standard set (compile error 3×, test fail 3×,
new dep, frozen-interface mod, perf miss after 1 opt pass,
spec/arch contradiction, Qt 6.10 anomaly, two plausible impls,
unexplained git failure).

## 4. Subtask details

### S0 — Concerns + optional ADR-008

**Inputs**: spec §3, §4, §7, §9 Notes; this plan §3; M10-done.md
freeze table.

**Deliverables**:
- `.claude/M11-concerns.md` with C1-C6 spelled out, including
  C1's α / β interpretation comparison, C4's measurement plan, and
  C6's gating policy.
- (Conditional) `docs/architecture/decisions/ADR-008-replay-ux.md`
  — only authored if S2 perf forces interpretation **β**, or if any
  spec contradiction surfaces during S1-S2 implementation. Default
  is no ADR.

**Build / test**: docs-only. CLAUDE.md exception applies.

**Done when**: M11-concerns.md committed; if no ADR needed, S0
explicitly states "no architectural divergence requiring ADR-008
at this point."

### S1 — Module scaffold + freeze headers

**Deliverables**:
- `src/replay/CMakeLists.txt` (new static lib `signalforge_replay`,
  PUBLIC links: Qt6::Core / Qt6::Widgets / `signalforge_session` /
  `signalforge_buffer` / `signalforge_decoder` / `signalforge_connection`
  / `signalforge_observability`; AUTOMOC ON).
- `playback_controller.hpp` (frozen at M11 close): API per spec
  §4.1; Doxygen-complete; `Q_DISABLE_COPY_MOVE`.
- `session_player.hpp` (frozen at M11 close): API per spec §4.2,
  with corrected include path `decode/decoder_interface.hpp` (per
  C2). Construct with `signalforge::decoder::SignalValueSink&`.
- `replay_mode_manager.hpp` (frozen at M11 close): API per spec
  §4.3.
- Stub `.cpp` files that compile (each method body comments which S
  fills it in: S3-S6 for SessionPlayer, S7 for PlaybackController,
  S8 for ReplayModeManager).
- `src/CMakeLists.txt` adds the subdirectory.
- `tests/unit/replay/CMakeLists.txt` + smoke test (state-enum
  default values, default-constructible types).

**Build gate**: Debug + Release + debug-asan all build; ctest
unchanged count + 1 smoke target.

### S2 — SessionReader streaming + seek extension (C1.α)

**Deliverables** (additive on `src/session/session_reader.{hpp,cpp}`):
- Public `struct ReplayRecord { int64_t timestampNs; QString
  signalId; SignalValue value; };`.
- `bool readNextRecord(ReplayRecord& out)` — pulls one signal-value
  record. Catalog-extension records dispatched internally to the
  *currently-bound* sink (set via `bindCatalogSink(sink*)`); skipped
  records (heartbeat / marker / unknown) consumed transparently.
  Returns `false` at EOF or on parse error.
- `bool seekToTimestamp(int64_t targetNs)` — sequential scan; for
  backward seek, re-opens file to header end and scans forward.
  Replays the catalog into the bound sink as it walks.
- `std::size_t currentRecordIndex() const noexcept`,
  `int64_t currentTimestampNs() const noexcept`,
  `bool atEnd() const noexcept`.
- `replayAll(sink)` reimplemented as a loop calling
  `readNextRecord` (preserves M10 round-trip semantics).
- `bindCatalogSink(SignalValueSink*)` so streaming callers can
  receive `onSignalsRegistered` callbacks for in-stream catalog
  extensions.

**Tests** (new in `tests/unit/session/session_reader_streaming_test.cpp`):
- 6+ cases: streaming returns same events as `replayAll`; seek
  forward to mid-file timestamp; seek backward; seek past EOF
  clamps to last record; `currentRecordIndex` / `atEnd` tracking;
  catalog-extension callback fires on streaming seek across the
  extension boundary.

**Build gate**: Debug + Release green; existing M10 round-trip
tests must still pass (regression gate); +6 unit cases.

### S3 — SessionPlayer skeleton + lifecycle

**Deliverables**:
- `src/replay/session_player.cpp`:
  - `openFile(QString)` instantiates `SessionReader`, calls `open`,
    captures `metadata` + `durationNs` (last record timestamp from
    a quick scan, or from header if S2 adds a duration shortcut),
    binds catalog sink → owned sink reference.
  - `closeFile()` cleanly stops worker, `join`s thread, resets
    state.
  - `QThread` setup using the M10 `SessionFileWriter` shape:
    `moveToThread`, lambda-bound start signal, queued shutdown.
  - Counter atomics: `playing_`, `speedFactor_`,
    `currentPosNs_`, `currentRecordIdx_`, `totalRecords_`,
    `durationNs_`.
  - **No** timing dispatch yet — `play()` still a no-op.

**Tests**: 4 cases — open success, open invalid file, close
idempotent, destructor joins worker if file open.

### S4 — Timing dispatch at 1×

**Deliverables**:
- Worker loop per spec §4.4 (corrected for C2 include path):
  - Reads next record via `SessionReader::readNextRecord`.
  - `recordTimeDelta = record.timestampNs - previousTimestampNs_`
    (first record uses `0`).
  - `scaledDelta = recordTimeDelta / speedFactor_.load()` (S5
    enables non-1× speed; S4 only honours 1×).
  - `std::this_thread::sleep_for(std::chrono::nanoseconds(scaledDelta))`.
  - Dispatch `onSignal(now, signalId, value)` on the *main thread*
    via `QMetaObject::invokeMethod(sink_, ..., Qt::QueuedConnection)`.
  - Increments counters; emits `recordDispatched` from the worker
    thread (queued connection delivers on main).
- `play()` flips `playing_` → starts worker; `pause()` flips
  flag → worker exits the loop on next iteration.

**Tests**: 3 cases — 1× delivery preserves order; pause stops
delivery; resume from pause continues from same position.

### S5 — Speed scaling + step forward + 30 Hz throttle

**Deliverables**:
- `setSpeed(double)` — clamps to spec §3.3 set; stored atomically;
  takes effect on the next worker iteration. Logs WARN on clamp.
- `stepForward()` — synchronous one-record dispatch through the
  worker's read path (no sleep). Returns false at EOF.
- 30 Hz position throttle — `QDeadlineTimer` cooldown of 33 ms in
  the worker loop; emits skipped while cooldown active; the *last
  skipped* position is captured and emitted on the next boundary
  (so UI never falls behind by > 1 frame).

**Tests**: 4 cases — speed change mid-replay applies on next
record; clamping out-of-range speed; step at end-of-file returns
false; position-emit rate ≤ 30 Hz under sustained dispatch.

### S6 — Seek + step backward

**Deliverables**:
- `seek(int64_t)` — pause if playing → call
  `SessionReader::seekToTimestamp` → update counters → resume if
  was playing.
- `stepBackward()` — `seek(currentTimestampNs - 1)` then
  `stepForward()`. Documented as O(file) for V1.
- Integration with M11.X HALT trigger #4: the seek implementation
  measures wall-clock ms in the bench (S10).

**Tests**: 4 cases — seek to mid-file; seek to before-start →
clamp to start; seek to after-end → clamp to end; backward step
re-reads correctly.

### S7 — PlaybackController orchestration

**Deliverables**:
- `playback_controller.cpp`:
  - State machine per spec §3 diagram. Transitions:
    `Idle → Loaded` on `loadSession` success;
    `Loaded → Playing` on `play`;
    `Playing → Paused` on `pause`;
    `Paused → Playing` on `play`;
    `Playing → Ended` on SessionPlayer's `endReached`;
    any → `Error` on SessionPlayer's `error`;
    any → `Idle` on `closeSession`.
  - Wraps each control method with state-validity check (returns
    false on illegal transitions; logs WARN).
  - Hooks SessionPlayer's signals to its own (re-emits with
    `Qt::AutoConnection`).
  - Owns a `lastError_` QString for diagnostics.

**Tests**: 5 cases — full happy-path lifecycle; play in Idle
rejected; pause in Loaded rejected; load while in Playing
auto-closes prior; error state sticky until close.

### S8 — ReplayModeManager + Live↔Replay transitions

**Deliverables**:
- `replay_mode_manager.cpp`:
  - `enterReplay()` per spec §4.8; uses `ConnectionManager::
    connectionIds()` + `connection(id)->state()` to snapshot
    Connected IDs; per-ID `disconnectConnection`. Confirmation
    dialog only when ≥ 1 connection currently Connected.
  - `exitReplay()` — dialog with three options: Resume / Stay
    Disconnected / Cancel. On Resume, iterate stored IDs and call
    `connectConnection(id)`.
  - `clearChartBuffers()` hook — calls
    `SignalBufferRegistry::clear()` (or per-signal clear; if no
    public clear exists, surface as M11.X concern and propose
    additive method on the *registry* — which is also not in the
    M6 freeze surface; verify in S8).
  - Emits `modeChanged`, `connectionsPaused`, `connectionsRestored`
    Qt signals.

**Tests**: 4 cases — enter Replay with no connections (no
dialog); enter Replay with 1 + connection (dialog Yes path);
enter Replay cancel; exit Replay all 3 dialog branches.

### S9 — MainWindow integration

**Deliverables**:
- File menu: **Open Session…** action (Ctrl+O). Slot:
  `QFileDialog::getOpenFileName` with `*.sfreplay` filter →
  `replayMode_->enterReplay()` → on success, `playbackCtrl_->
  loadSession(path)`.
- Replay toolbar (`QToolBar*`): Play/Pause toggle, Step ◀/▶,
  timeline scrubber (`QSlider` 0..totalRecords; `valueChanged` →
  `seek(timestampOfIndex(value))`), speed combo (5 entries),
  Exit Replay button. Toolbar `setVisible(replayMode_->
  currentMode() == AppMode::Replay)`.
- Status bar: append `Replay: <filename> | <pos> / <duration> |
  <idx> / <total>` while in Replay mode; restore live status on
  exit.
- M9 connection controls disabled while in Replay (toolbar /
  context menus).
- C6 gating: **Open Session…** disabled while
  `sessionWriter_->isRecording()`. **Record…** disabled while
  `replayMode_->currentMode() == AppMode::Replay`.

**Tests**: covered by S11 integration tests (no new unit suite —
MainWindow tests are integration-shape).

### S10 — Benchmark + perf gate

**Deliverables**:
- `tests/benchmark/bench_replay.cpp`:
  - Generates a 60 k-record fixture file (or uses the M10
    soak-fixture if compatible).
  - Modes: `--realtime <seconds>` (1×), `--fast <factor>` (10×),
    `--seek-test`, `--memory-soak <seconds>`.
  - Internal acceptance gates per spec §5.1 / §5.2 → exit
    non-zero on miss.
- `tests/benchmark/CMakeLists.txt` appends target; opt-in via
  `-DSIGNALFORGE_BENCHMARKS=ON`.
- `tests/benchmark/results/M11-baseline.md` records all four
  metrics with verdict + headroom analysis.
- HALT trigger #2 / #3 / #4 fire on this subtask if gates miss.

### S11 — Integration tests (spec §2.1-12 — 7 tests)

| Spec test | File |
|---|---|
| `test_playback_controller_lifecycle` | `tests/integration/test_playback_controller_lifecycle.cpp` |
| `test_session_player_timing` | `tests/integration/test_session_player_timing.cpp` |
| `test_session_player_seek` | `tests/integration/test_session_player_seek.cpp` |
| `test_replay_mode_manager` | `tests/integration/test_replay_mode_manager.cpp` |
| `test_replay_charts_integration` | `tests/integration/test_replay_charts_integration.cpp` |
| `test_session_player_speed_change` | `tests/integration/test_session_player_speed_change.cpp` |
| `test_session_player_truncated` | `tests/integration/test_session_player_truncated.cpp` |

7 *named* test files per the spec — no hybrid mapping (M11 has
the latitude to deliver them as named; M10's hybrid was a
pragmatic compromise around a tighter S7 budget). Each file ≥ 1
case; some (lifecycle, mode-manager) will carry several.

### S12 — Closeout

**Deliverables**:
- `.claude/M11-done.md` mirroring M10-done.md shape:
  - Spec §2.1 deliverable checklist (all ✅);
  - PR + merge state placeholders;
  - Freeze sha256s for `playback_controller.hpp`,
    `session_player.hpp`, `replay_mode_manager.hpp`;
  - Acceptance self-check vs spec §8;
  - Test-count matrix;
  - HALT-trigger disposition (which fired / which did not);
  - Hand-off to M12 (Performance) + M13 (Packaging);
  - V1.5+ / V2 deferred items.
- `docs/m11-hardware-verification.md` — 6-test manual protocol
  mirroring M9 / M10 protocols (open `.sfreplay`, play, pause,
  step, seek, exit Replay).

## 5. Risk register + mitigation

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| `sleep_for` jitter at 10× speed | Medium | Timing > 5 % HALT | C4 fallback to QTimer; bench measures 3-run variance |
| Backward seek on 600 k-record file blows the < 500 ms gate | Medium | HALT trigger #4 | C1 documents indexed-seek as the M10 hotfix path |
| Registry clear API absent | Low-Medium | S8 stuck | If absent, propose additive clear method on registry (not in M6 freeze) |
| Mode-transition race when SessionWriter and PlaybackController both touch TeeSink | Low | HALT trigger #5 | Serialize mode transitions on main thread (Qt::DirectConnection inside main, queued from worker) |
| QFileDialog I/O hangs UI on slow disks | Low | UX | Show busy cursor on `loadSession`; SessionPlayer worker thread does the parse |
| Coverage < 80 % on small modes (e.g., Error state edges) | Medium | Spec miss | S11 covers `errorOccurred` paths via deliberately-corrupted fixtures |
| Existing M10 round-trip test breaks after S2 SessionReader refactor | Medium | Regression | S2 lands the streaming API as a thin layer with `replayAll` reimplemented to loop over it; round-trip test runs unchanged |

## 6. V1.5+ / V2 deferred items (mirror M11 §2.2)

V1.5+:
- Multi-file playlist / file diff / playlist UI
- Per-chart replay mode (vs global)
- Replay editing / annotations
- Bookmark UI for fast backward seek
- Continuous speed slider (Option Q)
- Keyboard shortcuts (space / arrows)

V2:
- Network / remote replay
- Multi-instance replay sync
- Side-by-side live vs replay comparison view (Option T from
  earlier M11.1 deliberation)

## 7. Closeout checklist

- [ ] All S0-S12 commits landed on `milestone/M11`
- [ ] CI green on every commit
- [ ] PR opened to `main`; CI green on PR
- [ ] M11-done.md published with PR # / head SHA / freeze sha256s
- [ ] Manual hardware verification protocol authored
- [ ] Phase 1 step 6 announce: "M11 ready. Awaiting approval to
      merge M11 and begin M12 bootstrap"
- [ ] Phase 2 follow-ups from M10 (30-min memory soak; combined
      hardware verification 12-test) explicitly carried in M11-done.md
      hand-off, **not** absorbed into M11

---

## 8. What I am NOT planning to do

- Modify any M2-M10 frozen `.hpp`. Only `SessionReader` (M10
  non-frozen) is extended additively (C1.α).
- Build a parallel SFREPLAY parser. Single source of truth in M10.
- Add a new top-level dependency. M11 uses Qt + existing in-tree
  modules.
- Change M8 chart code. Charts are agnostic to mode; data flow is
  registry → chart unchanged.
- Author ADR-008 unless C1.β becomes necessary or another
  contradiction surfaces.
- Refactor M10 SessionWriter / TeeSink / SFREPLAY format.

## 9. Phase 4 / 5 expectations

After this plan is approved (Phase 4 — "approved, execute M11"),
S0 begins immediately. Each subtask reports start + close in
`M11-progress.md`; CI watches happen *between* subtasks (matches
M10 cadence). At S12 Phase 1 step 6 fires.
