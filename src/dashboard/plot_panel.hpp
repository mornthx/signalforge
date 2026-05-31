// src/dashboard/plot_panel.hpp
#pragma once

#include "dashboard/panel.hpp"

namespace signalforge::buffer {
class SignalBufferRegistry;
}
namespace signalforge::chart {
class TimeAxisManager;
}

namespace signalforge::dashboard {

class PlotView;

/// Panel hosting a readable time-series trend. P2: backed by the new
/// QPainter `PlotView` (parallel to the frozen legacy `chart::Chart`,
/// which the app no longer uses). Multi-signal; the shared time window
/// comes from a `TimeAxisManager` owned by the Dashboard.
class PlotPanel : public Panel {
    Q_OBJECT

public:
    PlotPanel(PanelConfig config, signalforge::buffer::SignalBufferRegistry& registry,
              signalforge::chart::TimeAxisManager& timeAxis, QWidget* parent = nullptr);
    ~PlotPanel() override;

    [[nodiscard]] bool isWide() const override {
        return true;
    }
    [[nodiscard]] bool isMultiSignal() const override {
        return true;
    }

    void addSignal(const QString& signalId) override;
    void removeSignal(const QString& signalId) override;
    void refresh() override;
    void setSignalColorProvider(SignalColorProvider provider) override;

    /// The hosted plot view (not owned by callers).
    [[nodiscard]] PlotView* view() const noexcept {
        return view_;
    }

private:
    PlotView* view_ = nullptr;
};

}  // namespace signalforge::dashboard
