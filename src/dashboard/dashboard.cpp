// src/dashboard/dashboard.cpp

#include "dashboard/dashboard.hpp"

#include "buffer/signal_buffer.hpp"
#include "buffer/signal_buffer_registry.hpp"
#include "chart/chart_manager.hpp"
#include "dashboard/numeric_panel.hpp"
#include "dashboard/panel.hpp"
#include "dashboard/panel_factory.hpp"
#include "dashboard/plot_panel.hpp"
#include "dashboard/state_panel.hpp"
#include "decode/decoder_interface.hpp"

#include <QGridLayout>
#include <QLayoutItem>
#include <QTimer>

namespace signalforge::dashboard {

namespace {
constexpr int kRefreshIntervalMs = 66;  ///< ~15 Hz; Numeric/State cards.
}

Dashboard::Dashboard(signalforge::buffer::SignalBufferRegistry& registry,
                     signalforge::chart::ChartManager& chartManager, QWidget* parent)
    : QWidget(parent), registry_(&registry), chartManager_(&chartManager) {
    setObjectName(QStringLiteral("dashboardSurface"));
    grid_ = new QGridLayout(this);
    grid_->setContentsMargins(0, 0, 0, 0);
    grid_->setSpacing(2);

    refreshTimer_ = new QTimer(this);
    refreshTimer_->setInterval(kRefreshIntervalMs);
    connect(refreshTimer_, &QTimer::timeout, this, &Dashboard::refreshAll);
    refreshTimer_->start();
}

Dashboard::~Dashboard() = default;

QString Dashboard::nextPanelId() {
    QString id;
    do {
        id = QStringLiteral("panel-%1").arg(nextPanelSuffix_++);
    } while (panels_.contains(id));
    return id;
}

Panel* Dashboard::panel(const QString& panelId) const {
    return panels_.value(panelId, nullptr);
}

QString Dashboard::addPanel(PanelConfig config) {
    if (config.id.isEmpty() || panels_.contains(config.id)) {
        config.id = nextPanelId();
    }
    const QString id = config.id;

    Panel* created = nullptr;
    switch (config.type) {
    case PanelType::Numeric:
        created = new NumericPanel(config, *registry_, this);
        break;
    case PanelType::State:
        created = new StatePanel(config, *registry_, this);
        break;
    case PanelType::Plot: {
        signalforge::chart::ChartConfig chartCfg;
        chartCfg.title = config.title;
        const QString chartId = chartManager_->createChart(chartCfg);
        chartManager_->setActiveChartId(chartId);
        auto* chart = chartManager_->chart(chartId);
        created = new PlotPanel(config, chart, this);
        plotChartIds_.insert(id, chartId);
        break;
    }
    }

    created->setEditMode(editMode_);
    connect(created, &Panel::removeRequested, this, &Dashboard::removePanel);
    panels_.insert(id, created);
    panelOrder_.append(id);
    relayout();
    Q_EMIT panelsChanged();
    return id;
}

QString Dashboard::addSignal(const QString& signalId) {
    if (signalId.isEmpty()) {
        return {};
    }
    // Already shown somewhere?
    for (const QString& id : panelOrder_) {
        if (panels_.value(id)->hasSignal(signalId)) {
            return id;
        }
    }
    PanelType type = PanelType::Numeric;
    if (auto* buf = registry_->bufferFor(signalId); buf != nullptr) {
        type = suggestPanelType(buf->metadata().type);
    }
    PanelConfig cfg;
    cfg.type = type;
    cfg.signalIds << signalId;
    return addPanel(std::move(cfg));
}

QString Dashboard::addPlotPanel() {
    PanelConfig cfg;
    cfg.type = PanelType::Plot;
    return addPanel(std::move(cfg));
}

void Dashboard::removeSignalEverywhere(const QString& signalId) {
    // Copy the order: removePanel mutates panelOrder_.
    const QStringList order = panelOrder_;
    for (const QString& id : order) {
        Panel* p = panels_.value(id, nullptr);
        if (p == nullptr || !p->hasSignal(signalId)) {
            continue;
        }
        if (p->type() == PanelType::Plot) {
            static_cast<PlotPanel*>(p)->removeSignal(signalId);
        } else {
            // Single-signal card: removing its signal removes the card.
            removePanel(id);
        }
    }
}

void Dashboard::removePanel(const QString& panelId) {
    auto it = panels_.find(panelId);
    if (it == panels_.end()) {
        return;
    }
    Panel* p = it.value();
    panels_.erase(it);
    panelOrder_.removeAll(panelId);

    if (p->type() == PanelType::Plot) {
        // Detach the chart from the panel, then delete the chart, so the
        // panel's QQuickWidget never touches a deleted chart.
        static_cast<PlotPanel*>(p)->detachChart();
        const QString chartId = plotChartIds_.take(panelId);
        if (!chartId.isEmpty()) {
            (void)chartManager_->removeChart(chartId);
        }
    }
    p->deleteLater();
    relayout();
    Q_EMIT panelsChanged();
}

void Dashboard::setEditMode(bool on) {
    editMode_ = on;
    for (const QString& id : panelOrder_) {
        panels_.value(id)->setEditMode(on);
    }
}

void Dashboard::refreshAll() {
    for (const QString& id : panelOrder_) {
        panels_.value(id)->refresh();
    }
}

void Dashboard::relayout() {
    // Detach every panel from the grid without deleting it.
    while (QLayoutItem* item = grid_->takeAt(0)) {
        delete item;  // frees the layout item, not the widget.
    }

    int row = 0;
    int col = 0;
    for (const QString& id : panelOrder_) {
        Panel* p = panels_.value(id);
        if (p->isWide()) {
            if (col != 0) {
                ++row;
                col = 0;
            }
            grid_->addWidget(p, row, 0, 1, columns_);
            ++row;
        } else {
            grid_->addWidget(p, row, col, 1, 1);
            if (++col >= columns_) {
                ++row;
                col = 0;
            }
        }
    }
    // Push panels to the top-left; trailing row absorbs slack.
    grid_->setRowStretch(row + 1, 1);
}

}  // namespace signalforge::dashboard
