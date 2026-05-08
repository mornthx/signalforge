// src/replay/session_player.cpp
//
// S3 — file open/close lifecycle + worker QThread plumbing.
// S4 will add the timing dispatch loop on the worker thread.
// S5 fills in pause / play / stepForward + 30 Hz throttle.
// S6 fills seek / stepBackward.
#include "replay/session_player.hpp"

#include "observability/logging.hpp"
#include "session/session_metadata.hpp"

#include <QMetaObject>
#include <QThread>
#include <Qt>

namespace signalforge::replay {

SessionPlayer::SessionPlayer(signalforge::decoder::SignalValueSink& sink, QObject* parent)
    : QObject(parent), sink_(&sink) {}

SessionPlayer::~SessionPlayer() {
    // Defensive: if a file is still open at destruction (app
    // shutdown, owner forgot to closeFile), drain cleanly. Avoid
    // throwing through the destructor.
    closeFile();
}

bool SessionPlayer::openFile(const QString& filePath) {
    if (reader_) {
        SF_LOG_WARN("SessionPlayer::openFile called while another file is open; closing first");
        closeFile();
    }

    auto reader = std::make_unique<signalforge::session::SessionReader>();
    if (!reader->open(filePath)) {
        SF_LOG_ERROR("SessionPlayer::openFile: failed to open {}", filePath.toStdString());
        return false;
    }

    // Catalog sink wiring: streaming-mode catalog extensions are
    // forwarded to the same SignalValueSink the worker dispatches
    // signals to. Sink lives on the main thread; we call into it
    // via QMetaObject::invokeMethod from the worker (S4).
    reader->bindCatalogSink(sink_);

    // Initial catalog announcement: fire once at openFile time so
    // the sink (M6 SignalBufferRegistry, in production) sets up
    // buffers before the first record arrives. Mirrors M10
    // SessionReader::replayAll's first onSignalsRegistered call.
    sink_->onSignalsRegistered(QStringLiteral("session-replay"), reader->metadata().signalCatalog);

    // Capture file-derived counters: total records + duration come
    // from SessionReader's pre-scan (M11 S2 addition).
    durationNs_.store(reader->lastTimestampNs(), std::memory_order_relaxed);
    totalRecords_.store(reader->footerRecordCount(), std::memory_order_relaxed);
    currentPosNs_.store(0, std::memory_order_relaxed);
    currentRecordIdx_.store(0, std::memory_order_relaxed);
    atEnd_.store(false, std::memory_order_relaxed);
    playing_.store(false, std::memory_order_relaxed);
    speedFactor_.store(1.0, std::memory_order_relaxed);
    currentFilePath_ = filePath;

    // Worker thread is created here but not started until play() in
    // S4. closeFile() handles the case where it never started.
    workerThread_ = std::make_unique<QThread>();
    workerThread_->setObjectName(QStringLiteral("session-player-worker"));

    reader_ = std::move(reader);
    return true;
}

void SessionPlayer::closeFile() {
    if (workerThread_) {
        playing_.store(false, std::memory_order_relaxed);
        if (workerThread_->isRunning()) {
            workerThread_->requestInterruption();
            workerThread_->quit();
            workerThread_->wait();
        }
        workerThread_.reset();
    }
    if (reader_) {
        reader_->close();
        reader_.reset();
    }
    currentFilePath_.clear();
    currentPosNs_.store(0, std::memory_order_relaxed);
    currentRecordIdx_.store(0, std::memory_order_relaxed);
    durationNs_.store(0, std::memory_order_relaxed);
    totalRecords_.store(0, std::memory_order_relaxed);
    atEnd_.store(false, std::memory_order_relaxed);
    playing_.store(false, std::memory_order_relaxed);
}

bool SessionPlayer::isOpen() const noexcept {
    return reader_ != nullptr;
}

void SessionPlayer::play() {
    // S4: connect worker thread started() to the dispatch loop and
    // call workerThread_->start(). For S3, this is a no-op.
    SF_LOG_INFO("SessionPlayer::play not yet implemented (S4)");
}

void SessionPlayer::pause() {
    // S5: flip atomic playing_ → false.
    SF_LOG_INFO("SessionPlayer::pause not yet implemented (S5)");
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
