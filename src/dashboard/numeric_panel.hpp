// src/dashboard/numeric_panel.hpp
#pragma once

#include "dashboard/panel.hpp"

#include <optional>

class QLabel;

namespace signalforge::buffer {
class SignalBufferRegistry;
}

namespace signalforge::dashboard {

/// Panel showing a single signal's current value (large) with its
/// unit and a running observed min/max caption. Best for slow-changing
/// scalars ("what is X right now?"). Driven by the Dashboard refresh
/// tick via `refresh()`.
class NumericPanel : public Panel {
    Q_OBJECT

public:
    NumericPanel(PanelConfig config, signalforge::buffer::SignalBufferRegistry& registry, QWidget* parent = nullptr);
    ~NumericPanel() override;

    /// Re-read the bound signal's latest value and update the display.
    void refresh() override;

    /// Currently displayed value text (test/diagnostic accessor).
    [[nodiscard]] QString valueText() const;

    /// Currently displayed unit text.
    [[nodiscard]] QString unitText() const;

    /// Observed running min/max over numeric samples seen so far.
    [[nodiscard]] std::optional<double> observedMin() const noexcept {
        return observedMin_;
    }
    [[nodiscard]] std::optional<double> observedMax() const noexcept {
        return observedMax_;
    }

private:
    [[nodiscard]] QString boundSignalId() const;

    signalforge::buffer::SignalBufferRegistry* registry_;
    QLabel* valueLabel_ = nullptr;
    QLabel* unitLabel_ = nullptr;
    QLabel* rangeLabel_ = nullptr;
    std::optional<double> observedMin_;
    std::optional<double> observedMax_;
};

}  // namespace signalforge::dashboard
