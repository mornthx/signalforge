// src/chart/chart_manager.cpp
//
// S5 — create / remove / lookup + activeChartId tracking. yaml
// save/load mechanics are stubbed here and filled in by S7
// (the yaml schema is documented in `schemas/charts_v1.yaml` then).

#include "chart/chart_manager.hpp"

#include "observability/logging.hpp"

namespace signalforge::chart {

ChartManager::ChartManager(signalforge::buffer::SignalBufferRegistry& registry, QObject* parent)
    : QObject(parent), registry_(&registry), timeAxis_(std::make_unique<TimeAxisManager>(this)) {}

ChartManager::~ChartManager() = default;

QString ChartManager::createChart(const ChartConfig& config) {
    // Honor a non-empty `config.id` if it doesn't collide; otherwise
    // generate `chart-<n>` with a monotonic suffix.
    QString id = config.id;
    if (id.isEmpty() || charts_.contains(id)) {
        do {
            id = QStringLiteral("chart-%1").arg(nextChartIdSuffix_++);
        } while (charts_.contains(id));
    }
    auto chart = std::make_unique<Chart>(*registry_, *timeAxis_, config);
    auto* raw = chart.get();
    raw->setObjectName(id);

    ChartConfig cfg = config;
    cfg.id = id;
    raw->setConfig(std::move(cfg));

    charts_[id] = std::move(chart);
    chartOrder_.append(id);
    if (activeChartId_.isEmpty()) {
        activeChartId_ = id;
        Q_EMIT activeChartChanged(id);
    }
    Q_EMIT chartCreated(id);
    return id;
}

bool ChartManager::removeChart(const QString& chartId) {
    auto it = charts_.find(chartId);
    if (it == charts_.end()) {
        return false;
    }
    charts_.erase(it);
    chartOrder_.removeAll(chartId);
    if (activeChartId_ == chartId) {
        activeChartId_ = chartOrder_.isEmpty() ? QString{} : chartOrder_.first();
        Q_EMIT activeChartChanged(activeChartId_);
    }
    Q_EMIT chartRemoved(chartId);
    return true;
}

Chart* ChartManager::chart(const QString& chartId) const {
    auto it = charts_.find(chartId);
    return it == charts_.end() ? nullptr : it->second.get();
}

QStringList ChartManager::chartIds() const {
    return chartOrder_;
}

QString ChartManager::activeChartId() const {
    return activeChartId_;
}

void ChartManager::setActiveChartId(const QString& chartId) {
    if (chartId == activeChartId_) {
        return;
    }
    if (!charts_.contains(chartId)) {
        return;  // Defensive: ignore unknown ids.
    }
    activeChartId_ = chartId;
    Q_EMIT activeChartChanged(chartId);
}

TimeAxisManager& ChartManager::timeAxis() {
    return *timeAxis_;
}

void ChartManager::saveConfigToFile(const QString& path) const {
    // S7 implementation. For now, log a stub note so an integration
    // call won't silently fail without a trail.
    SF_LOG_WARN("ChartManager::saveConfigToFile is a stub (S7 deliverable); path={}", path.toStdString());
}

bool ChartManager::loadConfigFromFile(const QString& path) {
    // S7 implementation.
    SF_LOG_WARN("ChartManager::loadConfigFromFile is a stub (S7 deliverable); path={}", path.toStdString());
    return false;
}

}  // namespace signalforge::chart
