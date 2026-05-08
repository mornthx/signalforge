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
#include <algorithm>

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

    // Worker thread is created here but not started until play()
    // (S4). closeFile() handles the case where it never started.
    workerThread_ = std::make_unique<QThread>();
    workerThread_->setObjectName(QStringLiteral("session-player-worker"));

    // Capture an open-time steady-clock origin so the sink sees
    // monotonic chrono timestamps consistent with live mode (per
    // M10 SessionReader::replayAll's contract).
    openTimeSteady_ = std::chrono::steady_clock::now();

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
    pendingRecord_.reset();
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
    if (!reader_ || !workerThread_) {
        SF_LOG_WARN("SessionPlayer::play called without an open file");
        return;
    }
    if (playing_.load()) {
        return;
    }
    if (atEnd_.load()) {
        // End-of-file reached previously; caller must `seek()`
        // first to rewind. No automatic rewind on play().
        SF_LOG_INFO("SessionPlayer::play called at end-of-file; seek before resuming");
        return;
    }

    playing_.store(true, std::memory_order_release);

    // Re-arm the thread on every play(): each pause() exits the
    // dispatchLoop and quits the thread, so play() needs a fresh
    // started() connection. This is heavier than a condvar but
    // simpler and adequate for V1's pause / resume cadence (user
    // clicks; not high-frequency).
    disconnect(workerThread_.get(), &QThread::started, this, nullptr);
    connect(workerThread_.get(), &QThread::started, this, [this]() { dispatchLoop(); }, Qt::DirectConnection);
    if (!workerThread_->isRunning()) {
        workerThread_->start();
    }
}

void SessionPlayer::pause() {
    if (!playing_.load()) {
        return;
    }
    playing_.store(false, std::memory_order_release);
    // Wait for dispatchLoop to observe the flag and return, then
    // quit the worker's event loop and join. Safe to call from
    // the main thread because workerThread_->wait() blocks here
    // until the thread terminates (microseconds at worst — the
    // loop only sleeps in nanosecond chunks per iteration in
    // S4's 1× cadence).
    if (workerThread_ && workerThread_->isRunning()) {
        workerThread_->quit();
        workerThread_->wait();
    }
}

bool SessionPlayer::stepForward() {
    if (!reader_) {
        SF_LOG_WARN("SessionPlayer::stepForward called without an open file");
        return false;
    }
    if (playing_.load()) {
        SF_LOG_WARN("SessionPlayer::stepForward called while playing; pause first");
        return false;
    }
    if (atEnd_.load() && !pendingRecord_) {
        return false;
    }

    signalforge::session::ReplayRecord rec;
    if (pendingRecord_) {
        // Drain a pause-saved record first.
        rec = *pendingRecord_;
        pendingRecord_.reset();
    } else {
        if (!reader_->readNextRecord(rec)) {
            atEnd_.store(true, std::memory_order_release);
            return false;
        }
    }

    // Step is synchronous on the main thread, so the sink can be
    // called directly without queued dispatch.
    const auto t = openTimeSteady_ + std::chrono::nanoseconds{rec.timestampNs};
    sink_->onSignal(t, rec.signalId, rec.value);

    currentPosNs_.store(rec.timestampNs, std::memory_order_release);
    const std::size_t newIdx = currentRecordIdx_.fetch_add(1, std::memory_order_acq_rel) + 1;
    emit positionUpdated(rec.timestampNs, newIdx);
    return true;
}

bool SessionPlayer::stepBackward() {
    if (!reader_) {
        return false;
    }
    if (playing_.load()) {
        SF_LOG_WARN("SessionPlayer::stepBackward called while playing; pause first");
        return false;
    }
    const std::size_t curIdx = currentRecordIdx_.load();
    if (curIdx == 0) {
        // Already at the start; nothing to back up to.
        return false;
    }

    // V1 strategy: rewind to start, replay forward to (curIdx - 1).
    // O(N) on the record count — acceptable per M11 spec §9 Note 2.
    const std::size_t targetIdx = curIdx - 1;
    if (!seek(0)) {
        return false;
    }
    for (std::size_t i = 0; i < targetIdx; ++i) {
        if (!stepForward()) {
            return false;
        }
    }
    return true;
}

bool SessionPlayer::seek(std::int64_t timestampNs) {
    if (!reader_) {
        SF_LOG_WARN("SessionPlayer::seek called without an open file");
        return false;
    }
    const bool wasPlaying = playing_.load();
    if (wasPlaying) {
        pause();  // blocks until worker exits
    }
    // Discard any record the worker had read but not dispatched
    // before pausing — the seek invalidates it.
    pendingRecord_.reset();

    if (!reader_->seekToTimestamp(timestampNs)) {
        return false;
    }

    // Sync player counters from reader's post-seek state.
    currentPosNs_.store(reader_->currentTimestampNs(), std::memory_order_release);
    currentRecordIdx_.store(reader_->currentRecordIndex(), std::memory_order_release);
    atEnd_.store(reader_->atEnd(), std::memory_order_release);

    if (wasPlaying && !atEnd_.load()) {
        play();
    }
    return true;
}

void SessionPlayer::setSpeed(double factor) {
    constexpr double kMinSpeed = 0.5;
    constexpr double kMaxSpeed = 10.0;
    if (factor < kMinSpeed || factor > kMaxSpeed) {
        SF_LOG_WARN("SessionPlayer::setSpeed: {} out of range [{}, {}]; clamping", factor, kMinSpeed, kMaxSpeed);
        factor = std::clamp(factor, kMinSpeed, kMaxSpeed);
    }
    speedFactor_.store(factor, std::memory_order_release);
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

void SessionPlayer::dispatchLoop() {
    if (!reader_) {
        return;
    }

    std::int64_t prevTs = currentPosNs_.load();
    std::chrono::steady_clock::time_point lastEmitTime{};

    while (playing_.load(std::memory_order_acquire)) {
        if (workerThread_ && workerThread_->isInterruptionRequested()) {
            break;
        }

        signalforge::session::ReplayRecord rec;
        if (pendingRecord_) {
            // Drained from a prior pause that interrupted between
            // read and dispatch. Re-use the same record so no event
            // is lost across pause/resume.
            rec = *pendingRecord_;
            pendingRecord_.reset();
        } else {
            if (!reader_->readNextRecord(rec)) {
                atEnd_.store(true, std::memory_order_release);
                playing_.store(false, std::memory_order_release);
                QMetaObject::invokeMethod(this, [this]() { emit endReached(); }, Qt::QueuedConnection);
                return;
            }
        }

        // Compute scaled delay since previous dispatch. At 1×
        // speed (S4 default) this is real-time; S5 will add
        // non-1× scaling. Sleep is chunked so `pause()` can break
        // a long inter-record wait within ~5 ms.
        const std::int64_t deltaNs = rec.timestampNs - prevTs;
        if (deltaNs > 0) {
            const double speed = speedFactor_.load(std::memory_order_acquire);
            const std::int64_t scaledNs = static_cast<std::int64_t>(static_cast<double>(deltaNs) / speed);
            constexpr auto kChunk = std::chrono::milliseconds{5};
            auto remaining = std::chrono::nanoseconds{scaledNs};
            while (remaining.count() > 0 && playing_.load(std::memory_order_acquire)) {
                const auto chunk =
                    (remaining > std::chrono::nanoseconds{kChunk}) ? std::chrono::nanoseconds{kChunk} : remaining;
                std::this_thread::sleep_for(chunk);
                remaining -= chunk;
            }
            if (!playing_.load(std::memory_order_acquire)) {
                // Pause interrupted mid-sleep. Save the record so
                // resume picks it up without re-reading (which
                // would skip past it on the file pointer).
                pendingRecord_ = rec;
                break;
            }
        }

        // Update counters before dispatch so pause / state-query
        // observers see the correct position promptly.
        currentPosNs_.store(rec.timestampNs, std::memory_order_release);
        currentRecordIdx_.fetch_add(1, std::memory_order_acq_rel);

        // Dispatch onSignal directly on the worker thread. The
        // production sink (SignalBufferRegistry, M6) is documented
        // thread-safe; bench's SilentSink is trivially safe. This
        // avoids per-record queued-event overhead which was the
        // 10× bottleneck (S10 finding: queued dispatch capped at
        // ~170 k records/sec on this host).
        //
        // Sinks that are NOT thread-safe must be wrapped behind a
        // TeeSignalValueSink-style adapter; M11 doesn't introduce
        // any in V1.
        const auto dispatchTs = openTimeSteady_ + std::chrono::nanoseconds{rec.timestampNs};
        sink_->onSignal(dispatchTs, rec.signalId, rec.value);

        // 30 Hz position throttle (C5). The first record always
        // emits; subsequent ones are skipped until 33 ms elapse.
        const auto now = std::chrono::steady_clock::now();
        if (lastEmitTime.time_since_epoch().count() == 0 || (now - lastEmitTime) >= std::chrono::milliseconds(33)) {
            const std::int64_t emitTs = rec.timestampNs;
            const std::size_t emitIdx = currentRecordIdx_.load(std::memory_order_acquire);
            QMetaObject::invokeMethod(
                this, [this, emitTs, emitIdx]() { emit positionUpdated(emitTs, emitIdx); }, Qt::QueuedConnection);
            lastEmitTime = now;
        }

        prevTs = rec.timestampNs;
    }

    // Drop here when paused. The thread loop exits but the QThread
    // event loop keeps running; play() can re-enter dispatchLoop
    // by re-emitting started() — but in practice we re-arm via
    // a fresh connect() on each play() call.
}

}  // namespace signalforge::replay
