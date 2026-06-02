// src/connection/connection_dialog.hpp
#pragma once

#include "connection/connection.hpp"

#include <QDialog>
#include <QStringList>

class QStackedWidget;
class QComboBox;
class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QGroupBox;
class QPushButton;
class QListWidget;

namespace signalforge::connection {

/// Modal `QDialog` for adding / editing one connection (per
/// design decision M9.1 + M9.3). The dialog hosts a driver-type
/// combo that drives a `QStackedWidget` so each driver type
/// shows only its own fields plus the common header.
///
/// Per-driver validation runs continuously; the OK button is
/// disabled until the inputs form a valid `ConnectionConfig`.
class ConnectionDialog : public QDialog {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ConnectionDialog)

public:
    /// `availableSchemaIds` populates the decoder schema combo.
    /// Caller (typically MainWindow) is responsible for
    /// enumerating schemas (e.g., via filesystem walk of
    /// `examples/schemas/*.yaml`); this dialog does not query
    /// `DecoderRegistrar` directly to keep M5's frozen surface
    /// untouched.
    explicit ConnectionDialog(QStringList availableSchemaIds, QWidget* parent = nullptr);
    ~ConnectionDialog() override;

    /// Pre-fill the dialog with an existing connection's config.
    /// Used by edit-mode invocations.
    void setConfig(const ConnectionConfig& config);

    /// Read the current dialog state as a `ConnectionConfig`.
    /// Only meaningful when `isValid()` is `true`.
    [[nodiscard]] ConnectionConfig config() const;

    /// Whether the current dialog state forms a valid config.
    [[nodiscard]] bool isValid() const;

    // Test hooks — let unit tests drive the dialog without
    // synthesising mouse events. Not part of production callers.
    void setDriverType(DriverType t);
    [[nodiscard]] QStackedWidget* stackedWidget() const noexcept {
        return stack_;
    }
    [[nodiscard]] QPushButton* okButton() const noexcept {
        return okButton_;
    }
    [[nodiscard]] QGroupBox* advancedCommandsGroup() const noexcept {
        return commandsGroup_;
    }
    [[nodiscard]] QWidget* advancedCommandsBody() const noexcept {
        return commandsBody_;
    }
    [[nodiscard]] QPushButton* validateButton() const noexcept {
        return testBtn_;
    }

private slots:
    void onDriverTypeChanged(int idx);
    void revalidate();
    void onAddCommand();
    void onRemoveCommand();
    void onTestConnectionClicked();

private:
    QWidget* buildSerialPage();
    QWidget* buildTcpPage();
    QWidget* buildUdpPage();
    QWidget* buildReplayPage();
    QWidget* buildCommandsPage();

    void wireValidationSignals();

    ConnectionConfig readSerial() const;
    ConnectionConfig readTcp() const;
    ConnectionConfig readUdp() const;
    ConnectionConfig readReplay() const;
    ConnectionConfig readCommon(ConnectionConfig cfg) const;

    void applyCommon(const ConnectionConfig& cfg);
    void applySerial(const signalforge::drivers::SerialConfig& s);
    void applyTcp(const signalforge::drivers::TcpConfig& t);
    void applyUdp(const signalforge::drivers::UdpConfig& u);
    void applyReplay(const signalforge::drivers::ReplayConfig& r);

    QStringList availableSchemaIds_;
    QString id_;  // immutable across edit; preserved across applyCommon

    // Common fields
    QLineEdit* displayName_{nullptr};
    QComboBox* schemaCombo_{nullptr};
    QCheckBox* autoConnectStartup_{nullptr};

    // Driver type selector + stacked pages
    QComboBox* typeCombo_{nullptr};
    QStackedWidget* stack_{nullptr};

    // Serial fields
    QLineEdit* serialDevice_{nullptr};
    QSpinBox* serialBaud_{nullptr};
    QSpinBox* serialDataBits_{nullptr};
    QComboBox* serialParity_{nullptr};
    QSpinBox* serialStopBits_{nullptr};
    QComboBox* serialFlow_{nullptr};

    // TCP fields
    QLineEdit* tcpHost_{nullptr};
    QSpinBox* tcpPort_{nullptr};
    QSpinBox* tcpTimeout_{nullptr};

    // UDP fields
    QLineEdit* udpLocalAddress_{nullptr};
    QSpinBox* udpLocalPort_{nullptr};
    QLineEdit* udpRemoteHost_{nullptr};
    QSpinBox* udpRemotePort_{nullptr};
    QLineEdit* udpMulticastGroup_{nullptr};
    QSpinBox* udpMulticastTtl_{nullptr};
    QCheckBox* udpVideoEnabled_{nullptr};  // M35: bind a dedicated video port on connect
    QSpinBox* udpVideoPort_{nullptr};

    // Replay fields
    QLineEdit* replayPath_{nullptr};
    QDoubleSpinBox* replaySpeed_{nullptr};
    QCheckBox* replayLoop_{nullptr};

    // Advanced auto-connect commands. Hidden by default because it is a rare driver-initialisation path.
    QGroupBox* commandsGroup_{nullptr};
    QWidget* commandsBody_{nullptr};
    QListWidget* commandsList_{nullptr};
    QLineEdit* cmdName_{nullptr};
    QLineEdit* cmdPayloadHex_{nullptr};
    QLineEdit* cmdExpectedHex_{nullptr};
    QSpinBox* cmdTimeout_{nullptr};
    QSpinBox* cmdDelay_{nullptr};
    QPushButton* addCmdBtn_{nullptr};
    QPushButton* removeCmdBtn_{nullptr};
    std::vector<AutoConnectCommand> commands_;

    // Buttons
    QPushButton* okButton_{nullptr};
    QPushButton* cancelButton_{nullptr};
    QPushButton* testBtn_{nullptr};
};

}  // namespace signalforge::connection
