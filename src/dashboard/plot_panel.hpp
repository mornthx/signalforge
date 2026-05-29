// src/dashboard/plot_panel.hpp
#pragma once

#include "dashboard/panel.hpp"

class QQuickWidget;

namespace signalforge::chart {
class Chart;
}

namespace signalforge::dashboard {

/// Panel hosting a time-series trend. P0 wraps the legacy
/// `chart::Chart` (a QQuickItem) inside a `QQuickWidget` — the hosting
/// logic that previously lived in `MainWindow::rebuildChartWidgets`.
///
/// Ownership: the `Chart` is owned by the caller's `ChartManager`, NOT
/// by this panel. PlotPanel only hosts it visually; the destructor
/// detaches it (`setParentItem(nullptr)`) so destroying the panel's
/// QQuickWidget never deletes the manager-owned chart.
class PlotPanel : public Panel {
    Q_OBJECT

public:
    PlotPanel(PanelConfig config, signalforge::chart::Chart* chart, QWidget* parent = nullptr);
    ~PlotPanel() override;

    /// Plot occupies a full grid row.
    [[nodiscard]] bool isWide() const override {
        return true;
    }

    /// Add a signal to the underlying chart and the panel binding.
    void addSignal(const QString& signalId);

    /// Remove a signal from the underlying chart and the panel binding.
    void removeSignal(const QString& signalId);

    /// Detach the hosted chart (visually unparent it and drop our
    /// pointer) so the owner can safely delete the chart afterwards.
    /// Called by `Dashboard::removePanel` before `ChartManager::removeChart`.
    void detachChart();

    /// The hosted chart (not owned).
    [[nodiscard]] signalforge::chart::Chart* chart() const noexcept {
        return chart_;
    }

private:
    signalforge::chart::Chart* chart_;
    QQuickWidget* host_ = nullptr;
};

}  // namespace signalforge::dashboard
