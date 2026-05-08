// src/replay/session_player.cpp
//
// S1 stub: methods compile but perform no I/O.
// S3 fills openFile / closeFile + worker thread setup.
// S4 fills the timing dispatch loop.
// S5 fills setSpeed / pause / play / stepForward + 30 Hz throttle.
// S6 fills seek / stepBackward.
#include "replay/session_player.hpp"

#include "observability/logging.hpp"

#include <QThread>

namespace signalforge::replay {

SessionPlayer::SessionPlayer(signalforge::decoder::SignalValueSink& sink, QObject* parent)
    : QObject(parent), sink_(&sink) {
    // Worker thread + reader created in openFile() at S3.
}

SessionPlayer::~SessionPlayer() = default;

bool SessionPlayer::openFile(const QString& filePath) {
    (void)filePath;
    SF_LOG_INFO("SessionPlayer::openFile not yet implemented (S3)");
    return false;
}

void SessionPlayer::closeFile() {
    SF_LOG_INFO("SessionPlayer::closeFile not yet implemented (S3)");
}

bool SessionPlayer::isOpen() const noexcept {
    return reader_ != nullptr;
}

void SessionPlayer::play() {
    // S4: start worker dispatch loop.
}

void SessionPlayer::pause() {
    // S4: flip atomic flag.
}

bool SessionPlayer::stepForward() {
    // S5: synchronous one-record dispatch.
    return false;
}

bool SessionPlayer::stepBackward() {
    // S6: seek + stepForward.
    return false;
}

bool SessionPlayer::seek(std::int64_t timestampNs) {
    // S6: SessionReader::seekToTimestamp.
    (void)timestampNs;
    return false;
}

void SessionPlayer::setSpeed(double factor) {
    // S5: clamped atomic store.
    (void)factor;
}

bool SessionPlayer::isPlaying() const noexcept {
    return playing_.load();
}

double SessionPlayer::currentSpeed() const noexcept {
    return speedFactor_.load();
}

std::int64_t SessionPlayer::currentPositionNs() const noexcept {
    return currentPosNs_.load();
}

std::size_t SessionPlayer::currentRecordIndex() const noexcept {
    return currentRecordIdx_.load();
}

std::int64_t SessionPlayer::durationNs() const noexcept {
    return durationNs_.load();
}

std::size_t SessionPlayer::totalRecords() const noexcept {
    return totalRecords_.load();
}

bool SessionPlayer::atEnd() const noexcept {
    return atEnd_.load();
}

}  // namespace signalforge::replay
