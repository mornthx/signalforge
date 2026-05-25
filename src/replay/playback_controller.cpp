// src/replay/playback_controller.cpp
//
// S7 — full state machine + Qt signal fan-out.
// Owns one SessionPlayer at a time; SessionPlayer is constructed
// at loadSession() and destroyed at closeSession().
//
// State machine per spec §3 + plan §S7:
//   Idle    → Loaded   on loadSession success
//   Loaded  → Playing  on play()
//   Playing → Paused   on pause()
//   Paused  → Playing  on play()
//   Playing → Ended    on SessionPlayer::endReached
//   any     → Error    on SessionPlayer::error
//   any     → Idle     on closeSession()
#include "replay/playback_controller.hpp"

#include "buffer/signal_buffer_registry.hpp"
#include "observability/logging.hpp"

namespace signalforge::replay {

PlaybackController::PlaybackController(signalforge::buffer::SignalBufferRegistry& registry, QObject* parent)
    : QObject(parent), registry_(&registry), state_(PlaybackState::Idle) {}

PlaybackController::~PlaybackController() = default;

bool PlaybackController::loadSession(const QString& filePath) {
    // Auto-close any prior session per spec §2.1-8 / §3.5.
    if (player_) {
        closeSession();
    }

    player_ = std::make_unique<SessionPlayer>(*registry_, this);

    // Wire SessionPlayer signals to controller's own. positionUpdated
    // is already 30 Hz throttled at the player; just forward.
    connect(player_.get(), &SessionPlayer::positionUpdated, this,
            [this](std::int64_t ts, std::size_t idx) { emit positionChanged(ts, idx); });
    connect(player_.get(), &SessionPlayer::endReached, this, [this]() {
        if (state_ == PlaybackState::Playing) {
            state_ = PlaybackState::Ended;
            emit stateChanged(state_);
        }
    });
    connect(player_.get(), &SessionPlayer::error, this, [this](const QString& msg) {
        state_ = PlaybackState::Error;
        lastError_ = msg;
        emit stateChanged(state_);
        emit errorOccurred(msg);
    });

    if (!player_->openFile(filePath)) {
        lastError_ = QStringLiteral("Failed to open session file: %1").arg(filePath);
        SF_LOG_ERROR("PlaybackController::loadSession: {}", lastError_.toStdString());
        player_.reset();
        // Stay in Idle (can't transition to Error from Idle without
        // an active session — Error is for runtime failures during
        // an active replay).
        emit errorOccurred(lastError_);
        return false;
    }

    currentFilePath_ = filePath;
    state_ = PlaybackState::Loaded;
    emit stateChanged(state_);
    emit sessionLoaded(filePath, player_->durationNs(), player_->totalRecords());
    return true;
}

void PlaybackController::closeSession() {
    if (state_ == PlaybackState::Idle && !player_) {
        return;
    }
    if (player_) {
        player_->closeFile();
        player_.reset();
    }
    currentFilePath_.clear();
    lastError_.clear();
    state_ = PlaybackState::Idle;
    emit stateChanged(state_);
    emit sessionClosed();
}

QString PlaybackController::currentFilePath() const {
    return currentFilePath_;
}

bool PlaybackController::play() {
    if (!player_) {
        SF_LOG_WARN("PlaybackController::play with no session loaded");
        return false;
    }
    if (state_ != PlaybackState::Loaded && state_ != PlaybackState::Paused) {
        SF_LOG_WARN("PlaybackController::play: invalid state ({})", static_cast<int>(state_));
        return false;
    }
    player_->play();
    state_ = PlaybackState::Playing;
    emit stateChanged(state_);
    return true;
}

bool PlaybackController::pause() {
    if (!player_) {
        return false;
    }
    if (state_ != PlaybackState::Playing) {
        SF_LOG_WARN("PlaybackController::pause: invalid state ({})", static_cast<int>(state_));
        return false;
    }
    player_->pause();
    state_ = PlaybackState::Paused;
    emit stateChanged(state_);
    return true;
}

bool PlaybackController::stepForward() {
    if (!player_) {
        return false;
    }
    if (state_ != PlaybackState::Loaded && state_ != PlaybackState::Paused) {
        SF_LOG_WARN("PlaybackController::stepForward: invalid state");
        return false;
    }
    if (!player_->stepForward()) {
        // EOF reached during step.
        state_ = PlaybackState::Ended;
        emit stateChanged(state_);
        return false;
    }
    if (state_ == PlaybackState::Loaded) {
        // First step from a fresh-loaded session implicitly puts us
        // into Paused (a single record dispatched, awaiting next
        // command).
        state_ = PlaybackState::Paused;
        emit stateChanged(state_);
    }
    return true;
}

bool PlaybackController::stepBackward() {
    if (!player_) {
        return false;
    }
    if (state_ != PlaybackState::Loaded && state_ != PlaybackState::Paused && state_ != PlaybackState::Ended) {
        SF_LOG_WARN("PlaybackController::stepBackward: invalid state");
        return false;
    }
    if (!player_->stepBackward()) {
        return false;
    }
    if (state_ == PlaybackState::Ended) {
        state_ = PlaybackState::Paused;
        emit stateChanged(state_);
    }
    return true;
}

bool PlaybackController::seek(std::int64_t timestampNs) {
    if (!player_) {
        return false;
    }
    if (state_ == PlaybackState::Idle || state_ == PlaybackState::Error) {
        return false;
    }
    if (!player_->seek(timestampNs)) {
        return false;
    }
    // If we were Ended and seek brought us back into the file,
    // transition to Paused.
    if (state_ == PlaybackState::Ended && !player_->atEnd()) {
        state_ = PlaybackState::Paused;
        emit stateChanged(state_);
    }
    emit positionChanged(player_->currentPositionNs(), player_->currentRecordIndex());
    return true;
}

bool PlaybackController::setSpeed(double factor) {
    if (!player_) {
        return false;
    }
    const bool wasPlaying = state_ == PlaybackState::Playing;
    if (wasPlaying) {
        player_->pause();
    }
    player_->setSpeed(factor);
    if (wasPlaying && !player_->atEnd()) {
        player_->play();
    }
    emit speedChanged(player_->currentSpeed());
    emit positionChanged(player_->currentPositionNs(), player_->currentRecordIndex());
    return true;
}

PlaybackState PlaybackController::state() const noexcept {
    return state_;
}

double PlaybackController::currentSpeed() const noexcept {
    return player_ ? player_->currentSpeed() : 1.0;
}

std::int64_t PlaybackController::currentPositionNs() const noexcept {
    return player_ ? player_->currentPositionNs() : 0;
}

std::size_t PlaybackController::currentRecordIndex() const noexcept {
    return player_ ? player_->currentRecordIndex() : 0;
}

std::int64_t PlaybackController::durationNs() const noexcept {
    return player_ ? player_->durationNs() : 0;
}

std::size_t PlaybackController::totalRecords() const noexcept {
    return player_ ? player_->totalRecords() : 0;
}

const QString& PlaybackController::lastError() const noexcept {
    return lastError_;
}

}  // namespace signalforge::replay
