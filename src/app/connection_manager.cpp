// src/app/connection_manager.cpp
#include "app/connection_manager.hpp"

#include "drivers/replay_driver.hpp"
#include "drivers/serial_driver.hpp"
#include "drivers/tcp_driver.hpp"
#include "drivers/udp_driver.hpp"
#include "observability/logging.hpp"
#include "pipeline/frame_pipeline.hpp"
#include "pipeline/pipeline_manager.hpp"

#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <chrono>
#include <cstdint>

namespace signalforge::app {

namespace {

QString hexDump(const QByteArray& bytes, int maxBytes = 64) {
    const int n = std::min<int>(bytes.size(), maxBytes);
    QString hex;
    hex.reserve(n * 3);
    for (int i = 0; i < n; ++i) {
        if (i > 0)
            hex.append(QChar::fromLatin1(' '));
        hex.append(QStringLiteral("%1").arg(static_cast<unsigned char>(bytes[i]), 2, 16, QChar::fromLatin1('0')));
    }
    if (bytes.size() > maxBytes) {
        hex.append(QStringLiteral(" ... (%1 more)").arg(bytes.size() - maxBytes));
    }
    return hex;
}

QString stateLabel(signalforge::drivers::DriverState s) {
    using signalforge::drivers::DriverState;
    switch (s) {
    case DriverState::Idle:
        return QStringLiteral("Idle");
    case DriverState::Opening:
        return QStringLiteral("Opening");
    case DriverState::Open:
        return QStringLiteral("Open");
    case DriverState::Running:
        return QStringLiteral("Running");
    case DriverState::Stopping:
        return QStringLiteral("Stopping");
    case DriverState::Closing:
        return QStringLiteral("Closing");
    case DriverState::Error:
        return QStringLiteral("Error");
    }
    return QStringLiteral("?");
}

QString stateColor(signalforge::drivers::DriverState s) {
    using signalforge::drivers::DriverState;
    switch (s) {
    case DriverState::Idle:
        return QStringLiteral("#808080");  // gray
    case DriverState::Opening:
    case DriverState::Closing:
    case DriverState::Stopping:
        return QStringLiteral("#c0a000");  // yellow
    case DriverState::Open:
    case DriverState::Running:
        return QStringLiteral("#008000");  // green
    case DriverState::Error:
        return QStringLiteral("#c03030");  // red
    }
    return QStringLiteral("#808080");
}

}  // namespace

ConnectionManager::ConnectionManager(QWidget* parent) : ConnectionManager(nullptr, parent) {}

ConnectionManager::ConnectionManager(signalforge::pipeline::PipelineManager* pipelineManager, QWidget* parent)
    : QDialog(parent), pipelineManager_(pipelineManager) {
    signalforge::frame::registerMetatypes();
    signalforge::drivers::registerMetatypes();
    setWindowTitle(QStringLiteral("Connection Manager"));
    resize(620, 560);
    buildUi();
    updateStateBadge(signalforge::drivers::DriverState::Idle);
}

ConnectionManager::~ConnectionManager() {
    if (driver_) {
        // Synchronous close: request, then the driver's destructor joins
        // the IO thread with a 500 ms budget. Per spec §7-5 this stays
        // well under the 200 ms UI-block threshold on a normal shutdown.
        driver_->close();
        // Spin briefly so the worker can post back the Idle transition
        // via QueuedConnection; if it doesn't, the destructor still
        // terminates cleanly with terminate() fallback.
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(150);
        while (driver_->state() != signalforge::drivers::DriverState::Idle &&
               std::chrono::steady_clock::now() < deadline) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        }
        // If the stateChanged(Idle) callback did not already run (e.g.
        // spin timeout), detach the pipeline here to avoid a dangling
        // driver pointer inside the manager.
        if (pipelineManager_ && !currentDriverId_.isEmpty()) {
            pipelineManager_->detach(currentDriverId_);
            currentDriverId_.clear();
            pipeline_ = nullptr;
        }
    }
}

void ConnectionManager::buildUi() {
    typeCombo_ = new QComboBox(this);
    typeCombo_->addItem(QStringLiteral("Serial"), static_cast<int>(DriverType::Serial));
    typeCombo_->addItem(QStringLiteral("TCP"), static_cast<int>(DriverType::Tcp));
    typeCombo_->addItem(QStringLiteral("UDP"), static_cast<int>(DriverType::Udp));
    typeCombo_->addItem(QStringLiteral("Replay"), static_cast<int>(DriverType::Replay));

    stack_ = new QStackedWidget(this);
    stack_->addWidget(buildSerialForm());
    stack_->addWidget(buildTcpForm());
    stack_->addWidget(buildUdpForm());
    stack_->addWidget(buildReplayForm());

    connectButton_ = new QPushButton(QStringLiteral("Connect"), this);
    disconnectButton_ = new QPushButton(QStringLiteral("Disconnect"), this);
    disconnectButton_->setEnabled(false);

    stateBadge_ = new QLabel(this);
    stateBadge_->setFrameShape(QFrame::Panel);
    stateBadge_->setFrameShadow(QFrame::Sunken);
    stateBadge_->setMinimumWidth(120);
    stateBadge_->setAlignment(Qt::AlignCenter);

    errorLabel_ = new QLabel(this);
    errorLabel_->setWordWrap(true);
    errorLabel_->setStyleSheet(QStringLiteral("color: #a03030;"));

    statsLabel_ = new QLabel(this);

    frameLog_ = new QPlainTextEdit(this);
    frameLog_->setReadOnly(true);
    frameLog_->setMaximumBlockCount(kMaxLogLines);
    frameLog_->setPlaceholderText(QStringLiteral("Received frames appear here (hex dump)..."));

    auto* buttons = new QHBoxLayout();
    buttons->addWidget(connectButton_);
    buttons->addWidget(disconnectButton_);
    buttons->addStretch();
    buttons->addWidget(new QLabel(QStringLiteral("State:"), this));
    buttons->addWidget(stateBadge_);

    auto* root = new QVBoxLayout(this);
    root->addWidget(new QLabel(QStringLiteral("Driver Type:"), this));
    root->addWidget(typeCombo_);
    root->addWidget(stack_);
    root->addLayout(buttons);
    root->addWidget(statsLabel_);
    root->addWidget(errorLabel_);
    root->addWidget(new QLabel(QStringLiteral("Frame log:"), this));
    root->addWidget(frameLog_, 1);

    connect(typeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &ConnectionManager::onDriverTypeChanged);
    connect(connectButton_, &QPushButton::clicked, this, &ConnectionManager::onConnectClicked);
    connect(disconnectButton_, &QPushButton::clicked, this, &ConnectionManager::onDisconnectClicked);

    statsTimer_ = new QTimer(this);
    statsTimer_->setInterval(1000);
    connect(statsTimer_, &QTimer::timeout, this, &ConnectionManager::onThroughputTick);
}

QWidget* ConnectionManager::buildSerialForm() {
    auto* w = new QWidget(this);
    auto* f = new QFormLayout(w);
    serialDevice_ = new QLineEdit(QStringLiteral("/dev/ttyUSB0"), w);
    serialBaud_ = new QComboBox(w);
    for (int rate : {9600, 38400, 115200, 921600}) {
        serialBaud_->addItem(QString::number(rate), rate);
    }
    serialBaud_->setCurrentText(QStringLiteral("115200"));
    serialDataBits_ = new QSpinBox(w);
    serialDataBits_->setRange(5, 8);
    serialDataBits_->setValue(8);
    serialParity_ = new QComboBox(w);
    serialParity_->addItems({QStringLiteral("none"), QStringLiteral("even"), QStringLiteral("odd"),
                             QStringLiteral("mark"), QStringLiteral("space")});
    serialStopBits_ = new QSpinBox(w);
    serialStopBits_->setRange(1, 2);
    serialStopBits_->setValue(1);
    serialFlow_ = new QComboBox(w);
    serialFlow_->addItems({QStringLiteral("none"), QStringLiteral("hardware"), QStringLiteral("software")});
    f->addRow(QStringLiteral("Device:"), serialDevice_);
    f->addRow(QStringLiteral("Baud:"), serialBaud_);
    f->addRow(QStringLiteral("Data Bits:"), serialDataBits_);
    f->addRow(QStringLiteral("Parity:"), serialParity_);
    f->addRow(QStringLiteral("Stop Bits:"), serialStopBits_);
    f->addRow(QStringLiteral("Flow Control:"), serialFlow_);
    return w;
}

QWidget* ConnectionManager::buildTcpForm() {
    auto* w = new QWidget(this);
    auto* f = new QFormLayout(w);
    tcpHost_ = new QLineEdit(QStringLiteral("127.0.0.1"), w);
    tcpPort_ = new QSpinBox(w);
    tcpPort_->setRange(1, 65535);
    tcpPort_->setValue(9000);
    tcpTimeoutMs_ = new QSpinBox(w);
    tcpTimeoutMs_->setRange(100, 60000);
    tcpTimeoutMs_->setValue(5000);
    f->addRow(QStringLiteral("Host:"), tcpHost_);
    f->addRow(QStringLiteral("Port:"), tcpPort_);
    f->addRow(QStringLiteral("Connect Timeout (ms):"), tcpTimeoutMs_);
    return w;
}

QWidget* ConnectionManager::buildUdpForm() {
    auto* w = new QWidget(this);
    auto* f = new QFormLayout(w);
    udpLocalAddr_ = new QLineEdit(QStringLiteral("0.0.0.0"), w);
    udpLocalPort_ = new QSpinBox(w);
    udpLocalPort_->setRange(0, 65535);
    udpLocalPort_->setValue(0);
    udpRemoteHost_ = new QLineEdit(QStringLiteral(""), w);
    udpRemotePort_ = new QSpinBox(w);
    udpRemotePort_->setRange(0, 65535);
    udpRemotePort_->setValue(0);
    udpMulticastGroup_ = new QLineEdit(QStringLiteral(""), w);
    udpMulticastTtl_ = new QSpinBox(w);
    udpMulticastTtl_->setRange(1, 255);
    udpMulticastTtl_->setValue(1);
    f->addRow(QStringLiteral("Local Bind Address:"), udpLocalAddr_);
    f->addRow(QStringLiteral("Local Bind Port:"), udpLocalPort_);
    f->addRow(QStringLiteral("Remote Host:"), udpRemoteHost_);
    f->addRow(QStringLiteral("Remote Port:"), udpRemotePort_);
    f->addRow(QStringLiteral("Multicast Group:"), udpMulticastGroup_);
    f->addRow(QStringLiteral("Multicast TTL:"), udpMulticastTtl_);
    return w;
}

QWidget* ConnectionManager::buildReplayForm() {
    auto* w = new QWidget(this);
    auto* layout = new QVBoxLayout(w);
    auto* row = new QHBoxLayout();
    replayPath_ = new QLineEdit(w);
    replayPath_->setPlaceholderText(QStringLiteral("Path to .sfreplay session file"));
    auto* browse = new QPushButton(QStringLiteral("Browse..."), w);
    row->addWidget(replayPath_);
    row->addWidget(browse);
    layout->addLayout(row);
    layout->addStretch();
    connect(browse, &QPushButton::clicked, this, &ConnectionManager::onBrowseSessionFile);
    return w;
}

void ConnectionManager::onBrowseSessionFile() {
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Choose session file"), QString(),
                                                      QStringLiteral("Session files (*.sfreplay *.sf);;All files (*)"));
    if (!path.isEmpty()) {
        replayPath_->setText(path);
    }
}

void ConnectionManager::onDriverTypeChanged(int idx) {
    stack_->setCurrentIndex(idx);
}

void ConnectionManager::setDriverType(DriverType t) {
    typeCombo_->setCurrentIndex(static_cast<int>(t));
}

void ConnectionManager::setReplaySessionFile(const QString& path) {
    replayPath_->setText(path);
}

void ConnectionManager::requestConnect() {
    onConnectClicked();
}

void ConnectionManager::requestDisconnect() {
    onDisconnectClicked();
}

signalforge::drivers::DriverState ConnectionManager::currentState() const noexcept {
    return driver_ ? driver_->state() : signalforge::drivers::DriverState::Idle;
}

const QString& ConnectionManager::lastErrorMessage() const noexcept {
    return lastError_;
}

signalforge::drivers::SerialConfig ConnectionManager::buildSerialConfig() const {
    signalforge::drivers::SerialConfig cfg;
    cfg.device = serialDevice_->text();
    cfg.baudRate = serialBaud_->currentData().toInt();
    cfg.dataBits = serialDataBits_->value();
    cfg.parity = serialParity_->currentText();
    cfg.stopBits = serialStopBits_->value();
    cfg.flowControl = serialFlow_->currentText();
    return cfg;
}

signalforge::drivers::TcpConfig ConnectionManager::buildTcpConfig() const {
    signalforge::drivers::TcpConfig cfg;
    cfg.host = tcpHost_->text();
    cfg.port = static_cast<quint16>(tcpPort_->value());
    cfg.connectTimeout = std::chrono::milliseconds{tcpTimeoutMs_->value()};
    return cfg;
}

signalforge::drivers::UdpConfig ConnectionManager::buildUdpConfig() const {
    signalforge::drivers::UdpConfig cfg;
    cfg.localBindAddress = udpLocalAddr_->text();
    cfg.localBindPort = static_cast<quint16>(udpLocalPort_->value());
    cfg.remoteHost = udpRemoteHost_->text();
    cfg.remotePort = static_cast<quint16>(udpRemotePort_->value());
    cfg.multicastGroup = udpMulticastGroup_->text();
    cfg.multicastTtl = static_cast<quint32>(udpMulticastTtl_->value());
    return cfg;
}

signalforge::drivers::ReplayConfig ConnectionManager::buildReplayConfig() const {
    signalforge::drivers::ReplayConfig cfg;
    cfg.sessionFilePath = replayPath_->text();
    return cfg;
}

void ConnectionManager::onConnectClicked() {
    if (driver_) {
        return;
    }
    lastError_.clear();
    errorLabel_->clear();

    const auto type = static_cast<DriverType>(typeCombo_->currentData().toInt());
    switch (type) {
    case DriverType::Serial:
        driver_ = std::make_unique<signalforge::drivers::SerialDriver>(buildSerialConfig());
        break;
    case DriverType::Tcp:
        driver_ = std::make_unique<signalforge::drivers::TcpDriver>(buildTcpConfig());
        break;
    case DriverType::Udp:
        driver_ = std::make_unique<signalforge::drivers::UdpDriver>(buildUdpConfig());
        break;
    case DriverType::Replay:
        driver_ = std::make_unique<signalforge::drivers::ReplayDriver>(buildReplayConfig());
        break;
    }

    connect(driver_.get(), &signalforge::drivers::DriverInterface::stateChanged, this,
            &ConnectionManager::onDriverStateChanged, Qt::QueuedConnection);
    connect(driver_.get(), &signalforge::drivers::DriverInterface::frameReceived, this,
            &ConnectionManager::onDriverFrameReceived, Qt::QueuedConnection);
    connect(driver_.get(), &signalforge::drivers::DriverInterface::errorOccurred, this,
            &ConnectionManager::onDriverError, Qt::QueuedConnection);

    const auto code = driver_->open();
    if (code != signalforge::drivers::DriverErrorCode::Success) {
        lastError_ = QStringLiteral("open() rejected: code=%1").arg(static_cast<int>(code));
        errorLabel_->setText(lastError_);
        driver_.reset();
        return;
    }

    if (pipelineManager_) {
        signalforge::pipeline::PipelineConfig cfg;
        cfg.driverId = makeDriverId(type);
        pipeline_ = pipelineManager_->attach(driver_.get(), cfg);
        if (pipeline_) {
            currentDriverId_ = cfg.driverId;
        } else {
            SF_LOG_ERROR("ConnectionManager: PipelineManager::attach returned nullptr for driverId '{}'",
                         cfg.driverId.toStdString());
        }
    }

    typeCombo_->setEnabled(false);
    connectButton_->setEnabled(false);
    disconnectButton_->setEnabled(true);
    statsTimer_->start();
}

void ConnectionManager::onDisconnectClicked() {
    if (!driver_) {
        return;
    }
    // close() is async — only queue the close; the Idle transition
    // arrives via stateChanged. Do not block the UI thread.
    driver_->close();
}

void ConnectionManager::onDriverStateChanged(signalforge::drivers::DriverState s) {
    updateStateBadge(s);
    if (s == signalforge::drivers::DriverState::Open) {
        if (driver_) {
            driver_->start();  // auto-advance to Running for M3 preview
        }
    } else if (s == signalforge::drivers::DriverState::Idle) {
        // close() completed — detach from pipeline manager (which
        // destroys the pipeline and disconnects its worker), then reset
        // ownership and re-enable controls. Detach before driver_.reset()
        // so the pipeline manager sees a valid driver pointer to unwire.
        if (pipelineManager_ && !currentDriverId_.isEmpty()) {
            pipelineManager_->detach(currentDriverId_);
            currentDriverId_.clear();
            pipeline_ = nullptr;
        }
        driver_.reset();
        typeCombo_->setEnabled(true);
        connectButton_->setEnabled(true);
        disconnectButton_->setEnabled(false);
        statsTimer_->stop();
    }
}

void ConnectionManager::onDriverFrameReceived(const signalforge::frame::RawFrame& f) {
    appendFrameToLog(f);
}

void ConnectionManager::onDriverError(const signalforge::drivers::DriverError& e) {
    lastError_ = e.message;
    errorLabel_->setText(QStringLiteral("Error (%1): %2").arg(static_cast<int>(e.code)).arg(e.message));
}

void ConnectionManager::onThroughputTick() {
    if (!driver_) {
        return;
    }
    const auto stats = driver_->statistics();
    statsLabel_->setText(QStringLiteral("rx: %1 frames / %2 bytes   tx: %3 frames / %4 bytes")
                             .arg(stats.rx.framesTotal)
                             .arg(stats.rx.bytesTotal)
                             .arg(stats.tx.framesTotal)
                             .arg(stats.tx.bytesTotal));
}

void ConnectionManager::appendFrameToLog(const signalforge::frame::RawFrame& f) {
    if (logLineCount_ >= kMaxLogLines * 2) {
        // Hard cap the explicit count; QPlainTextEdit also enforces via
        // maximumBlockCount.
        logLineCount_ = kMaxLogLines;
    }
    ++logLineCount_;
    const QString line = QStringLiteral("[%1] %2 %3 bytes: %4")
                             .arg(logLineCount_, 4)
                             .arg(f.sourceId)
                             .arg(f.payload.size())
                             .arg(hexDump(f.payload));
    frameLog_->appendPlainText(line);
}

void ConnectionManager::updateStateBadge(signalforge::drivers::DriverState s) {
    stateBadge_->setText(stateLabel(s));
    stateBadge_->setStyleSheet(
        QStringLiteral("color: white; background-color: %1; padding: 2px 8px;").arg(stateColor(s)));
}

QString ConnectionManager::makeDriverId(DriverType t) const {
    // Format per plan §2 S6: `<driver-type>:<disambiguating-suffix>`.
    // Uniqueness is required only across concurrent connections, which
    // M3's single-connection dialog guarantees trivially. ADR-003 accepts
    // `:` and `/` verbatim so no sanitization helper is needed.
    switch (t) {
    case DriverType::Serial:
        return QStringLiteral("serial:") + serialDevice_->text();
    case DriverType::Tcp:
        return QStringLiteral("tcp:%1:%2").arg(tcpHost_->text()).arg(tcpPort_->value());
    case DriverType::Udp: {
        // Prefer remoteHost:port when set (send-configured drivers), else
        // the local bind. This keeps the id readable across both modes.
        const QString remote = udpRemoteHost_->text();
        if (!remote.isEmpty()) {
            return QStringLiteral("udp:%1:%2").arg(remote).arg(udpRemotePort_->value());
        }
        return QStringLiteral("udp:%1:%2").arg(udpLocalAddr_->text()).arg(udpLocalPort_->value());
    }
    case DriverType::Replay: {
        const QFileInfo fi(replayPath_->text());
        const QString base = fi.fileName();
        return QStringLiteral("replay:") + (base.isEmpty() ? QStringLiteral("<unset>") : base);
    }
    }
    return QStringLiteral("unknown");
}

}  // namespace signalforge::app
