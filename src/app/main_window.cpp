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

#include <QAction>
#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
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

    pipelineManager_ = std::make_unique<signalforge::pipeline::PipelineManager>(this);
    signalBufferRegistry_ = std::make_unique<signalforge::buffer::SignalBufferRegistry>();
    {
        std::shared_ptr<signalforge::decoder::SignalValueSink> sink(signalBufferRegistry_.get(),
                                                                    [](signalforge::decoder::SignalValueSink*) {});
        decoderRegistrar_ = std::make_unique<signalforge::decoder::DecoderRegistrar>(
            pipelineManager_.get(), std::unordered_map<QString, QString>{}, std::move(sink), this);
    }
    connectionManager_ = std::make_unique<signalforge::connection::ConnectionManager>(*decoderRegistrar_, this);

    // Auto-load the persisted connection list. Missing file is
    // benign (first launch); ERROR-level parse failures degrade
    // to an empty manager (logged by ConnectionManager).
    const QString cfgPath = signalforge::connection::ConnectionManager::defaultConfigPath();
    QDir().mkpath(QFileInfo(cfgPath).absolutePath());
    (void)connectionManager_->loadConfigFile(cfgPath);

    chartManager_ = std::make_unique<signalforge::chart::ChartManager>(*signalBufferRegistry_, this);

    buildChartUi();
    buildConnectionUi();
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
    statusBar()->addPermanentWidget(fpsLabel_);
    statusBar()->addPermanentWidget(droppedLabel_);
    statusBar()->addPermanentWidget(throttledLabel_);

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
        auto* hostWidget = new QQuickWidget(chartContainer_);
        hostWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
        chart->setParentItem(hostWidget->rootObject());
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

}  // namespace signalforge::app
