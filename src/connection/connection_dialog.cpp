// src/connection/connection_dialog.cpp

#include "connection/connection_dialog.hpp"

#include "observability/logging.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSerialPortInfo>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace signalforge::connection {

namespace {

QByteArray hexToBytes(const QString& hex) {
    return QByteArray::fromHex(hex.toLatin1());
}

}  // namespace

ConnectionDialog::ConnectionDialog(QStringList availableSchemaIds, QWidget* parent)
    : QDialog(parent), availableSchemaIds_(std::move(availableSchemaIds)) {
    setWindowTitle(tr("Connection"));
    setModal(true);

    auto* root = new QVBoxLayout(this);

    // ── Common header ────────────────────────────────────────
    auto* commonGroup = new QGroupBox(tr("General"), this);
    auto* commonForm = new QFormLayout(commonGroup);

    displayName_ = new QLineEdit(commonGroup);
    commonForm->addRow(tr("Display name"), displayName_);

    schemaCombo_ = new QComboBox(commonGroup);
    schemaCombo_->setEditable(true);            // allow custom schema id
    schemaCombo_->addItem(QStringLiteral(""));  // empty = no decoder
    for (const auto& id : availableSchemaIds_) {
        schemaCombo_->addItem(id);
    }
    commonForm->addRow(tr("Decoder schema"), schemaCombo_);

    autoConnectStartup_ = new QCheckBox(tr("Auto-connect on startup (V1.5+: currently has no effect)"), commonGroup);
    autoConnectStartup_->setToolTip(tr("Per design decision M9.2, V1 reconnect is manual. "
                                       "This flag is preserved in yaml for forward compatibility."));
    commonForm->addRow(QString(), autoConnectStartup_);

    typeCombo_ = new QComboBox(commonGroup);
    typeCombo_->addItem(tr("Serial"), QVariant::fromValue(static_cast<int>(DriverType::Serial)));
    typeCombo_->addItem(tr("TCP"), QVariant::fromValue(static_cast<int>(DriverType::Tcp)));
    typeCombo_->addItem(tr("UDP"), QVariant::fromValue(static_cast<int>(DriverType::Udp)));
    typeCombo_->addItem(tr("Replay"), QVariant::fromValue(static_cast<int>(DriverType::Replay)));
    commonForm->addRow(tr("Driver type"), typeCombo_);

    root->addWidget(commonGroup);

    // ── Stacked driver-config pages ─────────────────────────
    stack_ = new QStackedWidget(this);
    stack_->addWidget(buildSerialPage());
    stack_->addWidget(buildTcpPage());
    stack_->addWidget(buildUdpPage());
    stack_->addWidget(buildReplayPage());
    root->addWidget(stack_);

    // ── Auto-connect commands page ──────────────────────────
    root->addWidget(buildCommandsPage());

    // ── Buttons ─────────────────────────────────────────────
    auto* buttonRow = new QHBoxLayout();
    testBtn_ = new QPushButton(tr("Test connection"), this);
    okButton_ = new QPushButton(tr("OK"), this);
    cancelButton_ = new QPushButton(tr("Cancel"), this);
    okButton_->setDefault(true);
    buttonRow->addWidget(testBtn_);
    buttonRow->addStretch();
    buttonRow->addWidget(okButton_);
    buttonRow->addWidget(cancelButton_);
    root->addLayout(buttonRow);

    connect(typeCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, &ConnectionDialog::onDriverTypeChanged);
    connect(okButton_, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelButton_, &QPushButton::clicked, this, &QDialog::reject);
    connect(testBtn_, &QPushButton::clicked, this, &ConnectionDialog::onTestConnectionClicked);

    wireValidationSignals();
    revalidate();
}

ConnectionDialog::~ConnectionDialog() = default;

QWidget* ConnectionDialog::buildSerialPage() {
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);

    serialDevice_ = new QLineEdit(page);
    serialDevice_->setPlaceholderText(QStringLiteral("/dev/ttyUSB0"));
    // Pre-populate from available serial ports.
    for (const auto& info : QSerialPortInfo::availablePorts()) {
        if (serialDevice_->text().isEmpty()) {
            serialDevice_->setText(info.systemLocation());
        }
    }
    form->addRow(tr("Device"), serialDevice_);

    serialBaud_ = new QSpinBox(page);
    serialBaud_->setRange(50, 4000000);
    serialBaud_->setValue(115200);
    form->addRow(tr("Baud rate"), serialBaud_);

    serialDataBits_ = new QSpinBox(page);
    serialDataBits_->setRange(5, 8);
    serialDataBits_->setValue(8);
    form->addRow(tr("Data bits"), serialDataBits_);

    serialParity_ = new QComboBox(page);
    serialParity_->addItems({QStringLiteral("none"), QStringLiteral("even"), QStringLiteral("odd"),
                             QStringLiteral("mark"), QStringLiteral("space")});
    form->addRow(tr("Parity"), serialParity_);

    serialStopBits_ = new QSpinBox(page);
    serialStopBits_->setRange(1, 2);
    serialStopBits_->setValue(1);
    form->addRow(tr("Stop bits"), serialStopBits_);

    serialFlow_ = new QComboBox(page);
    serialFlow_->addItems({QStringLiteral("none"), QStringLiteral("hardware"), QStringLiteral("software")});
    form->addRow(tr("Flow control"), serialFlow_);

    return page;
}

QWidget* ConnectionDialog::buildTcpPage() {
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);

    tcpHost_ = new QLineEdit(page);
    tcpHost_->setPlaceholderText(QStringLiteral("192.168.1.100"));
    form->addRow(tr("Host"), tcpHost_);

    tcpPort_ = new QSpinBox(page);
    tcpPort_->setRange(1, 65535);
    tcpPort_->setValue(9090);
    form->addRow(tr("Port"), tcpPort_);

    tcpTimeout_ = new QSpinBox(page);
    tcpTimeout_->setRange(0, 300000);
    tcpTimeout_->setValue(5000);
    tcpTimeout_->setSuffix(QStringLiteral(" ms"));
    form->addRow(tr("Connect timeout"), tcpTimeout_);

    return page;
}

QWidget* ConnectionDialog::buildUdpPage() {
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);

    udpLocalAddress_ = new QLineEdit(page);
    udpLocalAddress_->setText(QStringLiteral("0.0.0.0"));
    form->addRow(tr("Local bind address"), udpLocalAddress_);

    udpLocalPort_ = new QSpinBox(page);
    udpLocalPort_->setRange(0, 65535);
    udpLocalPort_->setValue(0);
    form->addRow(tr("Local bind port"), udpLocalPort_);

    udpRemoteHost_ = new QLineEdit(page);
    udpRemoteHost_->setPlaceholderText(QStringLiteral("(empty: receive-only)"));
    form->addRow(tr("Remote host"), udpRemoteHost_);

    udpRemotePort_ = new QSpinBox(page);
    udpRemotePort_->setRange(0, 65535);
    udpRemotePort_->setValue(0);
    form->addRow(tr("Remote port"), udpRemotePort_);

    udpMulticastGroup_ = new QLineEdit(page);
    udpMulticastGroup_->setPlaceholderText(QStringLiteral("(optional, e.g. 239.0.0.1)"));
    form->addRow(tr("Multicast group"), udpMulticastGroup_);

    udpMulticastTtl_ = new QSpinBox(page);
    udpMulticastTtl_->setRange(1, 255);
    udpMulticastTtl_->setValue(1);
    form->addRow(tr("Multicast TTL"), udpMulticastTtl_);

    return page;
}

QWidget* ConnectionDialog::buildReplayPage() {
    auto* page = new QWidget(this);
    auto* outer = new QVBoxLayout(page);
    auto* form = new QFormLayout();

    auto* pathRow = new QHBoxLayout();
    replayPath_ = new QLineEdit(page);
    auto* browse = new QPushButton(tr("Browse…"), page);
    pathRow->addWidget(replayPath_);
    pathRow->addWidget(browse);
    form->addRow(tr("Session file"), pathRow);

    replaySpeed_ = new QDoubleSpinBox(page);
    replaySpeed_->setRange(0.01, 100.0);
    replaySpeed_->setSingleStep(0.1);
    replaySpeed_->setValue(1.0);
    form->addRow(tr("Playback speed"), replaySpeed_);

    replayLoop_ = new QCheckBox(tr("Loop at end of file"), page);
    form->addRow(QString(), replayLoop_);

    outer->addLayout(form);

    connect(browse, &QPushButton::clicked, this, [this]() {
        const QString path =
            QFileDialog::getOpenFileName(this, tr("Open session file"), QString(), tr("Session files (*.sfreplay)"));
        if (!path.isEmpty()) {
            replayPath_->setText(path);
        }
    });

    return page;
}

QWidget* ConnectionDialog::buildCommandsPage() {
    auto* group = new QGroupBox(tr("Auto-connect commands"), this);
    auto* outer = new QVBoxLayout(group);

    commandsList_ = new QListWidget(group);
    outer->addWidget(commandsList_);

    auto* form = new QFormLayout();
    cmdName_ = new QLineEdit(group);
    form->addRow(tr("Name"), cmdName_);

    cmdPayloadHex_ = new QLineEdit(group);
    cmdPayloadHex_->setPlaceholderText(QStringLiteral("hex bytes, e.g. 454e41424c450a"));
    form->addRow(tr("Payload (hex)"), cmdPayloadHex_);

    cmdExpectedHex_ = new QLineEdit(group);
    cmdExpectedHex_->setPlaceholderText(QStringLiteral("optional hex bytes for expected response"));
    form->addRow(tr("Expected (hex)"), cmdExpectedHex_);

    cmdTimeout_ = new QSpinBox(group);
    cmdTimeout_->setRange(0, 300000);
    cmdTimeout_->setValue(1000);
    cmdTimeout_->setSuffix(QStringLiteral(" ms"));
    form->addRow(tr("Timeout"), cmdTimeout_);

    cmdDelay_ = new QSpinBox(group);
    cmdDelay_->setRange(0, 60000);
    cmdDelay_->setValue(0);
    cmdDelay_->setSuffix(QStringLiteral(" ms"));
    form->addRow(tr("Delay before"), cmdDelay_);

    outer->addLayout(form);

    auto* btnRow = new QHBoxLayout();
    addCmdBtn_ = new QPushButton(tr("Add"), group);
    removeCmdBtn_ = new QPushButton(tr("Remove selected"), group);
    btnRow->addWidget(addCmdBtn_);
    btnRow->addWidget(removeCmdBtn_);
    btnRow->addStretch();
    outer->addLayout(btnRow);

    connect(addCmdBtn_, &QPushButton::clicked, this, &ConnectionDialog::onAddCommand);
    connect(removeCmdBtn_, &QPushButton::clicked, this, &ConnectionDialog::onRemoveCommand);

    return group;
}

void ConnectionDialog::wireValidationSignals() {
    connect(displayName_, &QLineEdit::textChanged, this, &ConnectionDialog::revalidate);
    connect(serialDevice_, &QLineEdit::textChanged, this, &ConnectionDialog::revalidate);
    connect(tcpHost_, &QLineEdit::textChanged, this, &ConnectionDialog::revalidate);
    connect(tcpPort_, qOverload<int>(&QSpinBox::valueChanged), this, &ConnectionDialog::revalidate);
    connect(udpLocalPort_, qOverload<int>(&QSpinBox::valueChanged), this, &ConnectionDialog::revalidate);
    connect(udpRemoteHost_, &QLineEdit::textChanged, this, &ConnectionDialog::revalidate);
    connect(udpRemotePort_, qOverload<int>(&QSpinBox::valueChanged), this, &ConnectionDialog::revalidate);
    connect(replayPath_, &QLineEdit::textChanged, this, &ConnectionDialog::revalidate);
    connect(replaySpeed_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &ConnectionDialog::revalidate);
    connect(typeCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, &ConnectionDialog::revalidate);
}

void ConnectionDialog::onDriverTypeChanged(int idx) {
    stack_->setCurrentIndex(idx);
}

void ConnectionDialog::setDriverType(DriverType t) {
    const int idx = static_cast<int>(t);
    typeCombo_->setCurrentIndex(idx);
    stack_->setCurrentIndex(idx);
}

void ConnectionDialog::revalidate() {
    okButton_->setEnabled(isValid());
}

bool ConnectionDialog::isValid() const {
    if (displayName_->text().trimmed().isEmpty()) {
        return false;
    }
    switch (static_cast<DriverType>(typeCombo_->currentIndex())) {
    case DriverType::Serial:
        return !serialDevice_->text().trimmed().isEmpty();
    case DriverType::Tcp:
        return !tcpHost_->text().trimmed().isEmpty() && tcpPort_->value() >= 1;
    case DriverType::Udp: {
        const bool bindIntent =
            udpLocalPort_->value() != 0 || (udpLocalAddress_->text().trimmed() != QStringLiteral("0.0.0.0") &&
                                            !udpLocalAddress_->text().trimmed().isEmpty());
        const bool sendIntent = !udpRemoteHost_->text().trimmed().isEmpty() && udpRemotePort_->value() >= 1;
        return bindIntent || sendIntent;
    }
    case DriverType::Replay:
        return !replayPath_->text().trimmed().isEmpty() && replaySpeed_->value() > 0.0;
    }
    return false;
}

void ConnectionDialog::onAddCommand() {
    AutoConnectCommand c;
    c.name = cmdName_->text();
    c.payload = hexToBytes(cmdPayloadHex_->text());
    if (c.payload.isEmpty()) {
        return;  // payload is required
    }
    const auto exp = cmdExpectedHex_->text().trimmed();
    if (!exp.isEmpty()) {
        c.expected = hexToBytes(exp);
    }
    c.timeout = std::chrono::milliseconds{cmdTimeout_->value()};
    c.delayBefore = std::chrono::milliseconds{cmdDelay_->value()};
    commands_.push_back(c);
    commandsList_->addItem(QStringLiteral("%1: %2 bytes (timeout %3ms, delay %4ms)")
                               .arg(c.name.isEmpty() ? QStringLiteral("(unnamed)") : c.name)
                               .arg(c.payload.size())
                               .arg(c.timeout.count())
                               .arg(c.delayBefore.count()));
    cmdName_->clear();
    cmdPayloadHex_->clear();
    cmdExpectedHex_->clear();
}

void ConnectionDialog::onRemoveCommand() {
    const int row = commandsList_->currentRow();
    if (row < 0 || row >= static_cast<int>(commands_.size())) {
        return;
    }
    commands_.erase(commands_.begin() + row);
    delete commandsList_->takeItem(row);
}

void ConnectionDialog::onTestConnectionClicked() {
    // V1: open + close round-trip without producing real frames.
    // The full "test" UX is V1.5+ per spec §3.1. The button is
    // wired so the slot exists for tests; full driver round-trip
    // is M9 S10's integration test territory.
    SF_LOG_INFO("ConnectionDialog::onTestConnectionClicked stub (V1)");
}

void ConnectionDialog::setConfig(const ConnectionConfig& cfg) {
    id_ = cfg.id;
    displayName_->setText(cfg.displayName);
    {
        const int idx = schemaCombo_->findText(cfg.decoderSchemaId);
        if (idx >= 0) {
            schemaCombo_->setCurrentIndex(idx);
        } else {
            schemaCombo_->setEditText(cfg.decoderSchemaId);
        }
    }
    autoConnectStartup_->setChecked(cfg.autoConnectOnStartup);

    setDriverType(cfg.driverType);
    switch (cfg.driverType) {
    case DriverType::Serial:
        applySerial(std::get<signalforge::drivers::SerialConfig>(cfg.driverConfig));
        break;
    case DriverType::Tcp:
        applyTcp(std::get<signalforge::drivers::TcpConfig>(cfg.driverConfig));
        break;
    case DriverType::Udp:
        applyUdp(std::get<signalforge::drivers::UdpConfig>(cfg.driverConfig));
        break;
    case DriverType::Replay:
        applyReplay(std::get<signalforge::drivers::ReplayConfig>(cfg.driverConfig));
        break;
    }

    commands_ = cfg.autoConnectCommands;
    commandsList_->clear();
    for (const auto& c : commands_) {
        commandsList_->addItem(QStringLiteral("%1: %2 bytes (timeout %3ms, delay %4ms)")
                                   .arg(c.name.isEmpty() ? QStringLiteral("(unnamed)") : c.name)
                                   .arg(c.payload.size())
                                   .arg(c.timeout.count())
                                   .arg(c.delayBefore.count()));
    }
    revalidate();
}

void ConnectionDialog::applySerial(const signalforge::drivers::SerialConfig& s) {
    serialDevice_->setText(s.device);
    serialBaud_->setValue(s.baudRate);
    serialDataBits_->setValue(s.dataBits);
    {
        const int idx = serialParity_->findText(s.parity);
        if (idx >= 0)
            serialParity_->setCurrentIndex(idx);
    }
    serialStopBits_->setValue(s.stopBits);
    {
        const int idx = serialFlow_->findText(s.flowControl);
        if (idx >= 0)
            serialFlow_->setCurrentIndex(idx);
    }
}

void ConnectionDialog::applyTcp(const signalforge::drivers::TcpConfig& t) {
    tcpHost_->setText(t.host);
    tcpPort_->setValue(t.port);
    tcpTimeout_->setValue(static_cast<int>(t.connectTimeout.count()));
}

void ConnectionDialog::applyUdp(const signalforge::drivers::UdpConfig& u) {
    udpLocalAddress_->setText(u.localBindAddress);
    udpLocalPort_->setValue(u.localBindPort);
    udpRemoteHost_->setText(u.remoteHost);
    udpRemotePort_->setValue(u.remotePort);
    udpMulticastGroup_->setText(u.multicastGroup);
    udpMulticastTtl_->setValue(static_cast<int>(u.multicastTtl));
}

void ConnectionDialog::applyReplay(const signalforge::drivers::ReplayConfig& r) {
    replayPath_->setText(r.sessionFilePath);
    replaySpeed_->setValue(r.playbackSpeed);
    replayLoop_->setChecked(r.loop);
}

ConnectionConfig ConnectionDialog::config() const {
    ConnectionConfig cfg;
    switch (static_cast<DriverType>(typeCombo_->currentIndex())) {
    case DriverType::Serial:
        cfg = readSerial();
        break;
    case DriverType::Tcp:
        cfg = readTcp();
        break;
    case DriverType::Udp:
        cfg = readUdp();
        break;
    case DriverType::Replay:
        cfg = readReplay();
        break;
    }
    return readCommon(std::move(cfg));
}

ConnectionConfig ConnectionDialog::readCommon(ConnectionConfig cfg) const {
    cfg.id = id_;
    cfg.displayName = displayName_->text().trimmed();
    cfg.decoderSchemaId = schemaCombo_->currentText().trimmed();
    cfg.autoConnectOnStartup = autoConnectStartup_->isChecked();
    cfg.autoConnectCommands = commands_;
    return cfg;
}

ConnectionConfig ConnectionDialog::readSerial() const {
    ConnectionConfig cfg;
    cfg.driverType = DriverType::Serial;
    signalforge::drivers::SerialConfig s;
    s.device = serialDevice_->text().trimmed();
    s.baudRate = serialBaud_->value();
    s.dataBits = serialDataBits_->value();
    s.parity = serialParity_->currentText();
    s.stopBits = serialStopBits_->value();
    s.flowControl = serialFlow_->currentText();
    cfg.driverConfig = s;
    return cfg;
}

ConnectionConfig ConnectionDialog::readTcp() const {
    ConnectionConfig cfg;
    cfg.driverType = DriverType::Tcp;
    signalforge::drivers::TcpConfig t;
    t.host = tcpHost_->text().trimmed();
    t.port = static_cast<quint16>(tcpPort_->value());
    t.connectTimeout = std::chrono::milliseconds{tcpTimeout_->value()};
    cfg.driverConfig = t;
    return cfg;
}

ConnectionConfig ConnectionDialog::readUdp() const {
    ConnectionConfig cfg;
    cfg.driverType = DriverType::Udp;
    signalforge::drivers::UdpConfig u;
    u.localBindAddress = udpLocalAddress_->text().trimmed();
    u.localBindPort = static_cast<quint16>(udpLocalPort_->value());
    u.remoteHost = udpRemoteHost_->text().trimmed();
    u.remotePort = static_cast<quint16>(udpRemotePort_->value());
    u.multicastGroup = udpMulticastGroup_->text().trimmed();
    u.multicastTtl = static_cast<quint32>(udpMulticastTtl_->value());
    cfg.driverConfig = u;
    return cfg;
}

ConnectionConfig ConnectionDialog::readReplay() const {
    ConnectionConfig cfg;
    cfg.driverType = DriverType::Replay;
    signalforge::drivers::ReplayConfig r;
    r.sessionFilePath = replayPath_->text().trimmed();
    r.playbackSpeed = replaySpeed_->value();
    r.loop = replayLoop_->isChecked();
    cfg.driverConfig = r;
    return cfg;
}

}  // namespace signalforge::connection
