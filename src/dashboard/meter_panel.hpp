// src/dashboard/meter_panel.hpp
#pragma once

#include "dashboard/panel.hpp"

#include <optional>

namespace signalforge::buffer {
class SignalBufferRegistry;
}

namespace signalforge::dashboard {

class MeterView;

/// Single-signal panel showing a scalar against a range, as a Bar or a
/// Gauge (chosen from `config.type`). Range is the per-panel
/// `rangeMin`/`rangeMax` if set, else the signal's observed min/max
/// (design §2.1). Driven by the Dashboard refresh tick.
class MeterPanel : public Panel {
    Q_OBJECT

public:
    MeterPanel(PanelConfig config, signalforge::buffer::SignalBufferRegistry& registry, QWidget* parent = nullptr);
    ~MeterPanel() override;

    void refresh() override;

    // --- test / diagnostic accessors ---
    [[nodiscard]] double displayValue() const noexcept {
        return value_;
    }
    [[nodiscard]] bool hasData() const noexcept {
        return hasData_;
    }
    [[nodiscard]] double rangeLo() const noexcept {
        return lo_;
    }
    [[nodiscard]] double rangeHi() const noexcept {
        return hi_;
    }

private:
    [[nodiscard]] QString boundSignalId() const;

    signalforge::buffer::SignalBufferRegistry* registry_;
    MeterView* meter_ = nullptr;
    std::optional<double> obsMin_;
    std::optional<double> obsMax_;
    double value_ = 0.0;
    double lo_ = 0.0;
    double hi_ = 1.0;
    bool hasData_ = false;
};

}  // namespace signalforge::dashboard
