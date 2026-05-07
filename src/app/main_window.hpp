#pragma once

#include <QMainWindow>
#include <memory>

class QComboBox;
class QDockWidget;
class QLabel;
class QSplitter;
class QToolButton;
class QVBoxLayout;

namespace signalforge::pipeline {
class PipelineManager;
}
namespace signalforge::buffer {
class SignalBufferRegistry;
}
namespace signalforge::decoder {
class DecoderRegistrar;
}
namespace signalforge::chart {
class ChartManager;
class SignalSelector;
}  // namespace signalforge::chart
namespace signalforge::connection {
class ConnectionManager;
class ConnectionListWidget;
class ConnectionStatusWidget;
}  // namespace signalforge::connection

namespace signalforge::app {

/// SignalForge main window. Hosts the M8 chart subsystem
/// (ChartManager + SignalSelector + toolbar + status bar) plus
/// the M9 Connection Manager (list panel + status widget +
/// menu/toolbar entries to add/edit connections).
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void showEvent(QShowEvent* event) override;

private slots:
    void onAddConnectionRequested();
    void onEditConnectionRequested(const QString& id);
    void onConnectAllAction();
    void onDisconnectAllAction();
    void onAddChart();
    void onLiveToggleChanged(bool live);
    void onTimePresetChanged(int index);
    void refreshStatusBar();

private:
    void buildChartUi();
    void buildConnectionUi();
    void rebuildChartWidgets();
    [[nodiscard]] QStringList enumerateAvailableSchemaIds() const;

    // Plumbing.
    std::unique_ptr<signalforge::pipeline::PipelineManager> pipelineManager_;
    std::unique_ptr<signalforge::buffer::SignalBufferRegistry> signalBufferRegistry_;
    std::unique_ptr<signalforge::decoder::DecoderRegistrar> decoderRegistrar_;
    std::unique_ptr<signalforge::connection::ConnectionManager> connectionManager_;

    // M8 chart UI.
    std::unique_ptr<signalforge::chart::ChartManager> chartManager_;
    signalforge::chart::SignalSelector* signalSelector_ = nullptr;
    QSplitter* centralSplitter_ = nullptr;
    QWidget* chartContainer_ = nullptr;
    QVBoxLayout* chartLayout_ = nullptr;
    QToolButton* liveToggle_ = nullptr;
    QComboBox* timePresetCombo_ = nullptr;
    QLabel* fpsLabel_ = nullptr;
    QLabel* droppedLabel_ = nullptr;
    QLabel* throttledLabel_ = nullptr;

    // M9 connection UI.
    QDockWidget* connectionDock_ = nullptr;
    signalforge::connection::ConnectionListWidget* connectionList_ = nullptr;
    signalforge::connection::ConnectionStatusWidget* connectionStatus_ = nullptr;
};

}  // namespace signalforge::app
