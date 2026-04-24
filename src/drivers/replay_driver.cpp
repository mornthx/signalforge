// src/drivers/replay_driver.cpp
#include "drivers/replay_driver.hpp"

#include "drivers/io_worker_base.hpp"
#include "observability/logging.hpp"

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QString>
#include <chrono>

namespace signalforge::drivers {

namespace {

constexpr std::int64_t kMinHeaderBytes = 16;

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
    void openOnIoThread() {
        QFile f(config_.sessionFilePath);
        if (!f.exists()) {
            emit workerErrorOccurred(
                makeError(DriverErrorCode::ResourceUnavailable,
                          QStringLiteral("Session file not found: %1").arg(config_.sessionFilePath)));
            return;
        }
        if (!f.open(QIODevice::ReadOnly)) {
            emit workerErrorOccurred(makeError(
                DriverErrorCode::PermissionDenied,
                QStringLiteral("Could not open session file: %1 (%2)").arg(config_.sessionFilePath, f.errorString())));
            return;
        }
        const QByteArray header = f.read(kMinHeaderBytes);
        f.close();
        if (header.size() < kMinHeaderBytes) {
            emit workerErrorOccurred(makeError(DriverErrorCode::ProtocolFailure,
                                               QStringLiteral("Session file too short: %1 bytes, expected ≥ %2")
                                                   .arg(header.size())
                                                   .arg(kMinHeaderBytes)));
            return;
        }
        // Reject all-zero header as a sentinel "empty" marker. Real magic-
        // byte validation is M9.
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
            return;
        }
        // TODO(M9): parse full session metadata (clock origin, format
        // version, stream descriptor) and keep the file open for streaming.
        emit workerOpened();
    }

    void closeOnIoThread() {
        emit workerClosed();
    }

    void startOnIoThread() {
        // TODO(M9): open file for reading, start replay timer, begin
        // emitting frames at recorded intervals.
        emit workerStarted();
    }

    void stopOnIoThread() {
        // TODO(M9): stop replay timer, close read-position file handle.
        emit workerStopped();
    }

protected:
    void onStarted() override {
        // TODO(M9): initialize replay timer, buffers, etc. For skeleton,
        // nothing to do.
    }

signals:
    void workerOpened();
    void workerClosed();
    void workerStarted();
    void workerStopped();
    void workerErrorOccurred(const signalforge::drivers::DriverError& error);

private:
    ReplayConfig config_;
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

void ReplayDriver::transitionTo(DriverState newState) {
    state_.store(newState, std::memory_order_release);
    emit stateChanged(newState);
}

}  // namespace signalforge::drivers

// The ReplayIoWorker is defined entirely in this translation unit; the
// moc include picks up its Q_OBJECT metadata.
#include "replay_driver.moc"
