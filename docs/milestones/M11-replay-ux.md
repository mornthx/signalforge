# M11 — Replay UX

| Field | Value |
|---|---|
| Milestone ID | M11 |
| Sprint | 11 |
| Estimated effort | 5-7 person-days |
| Prerequisites | M10 closed (main at v0.0.11-alpha.1 or later) |
| Next milestone | M12 (Performance) |
| Hard-stop type | **Interface freeze** (`PlaybackController` API + `SessionPlayer` API + replay state machine) + **Functional equivalence** (replay reproduces signal stream identically to original recording timing) |
| Soft-HALT allowed | **No** |
| Branch | `milestone/M11` |

**Cross-reference notation**:

- `[EM §N]` — Execution Manual, section N
- `[Arch §N]` — Architecture document, section N
- `[MR]` — Milestone Roadmap
- `[CM §X]` — CLAUDE.md, section X
- `[ADR-N]` — Architecture Decision Record N
- `[M<n> §N]` — M<n> spec
- `[M10 §X]` — M10 Session Writer spec section X

---

## 1. Goal

Visualize SFREPLAY v1 session files as a replay UX in the same chart UI used for live data. Users can open a recorded session file, see signals scroll in charts at original (or scaled) timing, pause, step, seek, and adjust playback speed.

After M11 the user can:

1. Quit current session
2. Open SignalForge
3. **File → Open Session** (M11) → select `.sfreplay` file
4. Charts switch to replay mode — same M8 charts, M5/M6/M7 derived signals, but data source is `SessionReader` instead of live driver
5. Use playback controls (play / pause / step / seek / speed) to navigate the recording
6. Switch back to live mode — replay session ends, charts can resume live data flow

This milestone freezes:

1. `PlaybackController` C++ API (state machine + Qt signals)
2. `SessionPlayer` C++ API (wraps M10 `SessionReader` with timing control)
3. Replay state machine (Idle / Loaded / Playing / Paused / Seeking / Ended / Error)

Quality philosophy from previous milestones: **leverage existing frozen surfaces**. M11 builds on M10 SessionReader (read), M8 charts (display), M6 SignalBufferRegistry (signal fan-out), M7 ExpressionEngine (derived signals). The new code is the **playback control layer**, not data flow.

---

## 2. Scope

### 2.1 Must deliver

1. **`PlaybackController`** at `src/replay/playback_controller.{hpp,cpp}` (QObject):
   - State machine: Idle → Loaded → Playing ↔ Paused → Ended (or → Error)
   - Owns a `SessionPlayer` instance (one at a time per V1)
   - Exposes API: `loadSession(filePath)`, `play()`, `pause()`, `step(direction)`, `seek(timestampNs)`, `setSpeed(factor)`, `closeSession()`
   - Emits Qt signals on state changes + position updates
   - Single global instance owned by MainWindow

2. **`SessionPlayer`** at `src/replay/session_player.{hpp,cpp}`:
   - Wraps M10 `SessionReader`
   - Owns a worker QThread that reads records + dispatches to `SignalValueSink`
   - Timing control: realtime (1×) / scaled (0.5× / 2× / 5× / 10×) / step-by-step
   - Pause / resume mid-stream
   - Seek to timestamp (re-position file pointer + replay catalog state)
   - Emits position + state Qt signals

3. **Replay state machine**:
   ```
   Idle ──loadSession()──▶ Loaded ──play()──▶ Playing ──pause()──▶ Paused
                                                  │                   │
                                                  │                  play()
                                                  │                   │
                                                  ▼                   ▼
                                                Ended ◄──────── (back to Playing)
                                                  
   Any state ──error()──▶ Error
   Any state ──closeSession()──▶ Idle
   ```

4. **`ReplayModeManager`** at `src/replay/replay_mode_manager.{hpp,cpp}` (per decision M11.4):
   - Coordinates global mode: Live ↔ Replay
   - On Replay enter: pause all M9 connections (`ConnectionManager::pauseAll()`)
   - On Replay exit: resume previously-active connections (user choice via dialog)
   - Charts read from same `SignalBufferRegistry` regardless of mode
   - Mode toggle is global (per decision M11.4 S, not per-chart)

5. **MainWindow integration** (per decision M11.1 B):
   - File menu: "Open Session..." (Ctrl+O) — opens `.sfreplay` via `QFileDialog::getOpenFileName`
   - Toolbar: replay control panel appears when in Replay mode
     - Play / Pause button
     - Step backward / forward (single record)
     - Seek slider (timeline scrubber, range = file's full timestamp range)
     - Speed combo (0.5× / 1× / 2× / 5× / 10×)
     - Current position label (timestamp + record number)
     - "Exit Replay" button → returns to Live mode
   - Status bar: "Replay: <filename> | <position> / <duration>"
   - When Replay active, M9 connection controls are disabled

6. **Playback controls** (per decision M11.2 X):
   - **Play**: starts/resumes timer-driven dispatch at current speed
   - **Pause**: stops timer; current state preserved
   - **Step**: dispatches next record immediately, stays paused
   - **Seek**: re-positions player to target timestamp; charts re-render with seek-position state
   - **Speed**: changes timer interval; current state preserved
   - **Exit**: stops player; returns to Live mode

7. **Speed control** (per decision M11.3 P):
   - Discrete options: 0.5× / 1× (real-time) / 2× / 5× / 10×
   - Changing speed mid-replay: immediate effect (next record dispatched at new interval)
   - V1.5+ may add continuous slider (Q)

8. **Single file replay** (per decision M11.5 V):
   - One `.sfreplay` file at a time
   - Loading new file closes current session first
   - V1.5+ may add playlist / multi-file sequential

9. **Live ↔ Replay mode transition** (per decision M11.4 S):
   - Enter Replay: confirmation dialog if any M9 connection is currently Connected
     - Options: "Pause connections and enter Replay" / "Cancel"
   - Exit Replay: confirmation dialog asking what to do with previously-paused connections
     - Options: "Resume connections" / "Stay disconnected" / "Cancel exit"
   - Charts re-render automatically on mode switch (clear buffer, populate from new source)

10. **Position tracking**:
    - `PlaybackController::currentPosition()` returns `std::pair<int64_t timestampNs, std::size_t recordIndex>`
    - Updated on every dispatched record
    - Emitted via `positionChanged(int64_t timestampNs, std::size_t recordIndex)` Qt signal at most every 30Hz (rate-limited)

11. **Error handling**:
    - File open failure: log ERROR, show MessageBox, transition to Error state
    - File corruption mid-replay: log ERROR, transition to Error state, partial replay preserved (user can navigate up to error point)
    - Invalid seek (timestamp out of file range): log WARN, clamp to nearest valid position
    - Backwards seek + step: M11 supports backward seek (re-reads file from beginning to target)

12. **Integration tests** at `tests/integration/`:
    - `test_playback_controller_lifecycle.cpp` — load / play / pause / stop / unload
    - `test_session_player_timing.cpp` — verify dispatch interval matches speed setting
    - `test_session_player_seek.cpp` — seek to various timestamps; verify position
    - `test_replay_mode_manager.cpp` — Live → Replay → Live transitions; verify connection state
    - `test_replay_charts_integration.cpp` — replay 1000-record file; verify charts populate correctly
    - `test_session_player_speed_change.cpp` — change speed mid-replay; verify timing adjustment
    - `test_session_player_truncated.cpp` — replay truncated file; verify graceful end-at-last-record

13. **Unit tests** ≥ 80% coverage on replay modules

14. **Benchmark** at `tests/benchmark/bench_replay.cpp`:
    - Replay 10s × 60 signals × 1kHz file at various speeds
    - Target: 1× speed maintains real-time within 5% timing error
    - 10× speed completes in 1s ± 10%
    - Memory bounded across full file replay
    - Results to `tests/benchmark/results/M11-baseline.md`

15. **Doxygen** on all public declarations

16. **`.claude/M11-done.md`** with standard completion report + freeze record

### 2.2 Must not do

1. **No modifications to M2-M10 frozen `.hpp`**.
2. **No multi-file playlist**. V1 single file only (decision M11.5 V).
3. **No per-chart replay mode**. Global mode toggle only (decision M11.4 S).
4. **No replay editing**. Read-only playback. V1.5+ may add markers / annotations.
5. **No replay-side recording** (replay → record into new session). Out of scope. V1.5+ if needed.
6. **No format conversion**. M11 only reads SFREPLAY v1 (via M10 SessionReader). Other formats out of scope.
7. **No remote / network replay**. Local files only. V2 territory.
8. **No replay synchronization** across multiple SignalForge instances. V2.
9. **No new top-level dependencies**. Use existing Qt + M5-M10.
10. **No QML scene customization for replay UI**. Pure C++ Qt Widgets.
11. **No backward time-axis convention change**. M8 forward-time axis preserved. Backward step is record-level navigation (decrement in record index), not time-axis reversal.
12. **No replay schedule / scripted scenarios**. V1 manual control only. V1.5+ may add scripting.

---

## 3. Design Decisions (locked by this spec)

### 3.1 Same-window replay UI (decision M11.1 Option B)

Replay UI appears in the same MainWindow as live mode. M8 charts are reused with `SessionReader` as data source instead of live drivers.

**Rationale**:
- Charts already display data from `SignalBufferRegistry`; M11 swaps the data feeder
- No need for separate window / new chart subsystem
- User mental model: "the data flowing through charts is now from a recording"
- Simpler than multi-window UX (M9 already complex)

V1.5+ may add: side-by-side live vs replay comparison view (Option T from earlier M11 deliberation).

### 3.2 Standard playback controls (decision M11.2 Option X)

Full control set: play / pause / step / seek / speed.

**Rationale**:
- Embedded debugging needs all of these (find a specific event, replay around it)
- Standard mental model from media players
- Step-by-step is critical for understanding signal interactions at decoded boundaries

V1 includes all features; V1.5+ may add: keyboard shortcuts (space=play/pause, arrows=step), bookmark UI.

### 3.3 Discrete speed multipliers (decision M11.3 Option P)

Speed combo: **0.5× / 1× / 2× / 5× / 10×**

**Rationale**:
- Discrete values prevent UI jitter from analog slider
- Common debugging speeds
- Continuous slider (Option Q) is V1.5+ if needed
- 10× is fast enough to skim long sessions; backwards step provides fine-grained navigation

**Real-time = 1×** uses original recorded timing. Other speeds scale `delayBetweenRecords = originalDelta / speed`.

### 3.4 Global Live/Replay mode toggle (decision M11.4 Option S)

Entering Replay mode: pause all M9 connections, charts switch to SessionReader data.
Exiting Replay mode: dialog asks user what to do with previously-paused connections.

**Rationale**:
- User can't be in two modes simultaneously (V1 simplicity)
- Avoids confusion about which signal data charts show
- Per-chart mode (Option U) is V1.5+ — adds complexity user doesn't need yet
- M9 connection state preservation respects user's prior intent

**Implementation**: `ReplayModeManager` is a small QObject that orchestrates the transitions. Charts don't know about modes; they just read from `SignalBufferRegistry`.

### 3.5 Single file replay (decision M11.5 Option V)

V1: one `.sfreplay` at a time. Loading new file closes current.

V1.5+ may add: playlist / multi-file sequential / file diff comparison.

**Rationale**:
- V1 simplicity
- Multi-file UX requires playlist UI (significant scope expansion)
- 95% of debugging use cases are single-session replay

### 3.6 Playback uses M10 SessionReader directly

`SessionPlayer` wraps `SessionReader`. No format-specific knowledge in M11; all SFREPLAY v1 parsing is M10's responsibility.

**Implication**: future V2 formats (if added) would create new `Reader` classes that `SessionPlayer` could dispatch to. M11 design accommodates this without freeze breakage (only `SessionPlayer` constructor would need new ctor overload taking generic Reader interface).

### 3.7 No soft-HALT (inherits M2-M10)

### 3.8 Metric naming

- `replay_state` (gauge): 0=Idle, 1=Loaded, 2=Playing, 3=Paused, 4=Ended, 5=Error
- `replay_position_ns` (gauge): current timestamp position in file
- `replay_records_dispatched_total` (counter): total records replayed
- `replay_speed_factor` (gauge): current speed multiplier
- `replay_seeks_total` (counter): seek operations
- `replay_mode_transitions_total` (counter): live ↔ replay transitions

---

## 4. Key Implementation Details

### 4.1 `PlaybackController` class

Place at `src/replay/playback_controller.hpp`.

```cpp
// src/replay/playback_controller.hpp
#pragma once

#include "replay/session_player.hpp"

#include <QObject>
#include <QString>
#include <chrono>
#include <memory>

namespace signalforge::replay {

enum class PlaybackState {
    Idle,         ///< No session loaded
    Loaded,       ///< Session loaded, not playing
    Playing,      ///< Active replay
    Paused,       ///< Replay paused
    Seeking,      ///< Internal: re-positioning
    Ended,        ///< Replay reached end of file
    Error,        ///< Error state
};

/// Top-level orchestrator for replay UX.
///
/// Owns SessionPlayer; coordinates with M9 ConnectionManager via ReplayModeManager.
/// MainWindow holds one PlaybackController instance.
///
/// Threading: lives on main thread. SessionPlayer's worker thread does file I/O.
///
/// Freeze scope: this class is frozen at M11 close.
class PlaybackController : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PlaybackController)

public:
    explicit PlaybackController(
        signalforge::buffer::SignalBufferRegistry& registry,
        QObject* parent = nullptr);
    ~PlaybackController() override;

    // Session management

    /// Load .sfreplay file. Transitions to Loaded state.
    /// Returns false on file open / parse error.
    [[nodiscard]] bool loadSession(const QString& filePath);

    /// Close current session. Transitions to Idle.
    void closeSession();

    [[nodiscard]] QString currentFilePath() const;

    // Playback controls

    [[nodiscard]] bool play();
    [[nodiscard]] bool pause();
    [[nodiscard]] bool stepForward();
    [[nodiscard]] bool stepBackward();

    /// Seek to absolute timestamp (ns since epoch). Clamps to file range.
    [[nodiscard]] bool seek(std::int64_t timestampNs);

    /// Set playback speed multiplier. Discrete values: 0.5, 1.0, 2.0, 5.0, 10.0.
    [[nodiscard]] bool setSpeed(double factor);

    // State accessors

    [[nodiscard]] PlaybackState state() const noexcept;
    [[nodiscard]] double currentSpeed() const noexcept;
    [[nodiscard]] std::int64_t currentPositionNs() const noexcept;
    [[nodiscard]] std::size_t currentRecordIndex() const noexcept;
    [[nodiscard]] std::int64_t durationNs() const noexcept;
    [[nodiscard]] std::size_t totalRecords() const noexcept;
    
    // Diagnostics
    [[nodiscard]] const QString& lastError() const noexcept;

signals:
    void stateChanged(PlaybackState newState);
    void positionChanged(std::int64_t timestampNs, std::size_t recordIndex);
    void speedChanged(double newSpeed);
    void sessionLoaded(const QString& filePath, std::int64_t durationNs, std::size_t totalRecords);
    void sessionClosed();
    void errorOccurred(const QString& errorMessage);

private:
    signalforge::buffer::SignalBufferRegistry* registry_;
    std::unique_ptr<SessionPlayer> player_;
    PlaybackState state_ = PlaybackState::Idle;
    QString currentFilePath_;
    QString lastError_;
};

}  // namespace signalforge::replay
```

### 4.2 `SessionPlayer` class

Place at `src/replay/session_player.hpp`.

```cpp
// src/replay/session_player.hpp
#pragma once

#include "session/session_reader.hpp"  // M10 frozen
#include "decoder/decoder_interface.hpp"  // M5 frozen for SignalValueSink

#include <QObject>
#include <QThread>
#include <QTimer>
#include <atomic>
#include <chrono>
#include <memory>

namespace signalforge::replay {

/// Wraps M10 SessionReader with timing + control.
///
/// Worker thread reads records from SessionReader; main thread controls state.
/// Records are dispatched to a SignalValueSink at the configured speed.
///
/// Threading: caller (main thread) controls; worker QThread does file reads.
class SessionPlayer : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SessionPlayer)

public:
    explicit SessionPlayer(
        signalforge::decoder::SignalValueSink& sink,
        QObject* parent = nullptr);
    ~SessionPlayer() override;

    /// Open file. Returns false on error.
    [[nodiscard]] bool openFile(const QString& filePath);

    /// Close file. Stops playback if active.
    void closeFile();

    /// Begin playing from current position.
    void play();

    /// Pause; current position preserved.
    void pause();

    /// Dispatch next single record without timing delay.
    /// Returns false if at end of file.
    [[nodiscard]] bool stepForward();

    /// Step backward (re-reads file from beginning to (current_record - 1)).
    /// Slow but correct for V1.
    [[nodiscard]] bool stepBackward();

    /// Seek to timestamp. Re-positions internal state.
    [[nodiscard]] bool seek(std::int64_t timestampNs);

    /// Set speed multiplier; takes effect immediately.
    void setSpeed(double factor);

    // State queries

    [[nodiscard]] bool isPlaying() const noexcept;
    [[nodiscard]] std::int64_t currentPositionNs() const noexcept;
    [[nodiscard]] std::size_t currentRecordIndex() const noexcept;
    [[nodiscard]] std::int64_t durationNs() const noexcept;
    [[nodiscard]] std::size_t totalRecords() const noexcept;
    [[nodiscard]] bool atEnd() const noexcept;

signals:
    void recordDispatched(std::int64_t timestampNs, std::size_t recordIndex);
    void endReached();
    void error(const QString& errorMessage);

private:
    signalforge::decoder::SignalValueSink* sink_;
    std::unique_ptr<signalforge::session::SessionReader> reader_;
    std::unique_ptr<QThread> workerThread_;
    
    std::atomic<bool> playing_{false};
    std::atomic<double> speedFactor_{1.0};
    std::atomic<std::int64_t> currentPosNs_{0};
    std::atomic<std::size_t> currentRecordIdx_{0};
    std::atomic<std::size_t> totalRecords_{0};
    std::atomic<std::int64_t> durationNs_{0};
};

}  // namespace signalforge::replay
```

### 4.3 `ReplayModeManager` class

Place at `src/replay/replay_mode_manager.hpp`.

```cpp
// src/replay/replay_mode_manager.hpp
#pragma once

#include "replay/playback_controller.hpp"
#include "connection/connection_manager.hpp"

#include <QObject>
#include <QStringList>

namespace signalforge::replay {

enum class AppMode {
    Live,
    Replay,
};

/// Coordinates Live ↔ Replay transitions.
class ReplayModeManager : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ReplayModeManager)

public:
    explicit ReplayModeManager(
        signalforge::connection::ConnectionManager& connectionMgr,
        PlaybackController& playbackCtrl,
        QObject* parent = nullptr);
    ~ReplayModeManager() override;

    [[nodiscard]] AppMode currentMode() const noexcept;

    /// Enter Replay mode. Pauses M9 connections.
    /// Returns false if user cancelled (via prompt) or already in Replay.
    [[nodiscard]] bool enterReplay();

    /// Exit Replay mode. Prompts user about previously-paused connections.
    /// Returns false if user cancelled.
    [[nodiscard]] bool exitReplay();

signals:
    void modeChanged(AppMode newMode);
    void connectionsPaused();   // On enter Replay
    void connectionsRestored(); // On exit Replay (if user chose to resume)

private:
    signalforge::connection::ConnectionManager* connectionMgr_;
    PlaybackController* playbackCtrl_;
    AppMode currentMode_ = AppMode::Live;
    
    // Snapshot of connection states at entry to Replay (to optionally restore)
    QStringList previouslyConnectedIds_;
};

}  // namespace signalforge::replay
```

### 4.4 Replay timer-driven dispatch

`SessionPlayer`'s worker thread runs:

```cpp
void SessionPlayer::workerLoop() {
    while (playing_ && !atEnd()) {
        auto record = reader_->readNext();
        if (!record) break;
        
        // Calculate delay since previous record
        auto recordTimeDelta = record->timestampNs - previousTimestamp_;
        auto realtimeDelta = std::chrono::nanoseconds(recordTimeDelta);
        auto scaledDelta = std::chrono::nanoseconds(
            static_cast<int64_t>(recordTimeDelta / speedFactor_.load()));
        
        // Sleep until time to dispatch
        std::this_thread::sleep_for(scaledDelta);
        
        // Dispatch via QMetaObject::invokeMethod to main thread sink
        QMetaObject::invokeMethod(this, [this, rec=*record]() {
            sink_->onSignal(rec.timestamp, rec.signalId, rec.value);
        }, Qt::QueuedConnection);
        
        previousTimestamp_ = record->timestampNs;
        currentRecordIdx_.fetch_add(1);
        currentPosNs_.store(record->timestampNs);
        
        emit recordDispatched(record->timestampNs, currentRecordIdx_.load());
    }
    
    if (atEnd()) {
        emit endReached();
    }
}
```

### 4.5 Seek implementation

```cpp
bool SessionPlayer::seek(std::int64_t timestampNs) {
    auto wasPlaying = playing_.load();
    if (wasPlaying) {
        playing_.store(false);
        // Wait for worker to drain
    }
    
    // Re-position SessionReader
    if (!reader_->seekToTimestamp(timestampNs)) {
        // Reader API: scans from beginning if needed
        return false;
    }
    
    currentPosNs_.store(timestampNs);
    currentRecordIdx_.store(reader_->currentRecordIndex());
    
    if (wasPlaying) {
        play();
    }
    
    return true;
}
```

**Note**: M10 `SessionReader` may need a `seekToTimestamp(ns)` method. If not present, M11 builds on top by sequential read. **Check M10 SessionReader API at S2 implementation; if absent, log as M11 concern but proceed with sequential read approach (slower but functional).**

### 4.6 Speed change mid-playback

```cpp
void SessionPlayer::setSpeed(double factor) {
    if (factor < 0.5 || factor > 10.0) {
        SF_LOG_WARN("Speed {} out of range; clamping", factor);
        factor = std::clamp(factor, 0.5, 10.0);
    }
    speedFactor_.store(factor);
    // Worker loop reads atomic on next iteration; effects on next record's delay
}
```

### 4.7 MainWindow integration

Add to MainWindow:

- **File menu**:
  - "Open Session..." (Ctrl+O) — open `.sfreplay`
  
- **Toolbar** (when Replay mode active):
  - Play/Pause toggle button
  - Step backward / forward buttons
  - Timeline scrubber (slider; shows record index 0..totalRecords)
  - Speed combo (0.5 / 1.0 / 2.0 / 5.0 / 10.0)
  - "Exit Replay" button
  
- **Status bar**:
  - "Replay: <filename> | Position: <timestamp> / <duration> | Records: <idx> / <total>"

When Replay active, M9 connection controls are disabled (greyed out).

When Replay inactive (Live mode), M9 controls active; replay toolbar hidden.

### 4.8 Live ↔ Replay transition

```cpp
bool ReplayModeManager::enterReplay() {
    if (currentMode_ == AppMode::Replay) return false;
    
    // Check if any M9 connections are currently Connected
    QStringList activeConnections;
    for (const auto& id : connectionMgr_->connectionIds()) {
        auto* conn = connectionMgr_->connection(id);
        if (conn && conn->state() == Connection::State::Connected) {
            activeConnections.append(id);
        }
    }
    
    if (!activeConnections.isEmpty()) {
        // Show confirmation dialog
        auto reply = QMessageBox::warning(nullptr,
            "Pause connections to enter Replay?",
            QString("Entering Replay mode will pause %1 active connections. Continue?").arg(activeConnections.size()),
            QMessageBox::Yes | QMessageBox::No);
        
        if (reply != QMessageBox::Yes) {
            return false;
        }
    }
    
    // Pause connections
    previouslyConnectedIds_ = activeConnections;
    for (const auto& id : activeConnections) {
        connectionMgr_->disconnectConnection(id);
    }
    emit connectionsPaused();
    
    // Switch mode
    currentMode_ = AppMode::Replay;
    emit modeChanged(AppMode::Replay);
    
    return true;
}
```

---

## 5. Performance gates

### 5.1 Timing accuracy

| Metric | Target | HALT |
|---|---|---|
| 1× speed timing error over 10s replay | < 5% | > 20% |
| 10× speed completion time for 10s file | 1s ± 10% | > 1.5s |
| Seek to arbitrary timestamp in 600k-record file | < 500ms | > 2s |
| Step latency (single record dispatch) | < 50ms | > 200ms |

### 5.2 Memory

| Metric | Target | HALT |
|---|---|---|
| Memory during full file replay | < 100MB above baseline | > 300MB |
| No memory leak across full replay | yes | growth > 10% |

### 5.3 Mode transition

| Metric | Target | HALT |
|---|---|---|
| Live → Replay transition latency | < 1s | > 3s |
| Replay → Live transition latency | < 1s | > 3s |

### 5.4 Run-to-run variance

3-run mean variance < 5% on timing metrics.

---

## 6. Freeze protocol

### 6.1 What freezes at M11 close

**C++ interfaces**:
- `src/replay/playback_controller.hpp`: `PlaybackController` class, `PlaybackState` enum
- `src/replay/session_player.hpp`: `SessionPlayer` class
- `src/replay/replay_mode_manager.hpp`: `ReplayModeManager` class, `AppMode` enum

Once frozen, modifications require new ADR.

### 6.2 What does NOT freeze

- `SessionPlayer` worker thread internals
- Speed multiplier defaults (could be tuned based on user feedback)
- `PlaybackController::Impl` PIMPL layout
- Replay UI widget layouts (visual changes OK without ADR)

### 6.3 Freeze record format

`.claude/M11-done.md`:

```markdown
## Freezes established in this milestone

| File | sha256 |
|---|---|
| `src/replay/playback_controller.hpp` | <...> |
| `src/replay/session_player.hpp` | <...> |
| `src/replay/replay_mode_manager.hpp` | <...> |
```

---

## 7. M11-specific HALT triggers

Beyond CLAUDE.md §HALT:

1. **Modification to M2-M10 frozen `.hpp`** → HALT.
2. **Replay timing error > 20%** at 1× speed → HALT (timer / threading bug).
3. **Memory leak in full file replay** (growth > 10%) → HALT.
4. **M10 SessionReader missing required API** (e.g., seek to timestamp) → HALT and discuss whether M10 patch needed (parallel to M6 ADR-005 pattern).
5. **Live ↔ Replay mode transition leaves charts in inconsistent state** → HALT.
6. **Step backward incorrect** (re-reads from start, but state mismatched) → HALT.
7. **Seek to invalid timestamp crashes or hangs** → HALT.

---

## 8. Acceptance criteria

### 8.1 Build and test

- [ ] Debug, Release, debug-asan all build clean under C++23
- [ ] All unit + integration tests pass under all three presets
- [ ] Coverage ≥ 80% per §2.1-13
- [ ] CI green on milestone/M11 head

### 8.2 Performance (per §5)

- [ ] 1× speed timing error < 5% over 10s replay
- [ ] 10× speed completion in 1s ± 10%
- [ ] Seek latency < 500ms for 600k-record file
- [ ] Step latency < 50ms per record
- [ ] Full file replay memory growth < 10%

### 8.3 Functional correctness

- [ ] Open `.sfreplay`: parse + load successfully
- [ ] Play / pause / step / seek / speed change all work as documented
- [ ] Live → Replay → Live transitions preserve user's connection intent
- [ ] Charts populate correctly during replay
- [ ] Speed change mid-replay applies on next record
- [ ] Seek to timestamp out of file range: clamp + log
- [ ] Truncated file: replay up to last complete record, then end gracefully
- [ ] Error file: log + show MessageBox + transition to Error state

### 8.4 Lifecycle correctness

- [ ] Load → Close → Load cycle works
- [ ] Worker thread joined before SessionPlayer destructor returns
- [ ] App close during replay: file closed gracefully

### 8.5 Threading safety

- [ ] ASan clean (CI authoritative)
- [ ] No race between dispatch worker and main thread state queries

### 8.6 Freeze record

- [ ] M11-done.md has Freezes section per §6.3
- [ ] Sha256s recorded for 3 files
- [ ] No modifications to M2-M10 frozen files

### 8.7 Hand-off

- [ ] M11-done.md hand-off section covers:
  - For M12 Performance: replay timing precision is a measurable metric; if optimization needed, this is the candidate
  - For M13 Packaging: replay UX integrates with existing app bundle; no new components

---

## 9. Notes for CC

- **M10 SessionReader API check at S2**: If `SessionReader` doesn't have `seekToTimestamp(ns)`, you can either:
  1. Add it to M10's frozen API → this is M10 amendment, file as M11 concern + ADR
  2. Build on top via sequential read (slower) → no M10 change
  
  Option 2 is preferred unless M11 perf tests fail (HALT trigger #4).

- **Step backward is slow**. Re-reads file from start to target. V1.5+ may add bookmark-based fast-rewind. V1 can document the limitation.

- **Speed change race conditions**: Atomic `speedFactor_` is read by worker thread on every record. Fine for V1 latency. Lock-free.

- **Mode transition during recording is undefined**. If user is recording (M10 SessionWriter active) and tries to enter Replay, V1 should disable the menu item or show error. Document this constraint.

- **QFileDialog for file selection**: Use `QFileDialog::getOpenFileName` with filter "*.sfreplay". M13 packaging will register the file extension association.

- **Don't optimize before measuring**. V1 priorities: correctness > performance. M12 covers cross-cutting performance.

- **Charts read from registry**: M11 doesn't touch chart code. SessionPlayer dispatches `SignalValue` to `SignalValueSink` (registry); registry fans out to charts (M8). Same path as live mode.

- **ReplayModeManager dialog**: Use Qt's existing `QMessageBox` patterns. Don't invent custom dialogs.

---

## 10. Closing note

M11 makes recorded sessions **navigable**. Combined with M9 (live capture) and M10 (recording), the user workflow becomes:

1. Connect to device
2. Record a session
3. Disconnect
4. **Open the recording**
5. **Step / seek / replay** to find specific moments
6. **Navigate forward and backward** through the recording
7. **Return to live** mode if needed

After M11, the V1 daily-use loop is complete: live debugging → record interesting events → analyze recordings later. M12 (Performance) and M13 (Packaging) round out V1 release.

Quality discipline:
- Replay must be **deterministic**: same file, same controls = identical output
- UI must be **responsive**: pause/play/seek immediate, no jank
- Charts must **resync correctly** on mode transitions
- File errors must be **graceful**: never crash, always show user-friendly error

When in doubt about UX (button placement, dialog wording, behavior on edge cases), choose the **most predictable** option. V1 prioritizes correctness over fancy UX.

Performance is bounded by file I/O + dispatch overhead. V1 60k events/sec dispatch through M10 SessionReader is well within budget. M12 may optimize if real workloads exceed this.
