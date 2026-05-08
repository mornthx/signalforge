// src/replay/replay_mode_manager.hpp
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace signalforge::connection {
class ConnectionManager;
}  // namespace signalforge::connection

namespace signalforge::replay {

class PlaybackController;

/// Application-level mode. Per M11 §3.4, mode is global (per-app),
/// not per-chart.
///
/// Freeze scope: this enum is frozen at M11 close.
enum class AppMode {
    Live,    ///< M9 connections may be active; charts show live data.
    Replay,  ///< M9 connections paused; charts show recorded data.
};

/// Coordinates Live ↔ Replay transitions.
///
/// Entering Replay snapshots the IDs of currently-Connected M9
/// connections, calls `disconnectConnection` on each, and emits
/// `connectionsPaused`. Exiting Replay prompts the user about the
/// previously-paused connections (Resume / Stay Disconnected /
/// Cancel) and on Resume calls `connectConnection` on each stored
/// id, then emits `connectionsRestored`.
///
/// **Why disconnect/connect, not pause/resume**: M9 freezes
/// `ConnectionManager` at M9 close and exposes only connect /
/// disconnect lifecycle (no pause). Synthesising pause via
/// disconnect is functionally equivalent from the user's
/// viewpoint and avoids touching the M9 freeze surface (HALT
/// trigger #1). See `.claude/M11-concerns.md` C3.
///
/// Threading: main-thread-only (matches M9 ConnectionManager).
///
/// Freeze scope: this class (its public API + signals + the
/// `AppMode` enum) is frozen at M11 close. Modifications
/// post-freeze require a new ADR.
class ReplayModeManager : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ReplayModeManager)

public:
    /// `connectionMgr` and `playbackCtrl` outlive this manager.
    explicit ReplayModeManager(signalforge::connection::ConnectionManager& connectionMgr,
                               PlaybackController& playbackCtrl, QObject* parent = nullptr);
    ~ReplayModeManager() override;

    [[nodiscard]] AppMode currentMode() const noexcept;

    /// Enter Replay mode. If any M9 connections are currently
    /// `Connected`, prompts the user via `QMessageBox`; on Yes,
    /// snapshots their ids, calls `disconnectConnection` on each,
    /// and transitions. Returns `false` if the user cancelled or
    /// already in Replay.
    [[nodiscard]] bool enterReplay();

    /// Exit Replay mode. If the snapshot is non-empty, prompts
    /// the user with three choices (Resume / Stay Disconnected /
    /// Cancel). On Resume, iterates the snapshot and calls
    /// `connectConnection` on each; emits `connectionsRestored`.
    /// Returns `false` if the user cancelled or already in Live.
    [[nodiscard]] bool exitReplay();

    /// Snapshot of connection ids paused on the most recent
    /// `enterReplay()`. Empty when `currentMode() == Live`.
    [[nodiscard]] QStringList pausedConnectionIds() const;

signals:
    void modeChanged(signalforge::replay::AppMode newMode);
    void connectionsPaused();
    void connectionsRestored();

private:
    signalforge::connection::ConnectionManager* connectionMgr_;
    PlaybackController* playbackCtrl_;
    AppMode currentMode_;
    QStringList previouslyConnectedIds_;
};

}  // namespace signalforge::replay
