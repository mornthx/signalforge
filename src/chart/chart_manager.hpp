// src/chart/chart_manager.hpp
#pragma once

#include "buffer/signal_buffer_registry.hpp"
#include "chart/chart.hpp"
#include "chart/time_axis_manager.hpp"

#include <QObject>
#include <QString>
#include <QStringList>
#include <memory>
#include <unordered_map>

namespace signalforge::chart {

/// Owns N `Chart` instances + the global `TimeAxisManager`. Tracks
/// `activeChartId` (last-clicked chart) used by `SignalSelector` to
/// route checkbox toggles. Provides yaml save/load.
///
/// Threading: lives on the main thread. Chart pointers are aliased
/// out via `chart(id)`; lifetime is bound to this manager.
///
/// Freeze scope: this class is frozen at M8 close. See spec §6.1.
class ChartManager : public QObject {
    Q_OBJECT

public:
    explicit ChartManager(signalforge::buffer::SignalBufferRegistry& registry, QObject* parent = nullptr);
    ~ChartManager() override;

    ChartManager(const ChartManager&) = delete;
    ChartManager& operator=(const ChartManager&) = delete;
    ChartManager(ChartManager&&) = delete;
    ChartManager& operator=(ChartManager&&) = delete;

    /// Create a new chart. Returns a generated unique chart id (e.g.
    /// `chart-1`). If `config.id` is non-empty it is honored as long
    /// as it does not collide; on collision a suffix is appended.
    [[nodiscard]] QString createChart(const ChartConfig& config = {});

    /// Remove a chart by id. Returns `false` if id not found.
    bool removeChart(const QString& chartId);

    /// Look up chart by id; returns nullptr if not found.
    [[nodiscard]] Chart* chart(const QString& chartId) const;

    /// All chart ids in creation order.
    [[nodiscard]] QStringList chartIds() const;

    /// Active chart id (last clicked; used by `SignalSelector`).
    [[nodiscard]] QString activeChartId() const;

    /// Set the active chart id. Emits `activeChartChanged` if it
    /// changed. No-op if `chartId` is not a known chart.
    void setActiveChartId(const QString& chartId);

    /// Shared time axis (by reference; lifetime is the manager's).
    [[nodiscard]] TimeAxisManager& timeAxis();

    /// Save all charts' configs to `path` as yaml.
    void saveConfigToFile(const QString& path) const;

    /// Load charts from `path`. On parse error or missing file,
    /// returns false and leaves the manager unchanged. ERROR is
    /// logged on parse error.
    [[nodiscard]] bool loadConfigFromFile(const QString& path);

Q_SIGNALS:
    void chartCreated(const QString& chartId);
    void chartRemoved(const QString& chartId);
    void activeChartChanged(const QString& chartId);

private:
    signalforge::buffer::SignalBufferRegistry* registry_;
    std::unique_ptr<TimeAxisManager> timeAxis_;
    std::unordered_map<QString, std::unique_ptr<Chart>> charts_;
    QStringList chartOrder_;  ///< Insertion order for `chartIds()`.
    QString activeChartId_;
    int nextChartIdSuffix_ = 1;
};

}  // namespace signalforge::chart
