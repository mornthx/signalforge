// src/drivers/tcp_driver.cpp
#include "drivers/tcp_driver.hpp"

#include "drivers/io_worker_base.hpp"
#include "frame/raw_frame.hpp"
#include "observability/logging.hpp"

#include <QAbstractSocket>
#include <QByteArray>
#include <QMetaObject>
#include <QString>
#include <QTcpSocket>
#include <QTimer>
#include <chrono>

namespace signalforge::drivers {

namespace {

DriverError makeError(DriverErrorCode code, QString message) {
    DriverError e;
    e.code = code;
    e.message = std::move(message);
    e.at = std::chrono::steady_clock::now();
    return e;
}

DriverErrorCode mapTcpError(QAbstractSocket::SocketError err) {
    switch (err) {
    case QAbstractSocket::HostNotFoundError:
    case QAbstractSocket::ConnectionRefusedError:
        return DriverErrorCode::ResourceUnavailable;
    case QAbstractSocket::RemoteHostClosedError:
    case QAbstractSocket::NetworkError:
        return DriverErrorCode::ResourceLost;
    case QAbstractSocket::SocketAccessError:
        return DriverErrorCode::PermissionDenied;
    case QAbstractSocket::SocketTimeoutError:
        return DriverErrorCode::Timeout;
    default:
        return DriverErrorCode::IoFailure;
    }
}

DriverErrorCode validateConfig(const TcpConfig& cfg, QString& message) {
    if (cfg.host.isEmpty()) {
        message = QStringLiteral("TCP host is empty");
        return DriverErrorCode::ConfigInvalid;
    }
    if (cfg.port == 0) {
        message = QStringLiteral("TCP port must be > 0");
        return DriverErrorCode::ConfigInvalid;
    }
    if (cfg.connectTimeout <= std::chrono::milliseconds::zero()) {
        message = QStringLiteral("TCP connectTimeout must be > 0");
        return DriverErrorCode::ConfigInvalid;
    }
    return DriverErrorCode::Success;
}

}  // namespace

class TcpIoWorker : public IoWorkerBase {
    Q_OBJECT

public:
    TcpIoWorker(TcpConfig config, QString threadName)
        : IoWorkerBase(std::move(threadName)), config_(std::move(config)) {}

public slots:
    void openOnIoThread() {
        if (socket_ != nullptr) {
            emit workerOpened();
            return;
        }
        socket_ = new QTcpSocket(this);
        connect(socket_, &QTcpSocket::connected, this, &TcpIoWorker::onSocketConnected);
        connect(socket_, &QTcpSocket::errorOccurred, this, &TcpIoWorker::onSocketError);
        connect(socket_, &QTcpSocket::disconnected, this, &TcpIoWorker::onSocketDisconnected);
        connect(socket_, &QTcpSocket::readyRead, this, &TcpIoWorker::onReadyRead);

        timeoutTimer_ = new QTimer(this);
        timeoutTimer_->setSingleShot(true);
        connect(timeoutTimer_, &QTimer::timeout, this, &TcpIoWorker::onConnectTimeout);
        timeoutTimer_->start(static_cast<int>(config_.connectTimeout.count()));

        socket_->connectToHost(config_.host, config_.port);
    }

    void closeOnIoThread() {
        if (timeoutTimer_ != nullptr) {
            timeoutTimer_->stop();
            timeoutTimer_->deleteLater();
            timeoutTimer_ = nullptr;
        }
        if (socket_ != nullptr) {
            socket_->disconnect(this);  // prevent disconnect-during-close signals
            if (socket_->state() != QAbstractSocket::UnconnectedState) {
                socket_->abort();
            }
            socket_->deleteLater();
            socket_ = nullptr;
        }
        running_ = false;
        emit workerClosed();
    }

    void startOnIoThread() {
        running_ = true;
        emit workerStarted();
    }

    void stopOnIoThread() {
        running_ = false;
        emit workerStopped();
    }

    void writeOnIoThread(QByteArray payload) {
        if (socket_ == nullptr || !running_ || socket_->state() != QAbstractSocket::ConnectedState) {
            emit workerTxFailure();
            return;
        }
        const auto n = socket_->write(payload);
        if (n < 0) {
            emit workerTxFailure();
            return;
        }
        emit workerTxComplete(static_cast<std::uint64_t>(n));
    }

protected:
    void onStarted() override {}

private slots:
    void onSocketConnected() {
        if (timeoutTimer_ != nullptr) {
            timeoutTimer_->stop();
        }
        emit workerOpened();
    }

    void onConnectTimeout() {
        if (socket_ == nullptr) {
            return;
        }
        if (socket_->state() == QAbstractSocket::ConnectedState) {
            return;  // race: connected just before timer fired
        }
        socket_->abort();
        emit workerErrorOccurred(
            makeError(DriverErrorCode::Timeout, QStringLiteral("TCP connect timed out after %1 ms to %2:%3")
                                                    .arg(config_.connectTimeout.count())
                                                    .arg(config_.host)
                                                    .arg(config_.port)));
    }

    void onSocketError(QAbstractSocket::SocketError err) {
        const auto code = mapTcpError(err);
        const auto msg = QStringLiteral("TCP error to %1:%2: %3")
                             .arg(config_.host)
                             .arg(config_.port)
                             .arg(socket_ != nullptr ? socket_->errorString() : QStringLiteral("(socket gone)"));
        emit workerErrorOccurred(makeError(code, msg));
    }

    void onSocketDisconnected() {
        if (!running_) {
            return;  // we initiated the disconnect
        }
        emit workerErrorOccurred(
            makeError(DriverErrorCode::ResourceLost,
                      QStringLiteral("TCP peer closed connection to %1:%2").arg(config_.host).arg(config_.port)));
    }

    void onReadyRead() {
        if (!running_ || socket_ == nullptr) {
            return;
        }
        const QByteArray bytes = socket_->readAll();
        if (bytes.isEmpty()) {
            return;
        }
        signalforge::frame::RawFrame frame;
        frame.sourceId = QStringLiteral("tcp:%1:%2").arg(config_.host).arg(config_.port);
        frame.payload = bytes;
        frame.recvAt = std::chrono::steady_clock::now();
        frame.protocolHint = QStringLiteral("tcp");
        emit workerFrameReceived(frame);
    }

signals:
    void workerOpened();
    void workerClosed();
    void workerStarted();
    void workerStopped();
    void workerErrorOccurred(const signalforge::drivers::DriverError& error);
    void workerFrameReceived(const signalforge::frame::RawFrame& frame);
    void workerTxComplete(std::uint64_t bytesWritten);
    void workerTxFailure();

private:
    TcpConfig config_;
    QTcpSocket* socket_ = nullptr;
    QTimer* timeoutTimer_ = nullptr;
    bool running_ = false;
};

// =======================================================================
// TcpDriver
// =======================================================================

TcpDriver::TcpDriver(TcpConfig config, QObject* parent) : DriverInterface(parent), config_(std::move(config)) {
    const QString threadName = QStringLiteral("TcpIO-%1:%2").arg(config_.host).arg(config_.port);
    thread_ = std::make_unique<QThread>();
    thread_->setObjectName(threadName);

    worker_ = std::make_unique<TcpIoWorker>(config_, threadName);
    worker_->moveToThread(thread_.get());

    connect(thread_.get(), &QThread::started, worker_.get(), &IoWorkerBase::onThreadStart);

    connect(worker_.get(), &TcpIoWorker::workerOpened, this, &TcpDriver::onWorkerOpened, Qt::QueuedConnection);
    connect(worker_.get(), &TcpIoWorker::workerClosed, this, &TcpDriver::onWorkerClosed, Qt::QueuedConnection);
    connect(worker_.get(), &TcpIoWorker::workerStarted, this, &TcpDriver::onWorkerStarted, Qt::QueuedConnection);
    connect(worker_.get(), &TcpIoWorker::workerStopped, this, &TcpDriver::onWorkerStopped, Qt::QueuedConnection);
    connect(worker_.get(), &TcpIoWorker::workerErrorOccurred, this, &TcpDriver::onWorkerErrorOccurred,
            Qt::QueuedConnection);
    connect(worker_.get(), &TcpIoWorker::workerFrameReceived, this, &TcpDriver::onWorkerFrameReceived,
            Qt::QueuedConnection);
    connect(worker_.get(), &TcpIoWorker::workerTxComplete, this, &TcpDriver::onWorkerTxComplete, Qt::QueuedConnection);
    connect(worker_.get(), &TcpIoWorker::workerTxFailure, this, &TcpDriver::onWorkerTxFailure, Qt::QueuedConnection);

    thread_->start();
}

TcpDriver::~TcpDriver() {
    if (state_.load(std::memory_order_acquire) != DriverState::Idle) {
        SF_LOG_WARN("TcpDriver destroyed in non-Idle state; callers should close() first");
    }
    if (thread_ && thread_->isRunning()) {
        thread_->quit();
        if (!thread_->wait(500)) {
            SF_LOG_ERROR("TcpDriver IO thread did not exit within 500ms; forcing terminate");
            thread_->terminate();
            thread_->wait();
        }
    }
}

DriverErrorCode TcpDriver::open() {
    QString msg;
    if (const auto code = validateConfig(config_, msg); code != DriverErrorCode::Success) {
        SF_LOG_ERROR("TcpDriver: {}", msg.toStdString());
        return code;
    }
    if (state_.load(std::memory_order_acquire) != DriverState::Idle) {
        return DriverErrorCode::NotConfigured;
    }
    transitionTo(DriverState::Opening);
    QMetaObject::invokeMethod(worker_.get(), "openOnIoThread", Qt::QueuedConnection);
    return DriverErrorCode::Success;
}

void TcpDriver::close() {
    const auto s = state_.load(std::memory_order_acquire);
    if (s == DriverState::Idle || s == DriverState::Closing) {
        return;
    }
    transitionTo(DriverState::Closing);
    QMetaObject::invokeMethod(worker_.get(), "closeOnIoThread", Qt::QueuedConnection);
}

DriverErrorCode TcpDriver::start() {
    if (state_.load(std::memory_order_acquire) != DriverState::Open) {
        return DriverErrorCode::NotConfigured;
    }
    QMetaObject::invokeMethod(worker_.get(), "startOnIoThread", Qt::QueuedConnection);
    return DriverErrorCode::Success;
}

void TcpDriver::stop() {
    if (state_.load(std::memory_order_acquire) != DriverState::Running) {
        return;
    }
    transitionTo(DriverState::Stopping);
    QMetaObject::invokeMethod(worker_.get(), "stopOnIoThread", Qt::QueuedConnection);
}

DriverErrorCode TcpDriver::write(const QByteArray& payload) {
    if (state_.load(std::memory_order_acquire) != DriverState::Running) {
        return DriverErrorCode::NotConfigured;
    }
    txFrames_.fetch_add(1, std::memory_order_relaxed);
    QMetaObject::invokeMethod(worker_.get(), "writeOnIoThread", Qt::QueuedConnection, Q_ARG(QByteArray, payload));
    return DriverErrorCode::Success;
}

DriverState TcpDriver::state() const {
    return state_.load(std::memory_order_acquire);
}

DriverErrorCode TcpDriver::health() const {
    return health_.load(std::memory_order_acquire);
}

signalforge::frame::DriverStatistics TcpDriver::statistics() const {
    signalforge::frame::DriverStatistics s;
    s.rx.framesTotal = rxFrames_.load(std::memory_order_relaxed);
    s.rx.bytesTotal = rxBytes_.load(std::memory_order_relaxed);
    s.rx.errors = rxErrors_.load(std::memory_order_relaxed);
    s.tx.framesTotal = txFrames_.load(std::memory_order_relaxed);
    s.tx.bytesTotal = txBytes_.load(std::memory_order_relaxed);
    s.tx.failures = txFailures_.load(std::memory_order_relaxed);
    s.snapshotAt = std::chrono::steady_clock::now();
    return s;
}

const TcpConfig& TcpDriver::config() const noexcept {
    return config_;
}

void TcpDriver::onWorkerOpened() {
    transitionTo(DriverState::Open);
}
void TcpDriver::onWorkerClosed() {
    transitionTo(DriverState::Idle);
}
void TcpDriver::onWorkerStarted() {
    transitionTo(DriverState::Running);
}
void TcpDriver::onWorkerStopped() {
    transitionTo(DriverState::Open);
}
void TcpDriver::onWorkerErrorOccurred(const DriverError& err) {
    health_.store(err.code, std::memory_order_release);
    rxErrors_.fetch_add(1, std::memory_order_relaxed);
    emit errorOccurred(err);
    transitionTo(DriverState::Error);
}
void TcpDriver::onWorkerFrameReceived(const signalforge::frame::RawFrame& frame) {
    rxFrames_.fetch_add(1, std::memory_order_relaxed);
    rxBytes_.fetch_add(static_cast<std::uint64_t>(frame.payload.size()), std::memory_order_relaxed);
    emit frameReceived(frame);
}
void TcpDriver::onWorkerTxComplete(std::uint64_t bytesWritten) {
    txBytes_.fetch_add(bytesWritten, std::memory_order_relaxed);
}
void TcpDriver::onWorkerTxFailure() {
    txFailures_.fetch_add(1, std::memory_order_relaxed);
}

void TcpDriver::transitionTo(DriverState newState) {
    state_.store(newState, std::memory_order_release);
    emit stateChanged(newState);
}

}  // namespace signalforge::drivers

#include "tcp_driver.moc"
