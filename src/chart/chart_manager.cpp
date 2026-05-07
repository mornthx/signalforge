// src/chart/chart_manager.cpp
//
// S1 ctor/dtor stubs. Full implementation lands in S5 (createChart /
// removeChart / activeChartId tracking) and S7 (yaml save/load).

#include "chart/chart_manager.hpp"

namespace signalforge::chart {

ChartManager::ChartManager(signalforge::buffer::SignalBufferRegistry& registry, QObject* parent)
    : QObject(parent), registry_(&registry), timeAxis_(std::make_unique<TimeAxisManager>(this)) {}

ChartManager::~ChartManager() = default;

QString ChartManager::createChart(const ChartConfig& /*config*/) {
    // S5 implementation.
    return {};
}

bool ChartManager::removeChart(const QString& /*chartId*/) {
    // S5 implementation.
    return false;
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

void ChartManager::setActiveChartId(const QString& /*chartId*/) {
    // S5 implementation.
}

TimeAxisManager& ChartManager::timeAxis() {
    return *timeAxis_;
}

void ChartManager::saveConfigToFile(const QString& /*path*/) const {
    // S7 implementation.
}

bool ChartManager::loadConfigFromFile(const QString& /*path*/) {
    // S7 implementation.
    return false;
}

}  // namespace signalforge::chart
