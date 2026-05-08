// src/replay/replay_mode_manager.cpp
//
// S1 stub: state initialised; transitions implemented at S8.
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
    SF_LOG_INFO("ReplayModeManager::enterReplay not yet implemented (S8)");
    return false;
}

bool ReplayModeManager::exitReplay() {
    SF_LOG_INFO("ReplayModeManager::exitReplay not yet implemented (S8)");
    return false;
}

QStringList ReplayModeManager::pausedConnectionIds() const {
    return previouslyConnectedIds_;
}

}  // namespace signalforge::replay
