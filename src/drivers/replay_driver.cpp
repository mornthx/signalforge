// src/drivers/replay_driver.cpp
#include "drivers/replay_driver.hpp"

#include "drivers/io_worker_base.hpp"
#include "frame/raw_frame.hpp"
#include "observability/logging.hpp"

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QString>
#include <QTimer>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>

namespace signalforge::drivers {

namespace {

constexpr std::int64_t kMinHeaderBytes = 16;
constexpr const char* kReplayMagic = "SFREPLAY";  // 8 ASCII bytes
constexpr std::int64_t kReplayMagicLen = 8;

DriverError makeError(DriverErrorCode code, QString message) {
    DriverError e;
    e.code = code;
    e.message = std::move(message);
    e.at = std::chrono::steady_clock::now();
    return e;
}

}  // namespace

// =======================================================================
// ReplayIoWorker: private worker living on the driver's dedicated QThread.
// Lifecycle-only in M3; actual file parsing and frame emission live in M9.
// =======================================================================

class ReplayIoWorker : public IoWorkerBase {
    Q_OBJECT

public:
    ReplayIoWorker(ReplayConfig config, QString threadName)
        : IoWorkerBase(std::move(threadName)), config_(std::move(config)) {}

public slots:
    /// Execute `open` on the IO thread: verify file existence and header.
    /// M9 keeps the file open after a successful open so start() can stream
    /// frames without re-opening.
    void openOnIoThread() {
        if (config_.playbackSpeed <= 0.0) {
            emit workerErrorOccurred(
                makeError(DriverErrorCode::ConfigInvalid,
                          QStringLiteral("Replay playbackSpeed must be > 0 (got %1)").arg(config_.playbackSpeed)));
            return;
        }
        if (file_) {
            file_->close();
            file_.reset();
        }
        file_ = std::make_unique<QFile>(config_.sessionFilePath);
        if (!file_->exists()) {
            emit workerErrorOccurred(
                makeError(DriverErrorCode::ResourceUnavailable,
                          QStringLiteral("Session file not found: %1").arg(config_.sessionFilePath)));
            file_.reset();
            return;
        }
        if (!file_->open(QIODevice::ReadOnly)) {
            emit workerErrorOccurred(
                makeError(DriverErrorCode::PermissionDenied, QStringLiteral("Could not open session file: %1 (%2)")
                                                                 .arg(config_.sessionFilePath, file_->errorString())));
            file_.reset();
            return;
        }
        const QByteArray header = file_->read(kMinHeaderBytes);
        if (header.size() < kMinHeaderBytes) {
            emit workerErrorOccurred(makeError(DriverErrorCode::ProtocolFailure,
                                               QStringLiteral("Session file too short: %1 bytes, expected ≥ %2")
                                                   .arg(header.size())
                                                   .arg(kMinHeaderBytes)));
            file_->close();
            file_.reset();
            return;
        }
        bool allZero = true;
        for (char c : header) {
            if (c != '\0') {
                allZero = false;
                break;
            }
        }
        if (allZero) {
            emit workerErrorOccurred(makeError(DriverErrorCode::ProtocolFailure,
                                               QStringLiteral("Session file header is empty (all zero bytes)")));
            file_->close();
            file_.reset();
            return;
        }
        // M9 frame-stream format: bytes after the 16-byte header are
        // a sequence of frame records IFF the header starts with the
        // SFREPLAY magic. Otherwise (e.g., legacy 0xAB-only fixtures
        // from M3) we treat the file as "header-only, no frames" and
        // the driver runs without emitting frames. This preserves M3
        // test fixtures.
        hasFrameStream_ = header.startsWith(QByteArray(kReplayMagic, kReplayMagicLen));
        framesEmitted_ = 0;
        emit workerOpened();
    }

    void closeOnIoThread() {
        stopTimer();
        if (file_) {
            file_->close();
            file_.reset();
        }
        emit workerClosed();
    }

    void startOnIoThread() {
        if (!file_) {
            emit workerErrorOccurred(makeError(DriverErrorCode::NotConfigured,
                                               QStringLiteral("ReplayIoWorker::startOnIoThread without open file")));
            return;
        }
        // Reset stream position to just after the header. This matches
        // the M2 "start emits frames at recorded intervals" semantics.
        if (!file_->seek(kMinHeaderBytes)) {
            emit workerErrorOccurred(
                makeError(DriverErrorCode::IoFailure, QStringLiteral("Failed to seek past header")));
            return;
        }
        prevFrameNanos_ = -1;
        emit workerStarted();
        if (hasFrameStream_) {
            scheduleNextFrame();
        }
        // No frame stream: driver stays in Running with no emissions
        // (M3 fixture compatibility).
    }

    void stopOnIoThread() {
        stopTimer();
        emit workerStopped();
    }

protected:
    void onStarted() override {
        // The streaming timer is constructed lazily inside this thread
        // so its parent affinity is correct.
        if (!streamTimer_) {
            streamTimer_ = new QTimer(this);
            streamTimer_->setSingleShot(true);
            streamTimer_->setTimerType(Qt::PreciseTimer);
            connect(streamTimer_, &QTimer::timeout, this, &ReplayIoWorker::onStreamTick);
        }
    }

private slots:
    void onStreamTick() {
        emitNextFrame();
    }

private:
    /// Read one frame record at the current file position and emit it.
    /// On EOF: if loop, seek back to start of frames and recurse;
    /// otherwise leave the timer stopped (driver stays Running idle).
    void emitNextFrame() {
        if (!file_ || !file_->isOpen() || !hasFrameStream_) {
            return;
        }
        if (file_->atEnd()) {
            if (config_.loop) {
                if (!file_->seek(kMinHeaderBytes)) {
                    return;
                }
                prevFrameNanos_ = -1;
            } else {
                // EOF without loop: just stop scheduling. Driver
                // remains Running so caller can choose to stop().
                return;
            }
        }

        // Frame record layout (V1):
        //   u64 nanosOffset (little-endian)
        //   u32 payloadLen  (little-endian)
        //   u8[payloadLen] payload
        QByteArray header = file_->read(12);
        if (header.size() < 12) {
            // Truncated record; treat as soft EOF.
            return;
        }
        std::uint64_t nanos = 0;
        std::uint32_t len = 0;
        std::memcpy(&nanos, header.constData(), 8);
        std::memcpy(&len, header.constData() + 8, 4);

        const QByteArray payload = file_->read(static_cast<qint64>(len));
        if (static_cast<std::uint32_t>(payload.size()) < len) {
            return;
        }

        signalforge::frame::RawFrame f;
        f.payload = payload;
        f.recvAt = std::chrono::steady_clock::now();
        // Synthesize a sourceId so downstream pipelines can route.
        f.sourceId = QStringLiteral("replay:") + QFileInfo(config_.sessionFilePath).fileName();
        emit frameOut(f);
        ++framesEmitted_;

        // Schedule next frame based on delta-nanos and playbackSpeed.
        std::int64_t deltaNanos = 0;
        if (prevFrameNanos_ >= 0 && nanos > static_cast<std::uint64_t>(prevFrameNanos_)) {
            deltaNanos = static_cast<std::int64_t>(nanos - prevFrameNanos_);
        }
        prevFrameNanos_ = static_cast<std::int64_t>(nanos);
        const double scaledMs = (deltaNanos / 1.0e6) / std::max(0.01, config_.playbackSpeed);
        const int waitMs = std::max(0, static_cast<int>(scaledMs));
        if (streamTimer_) {
            streamTimer_->start(waitMs);
        }
    }

    void scheduleNextFrame() {
        // First frame fires immediately so the driver shows liveness.
        if (streamTimer_) {
            streamTimer_->start(0);
        }
    }

    void stopTimer() {
        if (streamTimer_) {
            streamTimer_->stop();
        }
    }

signals:
    void workerOpened();
    void workerClosed();
    void workerStarted();
    void workerStopped();
    void workerErrorOccurred(const signalforge::drivers::DriverError& error);
    void frameOut(signalforge::frame::RawFrame frame);

private:
    ReplayConfig config_;
    std::unique_ptr<QFile> file_;
    QTimer* streamTimer_ = nullptr;
    bool hasFrameStream_ = false;
    std::int64_t prevFrameNanos_ = -1;
    long long framesEmitted_ = 0;
};

// =======================================================================
// ReplayDriver
// =======================================================================

ReplayDriver::ReplayDriver(ReplayConfig config, QObject* parent) : DriverInterface(parent), config_(std::move(config)) {
    const QString threadName = QStringLiteral("ReplayIO-") + QFileInfo(config_.sessionFilePath).fileName();
    thread_ = std::make_unique<QThread>();
    thread_->setObjectName(threadName);

    worker_ = std::make_unique<ReplayIoWorker>(config_, threadName);
    worker_->moveToThread(thread_.get());

    connect(thread_.get(), &QThread::started, worker_.get(), &IoWorkerBase::onThreadStart);

    connect(worker_.get(), &ReplayIoWorker::workerOpened, this, &ReplayDriver::onWorkerOpened, Qt::QueuedConnection);
    connect(worker_.get(), &ReplayIoWorker::workerClosed, this, &ReplayDriver::onWorkerClosed, Qt::QueuedConnection);
    connect(worker_.get(), &ReplayIoWorker::workerStarted, this, &ReplayDriver::onWorkerStarted, Qt::QueuedConnection);
    connect(worker_.get(), &ReplayIoWorker::workerStopped, this, &ReplayDriver::onWorkerStopped, Qt::QueuedConnection);
    connect(worker_.get(), &ReplayIoWorker::workerErrorOccurred, this, &ReplayDriver::onWorkerErrorOccurred,
            Qt::QueuedConnection);
    connect(worker_.get(), &ReplayIoWorker::frameOut, this, &ReplayDriver::onWorkerFrame, Qt::QueuedConnection);

    thread_->start();
}

ReplayDriver::~ReplayDriver() {
    if (state_.load(std::memory_order_acquire) != DriverState::Idle) {
        SF_LOG_WARN("ReplayDriver destroyed in non-Idle state; callers should close() first");
    }
    if (thread_ && thread_->isRunning()) {
        thread_->quit();
        if (!thread_->wait(500)) {
            SF_LOG_ERROR("ReplayDriver IO thread did not exit within 500ms; forcing terminate");
            thread_->terminate();
            thread_->wait();
        }
    }
}

DriverErrorCode ReplayDriver::open() {
    if (config_.sessionFilePath.isEmpty()) {
        SF_LOG_ERROR("ReplayDriver: sessionFilePath is empty; refusing open()");
        return DriverErrorCode::ConfigInvalid;
    }
    const auto expected = DriverState::Idle;
    if (state_.load(std::memory_order_acquire) != expected) {
        return DriverErrorCode::NotConfigured;
    }
    transitionTo(DriverState::Opening);
    QMetaObject::invokeMethod(worker_.get(), "openOnIoThread", Qt::QueuedConnection);
    return DriverErrorCode::Success;
}

void ReplayDriver::close() {
    const auto s = state_.load(std::memory_order_acquire);
    if (s == DriverState::Idle || s == DriverState::Closing) {
        return;  // idempotent
    }
    transitionTo(DriverState::Closing);
    QMetaObject::invokeMethod(worker_.get(), "closeOnIoThread", Qt::QueuedConnection);
}

DriverErrorCode ReplayDriver::start() {
    if (state_.load(std::memory_order_acquire) != DriverState::Open) {
        return DriverErrorCode::NotConfigured;
    }
    QMetaObject::invokeMethod(worker_.get(), "startOnIoThread", Qt::QueuedConnection);
    return DriverErrorCode::Success;
}

void ReplayDriver::stop() {
    if (state_.load(std::memory_order_acquire) != DriverState::Running) {
        return;  // idempotent
    }
    transitionTo(DriverState::Stopping);
    QMetaObject::invokeMethod(worker_.get(), "stopOnIoThread", Qt::QueuedConnection);
}

DriverErrorCode ReplayDriver::write(const QByteArray& /*payload*/) {
    // ReplayDriver is read-only; explicit failure makes misuse obvious.
    return DriverErrorCode::NotConfigured;
}

DriverState ReplayDriver::state() const {
    return state_.load(std::memory_order_acquire);
}

DriverErrorCode ReplayDriver::health() const {
    return health_.load(std::memory_order_acquire);
}

signalforge::frame::DriverStatistics ReplayDriver::statistics() const {
    signalforge::frame::DriverStatistics s;
    // All counters remain 0 throughout M3 (no frames emitted, no bytes).
    // `snapshotAt` updates per call so consumers can distinguish snapshots.
    s.snapshotAt = std::chrono::steady_clock::now();
    return s;
}

const ReplayConfig& ReplayDriver::config() const noexcept {
    return config_;
}

// ---------------------------------------------------------------------
// Worker-signal slots. Each runs on the driver's own thread (queued).
// ---------------------------------------------------------------------

void ReplayDriver::onWorkerOpened() {
    transitionTo(DriverState::Open);
}

void ReplayDriver::onWorkerClosed() {
    transitionTo(DriverState::Idle);
}

void ReplayDriver::onWorkerStarted() {
    transitionTo(DriverState::Running);
}

void ReplayDriver::onWorkerStopped() {
    transitionTo(DriverState::Open);
}

void ReplayDriver::onWorkerErrorOccurred(const DriverError& err) {
    health_.store(err.code, std::memory_order_release);
    emit errorOccurred(err);
    transitionTo(DriverState::Error);
}

void ReplayDriver::onWorkerFrame(signalforge::frame::RawFrame frame) {
    emit frameReceived(std::move(frame));
}

void ReplayDriver::transitionTo(DriverState newState) {
    state_.store(newState, std::memory_order_release);
    emit stateChanged(newState);
}

}  // namespace signalforge::drivers

// The ReplayIoWorker is defined entirely in this translation unit; the
// moc include picks up its Q_OBJECT metadata.
#include "replay_driver.moc"
