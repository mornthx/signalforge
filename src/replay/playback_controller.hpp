// src/replay/playback_controller.hpp
#pragma once

#include "replay/session_player.hpp"

#include <QObject>
#include <QString>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace signalforge::buffer {
class SignalBufferRegistry;
}  // namespace signalforge::buffer

namespace signalforge::replay {

/// Replay state machine values.
///
/// Transitions:
///   - Idle    → Loaded   on `loadSession()` success
///   - Loaded  → Playing  on `play()`
///   - Playing → Paused   on `pause()`
///   - Paused  → Playing  on `play()`
///   - Playing → Ended    on SessionPlayer end-of-file
///   - any     → Error    on SessionPlayer error
///   - any     → Idle     on `closeSession()`
///
/// `Seeking` is an internal transient state that observers may
/// see between `Playing`/`Paused` and the same state at the new
/// position; it is never the resting state of the controller.
///
/// Freeze scope: this enum is frozen at M11 close.
enum class PlaybackState {
    Idle,     ///< No session loaded.
    Loaded,   ///< Session loaded, dispatch not yet started.
    Playing,  ///< Active replay.
    Paused,   ///< Replay paused; position preserved.
    Seeking,  ///< Internal: re-positioning in progress.
    Ended,    ///< Replay reached end of file.
    Error,    ///< Error state; close to recover.
};

/// Top-level orchestrator for replay UX.
///
/// Owns one `SessionPlayer` instance (one session at a time per
/// V1 — see M11 spec §3.5). Coordinates with M9
/// `ConnectionManager` via `ReplayModeManager`; this class itself
/// is mode-agnostic.
///
/// Threading: lives on the main thread. The owned `SessionPlayer`
/// runs disk I/O on a worker `QThread`. All public Qt signals
/// (state, position, lifecycle) are emitted on the main thread.
///
/// Freeze scope: this class (its public API + signals + the
/// `PlaybackState` enum) is frozen at M11 close. Modifications
/// post-freeze require a new ADR.
class PlaybackController : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PlaybackController)

public:
    /// `registry` outlives this controller; it receives every
    /// signal value dispatched by the active `SessionPlayer`.
    explicit PlaybackController(signalforge::buffer::SignalBufferRegistry& registry, QObject* parent = nullptr);
    ~PlaybackController() override;

    // ── Session management ──────────────────────────────────

    /// Load an SFREPLAY v1 file. On success, transitions to
    /// `Loaded`; on failure, returns `false`, sets `lastError()`,
    /// and stays in `Idle` (or transitions to `Error` if a session
    /// was previously loaded). Loading a new session while one is
    /// active first calls `closeSession()`.
    [[nodiscard]] bool loadSession(const QString& filePath);

    /// Close the active session. Stops dispatch if active. Any
    /// state transitions back to `Idle`.
    void closeSession();

    /// Path of the currently loaded session (empty if none).
    [[nodiscard]] QString currentFilePath() const;

    // ── Playback controls ───────────────────────────────────

    /// Start (or resume from `Paused`) replay. Returns `false` if
    /// the controller is not in a state where play is valid.
    [[nodiscard]] bool play();

    /// Pause active replay. Returns `false` if not currently
    /// `Playing`.
    [[nodiscard]] bool pause();

    /// Dispatch the next single record without timing delay; stay
    /// paused. Returns `false` at end of file or if not in a state
    /// permitting step.
    [[nodiscard]] bool stepForward();

    /// Move one record back. Internally re-reads the file from the
    /// nearest earlier point; O(file) per V1 — see spec §9 Note 2.
    /// Returns `false` if at start or not permitted.
    [[nodiscard]] bool stepBackward();

    /// Seek to absolute timestamp (ns since epoch in the recording's
    /// time base). Out-of-range values are clamped to the file's
    /// timestamp range and a WARN is logged. Returns `false` only on
    /// error (e.g., reader I/O failure).
    [[nodiscard]] bool seek(std::int64_t timestampNs);

    /// Set replay speed multiplier. Discrete spec values: 0.5, 1.0,
    /// 2.0, 5.0, 10.0 (per M11 §3.3). Out-of-range values are
    /// clamped and logged WARN. Effect is immediate (next dispatched
    /// record uses the new speed).
    [[nodiscard]] bool setSpeed(double factor);

    // ── State accessors ─────────────────────────────────────

    [[nodiscard]] PlaybackState state() const noexcept;
    [[nodiscard]] double currentSpeed() const noexcept;

    /// Most recently dispatched record's timestamp, in the
    /// recording's time base (ns from `recordedAt`).
    [[nodiscard]] std::int64_t currentPositionNs() const noexcept;

    /// Most recently dispatched record's ordinal index (0-based).
    [[nodiscard]] std::size_t currentRecordIndex() const noexcept;

    /// Total duration of the loaded session in ns (last record's
    /// timestamp). Returns 0 in `Idle` / `Error`.
    [[nodiscard]] std::int64_t durationNs() const noexcept;

    /// Total record count in the loaded session. Returns 0 in
    /// `Idle` / `Error`.
    [[nodiscard]] std::size_t totalRecords() const noexcept;

    // ── Diagnostics ─────────────────────────────────────────

    /// Most recent error message; empty if none.
    [[nodiscard]] const QString& lastError() const noexcept;

signals:
    void stateChanged(signalforge::replay::PlaybackState newState);
    void positionChanged(std::int64_t timestampNs, std::size_t recordIndex);
    void speedChanged(double newSpeed);
    void sessionLoaded(const QString& filePath, std::int64_t durationNs, std::size_t totalRecords);
    void sessionClosed();
    void errorOccurred(const QString& errorMessage);

private:
    signalforge::buffer::SignalBufferRegistry* registry_;
    std::unique_ptr<SessionPlayer> player_;
    PlaybackState state_;
    QString currentFilePath_;
    QString lastError_;
};

}  // namespace signalforge::replay
