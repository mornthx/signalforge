#pragma once

#include <QMainWindow>
#include <memory>

class QComboBox;
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

namespace signalforge::app {

class ConnectionManager;

/// SignalForge main window. Hosts the M8 chart subsystem
/// (ChartManager + SignalSelector + toolbar + status bar) plus
/// the existing Connection Manager dialog.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void showEvent(QShowEvent* event) override;

private slots:
    void openConnectionManager();
    void onAddChart();
    void onLiveToggleChanged(bool live);
    void onTimePresetChanged(int index);
    void refreshStatusBar();

private:
    void buildChartUi();
    void rebuildChartWidgets();

    // Existing pipeline / decoder plumbing.
    std::unique_ptr<signalforge::pipeline::PipelineManager> pipelineManager_;
    std::unique_ptr<signalforge::buffer::SignalBufferRegistry> signalBufferRegistry_;
    std::unique_ptr<signalforge::decoder::DecoderRegistrar> decoderRegistrar_;
    std::unique_ptr<ConnectionManager> connectionManager_;

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
};

}  // namespace signalforge::app
