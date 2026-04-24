// src/drivers/udp_driver.cpp
#include "drivers/udp_driver.hpp"

#include "drivers/io_worker_base.hpp"
#include "frame/raw_frame.hpp"
#include "observability/logging.hpp"

#include <QAbstractSocket>
#include <QByteArray>
#include <QHostAddress>
#include <QMetaObject>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QString>
#include <QUdpSocket>
#include <QVariant>
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

DriverErrorCode mapUdpError(QAbstractSocket::SocketError err) {
    switch (err) {
    case QAbstractSocket::AddressInUseError:
    case QAbstractSocket::SocketAddressNotAvailableError:
        return DriverErrorCode::ResourceUnavailable;
    case QAbstractSocket::SocketAccessError:
        return DriverErrorCode::PermissionDenied;
    case QAbstractSocket::NetworkError:
        return DriverErrorCode::ResourceLost;
    case QAbstractSocket::SocketTimeoutError:
        return DriverErrorCode::Timeout;
    default:
        return DriverErrorCode::IoFailure;
    }
}

bool hasBindIntent(const UdpConfig& cfg) {
    return cfg.localBindPort != 0 || cfg.localBindAddress != QStringLiteral("0.0.0.0");
}

bool hasSendIntent(const UdpConfig& cfg) {
    return !cfg.remoteHost.isEmpty();
}

DriverErrorCode validateConfig(const UdpConfig& cfg, QString& message) {
    if (!hasBindIntent(cfg) && !hasSendIntent(cfg)) {
        message = QStringLiteral("UDP config requires either a local bind or a remote endpoint");
        return DriverErrorCode::ConfigInvalid;
    }
    if (hasSendIntent(cfg) && cfg.remotePort == 0) {
        message = QStringLiteral("UDP remotePort must be > 0 when remoteHost is set");
        return DriverErrorCode::ConfigInvalid;
    }
    if (!cfg.multicastGroup.isEmpty()) {
        QHostAddress addr(cfg.multicastGroup);
        if (addr.isNull() || !addr.isMulticast()) {
            message =
                QStringLiteral("UDP multicastGroup '%1' is not a valid multicast address").arg(cfg.multicastGroup);
            return DriverErrorCode::ConfigInvalid;
        }
    }
    return DriverErrorCode::Success;
}

}  // namespace

class UdpIoWorker : public IoWorkerBase {
    Q_OBJECT

public:
    UdpIoWorker(UdpConfig config, QString threadName)
        : IoWorkerBase(std::move(threadName)), config_(std::move(config)) {}

public slots:
    void openOnIoThread() {
        if (socket_ != nullptr) {
            emit workerOpened();
            return;
        }
        socket_ = new QUdpSocket(this);
        connect(socket_, &QUdpSocket::errorOccurred, this, &UdpIoWorker::onSocketError);
        connect(socket_, &QUdpSocket::readyRead, this, &UdpIoWorker::onReadyRead);

        QHostAddress bindAddr(config_.localBindAddress);
        if (bindAddr.isNull()) {
            bindAddr = QHostAddress(QHostAddress::AnyIPv4);
        }
        if (!socket_->bind(bindAddr, config_.localBindPort, QUdpSocket::ShareAddress)) {
            emit workerErrorOccurred(
                makeError(mapUdpError(socket_->error()), QStringLiteral("UDP bind failed on %1:%2: %3")
                                                             .arg(config_.localBindAddress)
                                                             .arg(config_.localBindPort)
                                                             .arg(socket_->errorString())));
            socket_->deleteLater();
            socket_ = nullptr;
            return;
        }

        if (!config_.multicastGroup.isEmpty()) {
            const QHostAddress group(config_.multicastGroup);
            socket_->setSocketOption(QAbstractSocket::MulticastTtlOption,
                                     QVariant(static_cast<int>(config_.multicastTtl)));
            if (!socket_->joinMulticastGroup(group)) {
                emit workerErrorOccurred(
                    makeError(mapUdpError(socket_->error()), QStringLiteral("UDP joinMulticastGroup('%1') failed: %2")
                                                                 .arg(config_.multicastGroup)
                                                                 .arg(socket_->errorString())));
                socket_->deleteLater();
                socket_ = nullptr;
                return;
            }
        }

        emit workerOpened();
    }

    void closeOnIoThread() {
        if (socket_ != nullptr) {
            if (!config_.multicastGroup.isEmpty()) {
                socket_->leaveMulticastGroup(QHostAddress(config_.multicastGroup));
            }
            socket_->disconnect(this);
            socket_->close();
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
        if (socket_ == nullptr || !running_) {
            emit workerTxFailure();
            return;
        }
        QHostAddress remote(config_.remoteHost);
        if (remote.isNull()) {
            emit workerTxFailure();
            return;
        }
        // Qt 6.10 on Linux loopback surfaces a transient NetworkError
        // ("Unable to send a message") when two QUdpSockets in the same
        // process issue writeDatagram concurrently from different
        // threads. The send actually succeeds on a retry within ~100 µs.
        // See `.claude/M3-concerns.md`. One retry keeps legitimate
        // failures (unreachable host, routing) surfacing immediately on
        // the second attempt.
        auto n = socket_->writeDatagram(payload, remote, config_.remotePort);
        if (n < 0 && socket_->error() == QAbstractSocket::NetworkError) {
            SF_LOG_WARN("UDP writeDatagram transient NetworkError on {}:{}; retrying once",
                        config_.remoteHost.toStdString(), config_.remotePort);
            n = socket_->writeDatagram(payload, remote, config_.remotePort);
        }
        if (n < 0) {
            emit workerTxFailure();
            return;
        }
        emit workerTxComplete(static_cast<std::uint64_t>(n));
    }

protected:
    void onStarted() override {}

private slots:
    void onSocketError(QAbstractSocket::SocketError err) {
        // UDP is connectionless — Qt sometimes surfaces transient ICMP
        // unreachable events as errors on the sending socket. Only
        // report errors that occur while the socket is expected to be
        // usable.
        if (socket_ == nullptr) {
            return;
        }
        const auto code = mapUdpError(err);
        const auto msg = QStringLiteral("UDP error on %1:%2 (%3): %4")
                             .arg(config_.localBindAddress)
                             .arg(config_.localBindPort)
                             .arg(static_cast<int>(err))
                             .arg(socket_->errorString());
        emit workerErrorOccurred(makeError(code, msg));
    }

    void onReadyRead() {
        if (!running_ || socket_ == nullptr) {
            return;
        }
        while (socket_->hasPendingDatagrams()) {
            QNetworkDatagram dg = socket_->receiveDatagram();
            if (!dg.isValid()) {
                continue;
            }
            signalforge::frame::RawFrame frame;
            frame.sourceId = QStringLiteral("udp:%1:%2").arg(dg.senderAddress().toString()).arg(dg.senderPort());
            frame.payload = dg.data();
            frame.recvAt = std::chrono::steady_clock::now();
            frame.protocolHint = QStringLiteral("udp");
            emit workerFrameReceived(frame);
        }
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
    UdpConfig config_;
    QUdpSocket* socket_ = nullptr;
    bool running_ = false;
};

// =======================================================================
// UdpDriver
// =======================================================================

UdpDriver::UdpDriver(UdpConfig config, QObject* parent) : DriverInterface(parent), config_(std::move(config)) {
    const QString threadName = QStringLiteral("UdpIO-%1:%2").arg(config_.localBindAddress).arg(config_.localBindPort);
    thread_ = std::make_unique<QThread>();
    thread_->setObjectName(threadName);

    worker_ = std::make_unique<UdpIoWorker>(config_, threadName);
    worker_->moveToThread(thread_.get());

    connect(thread_.get(), &QThread::started, worker_.get(), &IoWorkerBase::onThreadStart);

    connect(worker_.get(), &UdpIoWorker::workerOpened, this, &UdpDriver::onWorkerOpened, Qt::QueuedConnection);
    connect(worker_.get(), &UdpIoWorker::workerClosed, this, &UdpDriver::onWorkerClosed, Qt::QueuedConnection);
    connect(worker_.get(), &UdpIoWorker::workerStarted, this, &UdpDriver::onWorkerStarted, Qt::QueuedConnection);
    connect(worker_.get(), &UdpIoWorker::workerStopped, this, &UdpDriver::onWorkerStopped, Qt::QueuedConnection);
    connect(worker_.get(), &UdpIoWorker::workerErrorOccurred, this, &UdpDriver::onWorkerErrorOccurred,
            Qt::QueuedConnection);
    connect(worker_.get(), &UdpIoWorker::workerFrameReceived, this, &UdpDriver::onWorkerFrameReceived,
            Qt::QueuedConnection);
    connect(worker_.get(), &UdpIoWorker::workerTxComplete, this, &UdpDriver::onWorkerTxComplete, Qt::QueuedConnection);
    connect(worker_.get(), &UdpIoWorker::workerTxFailure, this, &UdpDriver::onWorkerTxFailure, Qt::QueuedConnection);

    thread_->start();
}

UdpDriver::~UdpDriver() {
    if (state_.load(std::memory_order_acquire) != DriverState::Idle) {
        SF_LOG_WARN("UdpDriver destroyed in non-Idle state; callers should close() first");
    }
    if (thread_ && thread_->isRunning()) {
        thread_->quit();
        if (!thread_->wait(500)) {
            SF_LOG_ERROR("UdpDriver IO thread did not exit within 500ms; forcing terminate");
            thread_->terminate();
            thread_->wait();
        }
    }
}

DriverErrorCode UdpDriver::open() {
    QString msg;
    if (const auto code = validateConfig(config_, msg); code != DriverErrorCode::Success) {
        SF_LOG_ERROR("UdpDriver: {}", msg.toStdString());
        return code;
    }
    if (state_.load(std::memory_order_acquire) != DriverState::Idle) {
        return DriverErrorCode::NotConfigured;
    }
    transitionTo(DriverState::Opening);
    QMetaObject::invokeMethod(worker_.get(), "openOnIoThread", Qt::QueuedConnection);
    return DriverErrorCode::Success;
}

void UdpDriver::close() {
    const auto s = state_.load(std::memory_order_acquire);
    if (s == DriverState::Idle || s == DriverState::Closing) {
        return;
    }
    transitionTo(DriverState::Closing);
    QMetaObject::invokeMethod(worker_.get(), "closeOnIoThread", Qt::QueuedConnection);
}

DriverErrorCode UdpDriver::start() {
    if (state_.load(std::memory_order_acquire) != DriverState::Open) {
        return DriverErrorCode::NotConfigured;
    }
    QMetaObject::invokeMethod(worker_.get(), "startOnIoThread", Qt::QueuedConnection);
    return DriverErrorCode::Success;
}

void UdpDriver::stop() {
    if (state_.load(std::memory_order_acquire) != DriverState::Running) {
        return;
    }
    transitionTo(DriverState::Stopping);
    QMetaObject::invokeMethod(worker_.get(), "stopOnIoThread", Qt::QueuedConnection);
}

DriverErrorCode UdpDriver::write(const QByteArray& payload) {
    if (config_.remoteHost.isEmpty()) {
        return DriverErrorCode::ConfigInvalid;
    }
    if (state_.load(std::memory_order_acquire) != DriverState::Running) {
        return DriverErrorCode::NotConfigured;
    }
    txFrames_.fetch_add(1, std::memory_order_relaxed);
    QMetaObject::invokeMethod(worker_.get(), "writeOnIoThread", Qt::QueuedConnection, Q_ARG(QByteArray, payload));
    return DriverErrorCode::Success;
}

DriverState UdpDriver::state() const {
    return state_.load(std::memory_order_acquire);
}

DriverErrorCode UdpDriver::health() const {
    return health_.load(std::memory_order_acquire);
}

signalforge::frame::DriverStatistics UdpDriver::statistics() const {
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

const UdpConfig& UdpDriver::config() const noexcept {
    return config_;
}

void UdpDriver::onWorkerOpened() {
    transitionTo(DriverState::Open);
}
void UdpDriver::onWorkerClosed() {
    transitionTo(DriverState::Idle);
}
void UdpDriver::onWorkerStarted() {
    transitionTo(DriverState::Running);
}
void UdpDriver::onWorkerStopped() {
    transitionTo(DriverState::Open);
}
void UdpDriver::onWorkerErrorOccurred(const DriverError& err) {
    health_.store(err.code, std::memory_order_release);
    rxErrors_.fetch_add(1, std::memory_order_relaxed);
    emit errorOccurred(err);
    transitionTo(DriverState::Error);
}
void UdpDriver::onWorkerFrameReceived(const signalforge::frame::RawFrame& frame) {
    rxFrames_.fetch_add(1, std::memory_order_relaxed);
    rxBytes_.fetch_add(static_cast<std::uint64_t>(frame.payload.size()), std::memory_order_relaxed);
    emit frameReceived(frame);
}
void UdpDriver::onWorkerTxComplete(std::uint64_t bytesWritten) {
    txBytes_.fetch_add(bytesWritten, std::memory_order_relaxed);
}
void UdpDriver::onWorkerTxFailure() {
    txFailures_.fetch_add(1, std::memory_order_relaxed);
}

void UdpDriver::transitionTo(DriverState newState) {
    state_.store(newState, std::memory_order_release);
    emit stateChanged(newState);
}

}  // namespace signalforge::drivers

#include "udp_driver.moc"
