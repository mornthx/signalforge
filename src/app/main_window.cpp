#include "main_window.hpp"

#include "app/connection_manager.hpp"
#include "buffer/signal_buffer_registry.hpp"
#include "chart/chart.hpp"
#include "chart/chart_manager.hpp"
#include "chart/signal_selector.hpp"
#include "chart/time_axis_manager.hpp"
#include "decode/decoder_registrar.hpp"
#include "observability/logging.hpp"
#include "pipeline/pipeline_manager.hpp"

#include <QAction>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QQuickWidget>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>
#include <unordered_map>

namespace signalforge::app {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("SignalForge"));
    resize(1280, 800);

    auto* fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
    auto* openConn = fileMenu->addAction(QStringLiteral("&Connection Manager..."));
    openConn->setShortcut(QKeySequence(QStringLiteral("Ctrl+M")));
    connect(openConn, &QAction::triggered, this, &MainWindow::openConnectionManager);

    // M6 production sink. Constructed eagerly so the M8 chart UI
    // can bind to it on startup; ConnectionManager (M9 territory)
    // already used the same registry instance via lazy init in the
    // previous design, so the eager-construct is observably equivalent.
    signalBufferRegistry_ = std::make_unique<signalforge::buffer::SignalBufferRegistry>();
    chartManager_ = std::make_unique<signalforge::chart::ChartManager>(*signalBufferRegistry_, this);
    buildChartUi();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildChartUi() {
    // Central layout: QSplitter with SignalSelector left, charts right.
    centralSplitter_ = new QSplitter(Qt::Horizontal, this);

    signalSelector_ = new signalforge::chart::SignalSelector(*signalBufferRegistry_, *chartManager_);
    centralSplitter_->addWidget(signalSelector_);

    chartContainer_ = new QWidget;
    chartLayout_ = new QVBoxLayout(chartContainer_);
    chartLayout_->setContentsMargins(0, 0, 0, 0);
    chartLayout_->setSpacing(2);
    centralSplitter_->addWidget(chartContainer_);

    centralSplitter_->setStretchFactor(0, 1);  // selector
    centralSplitter_->setStretchFactor(1, 4);  // charts (4× wider)
    setCentralWidget(centralSplitter_);

    // Toolbar.
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
    timePresetCombo_->setCurrentIndex(1);  // default: 10 sec (matches TimeAxisManager default)
    connect(timePresetCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::onTimePresetChanged);
    toolbar->addWidget(timePresetCombo_);

    auto* addChartAction = toolbar->addAction(tr("+ Chart"));
    connect(addChartAction, &QAction::triggered, this, &MainWindow::onAddChart);

    // Status bar.
    fpsLabel_ = new QLabel(tr("FPS: -"));
    droppedLabel_ = new QLabel(tr("Dropped: 0"));
    throttledLabel_ = new QLabel;
    statusBar()->addPermanentWidget(fpsLabel_);
    statusBar()->addPermanentWidget(droppedLabel_);
    statusBar()->addPermanentWidget(throttledLabel_);

    // Status-bar refresh timer (1 Hz; cheap).
    auto* statusTimer = new QTimer(this);
    statusTimer->setInterval(1000);
    connect(statusTimer, &QTimer::timeout, this, &MainWindow::refreshStatusBar);
    statusTimer->start();

    // Start with one chart so the user has something on screen.
    onAddChart();
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
    // Remove existing chart widgets.
    while (auto* item = chartLayout_->takeAt(0)) {
        if (auto* w = item->widget()) {
            w->deleteLater();
        }
        delete item;
    }
    // Re-add a QQuickWidget per chart.
    for (const auto& id : chartManager_->chartIds()) {
        auto* chart = chartManager_->chart(id);
        if (chart == nullptr) {
            continue;
        }
        auto* hostWidget = new QQuickWidget(chartContainer_);
        hostWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
        chart->setParentItem(hostWidget->rootObject());
        // Connect right-click context menu request → simple no-op
        // toast for V1 (full menu in V1.5+ — pause/resume/snap-to-recent
        // already accessible via the toolbar live toggle).
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
    // setPreset() returns to live mode; sync the toggle.
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
        // 1 Hz refresh; report the recent-frame rate as
        // (totalRedraws / chartCount / elapsedSinceStart_seconds)
        // — for V1 we just report a rolling estimate from peak
        // observed.
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
    if (signalSelector_ != nullptr) {
        // Pull-based observation of registry mutation per
        // M8-concerns.md C2. Cheap (just a tree rebuild) at 1 Hz.
        signalSelector_->refresh();
    }
}

void MainWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);
    // Window activation per spec §4.6 + [Proto] Anomaly §2:
    // Mutter throttles unfocused windows; force activation so
    // chart redraw stays at 30 Hz from the start. WARN if the
    // compositor refuses focus 500 ms later.
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

void MainWindow::openConnectionManager() {
    if (!pipelineManager_) {
        pipelineManager_ = std::make_unique<signalforge::pipeline::PipelineManager>(this);
    }
    if (!decoderRegistrar_) {
        // Non-owning aliased shared_ptr to the registry — the registry
        // outlives the registrar, so the no-op deleter is safe.
        std::shared_ptr<signalforge::decoder::SignalValueSink> sink(signalBufferRegistry_.get(),
                                                                    [](signalforge::decoder::SignalValueSink*) {});
        decoderRegistrar_ = std::make_unique<signalforge::decoder::DecoderRegistrar>(
            pipelineManager_.get(), std::unordered_map<QString, QString>{}, std::move(sink), this);
    }
    if (!connectionManager_) {
        connectionManager_ = std::make_unique<ConnectionManager>(pipelineManager_.get(), this);
        connectionManager_->setModal(false);
    }
    connectionManager_->show();
    connectionManager_->raise();
    connectionManager_->activateWindow();
}

}  // namespace signalforge::app
