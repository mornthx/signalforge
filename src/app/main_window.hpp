#pragma once

#include "workbench/signal_identity.hpp"

#include <QElapsedTimer>
#include <QHash>
#include <QImage>
#include <QMainWindow>
#include <QString>
#include <memory>
#include <unordered_map>

class QComboBox;
class QDockWidget;
class QFrame;
class QLabel;
class QHBoxLayout;
class QPushButton;
class QSplitter;
class QStackedWidget;
class QTabWidget;
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
class FrameDissector;
}  // namespace signalforge::decoder
namespace signalforge::dashboard {
class Dashboard;
}  // namespace signalforge::dashboard
namespace signalforge::video {
class VideoPage;
class VideoUdpReceiver;
}  // namespace signalforge::video
namespace signalforge::inspect {
class ParsedSignalsView;
class RawFrameTap;
class RawPacketView;
}  // namespace signalforge::inspect
namespace signalforge::workbench {
class WorkbenchFrame;
class SegmentedControl;
class InspectorPanel;
class SelectionModel;
struct Selection;
}  // namespace signalforge::workbench

class QTreeWidgetItem;
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
class QMenu;
class QSlider;
class QToolBar;
class QTimer;

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

    /// Select a workspace tab by name ("raw" / "parsed" / "dashboard").
    /// No-op for an unknown name. Used by `--start-tab` for inspection and
    /// per-tier screenshot capture.
    void showWorkspaceTab(const QString& which);

    /// Add `signalId` to the first chart in `chartManager_`. Returns
    /// false if no chart exists or the signal id is empty.
    [[nodiscard]] bool autoSelectSignal(const QString& signalId);

    /// M21 visual harness: add `signalId` to the dashboard via the normal
    /// auto-suggest path (Numeric/State card or reused panel), as if the
    /// user ticked it in the signal list. Returns false if empty/no
    /// dashboard. Used to capture heterogeneous-panel baselines.
    [[nodiscard]] bool autoAddDashboardSignal(const QString& signalId);

    /// M22 visual harness: add a Table panel of every current signal, as the
    /// toolbar "+ Table" action does. Returns false if no dashboard.
    [[nodiscard]] bool autoAddTablePanel();

    /// M24 visual harness: add a Bar / Gauge panel for the first registered
    /// signal, as the toolbar "+ Bar" / "+ Gauge" actions do.
    [[nodiscard]] bool autoAddBarPanel();
    [[nodiscard]] bool autoAddGaugePanel();

    /// Grab the first chart-hosting QQuickWidget's framebuffer as a
    /// QImage. Returns a null QImage if no chart widget is laid out.
    [[nodiscard]] QImage grabChartImage() const;

    /// Programmatically start recording a session to `path`, reusing the
    /// same active-connection schema-discovery rule as the GUI Record
    /// menu (M14 F9). Returns false if a recording is already active or
    /// if SessionWriter::start fails. Used by the M14 S6 mechanical
    /// 18-test automation for T7 / T8 / T10 / T11.
    [[nodiscard]] bool autoStartRecording(const QString& path);

    /// Programmatically stop the active recording. Returns the byte
    /// count written (0 if no active recording).
    std::size_t autoStopRecording();

    /// M15 S1: capture the full MainWindow as a PNG at `path`. Uses
    /// `QWidget::grab()` (mechanism C per M15-concerns C1) which is
    /// in-process, headless-compatible, and reuses MainWindow's
    /// existing render path (no extra Qt event-loop spin required
    /// when called from a QTimer slot). Returns false if the path
    /// is empty or the save fails.
    ///
    /// Distinct from `grabChartImage()` (M14 S1 chart-pane-only) —
    /// `captureScreenshot` produces full-window state captures
    /// suitable for state-machine-complete baseline coverage
    /// (M15-concerns C3).
    [[nodiscard]] bool captureScreenshot(const QString& path);

    /// M15 S3 Round 2: load a connection-config YAML *without*
    /// auto-connecting any connection. Mirrors `autoLoadTestFixture`
    /// minus the ``connectAll()`` call, so visual baselines for
    /// `Idle` connection states (M15-concerns C3 §02) can be captured
    /// before any driver binds. Returns false on the same conditions
    /// as `autoLoadTestFixture`.
    [[nodiscard]] bool autoLoadFixtureNoConnect(const QString& yamlPath);

    /// M15 S3 Round 2: programmatically add ``extra`` charts beyond
    /// the default chart created during `buildChartUi`. Each call
    /// invokes ``ChartManager::createChart`` + ``rebuildChartWidgets``
    /// like the toolbar `+ Chart` action does. Used to capture
    /// multi-chart visual baselines (M15-concerns C3 §36, §37).
    /// Returns the chart count after the operation; 0 on failure.
    std::size_t autoAddCharts(int extra);

    /// M15 S3 Round 3: programmatically enter Replay mode + load a
    /// session file at ``path``. Mirrors the GUI ``onOpenSessionRequested``
    /// flow minus the QFileDialog + connection-pause confirmation
    /// dialog. Returns false if either ``replayModeManager_`` or
    /// ``playbackController_`` is null, the path is empty, the
    /// replay-mode transition fails, or the session-load fails.
    /// Used to capture replay-state visual baselines
    /// (M15-concerns C3 §17–§20).
    [[nodiscard]] bool autoLoadReplaySession(const QString& path);

    /// M15 S3 Round 3: start replay playback. Returns false if
    /// ``playbackController_`` is null or no session is loaded.
    [[nodiscard]] bool autoReplayPlay();

    /// M15 S3 Round 3: pause replay playback. Returns false on the
    /// same conditions as ``autoReplayPlay``.
    [[nodiscard]] bool autoReplayPause();

    /// M18 UX visual harness: step replay to its terminal Ended state.
    [[nodiscard]] bool autoReplayStepToEnd();

    /// M18 UX visual harness: force Buffer status strip warning/full states.
    [[nodiscard]] bool autoSetBufferStatusForVisualTest(const QString& status);

    /// M18 UX visual harness: force Chart status strip interrupted state.
    [[nodiscard]] bool autoSetChartStatusForVisualTest(const QString& status);

    /// M18 UX visual harness: force connection-config save status states.
    [[nodiscard]] bool autoSetConfigSaveStatusForVisualTest(const QString& status);

    /// M19 visual harness: force a connection row/status strip to render a
    /// transient/error state without mutating frozen connection contracts.
    [[nodiscard]] bool autoSetConnectionStateForVisualTest(const QString& status);

    /// M19 visual harness: show deterministic modal/fault-flow windows.
    [[nodiscard]] bool autoShowM19ModalForVisualTest(const QString& modal);

    /// M20 visual harness: focus a named keyboard-reachable control.
    [[nodiscard]] bool autoFocusWidgetForVisualTest(const QString& name);

    /// M15 S3 Round 3: seek replay to ``percent`` of the loaded
    /// session's duration (0–100). Returns false on out-of-range
    /// input or null playback controller.
    [[nodiscard]] bool autoReplaySeekPercent(int percent);

    /// M15 S3 Round 4: capture the full primary screen (xvfb
    /// framebuffer in CI; user's monitor when run directly) as
    /// a PNG at ``path``. Uses ``QScreen::grabWindow(0)`` so the
    /// capture includes top-level windows that are not children
    /// of MainWindow (e.g. open menus, modal dialogs). Distinct
    /// from ``captureScreenshot`` which grabs MainWindow only.
    /// Returns false if the path is empty, no primary screen is
    /// available, or the save fails.
    [[nodiscard]] bool captureFullScreen(const QString& path);

    /// M15 S3 Round 4: programmatically open one of the
    /// MainWindow's menu-bar menus by display name (e.g.
    /// ``"File"``, ``"Connections"``, ``"Session"``). Match is
    /// case-insensitive, ignoring ``&`` mnemonics. Returns false
    /// if the menu cannot be located or has no associated QMenu.
    /// Used to capture menu-open visual baselines (M15-concerns
    /// C3 §30–§32). Pairs with ``captureFullScreen`` since the
    /// menu popup is a separate top-level window.
    [[nodiscard]] bool autoOpenMenu(const QString& name);

    /// M15 S3 Round 4: programmatically show the connection-add
    /// dialog non-modally so it can be captured under
    /// mechanism-B. Differs from the production
    /// ``onAddConnectionRequested`` slot which uses ``exec()``
    /// (blocks the event loop) — this path uses ``show()``
    /// + ``WA_DeleteOnClose`` so the screenshot QTimer can fire
    /// in the regular event loop. Used for M15-concerns C3
    /// §24–§25. ``driverType`` accepts ``"serial"`` / ``"udp"``
    /// / ``"tcp"`` / ``"replay"`` (case-insensitive); empty
    /// string leaves the driver-type combo at its default.
    /// Returns false if the dialog cannot be created.
    [[nodiscard]] bool autoShowAddConnectionDialog(const QString& driverType = QString(), bool expandAdvanced = false);

    /// M15 S3 Round 4: same as ``autoShowAddConnectionDialog``
    /// but for the connection-edit flow. Picks the first
    /// connection in the registry; returns false if there is no
    /// connection to edit. Pairs with M15-concerns C3 §26.
    [[nodiscard]] bool autoShowEditConnectionDialog();

    /// M15 S3 Round 5: select the replay-speed combo entry at
    /// ``index`` (0=0.5×, 1=1×, 2=2×, 3=5×, 4=10×) and pop the
    /// dropdown so the popup is visible at capture time. Pairs
    /// with M15-concerns C3 §21. Returns false if no replay
    /// session is loaded or the index is out of range.
    [[nodiscard]] bool autoReplaySpeedComboPopup(int index);

protected:
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onAddConnectionRequested();
    void onEditConnectionRequested(const QString& id);
    void onConnectAllAction();
    void onDisconnectAllAction();
    void onAddChart();
    void onAddTable();
    void onAddBar();
    void onAddGauge();
    /// Switch the center workspace to the Dashboard tab (used when a panel is
    /// added, so the user/harness sees the result).
    void ensureDashboardVisible();
    /// Promote a signal from the Parsed tab into a dashboard panel of the named
    /// type ("numeric"/"state"/"plot"/"bar"/"gauge"), then show the dashboard.
    void onPromoteSignalToDashboard(const QString& signalId, const QString& typeToken);
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
    void onAbout();

private:
    void buildMenuBar();
    void buildChartUi();
    void buildConnectionUi();
    void buildSessionUi();
    void buildReplayUi();
    void buildThemeUi();
    void applyThemeFromAction(QAction* action);
    void updateReplayActionStates();
    void updateRecordingStatusLabel();
    void updateWorkflowModeLabel();
    void updateReplayStatusLabel();
    void updateEmptyStateVisibility();
    void onConfigurationSaveStateChanged(bool saved, const QString& path, const QString& message);
    [[nodiscard]] bool confirmExitReplayForClose();
    [[nodiscard]] QStringList enumerateAvailableSchemaIds() const;
    /// P3-S2: (re)build the Raw-view dissector for `driverType` from
    /// `decoderSchemaId` (the `examples/schemas/<id>.yaml` convention). An empty
    /// or unloadable schema clears the entry; the Raw view then shows the
    /// raw-bytes-only placeholder for that type's frames.
    void rebuildDissectorForType(const QString& driverType, const QString& decoderSchemaId);
    /// P5: populate the right inspector from the selected Parsed signal (empty id
    /// clears it) or the selected Raw dissection-tree field.
    void onSignalSelectedForInspector(const QString& signalId);
    void onDissectionFieldSelected(QTreeWidgetItem* item);
    /// P5: populate the right inspector with a selected dashboard panel's
    /// editable properties (range, size) + Remove.
    void onPanelSelectedForInspector(const QString& panelId);
    /// P5: react to the app-wide selection model (feeds the inspector; the
    /// backbone for cross-tier highlight).
    void onSelectionChanged(const signalforge::workbench::Selection& selection);
    /// P5: cross-tier drill-through — switch to the Raw tier filtered to the
    /// source of `signalId` (the packets that produced it).
    void onDrillToSourcePackets(const QString& signalId);
    /// P5: resolve a signal's colour — the user's per-signal override if set,
    /// else the driver's palette colour. The one provider shared by Parsed,
    /// Dashboard, and the inspector.
    [[nodiscard]] QColor resolveSignalColor(const QString& signalId);
    /// P5: re-push the colour provider to every view after an override changes.
    void refreshSignalColors();
    /// P5: per-signal colour override — pick (`onRecolorRequested`) / clear
    /// (`onResetColorRequested`), then refresh all tiers.
    void onRecolorRequested(const QString& signalId);
    void onResetColorRequested(const QString& signalId);
    /// P5: rename a signal — set/clear a per-signal display-name override (shown
    /// in Parsed + the inspector), then refresh.
    void onRenameRequested(const QString& signalId);

    // Plumbing.
    std::unique_ptr<signalforge::pipeline::PipelineManager> pipelineManager_;
    std::unique_ptr<signalforge::buffer::SignalBufferRegistry> signalBufferRegistry_;
    std::unique_ptr<signalforge::session::TeeSignalValueSink> teeSink_;
    std::unique_ptr<signalforge::decoder::DecoderRegistrar> decoderRegistrar_;
    std::unique_ptr<signalforge::connection::ConnectionManager> connectionManager_;
    std::unique_ptr<signalforge::session::SessionWriter> sessionWriter_;

    // M26: menu bar owned by buildMenuBar() in one place (File | Connections |
    // Session | View | Help); feature modules register their actions into these.
    QMenu* menuFile_ = nullptr;
    QMenu* menuConnections_ = nullptr;
    QMenu* menuSession_ = nullptr;
    QMenu* menuView_ = nullptr;
    QMenu* menuHelp_ = nullptr;

    // M23 dashboard surface (owns its own TimeAxisManager; the legacy
    // ChartManager/Chart are no longer used by the app).
    signalforge::dashboard::Dashboard* dashboard_ = nullptr;
    signalforge::inspect::ParsedSignalsView* parsedView_ = nullptr;
    std::shared_ptr<signalforge::inspect::RawFrameTap> rawFrameTap_;
    signalforge::inspect::RawPacketView* rawPacketView_ = nullptr;
    /// P3-S2: schema dissectors keyed by driver type ("udp"/"serial"/...), the
    /// `:`-prefix of a captured frame's source. Matches the DecoderRegistrar's
    /// per-type, last-config-wins model. The Raw view reads these to build its
    /// dissection tree.
    std::unordered_map<QString, std::shared_ptr<signalforge::decoder::FrameDissector>> dissectors_;
    // M34 redesign: activity-rail frame replaces the tab/stack workspace.
    signalforge::workbench::SignalIdentity signalIdentity_;  ///< P2: shared per-signal colour index (SSOT)
    QHash<QString, QString> displayNameOverrides_;           ///< P5: signal id → user rename (SSOT)
    signalforge::workbench::WorkbenchFrame* workbench_ = nullptr;
    signalforge::workbench::InspectorPanel* inspector_ = nullptr;       ///< P5: right-hand selection details
    signalforge::workbench::SelectionModel* selectionModel_ = nullptr;  ///< P5: app-wide current selection (backbone)
    signalforge::workbench::SegmentedControl* inspectSegments_ = nullptr;
    QStackedWidget* inspectStack_ = nullptr;              ///< Raw / Parsed / Dashboard views (Inspect mode)
    QStackedWidget* connectStack_ = nullptr;              ///< onboarding ↔ connection manager (Connect mode)
    signalforge::video::VideoPage* videoPage_ = nullptr;  ///< M35: Video mode content
    signalforge::video::VideoUdpReceiver* videoReceiver_ = nullptr;  ///< M35: dedicated video ingest
    QWidget* connectionManagerBody_ = nullptr;  ///< connection list + save banner (Connect mode page 1)
    QWidget* chartContainer_ = nullptr;         ///< Dashboard segment page: local toolbar + dashboard surface
    QWidget* chartEmptyState_ = nullptr;        ///< onboarding empty-state (Connect mode page 0)
    QPushButton* emptyAddConnectionButton_ = nullptr;
    QPushButton* emptyOpenSessionButton_ = nullptr;
    QPushButton* emptyLoadSchemaButton_ = nullptr;
    QVBoxLayout* chartLayout_ = nullptr;
    QToolButton* liveToggle_ = nullptr;
    QComboBox* timePresetCombo_ = nullptr;
    QFrame* statusStrip_ = nullptr;
    QHBoxLayout* statusStripLayout_ = nullptr;
    QLabel* workflowModeLabel_ = nullptr;
    QLabel* fpsLabel_ = nullptr;
    QLabel* droppedLabel_ = nullptr;
    QLabel* throttledLabel_ = nullptr;
    QLabel* bufferBudgetLabel_ = nullptr;  ///< M14 F15: signal_buffer budget usage
    QString bufferBudgetOverrideText_;
    QString bufferBudgetOverrideClass_;
    QString chartStatusOverrideText_;
    QString chartStatusOverrideClass_;

    // M9 connection UI. (M34: the legacy left dock is dissolved into Connect
    // mode — see connectionManagerBody_ above.)
    signalforge::connection::ConnectionListWidget* connectionList_ = nullptr;
    signalforge::connection::ConnectionStatusWidget* connectionStatus_ = nullptr;
    QLabel* configSaveDockBanner_ = nullptr;
    QLabel* configSaveStatusLabel_ = nullptr;
    bool configSaveHealthy_ = true;
    QString lastConfigSaveMessage_;

    // M10 session recording UI.
    QAction* recordAction_ = nullptr;
    QLabel* recordingStatusLabel_ = nullptr;
    QTimer* recordingHeartbeatTimer_ = nullptr;
    QElapsedTimer recordingElapsed_;
    QString currentRecordingPath_;
    std::size_t lastRecordingBytes_ = 0;

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

    // M20 theme runtime toggle.
    QAction* lightThemeAction_ = nullptr;
    QAction* darkThemeAction_ = nullptr;
    QAction* highContrastThemeAction_ = nullptr;
};

}  // namespace signalforge::app
