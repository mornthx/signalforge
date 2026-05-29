// src/dashboard/state_panel.hpp
#pragma once

#include "dashboard/panel.hpp"

class QLabel;

namespace signalforge::buffer {
class SignalBufferRegistry;
}

namespace signalforge::dashboard {

/// Panel showing a single signal's current discrete state: a filled
/// (●) / hollow (○) indicator plus a label. Best for booleans and
/// string/enum status signals. Driven by the Dashboard refresh tick.
class StatePanel : public Panel {
    Q_OBJECT

public:
    StatePanel(PanelConfig config, signalforge::buffer::SignalBufferRegistry& registry, QWidget* parent = nullptr);
    ~StatePanel() override;

    /// Re-read the bound signal's latest value and update the display.
    void refresh() override;

    /// Currently displayed state label (test/diagnostic accessor).
    [[nodiscard]] QString stateText() const;

    /// Whether the indicator is "active" (bool true, or a non-empty,
    /// non-"false" string).
    [[nodiscard]] bool isActive() const noexcept {
        return active_;
    }

private:
    [[nodiscard]] QString boundSignalId() const;

    signalforge::buffer::SignalBufferRegistry* registry_;
    QLabel* indicatorLabel_ = nullptr;
    QLabel* stateLabel_ = nullptr;
    bool active_ = false;
};

}  // namespace signalforge::dashboard
