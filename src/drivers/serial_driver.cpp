// src/drivers/serial_driver.cpp
#include "drivers/serial_driver.hpp"

#include "drivers/io_worker_base.hpp"
#include "frame/raw_frame.hpp"
#include "observability/logging.hpp"

#include <QByteArray>
#include <QFileInfo>
#include <QMetaObject>
#include <QSerialPort>
#include <QString>
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

DriverErrorCode mapSerialError(QSerialPort::SerialPortError err) {
    switch (err) {
    case QSerialPort::NoError:
        return DriverErrorCode::Success;
    case QSerialPort::DeviceNotFoundError:
        return DriverErrorCode::ResourceUnavailable;
    case QSerialPort::PermissionError:
        return DriverErrorCode::PermissionDenied;
    case QSerialPort::OpenError:
    case QSerialPort::NotOpenError:
        return DriverErrorCode::ResourceUnavailable;
    case QSerialPort::ResourceError:
        return DriverErrorCode::ResourceLost;
    case QSerialPort::TimeoutError:
        return DriverErrorCode::Timeout;
    case QSerialPort::UnsupportedOperationError:
        return DriverErrorCode::ConfigInvalid;
    default:
        return DriverErrorCode::IoFailure;
    }
}

QSerialPort::Parity parseParity(const QString& s, bool& ok) {
    ok = true;
    if (s == QStringLiteral("none"))
        return QSerialPort::NoParity;
    if (s == QStringLiteral("even"))
        return QSerialPort::EvenParity;
    if (s == QStringLiteral("odd"))
        return QSerialPort::OddParity;
    if (s == QStringLiteral("mark"))
        return QSerialPort::MarkParity;
    if (s == QStringLiteral("space"))
        return QSerialPort::SpaceParity;
    ok = false;
    return QSerialPort::NoParity;
}

QSerialPort::FlowControl parseFlowControl(const QString& s, bool& ok) {
    ok = true;
    if (s == QStringLiteral("none"))
        return QSerialPort::NoFlowControl;
    if (s == QStringLiteral("hardware"))
        return QSerialPort::HardwareControl;
    if (s == QStringLiteral("software"))
        return QSerialPort::SoftwareControl;
    ok = false;
    return QSerialPort::NoFlowControl;
}

DriverErrorCode validateConfig(const SerialConfig& cfg, QString& message) {
    if (cfg.device.isEmpty()) {
        message = QStringLiteral("Serial device path is empty");
        return DriverErrorCode::ConfigInvalid;
    }
    if (cfg.baudRate <= 0) {
        message = QStringLiteral("Serial baud rate must be positive (got %1)").arg(cfg.baudRate);
        return DriverErrorCode::ConfigInvalid;
    }
    if (cfg.dataBits < 5 || cfg.dataBits > 8) {
        message = QStringLiteral("Serial dataBits must be 5–8 (got %1)").arg(cfg.dataBits);
        return DriverErrorCode::ConfigInvalid;
    }
    if (cfg.stopBits != 1 && cfg.stopBits != 2) {
        message = QStringLiteral("Serial stopBits must be 1 or 2 (got %1)").arg(cfg.stopBits);
        return DriverErrorCode::ConfigInvalid;
    }
    bool okParity = true;
    (void)parseParity(cfg.parity, okParity);
    if (!okParity) {
        message = QStringLiteral("Unknown parity '%1' (expected none/even/odd/mark/space)").arg(cfg.parity);
        return DriverErrorCode::ConfigInvalid;
    }
    bool okFlow = true;
    (void)parseFlowControl(cfg.flowControl, okFlow);
    if (!okFlow) {
        message = QStringLiteral("Unknown flowControl '%1' (expected none/hardware/software)").arg(cfg.flowControl);
        return DriverErrorCode::ConfigInvalid;
    }
    return DriverErrorCode::Success;
}

}  // namespace

// =======================================================================
// SerialIoWorker
// =======================================================================

class SerialIoWorker : public IoWorkerBase {
    Q_OBJECT

public:
    SerialIoWorker(SerialConfig config, QString threadName)
        : IoWorkerBase(std::move(threadName)), config_(std::move(config)) {}

public slots:
    void openOnIoThread() {
        if (port_ != nullptr) {
            // Already open — defensive.
            emit workerOpened();
            return;
        }
        port_ = new QSerialPort(this);
        port_->setPortName(config_.device);
        port_->setBaudRate(config_.baudRate);
        port_->setDataBits(static_cast<QSerialPort::DataBits>(config_.dataBits));
        bool parityOk = true;
        port_->setParity(parseParity(config_.parity, parityOk));
        port_->setStopBits(config_.stopBits == 2 ? QSerialPort::TwoStop : QSerialPort::OneStop);
        bool flowOk = true;
        port_->setFlowControl(parseFlowControl(config_.flowControl, flowOk));

        if (!port_->open(QIODevice::ReadWrite)) {
            const auto code = mapSerialError(port_->error());
            const auto msg = QStringLiteral("Could not open %1: %2").arg(config_.device, port_->errorString());
            delete port_;
            port_ = nullptr;
            emit workerErrorOccurred(makeError(code, msg));
            return;
        }

        connect(port_, &QSerialPort::readyRead, this, &SerialIoWorker::onReadyRead);
        connect(port_, &QSerialPort::errorOccurred, this, &SerialIoWorker::onPortError);

        emit workerOpened();
    }

    void closeOnIoThread() {
        if (port_ != nullptr) {
            port_->close();
            port_->deleteLater();
            port_ = nullptr;
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
        if (port_ == nullptr || !running_) {
            emit workerTxFailure();
            return;
        }
        const auto n = port_->write(payload);
        if (n < 0) {
            emit workerTxFailure();
            return;
        }
        emit workerTxComplete(static_cast<std::uint64_t>(n));
    }

protected:
    void onStarted() override {
        // Nothing to init on thread-start; open() constructs the port.
    }

private slots:
    void onReadyRead() {
        if (!running_ || port_ == nullptr) {
            return;
        }
        const QByteArray bytes = port_->readAll();
        if (bytes.isEmpty()) {
            return;
        }
        signalforge::frame::RawFrame frame;
        frame.sourceId = QStringLiteral("serial:") + config_.device;
        frame.payload = bytes;
        frame.recvAt = std::chrono::steady_clock::now();
        frame.protocolHint = QStringLiteral("serial");
        emit workerFrameReceived(frame);
    }

    void onPortError(QSerialPort::SerialPortError err) {
        if (err == QSerialPort::NoError) {
            return;
        }
        if (!running_ && (err == QSerialPort::NotOpenError)) {
            // Ignore expected error from closing while idle.
            return;
        }
        const auto code = mapSerialError(err);
        const auto msg =
            QStringLiteral("Serial error on %1: %2")
                .arg(config_.device, port_ != nullptr ? port_->errorString() : QStringLiteral("(port gone)"));
        emit workerErrorOccurred(makeError(code, msg));
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
    SerialConfig config_;
    QSerialPort* port_ = nullptr;
    bool running_ = false;
};

// =======================================================================
// SerialDriver
// =======================================================================

SerialDriver::SerialDriver(SerialConfig config, QObject* parent) : DriverInterface(parent), config_(std::move(config)) {
    const QString threadName = QStringLiteral("SerialIO-") + QFileInfo(config_.device).fileName();
    thread_ = std::make_unique<QThread>();
    thread_->setObjectName(threadName);

    worker_ = std::make_unique<SerialIoWorker>(config_, threadName);
    worker_->moveToThread(thread_.get());

    connect(thread_.get(), &QThread::started, worker_.get(), &IoWorkerBase::onThreadStart);

    connect(worker_.get(), &SerialIoWorker::workerOpened, this, &SerialDriver::onWorkerOpened, Qt::QueuedConnection);
    connect(worker_.get(), &SerialIoWorker::workerClosed, this, &SerialDriver::onWorkerClosed, Qt::QueuedConnection);
    connect(worker_.get(), &SerialIoWorker::workerStarted, this, &SerialDriver::onWorkerStarted, Qt::QueuedConnection);
    connect(worker_.get(), &SerialIoWorker::workerStopped, this, &SerialDriver::onWorkerStopped, Qt::QueuedConnection);
    connect(worker_.get(), &SerialIoWorker::workerErrorOccurred, this, &SerialDriver::onWorkerErrorOccurred,
            Qt::QueuedConnection);
    connect(worker_.get(), &SerialIoWorker::workerFrameReceived, this, &SerialDriver::onWorkerFrameReceived,
            Qt::QueuedConnection);
    connect(worker_.get(), &SerialIoWorker::workerTxComplete, this, &SerialDriver::onWorkerTxComplete,
            Qt::QueuedConnection);
    connect(worker_.get(), &SerialIoWorker::workerTxFailure, this, &SerialDriver::onWorkerTxFailure,
            Qt::QueuedConnection);

    thread_->start();
}

SerialDriver::~SerialDriver() {
    if (state_.load(std::memory_order_acquire) != DriverState::Idle) {
        SF_LOG_WARN("SerialDriver destroyed in non-Idle state; callers should close() first");
    }
    if (thread_ && thread_->isRunning()) {
        thread_->quit();
        if (!thread_->wait(500)) {
            SF_LOG_ERROR("SerialDriver IO thread did not exit within 500ms; forcing terminate");
            thread_->terminate();
            thread_->wait();
        }
    }
}

DriverErrorCode SerialDriver::open() {
    QString msg;
    if (const auto code = validateConfig(config_, msg); code != DriverErrorCode::Success) {
        SF_LOG_ERROR("SerialDriver: {}", msg.toStdString());
        return code;  // no state transition on synchronous validation failure
    }
    if (state_.load(std::memory_order_acquire) != DriverState::Idle) {
        return DriverErrorCode::NotConfigured;
    }
    transitionTo(DriverState::Opening);
    QMetaObject::invokeMethod(worker_.get(), "openOnIoThread", Qt::QueuedConnection);
    return DriverErrorCode::Success;
}

void SerialDriver::close() {
    const auto s = state_.load(std::memory_order_acquire);
    if (s == DriverState::Idle || s == DriverState::Closing) {
        return;
    }
    transitionTo(DriverState::Closing);
    QMetaObject::invokeMethod(worker_.get(), "closeOnIoThread", Qt::QueuedConnection);
}

DriverErrorCode SerialDriver::start() {
    if (state_.load(std::memory_order_acquire) != DriverState::Open) {
        return DriverErrorCode::NotConfigured;
    }
    QMetaObject::invokeMethod(worker_.get(), "startOnIoThread", Qt::QueuedConnection);
    return DriverErrorCode::Success;
}

void SerialDriver::stop() {
    if (state_.load(std::memory_order_acquire) != DriverState::Running) {
        return;
    }
    transitionTo(DriverState::Stopping);
    QMetaObject::invokeMethod(worker_.get(), "stopOnIoThread", Qt::QueuedConnection);
}

DriverErrorCode SerialDriver::write(const QByteArray& payload) {
    if (state_.load(std::memory_order_acquire) != DriverState::Running) {
        return DriverErrorCode::NotConfigured;
    }
    // Count the submitted frame eagerly (async delivery). Byte count and
    // failure counter are updated on the worker's actual write result.
    txFrames_.fetch_add(1, std::memory_order_relaxed);
    QMetaObject::invokeMethod(worker_.get(), "writeOnIoThread", Qt::QueuedConnection, Q_ARG(QByteArray, payload));
    return DriverErrorCode::Success;
}

DriverState SerialDriver::state() const {
    return state_.load(std::memory_order_acquire);
}

DriverErrorCode SerialDriver::health() const {
    return health_.load(std::memory_order_acquire);
}

signalforge::frame::DriverStatistics SerialDriver::statistics() const {
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

const SerialConfig& SerialDriver::config() const noexcept {
    return config_;
}

void SerialDriver::onWorkerOpened() {
    transitionTo(DriverState::Open);
}
void SerialDriver::onWorkerClosed() {
    transitionTo(DriverState::Idle);
}
void SerialDriver::onWorkerStarted() {
    transitionTo(DriverState::Running);
}
void SerialDriver::onWorkerStopped() {
    transitionTo(DriverState::Open);
}
void SerialDriver::onWorkerErrorOccurred(const DriverError& err) {
    health_.store(err.code, std::memory_order_release);
    rxErrors_.fetch_add(1, std::memory_order_relaxed);
    emit errorOccurred(err);
    transitionTo(DriverState::Error);
}
void SerialDriver::onWorkerFrameReceived(const signalforge::frame::RawFrame& frame) {
    rxFrames_.fetch_add(1, std::memory_order_relaxed);
    rxBytes_.fetch_add(static_cast<std::uint64_t>(frame.payload.size()), std::memory_order_relaxed);
    emit frameReceived(frame);
}
void SerialDriver::onWorkerTxComplete(std::uint64_t bytesWritten) {
    txBytes_.fetch_add(bytesWritten, std::memory_order_relaxed);
}
void SerialDriver::onWorkerTxFailure() {
    txFailures_.fetch_add(1, std::memory_order_relaxed);
}

void SerialDriver::transitionTo(DriverState newState) {
    state_.store(newState, std::memory_order_release);
    emit stateChanged(newState);
}

}  // namespace signalforge::drivers

#include "serial_driver.moc"
