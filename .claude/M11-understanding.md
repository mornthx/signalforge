# M11 — Understanding

Source of truth: `docs/milestones/M11-replay-ux.md` (831 lines, merged
to `main` at `d96bb5b` via PR #19). Architectural prereqs: ADR-007
(M10 SFREPLAY v1 format pivot); M2 `DriverInterface`; M5
`SignalValueSink` / `SignalValue` / `SignalMetadata`; M6
`SignalBufferRegistry`; M8 `ChartManager`; M9 `ConnectionManager`;
M10 `SessionReader` / `SessionMetadata` / `TeeSignalValueSink`.

Cross-reference notation matches the spec (`[M11 §X]`, `[M10 §Y]`,
`[ADR-N]`, `[CM §Z]`).

---

## 1. Goal in one paragraph

Visualise SFREPLAY v1 session files as replay UX in the same chart
UI used for live data. After M11, the user can pick **File → Open
Session…**, watch the recording play through the existing M8 charts
at original (or scaled) timing, and use play / pause / step / seek /
speed controls to navigate. Live and Replay modes are mutually
exclusive and globally toggled. M11 freezes the playback control
layer (`PlaybackController`, `SessionPlayer`, `ReplayModeManager`)
and the replay state machine.

## 2. What ships (per spec §2.1)

1. `signalforge::replay::PlaybackController` (QObject, frozen) — top-level
   replay orchestrator. Owns one `SessionPlayer`. State machine
   `Idle → Loaded → Playing ↔ Paused → Ended` (or `Error`).
2. `signalforge::replay::SessionPlayer` (frozen) — wraps M10
   `SessionReader`; QThread worker dispatches records to a
   `SignalValueSink` at the configured speed. Owns play / pause / step /
   seek / speed control.
3. Replay state machine + `PlaybackState` enum (Idle / Loaded / Playing /
   Paused / Seeking / Ended / Error).
4. `signalforge::replay::ReplayModeManager` (frozen) + `AppMode` enum
   (Live / Replay). Pauses M9 connections on entry; restores on exit
   per user dialog.
5. MainWindow integration: **File → Open Session…** (Ctrl+O); replay
   toolbar (Play/Pause, step ±, timeline scrubber, speed combo,
   Exit Replay) shown only in Replay mode; status bar shows
   replay position; M9 connect controls disabled in Replay mode.
6. Discrete speed multipliers: 0.5× / 1× / 2× / 5× / 10× — change
   applies on the next dispatched record.
7. Single-file-at-a-time replay (loading a new file closes the
   current one).
8. Live ↔ Replay confirmation dialogs with connection-state
   preservation.
9. Position tracking — `currentPosition()` → `(timestampNs,
   recordIndex)`; `positionChanged` Qt signal rate-limited to ≤ 30 Hz.
10. Error handling: bad file → MessageBox + `Error` state; mid-stream
    corruption → `Error` state with partial replay preserved; invalid
    seek → clamp + WARN; backward seek + step supported (re-read from
    start).
11. Seven integration tests at `tests/integration/`; ≥ 80 % unit-test
    coverage on replay modules.
12. Bench at `tests/benchmark/bench_replay.cpp` with results in
    `tests/benchmark/results/M11-baseline.md`.
13. Doxygen on every public declaration; `.claude/M11-done.md` with
    freeze record (3 sha256s).

## 3. Out of scope (per spec §2.2)

- Modifications to any M2-M10 *frozen* `.hpp` (M10 SessionReader is
  *not* frozen — see §5.C1).
- Multi-file playlist; per-chart replay; replay editing /
  annotations / record-into-replay; format conversion; remote /
  network replay; multi-instance replay sync; new top-level
  dependencies; QML for replay UI; backward time-axis convention
  change; scripted replay schedules.

## 4. Locked design decisions (spec §3)

| ID | Decision | Implication |
|---|---|---|
| M11.1 B | Same-window replay UI | M8 charts reused; M11 swaps the data feeder, not the UI |
| M11.2 X | Full play / pause / step / seek / speed | Standard media-player controls; HALT-trigger #6 / #7 cover step + seek |
| M11.3 P | Discrete speed combo (0.5× / 1× / 2× / 5× / 10×) | No analog slider in V1; speed is `double` for ABI flexibility |
| M11.4 S | Global Live/Replay toggle (per-app, not per-chart) | `ReplayModeManager` orchestrates; charts unaware of mode |
| M11.5 V | Single file replay | Loading new file closes current; playlist V1.5+ |
| M11.6 | Playback uses M10 `SessionReader` directly | No format-specific code in M11 |
| M11.7 | No soft-HALT (inherits M2-M10) | Same as prior milestones |
| M11.8 | Metric naming locked at spec §3.8 | `replay_state`, `replay_position_ns`, `replay_records_dispatched_total`, `replay_speed_factor`, `replay_seeks_total`, `replay_mode_transitions_total` |

## 5. Concerns surfaced (recorded canonically in `.claude/M11-concerns.md`)

### C1 — M10 `SessionReader` API gap is real

The M11 spec sketch in §4.4 / §4.5 references the following on
`SessionReader`:

- `reader_->readNext()` → an opaque per-record struct with
  `timestampNs`, `signalId`, `value`
- `reader_->seekToTimestamp(int64_t ns)`
- `reader_->currentRecordIndex()`

**Actual M10 surface** (`src/session/session_reader.hpp`):

- `open(filePath)` → `bool`
- `replayAll(SignalValueSink&)` → `bool` (fire-and-forget, full file)
- `close()`, `isOpen()`, `metadata()`, `recordsRead()`, `fileComplete()`

There is **no** record-by-record streaming, **no** seek, **no**
current-record accessor. The spec's note (§4.5, §9 Notes #1)
acknowledges this gap and recommends "build on top via sequential
read (slower) → no M10 change".

**However** — `session_reader.hpp` lines 33-36 explicitly state the
class is *not* part of the M10 freeze surface and "M11 may extend
the public API additively". M10-done.md confirms this: only
`SessionWriter`, `SessionMetadata`, `RecordingState`, and the
SFREPLAY v1 binary format are frozen. SessionReader is open territory.

**Resolution proposal** (interpretation **α**, default): extend
SessionReader **additively** with the streaming / seek API M11
needs. No ADR required because SessionReader is explicitly outside
M10's freeze. New methods:

- `readNextRecord(out: ReplayRecord&)` → `bool` — pulls one record;
  dispatches catalog-extension records internally (so callers see
  only signal-value records).
- `seekToTimestamp(int64_t ns)` → `bool` — sequential scan from the
  current or earlier position (re-opens file on backward seek).
- `currentRecordIndex()` → `std::size_t`
- `currentTimestampNs()` → `std::int64_t`
- `atEnd()` → `bool`
- `replayAll(...)` retained as a thin loop over the streaming API
  (preserves M10 round-trip test and `sfreplay_inspect` flow).

A new public struct `ReplayRecord { int64_t timestampNs; QString
signalId; SignalValue value; }` is the per-record return type.

**Interpretation **β** (rejected unless perf forces it)**: build a
parallel reader from scratch in `src/replay/`, leave SessionReader
untouched. Adds ~600 LOC of duplicated parsing; prone to drift
against `docs/format/sfreplay-v1.md`. Costs more than the additive
approach.

**Plan target**: Sub-task **S2** delivers the SessionReader
extension. If S2 perf gates miss the §5.1 < 500 ms seek target on a
600 k-record file (HALT trigger #4), file an M11 concern + propose
an indexed-seek hotfix to SessionReader (parallel to M6 ADR-005's
post-freeze indexed-seek pattern).

### C2 — Header include-path typos in spec

Spec §4.2 shows `#include "decoder/decoder_interface.hpp"` for
SessionPlayer; the actual path is `decode/decoder_interface.hpp`
(singular "decode"). Spec §4.1 references `signalforge::buffer::
SignalBufferRegistry` — header at `src/buffer/signal_buffer_registry.hpp`
(consistent). Spec §4.3 references `connection/connection_manager.hpp`
— actual is also under `src/connection/`. Of these only the
`decoder/` typo affects code; treated as spec sketch artefact, not
a deviation. **Resolution**: implementation uses the actual paths;
no spec amendment required.

### C3 — M9 `ConnectionManager` has no pause/resume

Spec §3.4 / §4.8 describe "pause connections" semantics. M9 only
exposes:

- `disconnectConnection(id)` / `connectConnection(id)`
- `disconnectAll()` / `connectAll()`
- `connectionIds()` + `connection(id)->state()`

**Resolution**: `ReplayModeManager::enterReplay` snapshots the IDs
of connections currently in `Connected` state, calls
`disconnectConnection(id)` on each, and stores the snapshot.
`exitReplay` (if user chose Resume) calls `connectConnection(id)`
on each stored ID. Equivalent to pause/resume from the user's
viewpoint; no M9 change needed. Documented as the M9 → M11
adapter shape; if performance / UX demands a true pause API later,
that is a V1.5+ addition.

### C4 — Timing precision under `std::this_thread::sleep_for`

Spec §4.4 worker loop uses `std::this_thread::sleep_for(scaledDelta)`.
On Linux the typical scheduling granularity is ~50 µs (or worse
under load), which is fine at 1× speed for record cadences ≥ 1 ms,
but may visibly drift at 10× when records arrive sub-100 µs apart
in the file.

**Resolution**: Implement S5 (timing) with `sleep_for` first, then
measure under bench (S10). If 1× speed timing error > 5 % or 10×
completion drifts > 10 % (HALT trigger #2 watermark > 20 %), switch
the worker loop to a `QTimer`-based dispatch (single-shot timers
re-armed against an absolute deadline; mirrors the M10 S10 pacing
fix where `sleep_until` beat `sleep_for`). HALT only if `QTimer`
also misses.

### C5 — Position-emit rate limiter

Spec §2.1-10 says "rate-limited to ≤ 30 Hz" on `positionChanged`.
M11 implements with a `QDeadlineTimer` cooldown of `33 ms` in the
worker → main thread queued connection: emits skipped while
cooldown is active, but the *last* skipped position is always
captured on the next emit boundary so the UI never falls behind by
more than one frame.

### C6 — Mode transition while M10 SessionWriter is recording

Spec §9 Note 4 says "Mode transition during recording is undefined
… V1 should disable the menu item or show error." Neither
direction (`Record while Replaying`, `Replay while Recording`)
should be allowed in V1 — both leave the registry / TeeSink in
ambiguous state.

**Resolution**: MainWindow gates the **File → Open Session…** action
disabled while `sessionWriter_->isRecording()`. Symmetrically,
**Session → Record…** is disabled while
`replayMode_->currentMode() == AppMode::Replay`. Documented in
S7 (MainWindow integration).

## 6. Integration surfaces I will touch

| File | Status | Why I touch it |
|---|---|---|
| `src/session/session_reader.{hpp,cpp}` | **Not frozen** (M10-done.md §Freezes) | Add streaming API per C1.α |
| `src/app/main_window.{hpp,cpp}` | Live UX, not frozen | New File menu entry, replay toolbar, status bar |
| `src/replay/*` (new) | New module | All M11 freeze surfaces live here |
| `tests/unit/replay/*` (new) | New | Per spec §2.1-13 |
| `tests/integration/test_*.cpp` | Append | Spec §2.1-12: 7 integration tests |
| `tests/benchmark/bench_replay.cpp` | New | Spec §2.1-14 |
| `docs/architecture/decisions/ADR-008-*.md` | Maybe new | Only if S2 perf forces interpretation **β**; otherwise no ADR |

## 7. Definition of done (per spec §8 + CLAUDE.md)

A task is done when **all** of these hold (any miss → keep working
or HALT):

1. Code compiles cleanly under Debug / Release / debug-asan.
2. ctest green on Debug + Release; debug-asan green on CI.
3. Coverage ≥ 80 % on `signalforge::replay` public surface.
4. `clang-format -i` clean; `clang-tidy` no new warnings.
5. Doxygen on every public declaration in
   `playback_controller.hpp`, `session_player.hpp`,
   `replay_mode_manager.hpp`.
6. Spec §5 perf gates met (1× < 5 % timing error, 10× ≤ 1 ± 0.1 s,
   seek < 500 ms, step < 50 ms, full-replay memory growth < 10 %).
7. M11-done.md ships freeze sha256s for the 3 frozen headers.
8. M11-progress.md tracks every subtask start/close with build /
   test counts and deviations.
9. No M2-M10 freeze surface modified (M10 SessionReader explicitly
   excluded — see C1).
10. PR opened to `main`, CI green, awaiting Phase 2 approval.

## 8. Effort sketch

Spec target: 5-7 person-days. The bulk of M11 sits in
`SessionReader` extension (S2), `SessionPlayer` worker + timing
(S3-S5), and MainWindow rebuild for the replay toolbar (S7). The
control surface itself (`PlaybackController`, `ReplayModeManager`)
is straightforward QObject orchestration.

## 9. Phase 2 follow-ups inherited (from M10)

These are tracked but **non-blocking** for M11 progression:

1. M10 30-min memory soak (`bench_session_writer --soak 1800`).
   Operator-run; harness already in tree.
2. Combined M9 + M10 manual hardware verification (12 tests).
   Operator-run; protocols at `docs/m9-hardware-verification.md`
   and `docs/m10-hardware-verification.md`.

These are **hand-off line items**, not deliverables of M11 itself.
