#include "main_window.hpp"

#include "app_style.hpp"
#include "buffer/signal_buffer.hpp"
#include "buffer/signal_buffer_registry.hpp"
#include "chart/time_axis_manager.hpp"
#include "connection/connection.hpp"
#include "connection/connection_dialog.hpp"
#include "connection/connection_list_widget.hpp"
#include "connection/connection_manager.hpp"
#include "connection/connection_status_widget.hpp"
#include "dashboard/dashboard.hpp"
#include "dashboard/panel.hpp"
#include "dashboard/panel_types.hpp"
#include "dashboard/plot_view.hpp"
#include "dashboard/value_format.hpp"
#include "decode/decoder_registrar.hpp"
#include "decode/frame_dissector.hpp"
#include "decode/schema_validator.hpp"
#include "generated_style_tokens.hpp"
#include "inspect/parsed_signals_view.hpp"
#include "inspect/raw_frame_tap.hpp"
#include "inspect/raw_packet_view.hpp"
#include "observability/logging.hpp"
#include "pipeline/frame_pipeline.hpp"
#include "pipeline/pipeline_manager.hpp"
#include "replay/playback_controller.hpp"
#include "replay/replay_mode_manager.hpp"
#include "session/session_writer.hpp"
#include "session/tee_signal_value_sink.hpp"
#include "workbench/components/inspector_panel.hpp"
#include "workbench/components/segmented_control.hpp"
#include "workbench/selection_model.hpp"
#include "workbench/signal_identity.hpp"
#include "workbench/workbench_frame.hpp"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QQuickItem>
#include <QQuickWidget>
#include <QScreen>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QVector>
#include <QWindow>
#include <unordered_map>

namespace {

// M14 F12: format a relative-from-zero replay position in
// mm:ss.fff (audit §F12 — pre-fix formatter showed raw nanoseconds
// which looked like an absolute UTC timestamp). Negative or
// out-of-range inputs clamp to "00:00.000".
QString formatReplayPosition(std::int64_t ns) {
    if (ns < 0) {
        ns = 0;
    }
    const std::int64_t totalMs = ns / 1'000'000;
    const std::int64_t mm = totalMs / 60'000;
    const std::int64_t ss = (totalMs / 1'000) % 60;
    const std::int64_t fff = totalMs % 1'000;
    return QStringLiteral("%1:%2.%3")
        .arg(mm, 2, 10, QLatin1Char('0'))
        .arg(ss, 2, 10, QLatin1Char('0'))
        .arg(fff, 3, 10, QLatin1Char('0'));
}

QString formatReplayStatusSeconds(std::int64_t ns) {
    if (ns < 0) {
        ns = 0;
    }
    return QStringLiteral("%1s").arg(static_cast<double>(ns) / 1'000'000'000.0, 0, 'f', 1);
}

QString formatElapsedMs(qint64 elapsedMs) {
    if (elapsedMs < 0) {
        elapsedMs = 0;
    }
    const qint64 totalSeconds = elapsedMs / 1000;
    const qint64 hh = totalSeconds / 3600;
    const qint64 mm = (totalSeconds / 60) % 60;
    const qint64 ss = totalSeconds % 60;
    if (hh > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(hh, 2, 10, QLatin1Char('0'))
            .arg(mm, 2, 10, QLatin1Char('0'))
            .arg(ss, 2, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1:%2").arg(mm, 2, 10, QLatin1Char('0')).arg(ss, 2, 10, QLatin1Char('0'));
}

// driverId convention: `<type>:<connectionId>` — DecoderRegistrar
// splits on ':' and uses the prefix as its driver-type lookup key
// (so "udp:conn-3" → "udp"). Mirrors the `driverTypeToYamlInternal`
// helper in connection_manager.cpp's anonymous namespace.

void applyLabelClass(QLabel* label, const char* className) {
    if (label == nullptr) {
        return;
    }
    label->setProperty("class", QLatin1String(className));
    label->style()->unpolish(label);
    label->style()->polish(label);
    label->update();
}

void applyChartHostTheme(QQuickWidget* hostWidget) {
    if (hostWidget == nullptr) {
        return;
    }
    hostWidget->setClearColor(QApplication::palette().color(QPalette::Base));
}

QFrame* makeStatusCell(const QString& titleText, QWidget* valueWidget, QWidget* parent) {
    auto* frame = new QFrame(parent);
    frame->setObjectName(QStringLiteral("statusCell"));
    auto* layout = new QHBoxLayout(frame);
    layout->setContentsMargins(8, 0, 8, 0);
    layout->setSpacing(6);
    // DR-001 #5: omit the caption when empty — cells whose value already
    // names itself ("Record idle", "Buffer 0% 0 MiB", …) skip the title to
    // avoid the doubled "label: value" look.
    if (!titleText.isEmpty()) {
        auto* title = new QLabel(titleText, frame);
        title->setProperty("class", QLatin1String("caption"));
        title->setToolTip(titleText);
        layout->addWidget(title);
    }
    layout->addWidget(valueWidget);
    return frame;
}

QWidget* makeStatusValueRow(QWidget* parent, std::initializer_list<QWidget*> widgets) {
    auto* row = new QWidget(parent);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    for (auto* widget : widgets) {
        layout->addWidget(widget);
    }
    return row;
}

QString driverTypeName(signalforge::connection::DriverType t) {
    using DT = signalforge::connection::DriverType;
    switch (t) {
    case DT::Serial:
        return QStringLiteral("serial");
    case DT::Tcp:
        return QStringLiteral("tcp");
    case DT::Udp:
        return QStringLiteral("udp");
    case DT::Replay:
        return QStringLiteral("replay");
    }
    return QStringLiteral("unknown");
}

// M34 P2: resolve a signal-palette index (`color.signal.<i>`) to the active
// theme's QColor — the swatch colour the Parsed view shows.
QColor signalPaletteColor(int index) {
    using signalforge::app::SignalForgeStyle;
    const int i = ((index % 8) + 8) % 8;
    namespace tk = signalforge::tokens;
    const QColor light[] = {tk::light::signal0(), tk::light::signal1(), tk::light::signal2(), tk::light::signal3(),
                            tk::light::signal4(), tk::light::signal5(), tk::light::signal6(), tk::light::signal7()};
    const QColor dark[] = {tk::dark::signal0(), tk::dark::signal1(), tk::dark::signal2(), tk::dark::signal3(),
                           tk::dark::signal4(), tk::dark::signal5(), tk::dark::signal6(), tk::dark::signal7()};
    const QColor hc[] = {tk::high_contrast::signal0(), tk::high_contrast::signal1(), tk::high_contrast::signal2(),
                         tk::high_contrast::signal3(), tk::high_contrast::signal4(), tk::high_contrast::signal5(),
                         tk::high_contrast::signal6(), tk::high_contrast::signal7()};
    switch (SignalForgeStyle::activeTheme()) {
    case SignalForgeStyle::Theme::Dark:
        return dark[i];
    case SignalForgeStyle::Theme::HighContrast:
        return hc[i];
    default:
        return light[i];
    }
}

// M34 P2: resolve a signal quality to the active theme's status colour
// (good→connected/green, stale+uncertain→connecting/amber, bad→error/red).
QColor qualityColor(signalforge::workbench::Quality quality) {
    using signalforge::app::SignalForgeStyle;
    using Q = signalforge::workbench::Quality;
    namespace tk = signalforge::tokens;
    QColor good;
    QColor warn;
    QColor bad;
    switch (SignalForgeStyle::activeTheme()) {
    case SignalForgeStyle::Theme::Dark:
        good = tk::dark::statusConnected();
        warn = tk::dark::statusConnecting();
        bad = tk::dark::statusError();
        break;
    case SignalForgeStyle::Theme::HighContrast:
        good = tk::high_contrast::statusConnected();
        warn = tk::high_contrast::statusConnecting();
        bad = tk::high_contrast::statusError();
        break;
    default:
        good = tk::light::statusConnected();
        warn = tk::light::statusConnecting();
        bad = tk::light::statusError();
        break;
    }
    switch (quality) {
    case Q::Good:
        return good;
    case Q::Stale:
    case Q::Uncertain:
        return warn;
    case Q::Bad:
        return bad;
    }
    return good;
}

}  // namespace

namespace signalforge::app {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("SignalForge"));
    resize(1280, 800);

    pipelineManager_ = std::make_unique<signalforge::pipeline::PipelineManager>(this);
    signalBufferRegistry_ = std::make_unique<signalforge::buffer::SignalBufferRegistry>();

    // M31: Tier-1 raw-packet tap — one tap shared across all pipelines, so the
    // Raw view shows every driver's frames together. Registered on each
    // pipeline as it attaches (read-only FrameSink; never mutates frames).
    rawFrameTap_ = std::make_shared<signalforge::inspect::RawFrameTap>();
    connect(pipelineManager_.get(), &signalforge::pipeline::PipelineManager::pipelineAttached, this,
            [this](const QString&, signalforge::pipeline::FramePipeline* pipeline) {
                if (pipeline != nullptr) {
                    pipeline->addSink(rawFrameTap_);
                }
            });

    // M10 fan-out: route decoded signals through a TeeSink that
    // always feeds the SignalBufferRegistry (M6) and may
    // additionally feed a SessionWriter (M10) when a recording
    // is active. The TeeSink is the DecoderRegistrar's single
    // sink — preserving M5's frozen interface while enabling
    // multi-consumer routing per ADR-007.
    teeSink_ = std::make_unique<signalforge::session::TeeSignalValueSink>();
    teeSink_->addSink(signalBufferRegistry_.get());
    sessionWriter_ = std::make_unique<signalforge::session::SessionWriter>(*signalBufferRegistry_, this);
    // ADR-013 (F6): subscribe SessionWriter to the TeeSink at construction
    // so `onSignalsRegistered` events from the SchemaDecoder always reach
    // the writer's catalog cache, regardless of whether a recording is
    // active. SessionWriter::onSignal early-returns when state != Recording,
    // and `onSignalsRegistered` always caches; gating recording state at
    // start()/stop() is sufficient. Without this subscription, Connect →
    // Record → Stop produces a file with zero Type-1 records (silent data
    // loss documented in run5 audit §F6).
    teeSink_->addSink(sessionWriter_.get());
    {
        std::shared_ptr<signalforge::decoder::SignalValueSink> sink(teeSink_.get(),
                                                                    [](signalforge::decoder::SignalValueSink*) {});
        decoderRegistrar_ = std::make_unique<signalforge::decoder::DecoderRegistrar>(
            pipelineManager_.get(), std::unordered_map<QString, QString>{}, std::move(sink), this);
    }
    connectionManager_ = std::make_unique<signalforge::connection::ConnectionManager>(*decoderRegistrar_, this);

    // ADR-009: bridge ConnectionManager state transitions into
    // PipelineManager attach/detach. Without this, no production
    // code calls PipelineManager::attach(), pipelineAttached never
    // fires, and DecoderRegistrar never builds a SchemaDecoder.
    connect(connectionManager_.get(), &signalforge::connection::ConnectionManager::connectionStateChanged, this,
            [this](const QString& id, signalforge::connection::Connection::State state) {
                auto* conn = connectionManager_->connection(id);
                if (conn == nullptr || conn->driver() == nullptr) {
                    return;
                }
                const QString driverType = driverTypeName(conn->config().driverType);
                const QString driverId = driverType + QStringLiteral(":") + id;
                if (state == signalforge::connection::Connection::State::Connected) {
                    signalforge::pipeline::PipelineConfig cfg;
                    cfg.driverId = driverId;
                    (void)pipelineManager_->attach(conn->driver(), cfg);
                    // P3-S2: build a FrameDissector for the Raw view's dissection
                    // tree, keyed by driver type to match the DecoderRegistrar's
                    // per-type schema model (a captured frame's `source` carries
                    // this type as its `:`-prefix). Last connected schema wins.
                    rebuildDissectorForType(driverType, conn->config().decoderSchemaId);
                } else if (state == signalforge::connection::Connection::State::Idle ||
                           state == signalforge::connection::Connection::State::Error) {
                    pipelineManager_->detach(driverId);
                }
            });

    // Auto-load the persisted connection list. Missing file is
    // benign (first launch); ERROR-level parse failures degrade
    // to an empty manager (logged by ConnectionManager).
    // ADR-013 (F17): loadConfigFile sets configPath_ regardless of
    // whether the file exists, so subsequent addConnection /
    // editConnection / removeConnection mutations autosave to this
    // path through ConnectionManager::autoSave().
    const QString cfgPath = signalforge::connection::ConnectionManager::defaultConfigPath();
    QDir().mkpath(QFileInfo(cfgPath).absolutePath());
    (void)connectionManager_->loadConfigFile(cfgPath);
    const int startupConnects = connectionManager_->connectStartupConnections();
    if (startupConnects > 0) {
        SF_LOG_INFO("MainWindow: auto-connect-on-startup attempted {} connection(s)", startupConnects);
    }

    // ADR-013 (F17) defense-in-depth: also save on application exit.
    // Catches any future mutation path that bypasses autoSave() and
    // covers the "user adds a connection then immediately quits"
    // edge case (autoSave already runs synchronously on
    // addConnection, so this is belt-and-suspenders).
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this, cfgPath]() {
        if (connectionManager_ != nullptr) {
            (void)connectionManager_->saveConfigFile(cfgPath);
        }
    });

    // M11 replay plumbing. PlaybackController dispatches signals
    // back into the same SignalBufferRegistry the live path uses,
    // so charts work uniformly across modes. ReplayModeManager
    // orchestrates Live ↔ Replay transitions on top.
    playbackController_ = std::make_unique<signalforge::replay::PlaybackController>(*signalBufferRegistry_, this);
    replayModeManager_ =
        std::make_unique<signalforge::replay::ReplayModeManager>(*connectionManager_, *playbackController_, this);

    buildMenuBar();
    buildChartUi();
    buildConnectionUi();
    buildSessionUi();
    buildReplayUi();
    buildThemeUi();
}

MainWindow::~MainWindow() {
    // M21 C2: FramePipeline::~FramePipeline disconnects from its driver, so
    // the pipeline must be torn down while the ConnectionManager-owned
    // drivers are still alive. Members destroy in reverse declaration order
    // (which frees the ConnectionManager — and its drivers — before
    // pipelineManager_), and we cannot reorder the members because
    // construction has the opposite dependency (decoderRegistrar_ needs
    // pipelineManager_; connectionManager_ needs decoderRegistrar_). So here,
    // before automatic member teardown frees the drivers, sever the
    // connection→pipeline signal bridge and destroy the pipelines explicitly.
    if (connectionManager_ != nullptr) {
        connectionManager_->disconnect(this);
    }
    pipelineManager_.reset();
}

void MainWindow::buildMenuBar() {
    // M26 (DR-001 #4): one owned, conventionally-ordered menu bar
    // (File | Connections | Session | View | Help). Feature modules register
    // their actions into these menus rather than each calling addMenu() in an
    // arbitrary order. Names are kept stable for the visual harness.
    menuFile_ = menuBar()->addMenu(tr("&File"));
    menuConnections_ = menuBar()->addMenu(tr("&Connections"));
    menuSession_ = menuBar()->addMenu(tr("&Session"));
    menuView_ = menuBar()->addMenu(tr("&View"));
    menuHelp_ = menuBar()->addMenu(tr("&Help"));

    auto* aboutAction = menuHelp_->addAction(tr("&About SignalForge"));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::onAbout);
}

void MainWindow::onAbout() {
    QMessageBox::about(this, tr("About SignalForge"),
                       tr("<b>SignalForge</b><br>A desktop workbench for embedded-device "
                          "bring-up — connect a source, build a dashboard of panels, record "
                          "and replay sessions."));
}

void MainWindow::buildChartUi() {
    // ---- Tier 3 dashboard surface --------------------------------------
    dashboard_ = new signalforge::dashboard::Dashboard(*signalBufferRegistry_);
    // M34 P4: unify panel colours onto the shared SignalIdentity — a signal's
    // plot trace / bar / gauge fill now match its Parsed swatch (same lambda).
    dashboard_->setSignalColorProvider([this](const QString& id) { return resolveSignalColor(id); });
    connect(dashboard_, &signalforge::dashboard::Dashboard::panelsChanged, this,
            &MainWindow::updateEmptyStateVisibility);

    // M34 §7.4: the Dashboard segment owns a dashboard-local toolbar
    // (+widget / live / time-range). These actions are dashboard-scoped, so
    // they no longer sit on a global toolbar (owner's point #1 — "+Plot belongs
    // in the dashboard").
    chartContainer_ = new QWidget;
    chartLayout_ = new QVBoxLayout(chartContainer_);
    chartLayout_->setContentsMargins(0, 0, 0, 0);
    chartLayout_->setSpacing(0);

    auto* dashToolbar = new QToolBar(chartContainer_);
    dashToolbar->setObjectName(QStringLiteral("dashboardToolbar"));
    dashToolbar->setMovable(false);

    liveToggle_ = new QToolButton(dashToolbar);
    liveToggle_->setCheckable(true);
    liveToggle_->setChecked(true);
    liveToggle_->setText(tr("● Live"));
    liveToggle_->setToolTip(tr("Toggle live mode (vs paused)"));
    connect(liveToggle_, &QToolButton::toggled, this, &MainWindow::onLiveToggleChanged);
    dashToolbar->addWidget(liveToggle_);

    timePresetCombo_ = new QComboBox(dashToolbar);
    timePresetCombo_->addItem(tr("1 sec"));
    timePresetCombo_->addItem(tr("10 sec"));
    timePresetCombo_->addItem(tr("1 min"));
    timePresetCombo_->addItem(tr("10 min"));
    timePresetCombo_->addItem(tr("1 hour"));
    timePresetCombo_->setCurrentIndex(1);
    connect(timePresetCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::onTimePresetChanged);
    dashToolbar->addWidget(timePresetCombo_);

    auto* addChartAction = dashToolbar->addAction(tr("+ Plot"));
    addChartAction->setToolTip(tr("Add an empty plot panel"));
    connect(addChartAction, &QAction::triggered, this, &MainWindow::onAddChart);

    auto* addTableAction = dashToolbar->addAction(tr("+ Table"));
    addTableAction->setToolTip(tr("Add a table of every current signal's value"));
    connect(addTableAction, &QAction::triggered, this, &MainWindow::onAddTable);

    auto* addBarAction = dashToolbar->addAction(tr("+ Bar"));
    addBarAction->setToolTip(tr("Add a bar meter for the first signal"));
    connect(addBarAction, &QAction::triggered, this, &MainWindow::onAddBar);

    auto* addGaugeAction = dashToolbar->addAction(tr("+ Gauge"));
    addGaugeAction->setToolTip(tr("Add a gauge for the first signal"));
    connect(addGaugeAction, &QAction::triggered, this, &MainWindow::onAddGauge);

    chartLayout_->addWidget(dashToolbar);
    // Host the free-form dashboard in a vertical scroll area so cards may live
    // below the fold (the surface grows downward; the viewport scrolls). The
    // dashboard keeps the viewport width (cards wrap by width, never sideways).
    auto* dashScroll = new QScrollArea(chartContainer_);
    dashScroll->setObjectName(QStringLiteral("dashboardScroll"));
    dashScroll->setWidgetResizable(true);
    dashScroll->setFrameShape(QFrame::NoFrame);
    dashScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    dashScroll->setWidget(dashboard_);
    chartLayout_->addWidget(dashScroll, 1);

    // ---- Onboarding empty-state (Connect mode, M34 §7.1) ---------------
    chartEmptyState_ = new QFrame;
    chartEmptyState_->setObjectName(QStringLiteral("chartEmptyState"));
    {
        auto* emptyRoot = new QVBoxLayout(chartEmptyState_);
        emptyRoot->setContentsMargins(24, 18, 24, 18);
        emptyRoot->setSpacing(8);
        auto* title = new QLabel(tr("Start a SignalForge workflow"), chartEmptyState_);
        title->setProperty("class", QLatin1String("display"));
        emptyRoot->addWidget(title);
        auto* caption = new QLabel(
            tr("Connect a live source to inspect raw packets and decoded signals, then build a dashboard from "
               "them — or open an existing replay."),
            chartEmptyState_);
        caption->setProperty("class", QLatin1String("caption"));
        caption->setWordWrap(true);
        emptyRoot->addWidget(caption);
        auto* actions = new QHBoxLayout();
        actions->setSpacing(8);
        emptyAddConnectionButton_ = new QPushButton(tr("Add connection"), chartEmptyState_);
        emptyAddConnectionButton_->setProperty("class", QLatin1String("primary"));
        emptyOpenSessionButton_ = new QPushButton(tr("Open session"), chartEmptyState_);
        emptyOpenSessionButton_->setProperty("class", QLatin1String("primary"));
        emptyLoadSchemaButton_ = new QPushButton(tr("Load schema"), chartEmptyState_);
        emptyLoadSchemaButton_->setToolTip(tr("Decoder schemas are selected while adding or editing a connection."));
        actions->addWidget(emptyAddConnectionButton_);
        actions->addWidget(emptyOpenSessionButton_);
        actions->addWidget(emptyLoadSchemaButton_);
        actions->addStretch();
        emptyRoot->addLayout(actions);
        connect(emptyAddConnectionButton_, &QPushButton::clicked, this, &MainWindow::onAddConnectionRequested);
        connect(emptyOpenSessionButton_, &QPushButton::clicked, this, &MainWindow::onOpenSessionRequested);
        connect(emptyLoadSchemaButton_, &QPushButton::clicked, this, &MainWindow::onAddConnectionRequested);
    }

    // ---- Tier 1 Raw + Tier 2 Parsed views ------------------------------
    rawPacketView_ = new signalforge::inspect::RawPacketView(*rawFrameTap_);
    // P3-S2: feed the Raw dissection tree. A captured frame's source is the
    // driver's self-reported id (e.g. "udp:127.0.0.1:54321"); its `:`-prefix
    // is the driver type the dissector map is keyed by.
    rawPacketView_->setDissectorProvider([this](const QString& source) -> const signalforge::decoder::FrameDissector* {
        const auto it = dissectors_.find(source.section(QLatin1Char(':'), 0, 0));
        return (it != dissectors_.end()) ? it->second.get() : nullptr;
    });
    parsedView_ = new signalforge::inspect::ParsedSignalsView(*signalBufferRegistry_);
    connect(parsedView_, &signalforge::inspect::ParsedSignalsView::addToDashboardRequested, this,
            &MainWindow::onPromoteSignalToDashboard);
    // M34 P2: identity + dashboard providers (the view is in `inspect` and must
    // not depend on the theme or the dashboard — the app injects these).
    parsedView_->setSignalColorProvider([this](const QString& id) { return resolveSignalColor(id); });
    parsedView_->setQualityColorProvider([](signalforge::workbench::Quality q) { return qualityColor(q); });
    parsedView_->setDashboardMembershipProvider(
        [this](const QString& id) { return dashboard_ != nullptr && dashboard_->showsSignal(id); });
    parsedView_->setColorOverriddenProvider([this](const QString& id) { return signalIdentity_.hasOverride(id); });
    connect(parsedView_, &signalforge::inspect::ParsedSignalsView::removeFromDashboardRequested, this,
            [this](const QString& id) {
                if (dashboard_ != nullptr) {
                    dashboard_->removeSignalEverywhere(id);
                }
            });
    // M34 P5: per-signal colour override — pick / reset, then re-colour all tiers.
    connect(parsedView_, &signalforge::inspect::ParsedSignalsView::recolorRequested, this,
            &MainWindow::onRecolorRequested);
    connect(parsedView_, &signalforge::inspect::ParsedSignalsView::resetColorRequested, this,
            &MainWindow::onResetColorRequested);
    // Keep the "on dashboard" markers in sync the moment panels change.
    connect(dashboard_, &signalforge::dashboard::Dashboard::panelsChanged, parsedView_,
            &signalforge::inspect::ParsedSignalsView::refresh);

    // ---- Inspect mode: segmented [Raw | Parsed | Dashboard] over a stack
    // (M34 §6/§7 — the pipeline depth grouped as one activity). -----------
    inspectSegments_ = new signalforge::workbench::SegmentedControl;
    inspectSegments_->addSegment(QStringLiteral("raw"), tr("Raw"));              // Tier 1 (原报文)
    inspectSegments_->addSegment(QStringLiteral("parsed"), tr("Parsed"));        // Tier 2 (解析数据)
    inspectSegments_->addSegment(QStringLiteral("dashboard"), tr("Dashboard"));  // Tier 3

    inspectStack_ = new QStackedWidget;
    inspectStack_->addWidget(rawPacketView_);
    inspectStack_->addWidget(parsedView_);
    inspectStack_->addWidget(chartContainer_);
    connect(inspectSegments_, &signalforge::workbench::SegmentedControl::segmentSelected, this,
            [this](const QString& id) {
                if (id == QLatin1String("raw")) {
                    inspectStack_->setCurrentWidget(rawPacketView_);
                } else if (id == QLatin1String("parsed")) {
                    inspectStack_->setCurrentWidget(parsedView_);
                } else if (id == QLatin1String("dashboard")) {
                    inspectStack_->setCurrentWidget(chartContainer_);
                }
                // A selection belongs to its tier — don't let one tier's
                // inspector bleed into the next. Clear it on a manual segment
                // switch (the inspector reappears on the next selection).
                if (selectionModel_ != nullptr) {
                    selectionModel_->clear();
                }
                if (workbench_ != nullptr) {
                    workbench_->setInspectorVisible(false);
                }
            });

    auto* inspectPage = new QWidget;
    auto* inspectLayout = new QVBoxLayout(inspectPage);
    inspectLayout->setContentsMargins(0, 0, 0, 0);
    inspectLayout->setSpacing(0);
    auto* segmentBar = new QFrame(inspectPage);
    segmentBar->setObjectName(QStringLiteral("panelHeader"));  // reuse header chrome
    auto* segmentBarLayout = new QHBoxLayout(segmentBar);
    segmentBarLayout->setContentsMargins(8, 4, 8, 4);
    segmentBarLayout->setSpacing(8);
    segmentBarLayout->addWidget(inspectSegments_);
    segmentBarLayout->addStretch(1);
    inspectLayout->addWidget(segmentBar);
    inspectLayout->addWidget(inspectStack_, 1);
    inspectSegments_->setCurrentSegment(QStringLiteral("parsed"));  // default landing: Tier 2
    inspectStack_->setCurrentWidget(parsedView_);

    // ---- Connect mode: onboarding ↔ connection manager (M34 §7.1) -------
    // Page 0 is the onboarding empty-state; page 1 (the connection manager
    // body) is added by buildConnectionUi(), which runs next.
    connectStack_ = new QStackedWidget;
    connectStack_->addWidget(chartEmptyState_);

    // ---- Assemble the activity-rail frame -------------------------------
    workbench_ = new signalforge::workbench::WorkbenchFrame;
    workbench_->setTitle(tr("SignalForge"));
    workbench_->addMode(QStringLiteral("connect"), tr("Connect"), connectStack_);
    workbench_->addMode(QStringLiteral("inspect"), tr("Inspect"), inspectPage);
    setCentralWidget(workbench_);

    // ---- P5: right inspector — details of the current selection ----------
    // Hidden until the user selects a signal (Parsed) or a dissection field
    // (Raw). A Parsed selection flows through the app-wide SelectionModel (the
    // cross-tier backbone); the inspector observes it. A Parsed double-click /
    // "Show source packets" drills through to the Raw tier filtered to source.
    inspector_ = new signalforge::workbench::InspectorPanel;
    workbench_->setInspector(inspector_);
    workbench_->setInspectorVisible(false);
    // The inspector's × button dismisses the sidebar and drops the selection.
    connect(inspector_, &signalforge::workbench::InspectorPanel::closeRequested, this, [this]() {
        if (selectionModel_ != nullptr) {
            selectionModel_->clear();
        }
        if (workbench_ != nullptr) {
            workbench_->setInspectorVisible(false);
        }
    });
    selectionModel_ = new signalforge::workbench::SelectionModel(this);
    connect(parsedView_, &signalforge::inspect::ParsedSignalsView::signalSelected, this, [this](const QString& id) {
        if (id.isEmpty()) {
            selectionModel_->clear();
        } else {
            selectionModel_->select(signalforge::workbench::SelectionKind::Signal, id);
        }
    });
    connect(selectionModel_, &signalforge::workbench::SelectionModel::selectionChanged, this,
            &MainWindow::onSelectionChanged);
    connect(parsedView_, &signalforge::inspect::ParsedSignalsView::drillToSourceRequested, this,
            &MainWindow::onDrillToSourcePackets);
    if (rawPacketView_ != nullptr && rawPacketView_->dissectionTree() != nullptr) {
        connect(rawPacketView_->dissectionTree(), &QTreeWidget::currentItemChanged, this,
                [this](QTreeWidgetItem* item, QTreeWidgetItem*) { onDissectionFieldSelected(item); });
    }
    // P5: selecting a dashboard panel routes through the selection model too.
    connect(dashboard_, &signalforge::dashboard::Dashboard::panelSelected, this, [this](const QString& panelId) {
        if (selectionModel_ != nullptr) {
            selectionModel_->select(signalforge::workbench::SelectionKind::Widget, panelId);
        }
    });

    fpsLabel_ = new QLabel(tr("Chart idle"));
    fpsLabel_->setObjectName(QStringLiteral("fpsLabel"));
    droppedLabel_ = new QLabel(tr("Drops 0"));
    droppedLabel_->setObjectName(QStringLiteral("droppedLabel"));
    throttledLabel_ = new QLabel;
    throttledLabel_->setObjectName(QStringLiteral("throttledLabel"));
    // M14 F15: signal_buffer budget indicator (idle / 80% warn / FULL).
    bufferBudgetLabel_ = new QLabel;
    bufferBudgetLabel_->setObjectName(QStringLiteral("bufferBudgetLabel"));
    bufferBudgetLabel_->setToolTip(tr("Signal buffer memory budget usage"));
    statusBar()->setObjectName(QStringLiteral("mainStatusBar"));
    statusStrip_ = new QFrame(statusBar());
    statusStrip_->setObjectName(QStringLiteral("statusStrip"));
    statusStripLayout_ = new QHBoxLayout(statusStrip_);
    statusStripLayout_->setContentsMargins(0, 0, 0, 0);
    statusStripLayout_->setSpacing(2);
    workflowModeLabel_ = new QLabel(tr("Live"), statusStrip_);
    workflowModeLabel_->setObjectName(QStringLiteral("workflowModeLabel"));
    applyLabelClass(workflowModeLabel_, "mode-live");
    statusStripLayout_->addWidget(makeStatusCell(tr("Mode"), workflowModeLabel_, statusStrip_));
    auto* chartStatusRow = makeStatusValueRow(statusStrip_, {fpsLabel_, droppedLabel_, throttledLabel_});
    statusStripLayout_->addWidget(makeStatusCell(tr("Chart"), chartStatusRow, statusStrip_));
    statusStripLayout_->addWidget(makeStatusCell(QString(), bufferBudgetLabel_, statusStrip_));
    statusBar()->addPermanentWidget(statusStrip_, 1);

    auto* statusTimer = new QTimer(this);
    statusTimer->setInterval(1000);
    connect(statusTimer, &QTimer::timeout, this, &MainWindow::refreshStatusBar);
    statusTimer->start();

    // M21: start with an empty dashboard (the guided empty-state shows
    // until the user ticks a signal or adds a plot) — no auto chart.
    updateEmptyStateVisibility();
}

void MainWindow::buildConnectionUi() {
    // Connections menu (created/owned by buildMenuBar).
    auto* menu = menuConnections_;

    auto* addAction = menu->addAction(tr("&Add…"));
    addAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+M")));
    connect(addAction, &QAction::triggered, this, &MainWindow::onAddConnectionRequested);
    menu->addSeparator();

    auto* connectAllAction = menu->addAction(tr("Connect &all"));
    connect(connectAllAction, &QAction::triggered, this, &MainWindow::onConnectAllAction);

    auto* disconnectAllAction = menu->addAction(tr("&Disconnect all"));
    connect(disconnectAllAction, &QAction::triggered, this, &MainWindow::onDisconnectAllAction);

    // M34 §7.1: the connection manager lives inside Connect mode (the legacy
    // left dock is dissolved into the activity-rail frame). This body becomes
    // page 1 of the Connect stack; the onboarding empty-state is page 0.
    auto* connectionDockBody = new QWidget(this);
    auto* connectionDockLayout = new QVBoxLayout(connectionDockBody);
    connectionDockLayout->setContentsMargins(0, 0, 0, 0);
    connectionDockLayout->setSpacing(0);
    configSaveDockBanner_ = new QLabel(connectionDockBody);
    configSaveDockBanner_->setObjectName(QStringLiteral("configSaveDockBanner"));
    configSaveDockBanner_->setWordWrap(true);
    configSaveDockBanner_->setVisible(false);
    applyLabelClass(configSaveDockBanner_, "severity-error");
    connectionDockLayout->addWidget(configSaveDockBanner_);
    connectionList_ = new signalforge::connection::ConnectionListWidget(connectionManager_.get(), connectionDockBody);
    connectionDockLayout->addWidget(connectionList_, 1);
    connectionManagerBody_ = connectionDockBody;
    if (connectStack_ != nullptr) {
        connectStack_->addWidget(connectionManagerBody_);
    }

    connect(connectionList_, &signalforge::connection::ConnectionListWidget::addRequested, this,
            &MainWindow::onAddConnectionRequested);
    connect(connectionList_, &signalforge::connection::ConnectionListWidget::editRequested, this,
            &MainWindow::onEditConnectionRequested);

    // Connection status widget in the status strip.
    connectionStatus_ = new signalforge::connection::ConnectionStatusWidget(connectionManager_.get(), this);
    if (statusStripLayout_ != nullptr) {
        statusStripLayout_->insertWidget(0, makeStatusCell(tr("Connection"), connectionStatus_, statusStrip_));
    }
    connect(connectionStatus_, &signalforge::connection::ConnectionStatusWidget::clicked, this, [this]() {
        if (workbench_ != nullptr) {
            workbench_->setCurrentMode(QStringLiteral("connect"));
        }
    });
    configSaveStatusLabel_ = new QLabel(tr("Config ready"), statusStrip_);
    configSaveStatusLabel_->setObjectName(QStringLiteral("configSaveStatusLabel"));
    configSaveStatusLabel_->setToolTip(tr("Connection configuration save status"));
    applyLabelClass(configSaveStatusLabel_, "severity-info");
    if (statusStripLayout_ != nullptr) {
        statusStripLayout_->insertWidget(1, makeStatusCell(QString(), configSaveStatusLabel_, statusStrip_));
    }
    connect(connectionManager_.get(), &signalforge::connection::ConnectionManager::configurationSaveStateChanged, this,
            &MainWindow::onConfigurationSaveStateChanged);

    // M32: on every connection state change, re-evaluate the app-level
    // onboarding vs. workspace-tabs state.
    connect(connectionManager_.get(), &signalforge::connection::ConnectionManager::connectionStateChanged, this,
            [this](const QString&, signalforge::connection::Connection::State) { updateEmptyStateVisibility(); });
}

void MainWindow::buildThemeUi() {
    auto* themeMenu = menuView_->addMenu(tr("&Theme"));
    auto* group = new QActionGroup(this);
    group->setExclusive(true);

    lightThemeAction_ = themeMenu->addAction(tr("&Light"));
    lightThemeAction_->setCheckable(true);
    lightThemeAction_->setData(SignalForgeStyle::themeName(SignalForgeStyle::Theme::Light));
    lightThemeAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+1")));
    group->addAction(lightThemeAction_);

    darkThemeAction_ = themeMenu->addAction(tr("&Dark"));
    darkThemeAction_->setCheckable(true);
    darkThemeAction_->setData(SignalForgeStyle::themeName(SignalForgeStyle::Theme::Dark));
    darkThemeAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+2")));
    group->addAction(darkThemeAction_);

    highContrastThemeAction_ = themeMenu->addAction(tr("High &contrast"));
    highContrastThemeAction_->setCheckable(true);
    highContrastThemeAction_->setData(SignalForgeStyle::themeName(SignalForgeStyle::Theme::HighContrast));
    highContrastThemeAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+3")));
    group->addAction(highContrastThemeAction_);

    const auto active = SignalForgeStyle::activeTheme();
    if (active == SignalForgeStyle::Theme::Dark) {
        darkThemeAction_->setChecked(true);
    } else if (active == SignalForgeStyle::Theme::HighContrast) {
        highContrastThemeAction_->setChecked(true);
    } else {
        lightThemeAction_->setChecked(true);
    }

    connect(group, &QActionGroup::triggered, this, &MainWindow::applyThemeFromAction);

    // M34 P2: configurable dashboard refresh rate (owner request — no longer a
    // hard-coded constant). A future video panel may run at its own cadence;
    // this is the single-rate knob until per-panel rates land.
    auto* rateMenu = menuView_->addMenu(tr("&Refresh rate"));
    auto* rateGroup = new QActionGroup(this);
    rateGroup->setExclusive(true);
    for (const int hz : {15, 30, 60}) {
        QAction* rateAction = rateMenu->addAction(tr("%1 Hz").arg(hz));
        rateAction->setCheckable(true);
        rateAction->setData(hz);
        rateAction->setChecked(dashboard_ != nullptr && dashboard_->refreshRateHz() == hz);
        rateGroup->addAction(rateAction);
    }
    connect(rateGroup, &QActionGroup::triggered, this, [this](QAction* rateAction) {
        if (dashboard_ != nullptr) {
            dashboard_->setRefreshRateHz(rateAction->data().toInt());
        }
    });

    if (connectionList_ != nullptr && liveToggle_ != nullptr && timePresetCombo_ != nullptr &&
        replaySeekSlider_ != nullptr && replaySpeedCombo_ != nullptr) {
        setTabOrder(connectionList_, liveToggle_);
        setTabOrder(liveToggle_, timePresetCombo_);
        setTabOrder(timePresetCombo_, replaySeekSlider_);
        setTabOrder(replaySeekSlider_, replaySpeedCombo_);
    }
}

void MainWindow::applyThemeFromAction(QAction* action) {
    if (action == nullptr) {
        return;
    }
    bool ok = false;
    const auto theme = SignalForgeStyle::themeFromName(action->data().toString(), &ok);
    if (!ok) {
        SF_LOG_WARN("MainWindow::applyThemeFromAction: unknown theme '{}'", action->data().toString().toStdString());
        return;
    }
    SignalForgeStyle::setActiveTheme(theme);
    for (auto* hostWidget : findChildren<QQuickWidget*>()) {
        applyChartHostTheme(hostWidget);
    }
    update();
}

// ---- M14 S1 GUI smoke-test hooks ---------------------------------------

bool MainWindow::autoLoadTestFixture(const QString& yamlPath) {
    if (connectionManager_ == nullptr || yamlPath.isEmpty()) {
        return false;
    }
    if (!connectionManager_->loadConfigFile(yamlPath)) {
        SF_LOG_ERROR("MainWindow: autoLoadTestFixture: failed to load '{}'", yamlPath.toStdString());
        return false;
    }
    connectionManager_->connectAll();
    SF_LOG_INFO("MainWindow: autoLoadTestFixture: loaded + connectAll for '{}'", yamlPath.toStdString());
    return true;
}

bool MainWindow::autoSelectSignal(const QString& signalId) {
    if (dashboard_ == nullptr || signalId.isEmpty()) {
        return false;
    }
    // M23: create a plot panel showing the signal (the M14 smoke harness's
    // pixel check expects a rendered trace). PlotView renders via QPainter,
    // so grabChartImage captures it.
    const QString panelId = dashboard_->addPlotPanel({signalId});
    updateEmptyStateVisibility();
    ensureDashboardVisible();
    SF_LOG_INFO("MainWindow: autoSelectSignal: '{}' -> plot panel '{}'", signalId.toStdString(), panelId.toStdString());
    return !panelId.isEmpty();
}

bool MainWindow::autoAddDashboardSignal(const QString& signalId) {
    if (dashboard_ == nullptr || signalId.isEmpty()) {
        return false;
    }
    const QString panelId = dashboard_->addSignal(signalId);
    updateEmptyStateVisibility();
    ensureDashboardVisible();
    SF_LOG_INFO("MainWindow: autoAddDashboardSignal: '{}' -> panel '{}'", signalId.toStdString(),
                panelId.toStdString());
    return !panelId.isEmpty();
}

bool MainWindow::autoAddTablePanel() {
    if (dashboard_ == nullptr || signalBufferRegistry_ == nullptr) {
        return false;
    }
    onAddTable();
    return true;
}

bool MainWindow::autoAddBarPanel() {
    if (dashboard_ == nullptr || signalBufferRegistry_ == nullptr) {
        return false;
    }
    onAddBar();
    return true;
}

bool MainWindow::autoAddGaugePanel() {
    if (dashboard_ == nullptr || signalBufferRegistry_ == nullptr) {
        return false;
    }
    onAddGauge();
    return true;
}

bool MainWindow::autoStartRecording(const QString& path) {
    if (sessionWriter_ == nullptr || path.isEmpty() || sessionWriter_->isRecording()) {
        return false;
    }
    // M14 F9: derive decoderSchemaId from the last Connected connection
    // with a non-empty schema id (matches the onRecordToggle GUI path).
    QString recordingSchemaId;
    if (connectionManager_ != nullptr) {
        for (const auto& id : connectionManager_->connectionIds()) {
            const auto* conn = connectionManager_->connection(id);
            if (conn == nullptr || conn->state() != signalforge::connection::Connection::State::Connected) {
                continue;
            }
            const QString& cfgSchema = conn->config().decoderSchemaId;
            if (!cfgSchema.isEmpty()) {
                recordingSchemaId = cfgSchema;
            }
        }
    }
    if (!sessionWriter_->start(path, /*description*/ QString{}, recordingSchemaId)) {
        SF_LOG_ERROR("MainWindow::autoStartRecording: SessionWriter::start failed for '{}'", path.toStdString());
        return false;
    }
    currentRecordingPath_ = path;
    // M15 S3 Round 6: mirror the production onRecordToggle UI updates
    // so the GUI reflects the recording state in headless capture (the
    // S3 fidelity audit found state-14 / state-15 captures looked
    // identical to state-05 because these UI updates were skipped).
    lastRecordingBytes_ = 0;
    recordingElapsed_.restart();
    updateRecordingStatusLabel();
    updateWorkflowModeLabel();
    if (recordingHeartbeatTimer_ != nullptr) {
        recordingHeartbeatTimer_->start();
    }
    if (recordAction_ != nullptr) {
        recordAction_->setText(tr("&Stop recording"));
    }
    SF_LOG_INFO("MainWindow::autoStartRecording: -> {} (schemaId='{}')", path.toStdString(),
                recordingSchemaId.toStdString());
    return true;
}

std::size_t MainWindow::autoStopRecording() {
    if (sessionWriter_ == nullptr || !sessionWriter_->isRecording()) {
        return 0;
    }
    const std::size_t eventsBeforeStop = sessionWriter_->eventsRecorded();
    const std::size_t droppedBeforeStop = sessionWriter_->droppedEvents();
    const std::size_t bytes = sessionWriter_->stop();
    // M15 S3 Round 6: mirror the production onRecordToggle UI updates.
    if (recordingHeartbeatTimer_ != nullptr) {
        recordingHeartbeatTimer_->stop();
    }
    lastRecordingBytes_ = bytes;
    updateWorkflowModeLabel();
    if (recordingStatusLabel_ != nullptr) {
        recordingStatusLabel_->setText(
            tr("Record stopped %1 | %2 bytes").arg(formatElapsedMs(recordingElapsed_.elapsed())).arg(bytes));
        applyLabelClass(recordingStatusLabel_, "severity-info");
    }
    if (recordAction_ != nullptr) {
        recordAction_->setText(tr("&Record…"));
    }
    SF_LOG_INFO("MainWindow::autoStopRecording: stopped ({} bytes -> {}); events={} dropped={}", bytes,
                currentRecordingPath_.toStdString(), eventsBeforeStop, droppedBeforeStop);
    currentRecordingPath_.clear();
    return bytes;
}

bool MainWindow::autoLoadFixtureNoConnect(const QString& yamlPath) {
    if (connectionManager_ == nullptr || yamlPath.isEmpty()) {
        return false;
    }
    if (!connectionManager_->loadConfigFile(yamlPath)) {
        SF_LOG_ERROR("MainWindow: autoLoadFixtureNoConnect: failed to load '{}'", yamlPath.toStdString());
        return false;
    }
    SF_LOG_INFO("MainWindow: autoLoadFixtureNoConnect: loaded (no connect) for '{}'", yamlPath.toStdString());
    return true;
}

bool MainWindow::captureFullScreen(const QString& path) {
    if (path.isEmpty()) {
        SF_LOG_WARN("MainWindow::captureFullScreen: empty path");
        return false;
    }
    auto* screen = QGuiApplication::primaryScreen();
    if (screen == nullptr) {
        SF_LOG_ERROR("MainWindow::captureFullScreen: no primary screen");
        return false;
    }
    const QPixmap pm = screen->grabWindow(0);
    if (pm.isNull()) {
        SF_LOG_ERROR("MainWindow::captureFullScreen: grabWindow(0) returned null pixmap");
        return false;
    }
    if (!pm.save(path, "PNG")) {
        SF_LOG_ERROR("MainWindow::captureFullScreen: failed to save PNG to '{}'", path.toStdString());
        return false;
    }
    SF_LOG_INFO("MainWindow::captureFullScreen: {}x{} -> {}", pm.width(), pm.height(), path.toStdString());
    return true;
}

bool MainWindow::autoOpenMenu(const QString& name) {
    auto* bar = menuBar();
    if (bar == nullptr || name.isEmpty()) {
        return false;
    }
    for (auto* action : bar->actions()) {
        QString text = action->text();
        text.remove(QLatin1Char('&'));
        if (text.compare(name, Qt::CaseInsensitive) == 0) {
            auto* menu = action->menu();
            if (menu == nullptr) {
                SF_LOG_WARN("MainWindow::autoOpenMenu: '{}' action has no menu", name.toStdString());
                return false;
            }
            const QPoint pos = bar->mapToGlobal(bar->actionGeometry(action).bottomLeft());
            menu->popup(pos);
            SF_LOG_INFO("MainWindow::autoOpenMenu: '{}' popped at ({},{})", name.toStdString(), pos.x(), pos.y());
            return true;
        }
    }
    SF_LOG_WARN("MainWindow::autoOpenMenu: no menu named '{}'", name.toStdString());
    return false;
}

bool MainWindow::autoShowAddConnectionDialog(const QString& driverType, bool expandAdvanced) {
    auto* dlg = new signalforge::connection::ConnectionDialog(enumerateAvailableSchemaIds(), this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowModality(Qt::NonModal);
    if (!driverType.isEmpty()) {
        const QString d = driverType.toLower();
        using DT = signalforge::connection::DriverType;
        if (d == "serial") {
            dlg->setDriverType(DT::Serial);
        } else if (d == "tcp") {
            dlg->setDriverType(DT::Tcp);
        } else if (d == "udp") {
            dlg->setDriverType(DT::Udp);
        } else if (d == "replay") {
            dlg->setDriverType(DT::Replay);
        } else {
            SF_LOG_WARN("autoShowAddConnectionDialog: unknown driverType '{}', leaving default",
                        driverType.toStdString());
        }
    }
    if (expandAdvanced && dlg->advancedCommandsGroup() != nullptr) {
        dlg->advancedCommandsGroup()->setChecked(true);
    }
    dlg->show();
    SF_LOG_INFO("MainWindow::autoShowAddConnectionDialog: shown non-modal (driverType='{}', expandAdvanced={})",
                driverType.toStdString(), expandAdvanced);
    return true;
}

bool MainWindow::autoShowEditConnectionDialog() {
    if (connectionManager_ == nullptr) {
        return false;
    }
    const auto ids = connectionManager_->connectionIds();
    if (ids.isEmpty()) {
        SF_LOG_WARN("MainWindow::autoShowEditConnectionDialog: no connection to edit");
        return false;
    }
    auto* conn = connectionManager_->connection(ids.first());
    if (conn == nullptr) {
        return false;
    }
    auto* dlg = new signalforge::connection::ConnectionDialog(enumerateAvailableSchemaIds(), this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowModality(Qt::NonModal);
    dlg->setConfig(conn->config());
    dlg->show();
    SF_LOG_INFO("MainWindow::autoShowEditConnectionDialog: shown non-modal for '{}'", ids.first().toStdString());
    return true;
}

bool MainWindow::autoReplaySpeedComboPopup(int index) {
    if (replaySpeedCombo_ == nullptr || index < 0 || index >= replaySpeedCombo_->count()) {
        return false;
    }
    replaySpeedCombo_->setCurrentIndex(index);
    replaySpeedCombo_->showPopup();
    SF_LOG_INFO("MainWindow::autoReplaySpeedComboPopup: index={} ({}×)", index,
                replaySpeedCombo_->itemData(index).toDouble());
    return true;
}

bool MainWindow::autoLoadReplaySession(const QString& path) {
    if (replayModeManager_ == nullptr || playbackController_ == nullptr || path.isEmpty()) {
        return false;
    }
    if (!replayModeManager_->enterReplay()) {
        SF_LOG_ERROR("MainWindow::autoLoadReplaySession: enterReplay failed");
        return false;
    }
    if (!playbackController_->loadSession(path)) {
        SF_LOG_ERROR("MainWindow::autoLoadReplaySession: loadSession failed for '{}': {}", path.toStdString(),
                     playbackController_->lastError().toStdString());
        (void)replayModeManager_->exitReplay(false);
        return false;
    }
    if (replayToolbar_ != nullptr) {
        replayToolbar_->setVisible(true);
    }
    if (replaySeekSlider_ != nullptr) {
        replaySeekSlider_->setRange(0, static_cast<int>(playbackController_->totalRecords()));
    }
    if (replayStatusLabel_ != nullptr) {
        replayStatusLabel_->setText(tr("Replay loaded %1").arg(QFileInfo(path).fileName()));
        applyLabelClass(replayStatusLabel_, "mode-replay");
    }
    SF_LOG_INFO("MainWindow::autoLoadReplaySession: loaded '{}'", path.toStdString());
    return true;
}

bool MainWindow::autoReplayPlay() {
    if (playbackController_ == nullptr) {
        return false;
    }
    return playbackController_->play();
}

bool MainWindow::autoReplayPause() {
    if (playbackController_ == nullptr) {
        return false;
    }
    return playbackController_->pause();
}

bool MainWindow::autoReplayStepToEnd() {
    if (playbackController_ == nullptr || playbackController_->totalRecords() == 0) {
        return false;
    }
    if (playbackController_->state() == signalforge::replay::PlaybackState::Playing) {
        (void)playbackController_->pause();
    }
    const auto maxSteps = playbackController_->totalRecords() + 1;
    for (std::size_t i = 0; i < maxSteps; ++i) {
        if (playbackController_->state() == signalforge::replay::PlaybackState::Ended) {
            updateReplayStatusLabel();
            return true;
        }
        (void)playbackController_->stepForward();
    }
    updateReplayStatusLabel();
    return playbackController_->state() == signalforge::replay::PlaybackState::Ended;
}

bool MainWindow::autoSetBufferStatusForVisualTest(const QString& status) {
    const QString normalized = status.trimmed().toLower();
    if (normalized == QStringLiteral("warning") || normalized == QStringLiteral("warn")) {
        bufferBudgetOverrideText_ = tr("Buffer 86% 220/256 MiB");
        bufferBudgetOverrideClass_ = QStringLiteral("severity-warning");
    } else if (normalized == QStringLiteral("full") || normalized == QStringLiteral("error")) {
        bufferBudgetOverrideText_ = tr("Buffer full 256/256 MiB");
        bufferBudgetOverrideClass_ = QStringLiteral("severity-error");
    } else {
        SF_LOG_WARN("MainWindow::autoSetBufferStatusForVisualTest: unknown status '{}'", status.toStdString());
        return false;
    }
    if (bufferBudgetLabel_ != nullptr) {
        bufferBudgetLabel_->setText(bufferBudgetOverrideText_);
        bufferBudgetLabel_->setToolTip(tr("Signal buffer memory budget usage (%1 visual state)").arg(normalized));
        applyLabelClass(bufferBudgetLabel_, bufferBudgetOverrideClass_.toUtf8().constData());
    }
    return true;
}

bool MainWindow::autoSetChartStatusForVisualTest(const QString& status) {
    const QString normalized = status.trimmed().toLower();
    if (normalized == QStringLiteral("interrupted") || normalized == QStringLiteral("stale")) {
        chartStatusOverrideText_ = tr("Interrupted");
        chartStatusOverrideClass_ = QStringLiteral("severity-warning");
    } else {
        SF_LOG_WARN("MainWindow::autoSetChartStatusForVisualTest: unknown status '{}'", status.toStdString());
        return false;
    }
    if (throttledLabel_ != nullptr) {
        throttledLabel_->setText(chartStatusOverrideText_);
        throttledLabel_->setToolTip(tr("Chart data stream is interrupted or frames were dropped."));
        applyLabelClass(throttledLabel_, chartStatusOverrideClass_.toUtf8().constData());
    }
    return true;
}

bool MainWindow::autoSetConfigSaveStatusForVisualTest(const QString& status) {
    const QString normalized = status.trimmed().toLower();
    if (normalized == QStringLiteral("saved") || normalized == QStringLiteral("ok")) {
        onConfigurationSaveStateChanged(true, signalforge::connection::ConnectionManager::defaultConfigPath(),
                                        QString());
        return true;
    }
    if (normalized == QStringLiteral("failed") || normalized == QStringLiteral("error")) {
        const QString visualPath = QStringLiteral("/tmp/signalforge-visual/connections.yaml");
        onConfigurationSaveStateChanged(false, visualPath,
                                        tr("Could not save connection configuration to %1").arg(visualPath));
        return true;
    }
    SF_LOG_WARN("MainWindow::autoSetConfigSaveStatusForVisualTest: unknown status '{}'", status.toStdString());
    return false;
}

bool MainWindow::autoSetConnectionStateForVisualTest(const QString& status) {
    if (connectionManager_ == nullptr || connectionList_ == nullptr || connectionStatus_ == nullptr) {
        return false;
    }
    const QString normalized = status.trimmed().toLower();
    using CState = signalforge::connection::Connection::State;
    using AState = signalforge::connection::ConnectionStatusWidget::AggregateState;
    CState rowState = CState::Idle;
    AState aggregateState = AState::Idle;
    QString statusText;
    if (normalized == QStringLiteral("connecting")) {
        rowState = CState::Connecting;
        aggregateState = AState::Connecting;
        statusText = tr("0/1 connecting");
    } else if (normalized == QStringLiteral("disconnecting")) {
        rowState = CState::Disconnecting;
        aggregateState = AState::Connecting;
        statusText = tr("1/1 disconnecting");
    } else if (normalized == QStringLiteral("error")) {
        rowState = CState::Error;
        aggregateState = AState::Error;
        statusText = tr("0/1 connected · errors: 1");
    } else {
        SF_LOG_WARN("MainWindow::autoSetConnectionStateForVisualTest: unknown status '{}'", status.toStdString());
        return false;
    }

    QString id = connectionList_->currentId();
    if (id.isEmpty()) {
        const auto ids = connectionManager_->connectionIds();
        if (!ids.isEmpty()) {
            id = ids.first();
        }
    }
    if (id.isEmpty()) {
        signalforge::connection::ConnectionConfig cfg;
        cfg.id = QStringLiteral("m19-visual-udp");
        cfg.displayName = tr("M19 visual UDP");
        cfg.driverType = signalforge::connection::DriverType::Udp;
        signalforge::drivers::UdpConfig udp;
        udp.localBindAddress = QStringLiteral("127.0.0.1");
        udp.localBindPort = 0;
        cfg.driverConfig = udp;
        id = connectionManager_->addConnection(cfg);
    }
    if (id.isEmpty()) {
        return false;
    }
    if (!connectionList_->setVisualStateForTest(id, rowState)) {
        return false;
    }
    connectionStatus_->setVisualStateForTest(statusText, aggregateState);
    if (workbench_ != nullptr) {
        workbench_->setCurrentMode(QStringLiteral("connect"));
    }
    return true;
}

bool MainWindow::autoShowM19ModalForVisualTest(const QString& modal) {
    const QString normalized = modal.trimmed().toLower();
    if (normalized == QStringLiteral("replay-open-dialog") || normalized == QStringLiteral("open-session")) {
        auto* dialog = new QFileDialog(this, tr("Open session"), QString(), tr("SFREPLAY (*.sfreplay)"));
        dialog->setObjectName(QStringLiteral("m19ReplayOpenDialog"));
        dialog->setFileMode(QFileDialog::ExistingFile);
        dialog->setOption(QFileDialog::DontUseNativeDialog, true);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowModality(Qt::ApplicationModal);
        dialog->show();
        return true;
    }

    auto* box = new QMessageBox(this);
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->setWindowModality(Qt::ApplicationModal);
    if (normalized == QStringLiteral("live-to-replay")) {
        box->setIcon(QMessageBox::Warning);
        box->setWindowTitle(tr("Pause connections to enter Replay?"));
        box->setText(tr("Entering Replay mode will pause active connection(s). Continue?"));
        box->setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    } else if (normalized == QStringLiteral("replay-to-live")) {
        box->setIcon(QMessageBox::Question);
        box->setWindowTitle(tr("Exit Replay"));
        box->setText(tr("Resume the previously-paused connections?"));
        box->setStandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    } else if (normalized == QStringLiteral("recording-error")) {
        if (recordingStatusLabel_ != nullptr) {
            recordingStatusLabel_->setText(tr("Recording error | Could not open target file"));
            applyLabelClass(recordingStatusLabel_, "severity-error");
        }
        box->setIcon(QMessageBox::Critical);
        box->setWindowTitle(tr("Recording error"));
        box->setText(tr("Could not open the recording target file."));
        box->setStandardButtons(QMessageBox::Ok);
    } else if (normalized == QStringLiteral("replay-error")) {
        if (replayStatusLabel_ != nullptr) {
            replayStatusLabel_->setText(tr("Replay error"));
            applyLabelClass(replayStatusLabel_, "severity-error");
        }
        box->setIcon(QMessageBox::Critical);
        box->setWindowTitle(tr("Replay error"));
        box->setText(tr("Failed to load session: invalid or unreadable replay file."));
        box->setStandardButtons(QMessageBox::Ok);
    } else {
        delete box;
        SF_LOG_WARN("MainWindow::autoShowM19ModalForVisualTest: unknown modal '{}'", modal.toStdString());
        return false;
    }
    box->show();
    return true;
}

bool MainWindow::autoFocusWidgetForVisualTest(const QString& name) {
    const QString normalized = name.trimmed().toLower().replace(QLatin1Char('-'), QLatin1Char('_'));
    QWidget* target = nullptr;
    if (normalized == QStringLiteral("connection_list")) {
        target = connectionList_;
    } else if (normalized == QStringLiteral("signal_selector") || normalized == QStringLiteral("signal_list") ||
               normalized == QStringLiteral("parsed")) {
        target = parsedView_;  // M32: Parsed tab replaced the signal list
    } else if (normalized == QStringLiteral("live_toggle")) {
        target = liveToggle_;
    } else if (normalized == QStringLiteral("time_preset")) {
        target = timePresetCombo_;
    } else if (normalized == QStringLiteral("replay_seek")) {
        target = replaySeekSlider_;
    } else if (normalized == QStringLiteral("replay_speed")) {
        target = replaySpeedCombo_;
    } else if (normalized == QStringLiteral("empty_open_session")) {
        target = emptyOpenSessionButton_;
    }
    if (target == nullptr) {
        SF_LOG_WARN("MainWindow::autoFocusWidgetForVisualTest: unknown focus target '{}'", name.toStdString());
        return false;
    }
    target->setFocusPolicy(Qt::StrongFocus);
    target->setFocus(Qt::TabFocusReason);
    target->update();
    SF_LOG_INFO("MainWindow::autoFocusWidgetForVisualTest: focused '{}'", name.toStdString());
    return true;
}

bool MainWindow::autoReplaySeekPercent(int percent) {
    if (playbackController_ == nullptr || percent < 0 || percent > 100) {
        return false;
    }
    const auto duration = playbackController_->durationNs();
    const auto target = (duration * percent) / 100;
    if (!playbackController_->seek(target)) {
        return false;
    }
    // M15 S3 Round 6: PlaybackController::seek() does not emit
    // positionChanged on its own (positionChanged fires only from
    // dispatched records during play / step). Without an explicit
    // GUI update, the seek slider stays at 0 and the replay status
    // label shows only the filename — making seek-state captures
    // (states 19, 20) visually indistinguishable from the loaded
    // state. Mirror what onReplayPositionChanged does so the
    // headless screenshot reflects the seeked position.
    if (replaySeekSlider_ != nullptr) {
        const auto totalRecords = static_cast<int>(playbackController_->totalRecords());
        if (totalRecords > 0) {
            const int sliderValue = (totalRecords * percent) / 100;
            replaySliderUserDriven_ = false;
            replaySeekSlider_->setValue(sliderValue);
            replaySliderUserDriven_ = true;
        }
    }
    if (replayStatusLabel_ != nullptr) {
        const QString filename = QFileInfo(playbackController_->currentFilePath()).fileName();
        const auto recordIndex = (playbackController_->totalRecords() * static_cast<std::size_t>(percent)) / 100;
        replayStatusLabel_->setText(tr("Replay %1 | seek %2% | %3/%4 records")
                                        .arg(filename)
                                        .arg(percent)
                                        .arg(recordIndex)
                                        .arg(playbackController_->totalRecords()));
        applyLabelClass(replayStatusLabel_, "mode-replay");
    }
    return true;
}

std::size_t MainWindow::autoAddCharts(int extra) {
    if (dashboard_ == nullptr || extra <= 0) {
        return dashboard_ != nullptr ? static_cast<std::size_t>(dashboard_->panelCount()) : 0;
    }
    for (int i = 0; i < extra; ++i) {
        (void)dashboard_->addPlotPanel();
    }
    const auto count = static_cast<std::size_t>(dashboard_->panelCount());
    SF_LOG_INFO("MainWindow: autoAddCharts: +{} -> total {} panels", extra, count);
    return count;
}

bool MainWindow::captureScreenshot(const QString& path) {
    if (path.isEmpty()) {
        SF_LOG_WARN("MainWindow::captureScreenshot: empty path");
        return false;
    }
    // M15 S1 mechanism C: full-window in-process grab. Distinct from
    // M14 S1's grabChartImage() which captures the chart QQuickWidget
    // framebuffer only. Mechanism B (xvfb + xwd) for full-X-server
    // captures (e.g. menu open) lives in tests/visual/lib/capture.py
    // helpers and does NOT touch this binary.
    const QPixmap pm = grab();
    if (pm.isNull()) {
        SF_LOG_ERROR("MainWindow::captureScreenshot: QWidget::grab() returned null pixmap");
        return false;
    }
    if (!pm.save(path, "PNG")) {
        SF_LOG_ERROR("MainWindow::captureScreenshot: failed to save PNG to '{}'", path.toStdString());
        return false;
    }
    SF_LOG_INFO("MainWindow::captureScreenshot: {}x{} -> {}", pm.width(), pm.height(), path.toStdString());
    return true;
}

QImage MainWindow::grabChartImage() const {
    if (dashboard_ == nullptr) {
        return {};
    }
    // M23: the plot is now a QPainter `PlotView` (a plain QWidget), so a
    // direct QWidget::grab() captures it — no more QQuickWidget RHI
    // framebuffer dance (M14 §F4).
    auto* view = dashboard_->findChild<signalforge::dashboard::PlotView*>();
    if (view == nullptr) {
        SF_LOG_WARN("MainWindow::grabChartImage: no PlotView in the dashboard");
        return {};
    }
    view->refresh();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    const QImage img = view->grab().toImage();
    SF_LOG_INFO("MainWindow::grabChartImage: PlotView {}x{} signals={}", img.width(), img.height(),
                view->signalIds().size());
    return img;
}

// ---- existing private helpers ------------------------------------------

QStringList MainWindow::enumerateAvailableSchemaIds() const {
    // Walk examples/schemas/*.yaml relative to the binary's
    // working directory. Schema IDs are yaml file stems for V1;
    // future versions may parse `schemaId` from the yaml itself.
    QStringList ids;
    QDir dir(QStringLiteral("examples/schemas"));
    if (!dir.exists()) {
        return ids;
    }
    for (const QString& name : dir.entryList({QStringLiteral("*.yaml")}, QDir::Files)) {
        ids << QFileInfo(name).completeBaseName();
    }
    return ids;
}

void MainWindow::rebuildDissectorForType(const QString& driverType, const QString& decoderSchemaId) {
    QString stem = decoderSchemaId.trimmed();
    if (stem.endsWith(QLatin1String(".yaml"), Qt::CaseInsensitive)) {
        stem.chop(5);
    }
    if (stem.isEmpty()) {
        dissectors_.erase(driverType);  // no schema for this type → raw bytes only
        return;
    }
    const QString path = QStringLiteral("examples/schemas/%1.yaml").arg(stem);
    auto result = signalforge::decoder::SchemaValidator::validateFile(path);
    if (result.has_value()) {
        dissectors_[driverType] = std::make_shared<signalforge::decoder::FrameDissector>(std::move(*result));
    } else {
        SF_LOG_WARN("MainWindow: Raw dissection disabled for driver type '{}' — schema '{}' failed to load",
                    driverType.toStdString(), path.toStdString());
        dissectors_.erase(driverType);
    }
    if (rawPacketView_ != nullptr) {
        rawPacketView_->redissectCurrent();  // re-dissect the current selection against the new schema
    }
}

void MainWindow::onSignalSelectedForInspector(const QString& signalId) {
    if (inspector_ == nullptr) {
        return;
    }
    if (signalId.isEmpty()) {
        inspector_->showPlaceholder(tr("Select a signal or packet field to inspect."));
        if (workbench_ != nullptr) {
            workbench_->setInspectorVisible(false);
        }
        return;
    }
    const int slash = signalId.indexOf(QLatin1Char('/'));
    const QString source = slash > 0 ? signalId.left(slash) : signalId;
    QString title = slash >= 0 ? signalId.mid(slash + 1) : signalId;

    // The detail rows deliberately avoid duplicating the Parsed table columns
    // (source / type / unit / value / age are already there). The inspector adds
    // what the table can't show at a glance — the full id, a live value
    // headline, the description — plus an actions row below.
    QString typeName;
    QString unit;
    QString description;
    QString valueText = QStringLiteral("—");
    if (auto* buf = signalBufferRegistry_->bufferFor(signalId); buf != nullptr) {
        const auto& meta = buf->metadata();
        if (!meta.name.isEmpty()) {
            title = meta.name;
        }
        switch (meta.type) {
        case signalforge::decoder::SignalType::Bool:
            typeName = QStringLiteral("bool");
            break;
        case signalforge::decoder::SignalType::Int64:
            typeName = QStringLiteral("int");
            break;
        case signalforge::decoder::SignalType::Double:
            typeName = QStringLiteral("double");
            break;
        case signalforge::decoder::SignalType::String:
            typeName = QStringLiteral("string");
            break;
        }
        unit = meta.unit;
        if (meta.description.has_value()) {
            description = *meta.description;
        }
        if (const auto latest = buf->queryLatestOne(); latest.has_value()) {
            valueText = signalforge::dashboard::formatValue(latest->value, 3);
            if (!unit.isEmpty()) {
                valueText += QLatin1Char(' ') + unit;
            }
        }
    }
    // Subtitle packs the identity (source · type · unit) so it needn't be rows.
    QString subtitle = source;
    if (!typeName.isEmpty()) {
        subtitle += QStringLiteral(" · ") + typeName;
    }
    if (!unit.isEmpty()) {
        subtitle += QStringLiteral(" · ") + unit;
    }

    QVector<signalforge::workbench::InspectorPanel::Row> rows;
    rows.append({tr("Value"), valueText});
    rows.append({tr("Id"), signalId});
    if (!description.isEmpty()) {
        rows.append({tr("Description"), description});
    }
    inspector_->showDetails(title, subtitle, rows, resolveSignalColor(signalId));

    // Actions: recolour + dashboard membership (the inspector is now a control
    // surface, not just a readout).
    QVector<signalforge::workbench::InspectorPanel::Action> actions;
    actions.append({tr("Set colour…"), [this, signalId]() { onRecolorRequested(signalId); }});
    // Add buttons are always available (a signal can carry several card types);
    // Remove appears only once the signal is on the dashboard.
    actions.append(
        {tr("+ Plot"), [this, signalId]() { onPromoteSignalToDashboard(signalId, QStringLiteral("plot")); }});
    actions.append({tr("+ Bar"), [this, signalId]() { onPromoteSignalToDashboard(signalId, QStringLiteral("bar")); }});
    actions.append(
        {tr("+ Gauge"), [this, signalId]() { onPromoteSignalToDashboard(signalId, QStringLiteral("gauge")); }});
    if (dashboard_ != nullptr && dashboard_->showsSignal(signalId)) {
        actions.append({tr("Remove"), [this, signalId]() {
                            if (dashboard_ != nullptr) {
                                dashboard_->removeSignalEverywhere(signalId);
                            }
                        }});
    }
    inspector_->setActions(actions);

    if (workbench_ != nullptr) {
        workbench_->setInspectorVisible(true);
    }
}

void MainWindow::onDissectionFieldSelected(QTreeWidgetItem* item) {
    if (inspector_ == nullptr || item == nullptr) {
        return;  // keep the current inspector content on a cleared tree selection
    }
    // Raw dissection-tree columns: Field(0) · Value(1) · Type(2) · Bytes(3).
    QVector<signalforge::workbench::InspectorPanel::Row> rows;
    if (!item->text(1).isEmpty()) {
        rows.append({tr("Value"), item->text(1)});
    }
    if (!item->text(2).isEmpty()) {
        rows.append({tr("Type"), item->text(2)});
    }
    if (!item->text(3).isEmpty()) {
        rows.append({tr("Bytes"), item->text(3)});
    }
    inspector_->showDetails(item->text(0), tr("Packet field"), rows);
    inspector_->setActions({});  // a packet field carries no signal actions
    if (workbench_ != nullptr) {
        workbench_->setInspectorVisible(true);
    }
}

void MainWindow::onPanelSelectedForInspector(const QString& panelId) {
    if (inspector_ == nullptr || dashboard_ == nullptr) {
        return;
    }
    auto* panel = dashboard_->panel(panelId);
    if (panel == nullptr) {
        return;
    }
    namespace dash = signalforge::dashboard;
    const dash::PanelConfig cfg = panel->config();
    const QString signalList = cfg.signalIds.join(QStringLiteral(", "));
    const QString title = !cfg.title.isEmpty() ? cfg.title : !cfg.signalIds.isEmpty() ? cfg.signalIds.first() : panelId;
    inspector_->showDetails(title, tr("%1 card").arg(dash::panelTypeName(cfg.type)),
                            {{tr("Signals"), signalList.isEmpty() ? QStringLiteral("—") : signalList}});

    // Editable property form: range (where it applies) + card size.
    auto* form = new QWidget;
    auto* layout = new QFormLayout(form);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setHorizontalSpacing(8);
    layout->setVerticalSpacing(4);

    const bool rangeApplies = cfg.type == dash::PanelType::Numeric || cfg.type == dash::PanelType::Bar ||
                              cfg.type == dash::PanelType::Gauge || cfg.type == dash::PanelType::Plot;
    if (rangeApplies) {
        auto* minSpin = new QDoubleSpinBox(form);
        auto* maxSpin = new QDoubleSpinBox(form);
        for (QDoubleSpinBox* s : {minSpin, maxSpin}) {
            s->setRange(-1e9, 1e9);
            s->setDecimals(3);
        }
        minSpin->setValue(cfg.rangeMin.value_or(0.0));
        maxSpin->setValue(cfg.rangeMax.value_or(0.0));
        layout->addRow(tr("Range min"), minSpin);
        layout->addRow(tr("Range max"), maxSpin);
        auto* applyRange = new QPushButton(tr("Apply range"), form);
        connect(applyRange, &QPushButton::clicked, this, [this, panelId, minSpin, maxSpin]() {
            auto* p = (dashboard_ != nullptr) ? dashboard_->panel(panelId) : nullptr;
            if (p == nullptr) {
                return;
            }
            signalforge::dashboard::PanelConfig c = p->config();
            c.rangeMin = minSpin->value();
            c.rangeMax = maxSpin->value();
            dashboard_->applyPanelConfig(panelId, c);  // re-creates with the new range
        });
        layout->addRow(QString(), applyRange);
    }

    // Card size — applied live (no re-create) via setUserGeometry.
    auto* wSpin = new QSpinBox(form);
    auto* hSpin = new QSpinBox(form);
    wSpin->setRange(120, 4000);
    hSpin->setRange(80, 4000);
    const QRect g = panel->geometry();
    {
        const QSignalBlocker bw(wSpin);
        const QSignalBlocker bh(hSpin);
        wSpin->setValue(g.width());
        hSpin->setValue(g.height());
    }
    const auto applySize = [this, panelId, wSpin, hSpin]() {
        auto* p = (dashboard_ != nullptr) ? dashboard_->panel(panelId) : nullptr;
        if (p == nullptr) {
            return;
        }
        const QRect cur = p->geometry();
        p->setUserGeometry(QRect(cur.topLeft(), QSize(wSpin->value(), hSpin->value())));
    };
    connect(wSpin, &QSpinBox::valueChanged, this, [applySize](int) { applySize(); });
    connect(hSpin, &QSpinBox::valueChanged, this, [applySize](int) { applySize(); });
    layout->addRow(tr("Width"), wSpin);
    layout->addRow(tr("Height"), hSpin);

    inspector_->setContent(form);

    QVector<signalforge::workbench::InspectorPanel::Action> actions;
    actions.append({tr("Remove"), [this, panelId]() {
                        if (dashboard_ != nullptr) {
                            dashboard_->removePanel(panelId);
                        }
                        if (selectionModel_ != nullptr) {
                            selectionModel_->clear();
                        }
                        if (workbench_ != nullptr) {
                            workbench_->setInspectorVisible(false);
                        }
                    }});
    inspector_->setActions(actions);

    if (workbench_ != nullptr) {
        workbench_->setInspectorVisible(true);
    }
}

void MainWindow::onSelectionChanged(const signalforge::workbench::Selection& selection) {
    // The inspector observes the app-wide selection. A signal selection (or its
    // clearing) drives the signal-detail view; other kinds are handled by their
    // own feeders (e.g. Raw dissection fields) until they adopt the model too.
    if (selection.kind == signalforge::workbench::SelectionKind::Signal) {
        onSignalSelectedForInspector(selection.id);
    } else if (selection.kind == signalforge::workbench::SelectionKind::Widget) {
        onPanelSelectedForInspector(selection.id);
    } else if (selection.kind == signalforge::workbench::SelectionKind::None) {
        onSignalSelectedForInspector(QString());
    }
}

void MainWindow::onDrillToSourcePackets(const QString& signalId) {
    if (signalId.isEmpty()) {
        return;
    }
    // A signal id is "<type>:<connId>/<field>". A captured frame's `source` is
    // the *sender-stamped* "<type>:<addr>:<port>" — a different string, so they
    // share only the transport **type** (the prefix before the first ':'). Drill
    // through on that: `proto == "<type>"` shows the packets on this signal's
    // transport (exactly its source packets for the common single-connection
    // case; broader only when several connections share one transport).
    const int colon = signalId.indexOf(QLatin1Char(':'));
    const QString type = colon > 0 ? signalId.left(colon) : signalId;

    if (workbench_ != nullptr) {
        workbench_->setCurrentMode(QStringLiteral("inspect"));
    }
    if (inspectSegments_ != nullptr) {
        inspectSegments_->setCurrentSegment(QStringLiteral("raw"));  // no emission — switch the stack ourselves
    }
    if (inspectStack_ != nullptr && rawPacketView_ != nullptr) {
        inspectStack_->setCurrentWidget(rawPacketView_);
    }
    if (rawPacketView_ != nullptr) {
        rawPacketView_->setFilterText(QStringLiteral("proto == \"%1\"").arg(type));
    }
}

QColor MainWindow::resolveSignalColor(const QString& signalId) {
    if (const auto override = signalIdentity_.overrideColor(signalId); override.has_value()) {
        return *override;
    }
    return signalPaletteColor(signalIdentity_.colorIndex(signalId));
}

void MainWindow::refreshSignalColors() {
    // Re-push the (unchanged) provider so every view re-resolves colours: the
    // Parsed swatches, and every dashboard panel (PlotView re-colours its series,
    // MeterView its fill). The provider closures read the identity SSOT live.
    if (parsedView_ != nullptr) {
        parsedView_->refresh();
    }
    if (dashboard_ != nullptr) {
        dashboard_->setSignalColorProvider([this](const QString& id) { return resolveSignalColor(id); });
    }
}

void MainWindow::onRecolorRequested(const QString& signalId) {
    if (signalId.isEmpty()) {
        return;
    }
    const QColor current = resolveSignalColor(signalId);
    const QColor picked = QColorDialog::getColor(current, this, tr("Signal colour — %1").arg(signalId));
    if (!picked.isValid()) {
        return;  // user cancelled
    }
    signalIdentity_.setOverrideColor(signalId, picked);
    refreshSignalColors();
    if (selectionModel_ != nullptr &&
        selectionModel_->isSelected(signalforge::workbench::SelectionKind::Signal, signalId)) {
        onSignalSelectedForInspector(signalId);  // re-tint the inspector accent
    }
}

void MainWindow::onResetColorRequested(const QString& signalId) {
    if (signalId.isEmpty() || !signalIdentity_.hasOverride(signalId)) {
        return;
    }
    signalIdentity_.clearOverrideColor(signalId);
    refreshSignalColors();
    if (selectionModel_ != nullptr &&
        selectionModel_->isSelected(signalforge::workbench::SelectionKind::Signal, signalId)) {
        onSignalSelectedForInspector(signalId);
    }
}

void MainWindow::onAddConnectionRequested() {
    signalforge::connection::ConnectionDialog dlg(enumerateAvailableSchemaIds(), this);
    if (dlg.exec() != QDialog::Accepted || !dlg.isValid()) {
        return;
    }
    const QString id = connectionManager_->addConnection(dlg.config());
    if (id.isEmpty()) {
        QMessageBox::warning(this, tr("Add connection"), tr("Failed to add connection (id collision?)"));
    }
}

void MainWindow::onEditConnectionRequested(const QString& id) {
    auto* conn = connectionManager_->connection(id);
    if (!conn) {
        return;
    }
    if (conn->state() != signalforge::connection::Connection::State::Idle) {
        QMessageBox::information(this, tr("Edit connection"),
                                 tr("Disconnect the connection before editing its configuration."));
        return;
    }
    signalforge::connection::ConnectionDialog dlg(enumerateAvailableSchemaIds(), this);
    dlg.setConfig(conn->config());
    if (dlg.exec() != QDialog::Accepted || !dlg.isValid()) {
        return;
    }
    if (!connectionManager_->editConnection(id, dlg.config())) {
        QMessageBox::warning(this, tr("Edit connection"), tr("Failed to apply edits."));
    }
}

void MainWindow::onConnectAllAction() {
    if (connectionManager_) {
        connectionManager_->connectAll();
    }
}

void MainWindow::onDisconnectAllAction() {
    if (connectionManager_) {
        connectionManager_->disconnectAll();
    }
}

void MainWindow::ensureDashboardVisible() {
    showWorkspaceTab(QStringLiteral("dashboard"));
}

void MainWindow::showWorkspaceTab(const QString& which) {
    if (inspectSegments_ == nullptr || inspectStack_ == nullptr) {
        return;
    }
    const QString w = which.trimmed().toLower();
    QWidget* target = nullptr;
    if (w == QLatin1String("raw")) {
        target = rawPacketView_;
    } else if (w == QLatin1String("parsed")) {
        target = parsedView_;
    } else if (w == QLatin1String("dashboard")) {
        target = chartContainer_;
    }
    if (target != nullptr) {
        // M34: select the Inspect segment + ensure we're in Inspect mode.
        inspectSegments_->setCurrentSegment(w);
        inspectStack_->setCurrentWidget(target);
        if (workbench_ != nullptr) {
            workbench_->setCurrentMode(QStringLiteral("inspect"));
        }
    }
}

void MainWindow::onAddChart() {
    if (dashboard_ == nullptr) {
        return;
    }
    const QString panelId = dashboard_->addPlotPanel();
    ensureDashboardVisible();
    SF_LOG_INFO("MainWindow: added plot panel {}", panelId.toStdString());
}

void MainWindow::onAddTable() {
    if (dashboard_ == nullptr || signalBufferRegistry_ == nullptr) {
        return;
    }
    const QString panelId = dashboard_->addTablePanel(signalBufferRegistry_->signalIds());
    ensureDashboardVisible();
    SF_LOG_INFO("MainWindow: added table panel {}", panelId.toStdString());
}

void MainWindow::onAddBar() {
    if (dashboard_ == nullptr || signalBufferRegistry_ == nullptr) {
        return;
    }
    const auto ids = signalBufferRegistry_->signalIds();
    const QString panelId = dashboard_->addBarPanel(ids.isEmpty() ? QString() : ids.first());
    ensureDashboardVisible();
    SF_LOG_INFO("MainWindow: added bar panel {}", panelId.toStdString());
}

void MainWindow::onAddGauge() {
    if (dashboard_ == nullptr || signalBufferRegistry_ == nullptr) {
        return;
    }
    const auto ids = signalBufferRegistry_->signalIds();
    const QString panelId = dashboard_->addGaugePanel(ids.isEmpty() ? QString() : ids.first());
    ensureDashboardVisible();
    SF_LOG_INFO("MainWindow: added gauge panel {}", panelId.toStdString());
}

void MainWindow::updateEmptyStateVisibility() {
    if (connectStack_ == nullptr || workbench_ == nullptr) {
        return;
    }
    // M34 §7.1: the onboarding lives in Connect mode. With no connection we show
    // the onboarding empty-state and park on Connect; once the first connection
    // exists we reveal the connection manager and move the user to Inspect
    // (defaulting to Parsed). Subsequent connection changes leave the mode alone.
    const bool noConnection = connectionManager_ == nullptr || connectionManager_->connectionCount() == 0;
    if (noConnection) {
        connectStack_->setCurrentWidget(chartEmptyState_);
        workbench_->setCurrentMode(QStringLiteral("connect"));
    } else {
        const bool wasOnboarding =
            connectionManagerBody_ != nullptr && connectStack_->currentWidget() == chartEmptyState_;
        if (connectionManagerBody_ != nullptr) {
            connectStack_->setCurrentWidget(connectionManagerBody_);
        }
        if (wasOnboarding) {
            workbench_->setCurrentMode(QStringLiteral("inspect"));
        }
    }
}

void MainWindow::onPromoteSignalToDashboard(const QString& signalId, const QString& typeToken) {
    if (dashboard_ == nullptr || signalId.isEmpty()) {
        return;
    }
    if (const auto type = signalforge::dashboard::panelTypeFromName(typeToken); type.has_value()) {
        dashboard_->addSignalAs(signalId, *type);
    } else {
        dashboard_->addSignal(signalId);
    }
    // Stay on the current tier — adding a card no longer yanks the user to the
    // Dashboard, so several card types can be added for one signal in a row.
    // (Auto-jump-on-add is a deferred config option; see
    // docs/v0.4/configurable-options.md.) `panelsChanged` refreshes the Parsed
    // markers; re-show the inspector so its actions reflect the new membership.
    if (selectionModel_ != nullptr &&
        selectionModel_->isSelected(signalforge::workbench::SelectionKind::Signal, signalId)) {
        onSignalSelectedForInspector(signalId);
    }
}

void MainWindow::onLiveToggleChanged(bool live) {
    if (dashboard_ == nullptr) {
        return;
    }
    auto& axis = dashboard_->timeAxis();
    if (live) {
        axis.resume();
        liveToggle_->setText(tr("● Live"));
    } else {
        axis.pause();
        liveToggle_->setText(tr("⏸ Paused"));
    }
}

void MainWindow::onTimePresetChanged(int index) {
    if (dashboard_ == nullptr) {
        return;
    }
    using TP = signalforge::chart::TimeAxisManager::TimePreset;
    auto& axis = dashboard_->timeAxis();
    switch (index) {
    case 0:
        axis.setPreset(TP::Sec1);
        break;
    case 1:
        axis.setPreset(TP::Sec10);
        break;
    case 2:
        axis.setPreset(TP::Min1);
        break;
    case 3:
        axis.setPreset(TP::Min10);
        break;
    case 4:
        axis.setPreset(TP::Hour1);
        break;
    default:
        break;
    }
    if (liveToggle_ != nullptr && !liveToggle_->isChecked()) {
        liveToggle_->setChecked(true);
    }
}

void MainWindow::refreshStatusBar() {
    if (dashboard_ == nullptr) {
        return;
    }
    const int panelCount = dashboard_->panelCount();
    if (fpsLabel_ != nullptr) {
        fpsLabel_->setText(panelCount > 0 ? tr("%1 Hz").arg(15) : tr("idle"));
        fpsLabel_->setToolTip(panelCount > 0 ? tr("Dashboard refresh cadence") : tr("No panels"));
        applyLabelClass(fpsLabel_, "severity-info");
    }
    if (droppedLabel_ != nullptr) {
        // DR-001 #5: the dashboard doesn't drop frames, so don't show a
        // permanent "Drops 0" at idle (it read as a standing warning).
        droppedLabel_->setVisible(false);
    }
    if (throttledLabel_ != nullptr) {
        if (!chartStatusOverrideText_.isEmpty()) {
            throttledLabel_->setText(chartStatusOverrideText_);
            throttledLabel_->setToolTip(tr("Chart data stream is interrupted or frames were dropped."));
            applyLabelClass(throttledLabel_, chartStatusOverrideClass_.toUtf8().constData());
        } else {
            throttledLabel_->setText(QString{});
            throttledLabel_->setToolTip(QString{});
            applyLabelClass(throttledLabel_, "severity-info");
        }
    }
    // M14 F15: surface signal_buffer budget pressure to the user. Pre-fix
    // the registry silently logged "registration rejected: would exceed
    // budget" without any UI signal — operator could only tell the chart
    // wasn't drawing some signals by reading the log file. Now the
    // status bar shows the percentage utilized and flips to a warning
    // tone above the 80 % soft threshold.
    if (bufferBudgetLabel_ != nullptr && !bufferBudgetOverrideText_.isEmpty()) {
        bufferBudgetLabel_->setText(bufferBudgetOverrideText_);
        applyLabelClass(bufferBudgetLabel_, bufferBudgetOverrideClass_.toUtf8().constData());
    } else if (bufferBudgetLabel_ != nullptr && signalBufferRegistry_ != nullptr) {
        const std::size_t used = signalBufferRegistry_->totalMemoryBytes();
        const std::size_t budget = signalBufferRegistry_->totalBudgetBytes();
        if (budget == 0) {
            bufferBudgetLabel_->setText(QString{});
        } else {
            const int pct = static_cast<int>((100ULL * used) / budget);
            QString text;
            const char* cls = "severity-info";
            if (pct >= 100) {
                text = tr("Buffer full %1/%2 MiB").arg(used / (1024 * 1024)).arg(budget / (1024 * 1024));
                cls = "severity-error";
            } else if (pct >= 80) {
                text = tr("Buffer %1% %2/%3 MiB").arg(pct).arg(used / (1024 * 1024)).arg(budget / (1024 * 1024));
                cls = "severity-warning";
            } else {
                text = tr("Buffer %1% %2 MiB").arg(pct).arg(used / (1024 * 1024));
            }
            bufferBudgetLabel_->setText(text);
            applyLabelClass(bufferBudgetLabel_, cls);
        }
    }
    updateEmptyStateVisibility();
}

void MainWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);
    if (auto* handle = windowHandle(); handle != nullptr) {
        handle->raise();
        handle->requestActivate();
        QTimer::singleShot(500, this, [this]() {
            if (auto* h = windowHandle(); h != nullptr && !h->isActive()) {
                SF_LOG_WARN("MainWindow: chart window not active after show; redraw may be "
                            "throttled by the compositor (per [Proto] Anomaly §2)");
            }
        });
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (sessionWriter_ != nullptr && sessionWriter_->isRecording()) {
        const auto button = QMessageBox::question(
            this, tr("Recording in progress"), tr("A session is currently being recorded.\n\nStop recording and exit?"),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Yes);
        if (button != QMessageBox::Yes) {
            event->ignore();
            return;
        }
        // ADR-013 (F6): SessionWriter stays subscribed to TeeSink for the
        // MainWindow lifetime; stop() flips state to Idle and writes the
        // footer. No removeSink needed.
        (void)sessionWriter_->stop();
    }
    if (replayModeManager_ != nullptr && replayModeManager_->currentMode() == signalforge::replay::AppMode::Replay) {
        if (!confirmExitReplayForClose()) {
            event->ignore();
            return;
        }
    }
    if (!configSaveHealthy_) {
        const QString detail = lastConfigSaveMessage_.isEmpty() ? tr("Connection configuration has not been saved.")
                                                                : lastConfigSaveMessage_;
        const auto button =
            QMessageBox::warning(this, tr("Configuration not saved"), tr("%1\n\nExit anyway?").arg(detail),
                                 QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        if (button != QMessageBox::Yes) {
            event->ignore();
            return;
        }
    }
    QMainWindow::closeEvent(event);
}

void MainWindow::onConfigurationSaveStateChanged(bool saved, const QString& path, const QString& message) {
    configSaveHealthy_ = saved;
    lastConfigSaveMessage_ = message;
    if (configSaveStatusLabel_ == nullptr) {
        return;
    }
    if (saved) {
        configSaveStatusLabel_->setText(path.isEmpty() ? tr("Config ready") : tr("Config saved"));
        configSaveStatusLabel_->setToolTip(path.isEmpty() ? tr("Connection configuration save status")
                                                          : tr("Saved to %1").arg(path));
        applyLabelClass(configSaveStatusLabel_, "severity-info");
        if (configSaveDockBanner_ != nullptr) {
            configSaveDockBanner_->clear();
            configSaveDockBanner_->setVisible(false);
        }
        return;
    }
    configSaveStatusLabel_->setText(tr("Config save failed"));
    configSaveStatusLabel_->setToolTip(message.isEmpty() ? tr("Connection configuration was not saved.") : message);
    applyLabelClass(configSaveStatusLabel_, "severity-error");
    if (configSaveDockBanner_ != nullptr) {
        configSaveDockBanner_->setText(
            message.isEmpty()
                ? tr("Connection configuration was not saved. Fix the config path before relying on this setup.")
                : tr("%1\nChanges remain in memory until SignalForge exits.").arg(message));
        configSaveDockBanner_->setToolTip(configSaveDockBanner_->text());
        configSaveDockBanner_->setVisible(true);
        applyLabelClass(configSaveDockBanner_, "severity-error");
    }
}

void MainWindow::buildSessionUi() {
    // Session menu (owned by buildMenuBar): record + open-replay co-located
    // (DR-001 #4 — they were split across Session and File).
    recordAction_ = menuSession_->addAction(tr("&Record…"));
    recordAction_->setShortcut(QKeySequence(tr("Ctrl+R")));
    connect(recordAction_, &QAction::triggered, this, &MainWindow::onRecordToggle);

    recordingStatusLabel_ = new QLabel;
    recordingStatusLabel_->setObjectName(QStringLiteral("recordingStatusLabel"));
    recordingStatusLabel_->setText(tr("Record idle"));
    applyLabelClass(recordingStatusLabel_, "severity-info");
    recordingStatusLabel_->setToolTip(tr("Session recording status"));
    if (statusStripLayout_ != nullptr) {
        statusStripLayout_->insertWidget(2, makeStatusCell(QString(), recordingStatusLabel_, statusStrip_));
    }

    if (sessionWriter_ != nullptr) {
        connect(sessionWriter_.get(), &signalforge::session::SessionWriter::flushed, this,
                &MainWindow::onRecordingFlushed);
        connect(sessionWriter_.get(), &signalforge::session::SessionWriter::errorOccurred, this,
                &MainWindow::onRecordingError);
    }
    recordingHeartbeatTimer_ = new QTimer(this);
    recordingHeartbeatTimer_->setInterval(1000);
    connect(recordingHeartbeatTimer_, &QTimer::timeout, this, &MainWindow::updateRecordingStatusLabel);
}

void MainWindow::onRecordToggle() {
    if (sessionWriter_ == nullptr) {
        return;
    }
    if (sessionWriter_->isRecording()) {
        // Stop path: stop + close file. ADR-013 (F6): SessionWriter stays
        // subscribed to TeeSink across recording sessions; state gates
        // file writes.
        const std::size_t bytes = sessionWriter_->stop();
        if (recordingHeartbeatTimer_ != nullptr) {
            recordingHeartbeatTimer_->stop();
        }
        lastRecordingBytes_ = bytes;
        updateWorkflowModeLabel();
        recordAction_->setText(tr("&Record…"));
        if (recordingStatusLabel_ != nullptr) {
            recordingStatusLabel_->setText(
                tr("Record stopped %1 | %2 bytes").arg(formatElapsedMs(recordingElapsed_.elapsed())).arg(bytes));
            applyLabelClass(recordingStatusLabel_, "severity-info");
        }
        SF_LOG_INFO("MainWindow: recording stopped ({} bytes -> {})", bytes, currentRecordingPath_.toStdString());
        currentRecordingPath_.clear();
        return;
    }

    // Start path: ask for a path.
    const QString path =
        QFileDialog::getSaveFileName(this, tr("Save session recording"), QString(), tr("SFREPLAY (*.sfreplay)"));
    if (path.isEmpty()) {
        return;
    }
    // M14 F9: capture the active connection's decoderSchemaId in the
    // recording's metadata header. SessionWriter::start already
    // accepts a `decoderSchemaId` parameter (M10-frozen
    // session_writer.hpp:89) but MainWindow never passed one — the
    // recorded file's metadata field stayed empty, breaking M11
    // schema-match validation when re-opening the recording later
    // (audit run5 §F9).
    //
    // Selection rule: walk the connection list; pick the first
    // Connected connection that has a non-empty decoderSchemaId.
    // If multiple Connected drivers have schemas, last-Connected
    // wins is acceptable for V1 (operator-driven recording —
    // the operator can disconnect drivers they don't want
    // associated with the file). V1.0.1 may add a UI for explicit
    // selection.
    QString recordingSchemaId;
    if (connectionManager_ != nullptr) {
        for (const auto& id : connectionManager_->connectionIds()) {
            const auto* conn = connectionManager_->connection(id);
            if (conn == nullptr) {
                continue;
            }
            if (conn->state() != signalforge::connection::Connection::State::Connected) {
                continue;
            }
            const QString& cfgSchema = conn->config().decoderSchemaId;
            if (!cfgSchema.isEmpty()) {
                recordingSchemaId = cfgSchema;
            }
        }
    }
    if (!sessionWriter_->start(path, /*description*/ QString{}, recordingSchemaId)) {
        QMessageBox::critical(this, tr("Recording failed"), tr("Could not start recording: see log for details."));
        return;
    }
    // ADR-013 (F6): SessionWriter is already subscribed to TeeSink at
    // MainWindow ctor — no per-recording addSink needed.
    currentRecordingPath_ = path;
    lastRecordingBytes_ = 0;
    recordingElapsed_.restart();
    recordAction_->setText(tr("&Stop recording"));
    updateRecordingStatusLabel();
    updateWorkflowModeLabel();
    if (recordingHeartbeatTimer_ != nullptr) {
        recordingHeartbeatTimer_->start();
    }
    SF_LOG_INFO("MainWindow: recording started -> {}", path.toStdString());
}

void MainWindow::onRecordingFlushed(std::size_t bytes) {
    if (currentRecordingPath_.isEmpty()) {
        return;
    }
    lastRecordingBytes_ = bytes;
    updateRecordingStatusLabel();
}

void MainWindow::updateRecordingStatusLabel() {
    if (recordingStatusLabel_ == nullptr || currentRecordingPath_.isEmpty()) {
        return;
    }
    const QString filename = QFileInfo(currentRecordingPath_).fileName();
    recordingStatusLabel_->setText(tr("Recording %1 | %2 bytes | %3")
                                       .arg(formatElapsedMs(recordingElapsed_.elapsed()))
                                       .arg(lastRecordingBytes_)
                                       .arg(filename));
    applyLabelClass(recordingStatusLabel_, "mode-recording");
}

void MainWindow::updateWorkflowModeLabel() {
    if (workflowModeLabel_ == nullptr) {
        return;
    }
    const bool recording = sessionWriter_ != nullptr && sessionWriter_->isRecording();
    const bool inReplay =
        replayModeManager_ != nullptr && replayModeManager_->currentMode() == signalforge::replay::AppMode::Replay;
    if (recording) {
        workflowModeLabel_->setText(tr("Recording"));
        applyLabelClass(workflowModeLabel_, "mode-recording");
    } else if (inReplay) {
        using S = signalforge::replay::PlaybackState;
        const auto replayState = playbackController_ != nullptr ? playbackController_->state() : S::Loaded;
        if (replayState == S::Paused) {
            workflowModeLabel_->setText(tr("Paused"));
            applyLabelClass(workflowModeLabel_, "mode-paused");
        } else if (replayState == S::Ended) {
            workflowModeLabel_->setText(tr("Ended"));
            applyLabelClass(workflowModeLabel_, "mode-ended");
        } else {
            workflowModeLabel_->setText(tr("Replay"));
            applyLabelClass(workflowModeLabel_, "mode-replay");
        }
    } else {
        workflowModeLabel_->setText(tr("Live"));
        applyLabelClass(workflowModeLabel_, "mode-live");
    }
}

void MainWindow::onRecordingError(const QString& message) {
    if (recordingHeartbeatTimer_ != nullptr) {
        recordingHeartbeatTimer_->stop();
    }
    updateWorkflowModeLabel();
    if (recordingStatusLabel_ != nullptr) {
        recordingStatusLabel_->setText(tr("Recording error | %1").arg(message));
        applyLabelClass(recordingStatusLabel_, "severity-error");
    }
    if (recordAction_ != nullptr) {
        recordAction_->setText(tr("&Record…"));
    }
    // ADR-013 (F6): SessionWriter stays subscribed to TeeSink across
    // recording-error states; recording state has already flipped to
    // Error inside the writer's worker-error connector. No removeSink.
    QMessageBox::critical(this, tr("Recording error"), message);
}

// ── M11 replay UI ─────────────────────────────────────────────

void MainWindow::buildReplayUi() {
    // Session → Open Session… (Ctrl+O), co-located with Record (DR-001 #4).
    openSessionAction_ = menuSession_->addAction(tr("&Open Session…"));
    openSessionAction_->setShortcut(QKeySequence(tr("Ctrl+O")));
    connect(openSessionAction_, &QAction::triggered, this, &MainWindow::onOpenSessionRequested);

    // File → Quit (Ctrl+Q). close() routes through closeEvent() so the
    // recording-in-progress prompt + config-save persistence still fire.
    auto* quitAction = menuFile_->addAction(tr("&Quit"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &MainWindow::close);

    // Replay toolbar — initially hidden; visible only in Replay mode.
    replayToolbar_ = addToolBar(tr("Replay"));
    replayToolbar_->setObjectName(QStringLiteral("replayTransportToolbar"));
    replayToolbar_->setMovable(false);
    replayToolbar_->setVisible(false);

    auto* transportGroup = new QWidget(replayToolbar_);
    transportGroup->setObjectName(QStringLiteral("replayTransportGroup"));
    auto* transportLayout = new QHBoxLayout(transportGroup);
    transportLayout->setContentsMargins(0, 0, 0, 0);
    transportLayout->setSpacing(4);
    auto* stepBackButton = new QToolButton(transportGroup);
    replayStepBackAction_ = new QAction(tr("◀"), stepBackButton);
    replayStepBackAction_->setToolTip(tr("Step backward"));
    stepBackButton->setDefaultAction(replayStepBackAction_);
    connect(replayStepBackAction_, &QAction::triggered, this, &MainWindow::onReplayStepBackward);
    auto* playButton = new QToolButton(transportGroup);
    replayPlayPauseAction_ = new QAction(tr("▶ Play"), playButton);
    replayPlayPauseAction_->setToolTip(tr("Play or pause replay"));
    playButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    playButton->setDefaultAction(replayPlayPauseAction_);
    connect(replayPlayPauseAction_, &QAction::triggered, this, &MainWindow::onReplayPlayPause);
    auto* stepForwardButton = new QToolButton(transportGroup);
    replayStepForwardAction_ = new QAction(tr("▶"), stepForwardButton);
    replayStepForwardAction_->setToolTip(tr("Step forward"));
    stepForwardButton->setDefaultAction(replayStepForwardAction_);
    connect(replayStepForwardAction_, &QAction::triggered, this, &MainWindow::onReplayStepForward);
    transportLayout->addWidget(stepBackButton);
    transportLayout->addWidget(playButton);
    transportLayout->addWidget(stepForwardButton);
    replayToolbar_->addWidget(transportGroup);
    replayToolbar_->addSeparator();

    replaySeekSlider_ = new QSlider(Qt::Horizontal, replayToolbar_);
    replaySeekSlider_->setObjectName(QStringLiteral("replaySeekSlider"));
    replaySeekSlider_->setMinimumWidth(260);
    replaySeekSlider_->setRange(0, 0);
    connect(replaySeekSlider_, &QSlider::valueChanged, this, &MainWindow::onReplaySeekSliderChanged);
    replayToolbar_->addWidget(replaySeekSlider_);

    replaySpeedCombo_ = new QComboBox(replayToolbar_);
    replaySpeedCombo_->setObjectName(QStringLiteral("replaySpeedCombo"));
    replaySpeedCombo_->addItem(tr("0.5×"), 0.5);
    replaySpeedCombo_->addItem(tr("1×"), 1.0);
    replaySpeedCombo_->addItem(tr("2×"), 2.0);
    replaySpeedCombo_->addItem(tr("5×"), 5.0);
    replaySpeedCombo_->addItem(tr("10×"), 10.0);
    replaySpeedCombo_->setCurrentIndex(1);
    connect(replaySpeedCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this,
            &MainWindow::onReplaySpeedChanged);
    replayToolbar_->addWidget(replaySpeedCombo_);
    replayToolbar_->addSeparator();

    replayExitAction_ = replayToolbar_->addAction(tr("Exit Replay"));
    connect(replayExitAction_, &QAction::triggered, this, &MainWindow::onExitReplayRequested);

    // Status-bar replay info.
    replayStatusLabel_ = new QLabel;
    replayStatusLabel_->setObjectName(QStringLiteral("replayStatusLabel"));
    replayStatusLabel_->setText(tr("Replay idle"));
    applyLabelClass(replayStatusLabel_, "severity-info");
    if (statusStripLayout_ != nullptr) {
        statusStripLayout_->insertWidget(3, makeStatusCell(QString(), replayStatusLabel_, statusStrip_));
    }

    // Wire PlaybackController + ReplayModeManager signals.
    connect(playbackController_.get(), &signalforge::replay::PlaybackController::positionChanged, this,
            &MainWindow::onReplayPositionChanged);
    connect(playbackController_.get(), &signalforge::replay::PlaybackController::stateChanged, this,
            &MainWindow::onReplayStateChanged);
    connect(playbackController_.get(), &signalforge::replay::PlaybackController::errorOccurred, this,
            &MainWindow::onReplayError);

    // M14 F14: react to mode transitions so File→Open Session +
    // Record action enabled-state stay consistent with the current
    // mode (audit §F14). updateReplayActionStates re-reads
    // replayModeManager_->currentMode() and updates both actions.
    connect(replayModeManager_.get(), &signalforge::replay::ReplayModeManager::modeChanged, this,
            [this](signalforge::replay::AppMode) {
                updateReplayActionStates();
                updateWorkflowModeLabel();
            });

    updateReplayActionStates();
    updateWorkflowModeLabel();
}

void MainWindow::onOpenSessionRequested() {
    // C6: refuse to enter Replay while a recording is active.
    if (sessionWriter_ != nullptr && sessionWriter_->isRecording()) {
        QMessageBox::information(this, tr("Recording in progress"),
                                 tr("Stop the current recording before opening a session for replay."));
        return;
    }

    const QString path = QFileDialog::getOpenFileName(this, tr("Open session"), QString(), tr("SFREPLAY (*.sfreplay)"));
    if (path.isEmpty()) {
        return;
    }

    // Confirmation dialog if any M9 connection is currently
    // Connected (per spec §3.4 / M11.4).
    int activeCount = 0;
    if (connectionManager_ != nullptr) {
        for (const auto& id : connectionManager_->connectionIds()) {
            const auto* conn = connectionManager_->connection(id);
            if (conn && conn->state() == signalforge::connection::Connection::State::Connected) {
                ++activeCount;
            }
        }
    }
    if (activeCount > 0) {
        const auto reply = QMessageBox::warning(
            this, tr("Pause connections to enter Replay?"),
            tr("Entering Replay mode will pause %1 active connection(s). Continue?").arg(activeCount),
            QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes) {
            return;
        }
    }

    if (!replayModeManager_->enterReplay()) {
        QMessageBox::critical(this, tr("Replay error"), tr("Failed to enter Replay mode."));
        return;
    }
    if (!playbackController_->loadSession(path)) {
        // Roll back the mode transition if load failed.
        (void)replayModeManager_->exitReplay(false);
        QMessageBox::critical(this, tr("Replay error"),
                              tr("Failed to load session: %1").arg(playbackController_->lastError()));
        return;
    }

    replayToolbar_->setVisible(true);
    replaySeekSlider_->setRange(0, static_cast<int>(playbackController_->totalRecords()));
    if (replayStatusLabel_ != nullptr) {
        replayStatusLabel_->setText(tr("Replay loaded %1").arg(QFileInfo(path).fileName()));
        applyLabelClass(replayStatusLabel_, "mode-replay");
    }
    updateReplayActionStates();
    SF_LOG_INFO("MainWindow: replay started -> {}", path.toStdString());
}

void MainWindow::onReplayPlayPause() {
    if (playbackController_ == nullptr) {
        return;
    }
    if (playbackController_->state() == signalforge::replay::PlaybackState::Playing) {
        (void)playbackController_->pause();
    } else {
        (void)playbackController_->play();
    }
}

void MainWindow::onReplayStepForward() {
    if (playbackController_ != nullptr) {
        (void)playbackController_->stepForward();
    }
}

void MainWindow::onReplayStepBackward() {
    if (playbackController_ != nullptr) {
        (void)playbackController_->stepBackward();
    }
}

void MainWindow::onReplaySeekSliderChanged(int value) {
    if (playbackController_ == nullptr || !replaySliderUserDriven_) {
        return;
    }
    const std::size_t total = playbackController_->totalRecords();
    if (total == 0) {
        return;
    }
    const std::int64_t duration = playbackController_->durationNs();
    const auto target = static_cast<std::int64_t>((static_cast<double>(value) / static_cast<double>(total)) *
                                                  static_cast<double>(duration));
    if (playbackController_->seek(target)) {
        onReplayPositionChanged(playbackController_->currentPositionNs(), playbackController_->currentRecordIndex());
    }
}

void MainWindow::onReplaySpeedChanged(int index) {
    if (playbackController_ == nullptr || replaySpeedCombo_ == nullptr) {
        return;
    }
    const double speed = replaySpeedCombo_->itemData(index).toDouble();
    if (playbackController_->setSpeed(speed)) {
        onReplayPositionChanged(playbackController_->currentPositionNs(), playbackController_->currentRecordIndex());
    }
}

void MainWindow::onExitReplayRequested() {
    if (replayModeManager_ == nullptr || replayModeManager_->currentMode() != signalforge::replay::AppMode::Replay) {
        return;
    }
    bool resume = false;
    if (!replayModeManager_->pausedConnectionIds().isEmpty()) {
        const auto reply =
            QMessageBox::question(this, tr("Exit Replay"), tr("Resume the previously-paused connections?"),
                                  QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (reply == QMessageBox::Cancel) {
            return;
        }
        resume = (reply == QMessageBox::Yes);
    }

    playbackController_->closeSession();
    (void)replayModeManager_->exitReplay(resume);

    replayToolbar_->setVisible(false);
    if (replayStatusLabel_ != nullptr) {
        replayStatusLabel_->setText(tr("Replay idle"));
        applyLabelClass(replayStatusLabel_, "severity-info");
    }
    updateReplayActionStates();
}

bool MainWindow::confirmExitReplayForClose() {
    if (replayModeManager_ == nullptr || replayModeManager_->currentMode() != signalforge::replay::AppMode::Replay) {
        return true;
    }

    bool resume = false;
    const auto pausedIds = replayModeManager_->pausedConnectionIds();
    if (pausedIds.isEmpty()) {
        const auto reply = QMessageBox::question(this, tr("Exit Replay"),
                                                 tr("A replay session is open.\n\nExit Replay and close SignalForge?"),
                                                 QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Yes);
        if (reply != QMessageBox::Yes) {
            return false;
        }
    } else {
        const auto reply =
            QMessageBox::question(this, tr("Exit Replay"),
                                  tr("A replay session is open and %1 connection(s) were paused.\n\nResume paused "
                                     "connections before closing?")
                                      .arg(pausedIds.size()),
                                  QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);
        if (reply == QMessageBox::Cancel) {
            return false;
        }
        resume = (reply == QMessageBox::Yes);
    }

    if (playbackController_ != nullptr) {
        playbackController_->closeSession();
    }
    (void)replayModeManager_->exitReplay(resume);
    if (replayToolbar_ != nullptr) {
        replayToolbar_->setVisible(false);
    }
    if (replayStatusLabel_ != nullptr) {
        replayStatusLabel_->setText(tr("Replay idle"));
        applyLabelClass(replayStatusLabel_, "severity-info");
    }
    updateReplayActionStates();
    updateWorkflowModeLabel();
    return true;
}

void MainWindow::updateReplayStatusLabel() {
    if (replayStatusLabel_ == nullptr || playbackController_ == nullptr) {
        return;
    }
    using S = signalforge::replay::PlaybackState;
    const auto st = playbackController_->state();
    if (st == S::Idle) {
        replayStatusLabel_->setText(tr("Replay idle"));
        applyLabelClass(replayStatusLabel_, "severity-info");
        return;
    }
    if (st == S::Error) {
        replayStatusLabel_->setText(tr("Replay error"));
        applyLabelClass(replayStatusLabel_, "severity-error");
        return;
    }
    const QString filename = QFileInfo(playbackController_->currentFilePath()).fileName();
    QString stateText = tr("Loaded");
    if (st == S::Playing) {
        stateText = tr("Playing");
    } else if (st == S::Paused) {
        stateText = tr("Paused");
    } else if (st == S::Ended) {
        stateText = tr("Ended");
    }
    replayStatusLabel_->setText(tr("%1 %2/%3")
                                    .arg(stateText)
                                    .arg(formatReplayStatusSeconds(playbackController_->currentPositionNs()))
                                    .arg(formatReplayStatusSeconds(playbackController_->durationNs())));
    replayStatusLabel_->setToolTip(tr("%1 | %2/%3 | %4 records")
                                       .arg(filename)
                                       .arg(formatReplayPosition(playbackController_->currentPositionNs()))
                                       .arg(formatReplayPosition(playbackController_->durationNs()))
                                       .arg(playbackController_->totalRecords()));
    const char* cls = st == S::Paused ? "mode-paused" : st == S::Ended ? "mode-ended" : "mode-replay";
    applyLabelClass(replayStatusLabel_, cls);
}

void MainWindow::onReplayPositionChanged(std::int64_t timestampNs, std::size_t recordIndex) {
    if (replaySeekSlider_ != nullptr) {
        // Bypass user-driven seek dispatch when we update the
        // slider from the playback controller's position.
        replaySliderUserDriven_ = false;
        replaySeekSlider_->setValue(static_cast<int>(recordIndex));
        replaySliderUserDriven_ = true;
    }
    if (replayStatusLabel_ != nullptr && playbackController_ != nullptr) {
        const QString filename = QFileInfo(playbackController_->currentFilePath()).fileName();
        // M14 F12: relative-from-zero mm:ss.fff format per M11 §Test 4
        // / §Test 5 (audit §F12). Pre-fix the formatter emitted raw
        // nanoseconds which read like an absolute UTC timestamp.
        using S = signalforge::replay::PlaybackState;
        const auto st = playbackController_->state();
        const QString stateText = st == S::Playing  ? tr("Playing")
                                  : st == S::Paused ? tr("Paused")
                                  : st == S::Ended  ? tr("Ended")
                                                    : tr("Loaded");
        replayStatusLabel_->setText(tr("%1 %2/%3")
                                        .arg(stateText)
                                        .arg(formatReplayStatusSeconds(timestampNs))
                                        .arg(formatReplayStatusSeconds(playbackController_->durationNs())));
        replayStatusLabel_->setToolTip(tr("%1 | %2/%3 | %4/%5 records")
                                           .arg(filename)
                                           .arg(formatReplayPosition(timestampNs))
                                           .arg(formatReplayPosition(playbackController_->durationNs()))
                                           .arg(recordIndex)
                                           .arg(playbackController_->totalRecords()));
        const char* cls = st == S::Paused ? "mode-paused" : st == S::Ended ? "mode-ended" : "mode-replay";
        applyLabelClass(replayStatusLabel_, cls);
    }
}

void MainWindow::onReplayStateChanged() {
    updateReplayActionStates();
    updateWorkflowModeLabel();
    updateReplayStatusLabel();
}

void MainWindow::onReplayError(const QString& message) {
    QMessageBox::critical(this, tr("Replay error"), message);
}

void MainWindow::updateReplayActionStates() {
    if (playbackController_ == nullptr || replayPlayPauseAction_ == nullptr) {
        return;
    }
    const auto st = playbackController_->state();
    using S = signalforge::replay::PlaybackState;
    replayPlayPauseAction_->setText(st == S::Playing ? tr("Pause") : tr("▶ Play"));
    replayPlayPauseAction_->setEnabled(st == S::Loaded || st == S::Paused || st == S::Playing);
    replayStepForwardAction_->setEnabled(st == S::Loaded || st == S::Paused);
    replayStepBackAction_->setEnabled(st == S::Loaded || st == S::Paused || st == S::Ended);
    replaySeekSlider_->setEnabled(st != S::Idle && st != S::Error);

    // C6 cross-mode gating.
    const bool inReplay =
        replayModeManager_ != nullptr && replayModeManager_->currentMode() == signalforge::replay::AppMode::Replay;
    if (recordAction_ != nullptr) {
        recordAction_->setEnabled(!inReplay);
    }
    if (openSessionAction_ != nullptr) {
        // M14 F14: also disable while already in Replay mode (audit
        // §F14 — clicking Open Session in Replay otherwise produces an
        // error popup; M11 §S8 expects the operator to use Exit Replay
        // first). The `inReplay` guard layers on top of the existing
        // record-in-progress guard.
        const bool recording = (sessionWriter_ != nullptr && sessionWriter_->isRecording());
        openSessionAction_->setEnabled(!recording && !inReplay);
    }
}

}  // namespace signalforge::app
