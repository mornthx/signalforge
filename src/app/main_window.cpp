#include "main_window.hpp"

#include "buffer/signal_buffer_registry.hpp"
#include "chart/chart.hpp"
#include "chart/chart_manager.hpp"
#include "chart/signal_selector.hpp"
#include "chart/time_axis_manager.hpp"
#include "connection/connection.hpp"
#include "connection/connection_dialog.hpp"
#include "connection/connection_list_widget.hpp"
#include "connection/connection_manager.hpp"
#include "connection/connection_status_widget.hpp"
#include "decode/decoder_registrar.hpp"
#include "observability/logging.hpp"
#include "pipeline/pipeline_manager.hpp"
#include "replay/playback_controller.hpp"
#include "replay/replay_mode_manager.hpp"
#include "session/session_writer.hpp"
#include "session/tee_signal_value_sink.hpp"

#include <QAction>
#include <QCloseEvent>
#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QQuickItem>
#include <QQuickWidget>
#include <QSlider>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
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

// driverId convention: `<type>:<connectionId>` — DecoderRegistrar
// splits on ':' and uses the prefix as its driver-type lookup key
// (so "udp:conn-3" → "udp"). Mirrors the `driverTypeToYamlInternal`
// helper in connection_manager.cpp's anonymous namespace.
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

}  // namespace

namespace signalforge::app {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("SignalForge"));
    resize(1280, 800);

    pipelineManager_ = std::make_unique<signalforge::pipeline::PipelineManager>(this);
    signalBufferRegistry_ = std::make_unique<signalforge::buffer::SignalBufferRegistry>();

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
                const QString driverId = driverTypeName(conn->config().driverType) + QStringLiteral(":") + id;
                if (state == signalforge::connection::Connection::State::Connected) {
                    signalforge::pipeline::PipelineConfig cfg;
                    cfg.driverId = driverId;
                    (void)pipelineManager_->attach(conn->driver(), cfg);
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

    chartManager_ = std::make_unique<signalforge::chart::ChartManager>(*signalBufferRegistry_, this);

    // M11 replay plumbing. PlaybackController dispatches signals
    // back into the same SignalBufferRegistry the live path uses,
    // so charts work uniformly across modes. ReplayModeManager
    // orchestrates Live ↔ Replay transitions on top.
    playbackController_ = std::make_unique<signalforge::replay::PlaybackController>(*signalBufferRegistry_, this);
    replayModeManager_ =
        std::make_unique<signalforge::replay::ReplayModeManager>(*connectionManager_, *playbackController_, this);

    buildChartUi();
    buildConnectionUi();
    buildSessionUi();
    buildReplayUi();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildChartUi() {
    centralSplitter_ = new QSplitter(Qt::Horizontal, this);

    signalSelector_ = new signalforge::chart::SignalSelector(*signalBufferRegistry_, *chartManager_);
    centralSplitter_->addWidget(signalSelector_);

    chartContainer_ = new QWidget;
    chartLayout_ = new QVBoxLayout(chartContainer_);
    chartLayout_->setContentsMargins(0, 0, 0, 0);
    chartLayout_->setSpacing(2);
    centralSplitter_->addWidget(chartContainer_);

    centralSplitter_->setStretchFactor(0, 1);
    centralSplitter_->setStretchFactor(1, 4);
    // Initial split sizes (256 + 1024 = 1280, matching the window's
    // resize() above). QSplitter's stretch factor only governs *extra*
    // space distribution on subsequent resizes; initial pane sizes
    // come from sizeHint(). The chart pane's sizeHint() is dominated
    // by the QQuickWidget which reports an invalid hint until the
    // QML scene loads — without explicit setSizes() the chart pane
    // collapses to ~12 px under offscreen QPA (caught by M14 S1
    // smoke; see ADR-011 §"Implementation note: splitter sizing").
    centralSplitter_->setSizes({256, 1024});
    setCentralWidget(centralSplitter_);

    auto* toolbar = addToolBar(tr("Chart"));
    toolbar->setMovable(false);

    liveToggle_ = new QToolButton(toolbar);
    liveToggle_->setCheckable(true);
    liveToggle_->setChecked(true);
    liveToggle_->setText(tr("● Live"));
    liveToggle_->setToolTip(tr("Toggle live mode (vs paused)"));
    connect(liveToggle_, &QToolButton::toggled, this, &MainWindow::onLiveToggleChanged);
    toolbar->addWidget(liveToggle_);

    timePresetCombo_ = new QComboBox(toolbar);
    timePresetCombo_->addItem(tr("1 sec"));
    timePresetCombo_->addItem(tr("10 sec"));
    timePresetCombo_->addItem(tr("1 min"));
    timePresetCombo_->addItem(tr("10 min"));
    timePresetCombo_->addItem(tr("1 hour"));
    timePresetCombo_->setCurrentIndex(1);
    connect(timePresetCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::onTimePresetChanged);
    toolbar->addWidget(timePresetCombo_);

    auto* addChartAction = toolbar->addAction(tr("+ Chart"));
    connect(addChartAction, &QAction::triggered, this, &MainWindow::onAddChart);

    fpsLabel_ = new QLabel(tr("FPS: -"));
    droppedLabel_ = new QLabel(tr("Dropped: 0"));
    throttledLabel_ = new QLabel;
    // M14 F15: signal_buffer budget indicator (idle / 80% warn / FULL).
    bufferBudgetLabel_ = new QLabel;
    bufferBudgetLabel_->setToolTip(tr("Signal buffer memory budget usage"));
    statusBar()->addPermanentWidget(fpsLabel_);
    statusBar()->addPermanentWidget(droppedLabel_);
    statusBar()->addPermanentWidget(throttledLabel_);
    statusBar()->addPermanentWidget(bufferBudgetLabel_);

    auto* statusTimer = new QTimer(this);
    statusTimer->setInterval(1000);
    connect(statusTimer, &QTimer::timeout, this, &MainWindow::refreshStatusBar);
    statusTimer->start();

    onAddChart();
}

void MainWindow::buildConnectionUi() {
    // Connections menu.
    auto* menu = menuBar()->addMenu(tr("&Connections"));

    auto* addAction = menu->addAction(tr("&Add…"));
    addAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+M")));
    connect(addAction, &QAction::triggered, this, &MainWindow::onAddConnectionRequested);

    auto* connectAllAction = menu->addAction(tr("Connect &all"));
    connect(connectAllAction, &QAction::triggered, this, &MainWindow::onConnectAllAction);

    auto* disconnectAllAction = menu->addAction(tr("&Disconnect all"));
    connect(disconnectAllAction, &QAction::triggered, this, &MainWindow::onDisconnectAllAction);

    // Connection list dock (left side).
    connectionList_ = new signalforge::connection::ConnectionListWidget(connectionManager_.get(), this);
    connectionDock_ = new QDockWidget(tr("Connections"), this);
    connectionDock_->setWidget(connectionList_);
    connectionDock_->setObjectName(QStringLiteral("connectionsDock"));
    addDockWidget(Qt::LeftDockWidgetArea, connectionDock_);

    connect(connectionList_, &signalforge::connection::ConnectionListWidget::addRequested, this,
            &MainWindow::onAddConnectionRequested);
    connect(connectionList_, &signalforge::connection::ConnectionListWidget::editRequested, this,
            &MainWindow::onEditConnectionRequested);

    // Connection status widget in the status bar.
    connectionStatus_ = new signalforge::connection::ConnectionStatusWidget(connectionManager_.get(), this);
    statusBar()->addPermanentWidget(connectionStatus_);
    connect(connectionStatus_, &signalforge::connection::ConnectionStatusWidget::clicked, connectionDock_,
            &QDockWidget::raise);

    // After loading connections, refresh the SignalSelector tree
    // so any newly-arriving decoder signals appear in the chart's
    // selector. Per spec §4.2 we also re-run this on every
    // connection state change.
    connect(connectionManager_.get(), &signalforge::connection::ConnectionManager::connectionStateChanged, this,
            [this](const QString&, signalforge::connection::Connection::State) {
                if (signalSelector_ != nullptr) {
                    signalSelector_->refresh();
                }
            });
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
    if (chartManager_ == nullptr || signalId.isEmpty()) {
        return false;
    }
    const auto ids = chartManager_->chartIds();
    if (ids.isEmpty()) {
        SF_LOG_ERROR("MainWindow: autoSelectSignal: no charts to attach signal '{}'", signalId.toStdString());
        return false;
    }
    auto* chart = chartManager_->chart(ids.first());
    if (chart == nullptr) {
        return false;
    }
    chart->addSignal(signalId);
    SF_LOG_INFO("MainWindow: autoSelectSignal: '{}' added to chart '{}'", signalId.toStdString(),
                ids.first().toStdString());
    return true;
}

QImage MainWindow::grabChartImage() const {
    if (chartContainer_ == nullptr) {
        return {};
    }
    const auto widgets = chartContainer_->findChildren<QQuickWidget*>();
    if (widgets.isEmpty()) {
        SF_LOG_WARN("MainWindow::grabChartImage: no QQuickWidget children of chartContainer_");
        return {};
    }
    auto* qqw = widgets.first();
    SF_LOG_INFO("MainWindow::grabChartImage: QQuickWidget size={}x{} root={}", qqw->width(), qqw->height(),
                qqw->rootObject() == nullptr ? "null" : "ok");
    if (chartManager_ != nullptr) {
        for (const auto& cid : chartManager_->chartIds()) {
            auto* c = chartManager_->chart(cid);
            if (c == nullptr) {
                continue;
            }
            const auto sigs = c->visibleSignals();
            const auto st = c->stats();
            SF_LOG_INFO("MainWindow::grabChartImage: chart='{}' size={}x{} pos=({},{}) "
                        "visible={} enabled={} opacity={} signals={} redraws={} dropped={}",
                        cid.toStdString(), c->width(), c->height(), c->x(), c->y(), c->isVisible(), c->isEnabled(),
                        c->opacity(), sigs.size(), st.totalRedraws, st.droppedFrames);
            // Defensive: ensure visible + opacity 1 in case parent QML scene
            // didn't propagate. Diagnostic only — won't change anything if
            // already correct.
            if (!c->isVisible() || c->opacity() < 0.99) {
                c->setVisible(true);
                c->setOpacity(1.0);
                SF_LOG_INFO("MainWindow::grabChartImage: forced visible+opacity for chart '{}'", cid.toStdString());
            }
        }
        if (signalBufferRegistry_ != nullptr) {
            SF_LOG_INFO("MainWindow::grabChartImage: SignalBufferRegistry has {} signal id(s)",
                        signalBufferRegistry_->signalIds().size());
        }
    }
    // First, prefer grabFramebuffer() — it captures the QQuickWidget's
    // RHI scene-graph output. Force a synchronous update + event
    // processing pass so the most recent chart redraw lands in the
    // framebuffer before we grab.
    qqw->update();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    QImage img = qqw->grabFramebuffer();
    bool useFramebuffer = !img.isNull() && img.width() > 0 && img.height() > 0;
    if (useFramebuffer) {
        // Sanity-check: framebuffer must contain non-clear pixels OR we
        // should not trust it (some platform plugins return a successful
        // but stale/white image even when scene graph rendered).
        // We always log the framebuffer count separately so the harness
        // sees both paths.
        std::uint64_t fbNonWhite = 0;
        for (int y = 0; y < img.height(); ++y) {
            for (int x = 0; x < img.width(); ++x) {
                const QRgb px = img.pixel(x, y);
                if (qRed(px) != 255 || qGreen(px) != 255 || qBlue(px) != 255) {
                    ++fbNonWhite;
                }
            }
        }
        SF_LOG_INFO("MainWindow::grabChartImage: framebuffer path: non_white={} size={}x{}", fbNonWhite, img.width(),
                    img.height());
        if (fbNonWhite == 0) {
            // Framebuffer returned all-white; fall through to QWidget::grab().
            useFramebuffer = false;
        }
    }
    if (!useFramebuffer) {
        SF_LOG_INFO("MainWindow::grabChartImage: falling back to QWidget::grab()");
        const QPixmap pm = qqw->grab();
        img = pm.toImage();
    }
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

void MainWindow::onAddChart() {
    if (chartManager_ == nullptr) {
        return;
    }
    const QString chartId = chartManager_->createChart();
    rebuildChartWidgets();
    SF_LOG_INFO("MainWindow: created chart {}", chartId.toStdString());
}

void MainWindow::rebuildChartWidgets() {
    if (chartLayout_ == nullptr || chartManager_ == nullptr) {
        return;
    }
    while (auto* item = chartLayout_->takeAt(0)) {
        if (auto* w = item->widget()) {
            w->deleteLater();
        }
        delete item;
    }
    for (const auto& id : chartManager_->chartIds()) {
        auto* chart = chartManager_->chart(id);
        if (chart == nullptr) {
            continue;
        }
        // ADR-010: load the QML host scene so QQuickWidget exposes a
        // non-null rootObject(). Without it, setParentItem(nullptr)
        // orphans Chart and nothing renders. setSource is synchronous
        // for qrc:/ URLs, so rootObject() is ready immediately.
        auto* hostWidget = new QQuickWidget(chartContainer_);
        hostWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
        // QQuickWidget's default size policy is Preferred/Preferred and
        // its sizeHint() can be invalid until a scene is loaded; under
        // a QVBoxLayout that means the layout gives it 0 width even
        // with a stretch factor. Force Expanding/Expanding so the
        // layout fills the chart pane horizontally + vertically.
        hostWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        hostWidget->setMinimumSize(1, 1);
        hostWidget->setSource(QUrl(QStringLiteral("qrc:/qml/ChartHost.qml")));
        if (hostWidget->status() != QQuickWidget::Ready) {
            SF_LOG_ERROR("MainWindow: ChartHost.qml failed to load (status={})",
                         static_cast<int>(hostWidget->status()));
            hostWidget->deleteLater();
            continue;
        }
        auto* root = hostWidget->rootObject();
        if (root == nullptr) {
            SF_LOG_ERROR("MainWindow: ChartHost.qml loaded but rootObject() is null");
            hostWidget->deleteLater();
            continue;
        }
        chart->setParentItem(root);
        // ADR-011: bind Chart's geometry to the host scene root.
        // QQuickWidget::SizeRootObjectToView keeps `root` matched to the
        // widget; ChartHost.qml's anchors.fill keeps the QML scene matched
        // to root. The C++ chart child does NOT inherit either — it lands
        // at the QQuickItem default 0×0 unless we bind explicitly here.
        const auto syncSize = [chart, root]() { chart->setSize(QSizeF(root->width(), root->height())); };
        syncSize();
        connect(root, &QQuickItem::widthChanged, chart, syncSize);
        connect(root, &QQuickItem::heightChanged, chart, syncSize);
        chartLayout_->addWidget(hostWidget, 1);
    }
}

void MainWindow::onLiveToggleChanged(bool live) {
    if (chartManager_ == nullptr) {
        return;
    }
    auto& axis = chartManager_->timeAxis();
    if (live) {
        axis.resume();
        liveToggle_->setText(tr("● Live"));
    } else {
        axis.pause();
        liveToggle_->setText(tr("⏸ Paused"));
    }
}

void MainWindow::onTimePresetChanged(int index) {
    if (chartManager_ == nullptr) {
        return;
    }
    using TP = signalforge::chart::TimeAxisManager::TimePreset;
    auto& axis = chartManager_->timeAxis();
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
    if (chartManager_ == nullptr) {
        return;
    }
    std::uint64_t totalRedraws = 0;
    std::uint64_t totalDropped = 0;
    int chartCount = 0;
    for (const auto& id : chartManager_->chartIds()) {
        auto* chart = chartManager_->chart(id);
        if (chart == nullptr) {
            continue;
        }
        const auto stats = chart->stats();
        totalRedraws += stats.totalRedraws;
        totalDropped += stats.droppedFrames;
        ++chartCount;
    }
    if (fpsLabel_ != nullptr) {
        if (chartCount > 0) {
            fpsLabel_->setText(tr("FPS: ~%1 / chart").arg(30));
        } else {
            fpsLabel_->setText(tr("FPS: -"));
        }
    }
    if (droppedLabel_ != nullptr) {
        droppedLabel_->setText(tr("Dropped: %1").arg(totalDropped));
    }
    if (throttledLabel_ != nullptr) {
        throttledLabel_->setText(totalDropped > 0 ? tr("⚠ throttled") : QString{});
    }
    // M14 F15: surface signal_buffer budget pressure to the user. Pre-fix
    // the registry silently logged "registration rejected: would exceed
    // budget" without any UI signal — operator could only tell the chart
    // wasn't drawing some signals by reading the log file. Now the
    // status bar shows the percentage utilized and flips to a warning
    // tone above the 80 % soft threshold.
    if (bufferBudgetLabel_ != nullptr && signalBufferRegistry_ != nullptr) {
        const std::size_t used = signalBufferRegistry_->totalMemoryBytes();
        const std::size_t budget = signalBufferRegistry_->totalBudgetBytes();
        if (budget == 0) {
            bufferBudgetLabel_->setText(QString{});
        } else {
            const int pct = static_cast<int>((100ULL * used) / budget);
            QString text;
            if (pct >= 100) {
                text = tr("⚠ buffer FULL (%1 MiB / %2 MiB)").arg(used / (1024 * 1024)).arg(budget / (1024 * 1024));
            } else if (pct >= 80) {
                text = tr("⚠ buffer %1%% (%2 / %3 MiB)").arg(pct).arg(used / (1024 * 1024)).arg(budget / (1024 * 1024));
            } else {
                text = tr("buffer %1%% (%2 MiB)").arg(pct).arg(used / (1024 * 1024));
            }
            bufferBudgetLabel_->setText(text);
        }
    }
    if (signalSelector_ != nullptr) {
        signalSelector_->refresh();
    }
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
    QMainWindow::closeEvent(event);
}

void MainWindow::buildSessionUi() {
    // "Session" menu with the Record action.
    auto* menu = menuBar()->addMenu(tr("&Session"));
    recordAction_ = menu->addAction(tr("&Record…"));
    recordAction_->setShortcut(QKeySequence(tr("Ctrl+R")));
    connect(recordAction_, &QAction::triggered, this, &MainWindow::onRecordToggle);

    recordingStatusLabel_ = new QLabel;
    recordingStatusLabel_->setText(tr("Idle"));
    recordingStatusLabel_->setToolTip(tr("Session recording status"));
    statusBar()->addPermanentWidget(recordingStatusLabel_);

    if (sessionWriter_ != nullptr) {
        connect(sessionWriter_.get(), &signalforge::session::SessionWriter::flushed, this,
                &MainWindow::onRecordingFlushed);
        connect(sessionWriter_.get(), &signalforge::session::SessionWriter::errorOccurred, this,
                &MainWindow::onRecordingError);
    }
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
        recordAction_->setText(tr("&Record…"));
        if (recordingStatusLabel_ != nullptr) {
            recordingStatusLabel_->setText(tr("Stopped (%1 bytes)").arg(bytes));
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
    if (!sessionWriter_->start(path)) {
        QMessageBox::critical(this, tr("Recording failed"), tr("Could not start recording: see log for details."));
        return;
    }
    // ADR-013 (F6): SessionWriter is already subscribed to TeeSink at
    // MainWindow ctor — no per-recording addSink needed.
    currentRecordingPath_ = path;
    recordAction_->setText(tr("&Stop recording"));
    if (recordingStatusLabel_ != nullptr) {
        recordingStatusLabel_->setText(tr("● Recording: %1 (0 bytes)").arg(QFileInfo(path).fileName()));
    }
    SF_LOG_INFO("MainWindow: recording started -> {}", path.toStdString());
}

void MainWindow::onRecordingFlushed(std::size_t bytes) {
    if (recordingStatusLabel_ == nullptr || currentRecordingPath_.isEmpty()) {
        return;
    }
    recordingStatusLabel_->setText(
        tr("● Recording: %1 (%2 bytes)").arg(QFileInfo(currentRecordingPath_).fileName()).arg(bytes));
}

void MainWindow::onRecordingError(const QString& message) {
    if (recordingStatusLabel_ != nullptr) {
        recordingStatusLabel_->setText(tr("Recording error"));
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
    // File → Open Session… (Ctrl+O).
    auto* fileMenu = menuBar()->addMenu(tr("&File"));
    openSessionAction_ = fileMenu->addAction(tr("&Open Session…"));
    openSessionAction_->setShortcut(QKeySequence(tr("Ctrl+O")));
    connect(openSessionAction_, &QAction::triggered, this, &MainWindow::onOpenSessionRequested);

    // M14 F18: File → Quit (Ctrl+Q on Linux via QKeySequence::Quit).
    // Pre-fix the only exit path was the window-X button; M13
    // protocol §Test 9 ("Quit-while-recording prompt") could not be
    // initiated via keyboard. close() routes through the existing
    // closeEvent() handler so the recording-in-progress prompt + any
    // future aboutToQuit-bound persistence still fires (ADR-013 F17
    // defense-in-depth).
    fileMenu->addSeparator();
    auto* quitAction = fileMenu->addAction(tr("&Quit"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &MainWindow::close);

    // Replay toolbar — initially hidden; visible only in Replay mode.
    replayToolbar_ = addToolBar(tr("Replay"));
    replayToolbar_->setMovable(false);
    replayToolbar_->setVisible(false);

    replayPlayPauseAction_ = replayToolbar_->addAction(tr("Play"));
    connect(replayPlayPauseAction_, &QAction::triggered, this, &MainWindow::onReplayPlayPause);

    replayStepBackAction_ = replayToolbar_->addAction(tr("◀ Step"));
    connect(replayStepBackAction_, &QAction::triggered, this, &MainWindow::onReplayStepBackward);

    replayStepForwardAction_ = replayToolbar_->addAction(tr("Step ▶"));
    connect(replayStepForwardAction_, &QAction::triggered, this, &MainWindow::onReplayStepForward);

    replaySeekSlider_ = new QSlider(Qt::Horizontal, replayToolbar_);
    replaySeekSlider_->setMinimumWidth(200);
    replaySeekSlider_->setRange(0, 0);
    connect(replaySeekSlider_, &QSlider::valueChanged, this, &MainWindow::onReplaySeekSliderChanged);
    replayToolbar_->addWidget(replaySeekSlider_);

    replaySpeedCombo_ = new QComboBox(replayToolbar_);
    replaySpeedCombo_->addItem(tr("0.5×"), 0.5);
    replaySpeedCombo_->addItem(tr("1×"), 1.0);
    replaySpeedCombo_->addItem(tr("2×"), 2.0);
    replaySpeedCombo_->addItem(tr("5×"), 5.0);
    replaySpeedCombo_->addItem(tr("10×"), 10.0);
    replaySpeedCombo_->setCurrentIndex(1);
    connect(replaySpeedCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this,
            &MainWindow::onReplaySpeedChanged);
    replayToolbar_->addWidget(replaySpeedCombo_);

    replayExitAction_ = replayToolbar_->addAction(tr("Exit Replay"));
    connect(replayExitAction_, &QAction::triggered, this, &MainWindow::onExitReplayRequested);

    // Status-bar replay info.
    replayStatusLabel_ = new QLabel;
    replayStatusLabel_->setText(tr(""));
    statusBar()->addPermanentWidget(replayStatusLabel_);

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
            [this](signalforge::replay::AppMode) { updateReplayActionStates(); });

    updateReplayActionStates();
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
        replayStatusLabel_->setText(tr("Replay: %1").arg(QFileInfo(path).fileName()));
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
    (void)playbackController_->seek(target);
}

void MainWindow::onReplaySpeedChanged(int index) {
    if (playbackController_ == nullptr || replaySpeedCombo_ == nullptr) {
        return;
    }
    const double speed = replaySpeedCombo_->itemData(index).toDouble();
    (void)playbackController_->setSpeed(speed);
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
        replayStatusLabel_->clear();
    }
    updateReplayActionStates();
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
        replayStatusLabel_->setText(tr("Replay: %1 | %2 / %3 | %4 / %5 records")
                                        .arg(filename)
                                        .arg(formatReplayPosition(timestampNs))
                                        .arg(formatReplayPosition(playbackController_->durationNs()))
                                        .arg(recordIndex)
                                        .arg(playbackController_->totalRecords()));
    }
}

void MainWindow::onReplayStateChanged() {
    updateReplayActionStates();
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
    replayPlayPauseAction_->setText(st == S::Playing ? tr("Pause") : tr("Play"));
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
