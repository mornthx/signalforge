// src/replay/playback_controller.cpp
//
// S1 stub: methods compile but contain no behaviour beyond state
// initialisation. State machine + signal fan-out from SessionPlayer
// land at S7. SessionPlayer construction occurs at S3.
#include "replay/playback_controller.hpp"

#include "buffer/signal_buffer_registry.hpp"
#include "observability/logging.hpp"

namespace signalforge::replay {

PlaybackController::PlaybackController(signalforge::buffer::SignalBufferRegistry& registry, QObject* parent)
    : QObject(parent), registry_(&registry), state_(PlaybackState::Idle) {
    // Player_ is created lazily in loadSession() at S7.
}

PlaybackController::~PlaybackController() = default;

bool PlaybackController::loadSession(const QString& filePath) {
    // S7: drive SessionPlayer::openFile + state transition.
    (void)filePath;
    SF_LOG_INFO("PlaybackController::loadSession not yet implemented (S7)");
    return false;
}

void PlaybackController::closeSession() {
    // S7: stop player + state → Idle.
    SF_LOG_INFO("PlaybackController::closeSession not yet implemented (S7)");
}

QString PlaybackController::currentFilePath() const {
    return currentFilePath_;
}

bool PlaybackController::play() {
    // S7: validate state + delegate to SessionPlayer::play.
    return false;
}

bool PlaybackController::pause() {
    // S7: validate state + delegate to SessionPlayer::pause.
    return false;
}

bool PlaybackController::stepForward() {
    // S7: validate state + delegate.
    return false;
}

bool PlaybackController::stepBackward() {
    // S7: validate state + delegate.
    return false;
}

bool PlaybackController::seek(std::int64_t timestampNs) {
    // S7: validate state + delegate.
    (void)timestampNs;
    return false;
}

bool PlaybackController::setSpeed(double factor) {
    // S7: validate range + delegate.
    (void)factor;
    return false;
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
