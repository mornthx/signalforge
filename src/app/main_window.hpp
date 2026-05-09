#pragma once

#include <QImage>
#include <QMainWindow>
#include <QString>
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
namespace signalforge::session {
class SessionWriter;
class TeeSignalValueSink;
}  // namespace signalforge::session

namespace signalforge::replay {
class PlaybackController;
class ReplayModeManager;
enum class PlaybackState;
enum class AppMode;
}  // namespace signalforge::replay

class QAction;
class QSlider;
class QToolBar;

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

    // ---- M14 S1 GUI smoke-test hooks ------------------------------------
    // These are non-frozen public helpers used by the
    // `tests/integration/gui/release_binary_smoke.sh` harness via the
    // `--auto-load-test-fixture` / `--auto-select-signal` /
    // `--dump-chart-png-after-ms` CLI flags in `main.cpp`. They exercise
    // already-public ConnectionManager / ChartManager APIs so the smoke
    // path stays inside the production code path; only the orchestration
    // glue lives here.

    /// Load a connection-config YAML and connect every connection.
    /// Returns false if the file is missing or the YAML is malformed.
    [[nodiscard]] bool autoLoadTestFixture(const QString& yamlPath);

    /// Add `signalId` to the first chart in `chartManager_`. Returns
    /// false if no chart exists or the signal id is empty.
    [[nodiscard]] bool autoSelectSignal(const QString& signalId);

    /// Grab the first chart-hosting QQuickWidget's framebuffer as a
    /// QImage. Returns a null QImage if no chart widget is laid out.
    [[nodiscard]] QImage grabChartImage() const;

protected:
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onAddConnectionRequested();
    void onEditConnectionRequested(const QString& id);
    void onConnectAllAction();
    void onDisconnectAllAction();
    void onAddChart();
    void onLiveToggleChanged(bool live);
    void onTimePresetChanged(int index);
    void refreshStatusBar();
    void onRecordToggle();
    void onRecordingFlushed(std::size_t bytes);
    void onRecordingError(const QString& message);
    void onOpenSessionRequested();
    void onReplayPlayPause();
    void onReplayStepForward();
    void onReplayStepBackward();
    void onReplaySeekSliderChanged(int value);
    void onReplaySpeedChanged(int index);
    void onExitReplayRequested();
    void onReplayPositionChanged(std::int64_t timestampNs, std::size_t recordIndex);
    void onReplayStateChanged();
    void onReplayError(const QString& message);

private:
    void buildChartUi();
    void buildConnectionUi();
    void buildSessionUi();
    void buildReplayUi();
    void updateReplayActionStates();
    void rebuildChartWidgets();
    [[nodiscard]] QStringList enumerateAvailableSchemaIds() const;

    // Plumbing.
    std::unique_ptr<signalforge::pipeline::PipelineManager> pipelineManager_;
    std::unique_ptr<signalforge::buffer::SignalBufferRegistry> signalBufferRegistry_;
    std::unique_ptr<signalforge::session::TeeSignalValueSink> teeSink_;
    std::unique_ptr<signalforge::decoder::DecoderRegistrar> decoderRegistrar_;
    std::unique_ptr<signalforge::connection::ConnectionManager> connectionManager_;
    std::unique_ptr<signalforge::session::SessionWriter> sessionWriter_;

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

    // M10 session recording UI.
    QAction* recordAction_ = nullptr;
    QLabel* recordingStatusLabel_ = nullptr;
    QString currentRecordingPath_;

    // M11 replay UX.
    std::unique_ptr<signalforge::replay::PlaybackController> playbackController_;
    std::unique_ptr<signalforge::replay::ReplayModeManager> replayModeManager_;
    QAction* openSessionAction_ = nullptr;
    QToolBar* replayToolbar_ = nullptr;
    QAction* replayPlayPauseAction_ = nullptr;
    QAction* replayStepBackAction_ = nullptr;
    QAction* replayStepForwardAction_ = nullptr;
    QSlider* replaySeekSlider_ = nullptr;
    QComboBox* replaySpeedCombo_ = nullptr;
    QAction* replayExitAction_ = nullptr;
    QLabel* replayStatusLabel_ = nullptr;
    bool replaySliderUserDriven_ = true;
};

}  // namespace signalforge::app
