// src/app/connection_manager.hpp
#pragma once

#include "drivers/driver_configs.hpp"
#include "drivers/driver_interface.hpp"
#include "frame/raw_frame.hpp"

#include <QDialog>
#include <QString>
#include <memory>

class QComboBox;
class QLineEdit;
class QSpinBox;
class QStackedWidget;
class QPushButton;
class QLabel;
class QPlainTextEdit;
class QTimer;
class QWidget;

namespace signalforge::pipeline {
class FramePipeline;
class PipelineManager;
}  // namespace signalforge::pipeline

namespace signalforge::app {

/// Preview-level Connection Manager dialog per M3 spec §4.6. Holds at
/// most one driver at a time; Connect/Disconnect validate the current
/// form, construct the right concrete driver, and transition the badge
/// + frame log reactively via driver signals. Log is capped to the
/// last `kMaxLogLines` entries to bound memory.
///
/// The dialog only *constructs* drivers; it does not persist config,
/// multi-connection state, or favorites (deferred to M7).
class ConnectionManager : public QDialog {
    Q_OBJECT

public:
    enum class DriverType { Serial = 0, Tcp = 1, Udp = 2, Replay = 3 };

    explicit ConnectionManager(QWidget* parent = nullptr);

    /// Primary constructor once M4 lands. `pipelineManager` may be
    /// nullptr for test contexts that exercise the dialog in isolation
    /// (the existing M3 offscreen tests do this). When non-null, the
    /// dialog attaches a `FramePipeline` for each connected driver on
    /// `Connect` and detaches on `Disconnect`.
    ConnectionManager(signalforge::pipeline::PipelineManager* pipelineManager, QWidget* parent);

    ~ConnectionManager() override;

    // Test hooks — called directly by the integration test to drive
    // the dialog without synthesising GUI events. Not intended for
    // production callers.
    void setDriverType(DriverType t);
    void setReplaySessionFile(const QString& path);
    void requestConnect();
    void requestDisconnect();
    [[nodiscard]] signalforge::drivers::DriverState currentState() const noexcept;
    [[nodiscard]] const QString& lastErrorMessage() const noexcept;

private slots:
    void onDriverTypeChanged(int idx);
    void onConnectClicked();
    void onDisconnectClicked();
    void onDriverStateChanged(signalforge::drivers::DriverState s);
    void onDriverFrameReceived(const signalforge::frame::RawFrame& f);
    void onDriverError(const signalforge::drivers::DriverError& e);
    void onThroughputTick();
    void onBrowseSessionFile();

private:
    signalforge::drivers::SerialConfig buildSerialConfig() const;
    signalforge::drivers::TcpConfig buildTcpConfig() const;
    signalforge::drivers::UdpConfig buildUdpConfig() const;
    signalforge::drivers::ReplayConfig buildReplayConfig() const;

    void buildUi();
    QWidget* buildSerialForm();
    QWidget* buildTcpForm();
    QWidget* buildUdpForm();
    QWidget* buildReplayForm();

    void appendFrameToLog(const signalforge::frame::RawFrame& f);
    void updateStateBadge(signalforge::drivers::DriverState s);

    QComboBox* typeCombo_{nullptr};
    QStackedWidget* stack_{nullptr};
    QPushButton* connectButton_{nullptr};
    QPushButton* disconnectButton_{nullptr};
    QLabel* stateBadge_{nullptr};
    QLabel* errorLabel_{nullptr};
    QLabel* statsLabel_{nullptr};
    QPlainTextEdit* frameLog_{nullptr};
    QTimer* statsTimer_{nullptr};

    // Serial form widgets
    QLineEdit* serialDevice_{nullptr};
    QComboBox* serialBaud_{nullptr};
    QSpinBox* serialDataBits_{nullptr};
    QComboBox* serialParity_{nullptr};
    QSpinBox* serialStopBits_{nullptr};
    QComboBox* serialFlow_{nullptr};

    // TCP form widgets
    QLineEdit* tcpHost_{nullptr};
    QSpinBox* tcpPort_{nullptr};
    QSpinBox* tcpTimeoutMs_{nullptr};

    // UDP form widgets
    QLineEdit* udpLocalAddr_{nullptr};
    QSpinBox* udpLocalPort_{nullptr};
    QLineEdit* udpRemoteHost_{nullptr};
    QSpinBox* udpRemotePort_{nullptr};
    QLineEdit* udpMulticastGroup_{nullptr};
    QSpinBox* udpMulticastTtl_{nullptr};

    // Replay form widgets
    QLineEdit* replayPath_{nullptr};

    std::unique_ptr<signalforge::drivers::DriverInterface> driver_;
    QString lastError_;
    int logLineCount_{0};

    signalforge::pipeline::PipelineManager* pipelineManager_{nullptr};
    signalforge::pipeline::FramePipeline* pipeline_{nullptr};
    QString currentDriverId_;

    [[nodiscard]] QString makeDriverId(DriverType t) const;

    static constexpr int kMaxLogLines = 200;
};

}  // namespace signalforge::app
