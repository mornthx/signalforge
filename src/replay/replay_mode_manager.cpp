// src/replay/replay_mode_manager.cpp
//
// S8 — Live ↔ Replay transitions.
//
// Design note: the user-facing confirmation dialogs live in
// MainWindow (S9), not here. This class is pure orchestration:
// it snapshots Connected M9 connection ids, calls
// disconnectConnection() per id on enter, and (optionally)
// connectConnection() per id on exit. Decoupling the UI keeps
// the manager unit-testable and aligns with the C3 resolution
// in `.claude/M11-concerns.md` (M9 has no pause/resume; we
// synthesise via disconnect/connect).
#include "replay/replay_mode_manager.hpp"

#include "connection/connection_manager.hpp"
#include "observability/logging.hpp"
#include "replay/playback_controller.hpp"

namespace signalforge::replay {

ReplayModeManager::ReplayModeManager(signalforge::connection::ConnectionManager& connectionMgr,
                                     PlaybackController& playbackCtrl, QObject* parent)
    : QObject(parent), connectionMgr_(&connectionMgr), playbackCtrl_(&playbackCtrl), currentMode_(AppMode::Live) {}

ReplayModeManager::~ReplayModeManager() = default;

AppMode ReplayModeManager::currentMode() const noexcept {
    return currentMode_;
}

bool ReplayModeManager::enterReplay() {
    if (currentMode_ == AppMode::Replay) {
        return false;
    }

    // Snapshot ids of currently-Connected connections.
    previouslyConnectedIds_.clear();
    for (const auto& id : connectionMgr_->connectionIds()) {
        const auto* conn = connectionMgr_->connection(id);
        if (conn && conn->state() == signalforge::connection::Connection::State::Connected) {
            previouslyConnectedIds_.append(id);
        }
    }

    // Disconnect each snapshotted connection. We tolerate failures:
    // a connection that fails to disconnect cleanly is logged but
    // doesn't block the mode transition (the user explicitly chose
    // Replay; getting them there is the priority).
    for (const auto& id : previouslyConnectedIds_) {
        if (!connectionMgr_->disconnectConnection(id)) {
            SF_LOG_WARN("ReplayModeManager::enterReplay: disconnectConnection({}) returned false", id.toStdString());
        }
    }

    if (!previouslyConnectedIds_.isEmpty()) {
        emit connectionsPaused();
    }

    currentMode_ = AppMode::Replay;
    emit modeChanged(AppMode::Replay);
    return true;
}

bool ReplayModeManager::exitReplay(bool resumeConnections) {
    if (currentMode_ == AppMode::Live) {
        return false;
    }

    if (resumeConnections && !previouslyConnectedIds_.isEmpty()) {
        for (const auto& id : previouslyConnectedIds_) {
            if (!connectionMgr_->connectConnection(id)) {
                SF_LOG_WARN("ReplayModeManager::exitReplay: connectConnection({}) returned false", id.toStdString());
            }
        }
        emit connectionsRestored();
    }
    previouslyConnectedIds_.clear();

    currentMode_ = AppMode::Live;
    emit modeChanged(AppMode::Live);
    return true;
}

QStringList ReplayModeManager::pausedConnectionIds() const {
    return previouslyConnectedIds_;
}

}  // namespace signalforge::replay
