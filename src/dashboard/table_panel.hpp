// src/dashboard/table_panel.hpp
#pragma once

#include "dashboard/panel.hpp"

class QTableWidget;

namespace signalforge::buffer {
class SignalBufferRegistry;
}

namespace signalforge::dashboard {

/// Panel showing the current value of N signals, one row each:
/// Signal · Value · Unit · Updated(age). Best for "is this device sane?"
/// at-a-glance views. Multi-signal; driven by the Dashboard refresh tick.
class TablePanel : public Panel {
    Q_OBJECT

public:
    TablePanel(PanelConfig config, signalforge::buffer::SignalBufferRegistry& registry, QWidget* parent = nullptr);
    ~TablePanel() override;

    /// Table spans a full grid row and hosts many signals.
    [[nodiscard]] bool isWide() const override {
        return true;
    }
    [[nodiscard]] bool isMultiSignal() const override {
        return true;
    }

    /// Add / remove a signal row (and the panel binding).
    void addSignal(const QString& signalId) override;
    void removeSignal(const QString& signalId) override;

    /// Re-read every row's latest value + age.
    void refresh() override;

    /// Number of signal rows (test/diagnostic accessor).
    [[nodiscard]] int rowCount() const;

    /// Displayed value text for `signalId` ("—" if no data / not present).
    [[nodiscard]] QString valueTextFor(const QString& signalId) const;

private:
    void rebuildRows();
    [[nodiscard]] int rowForSignal(const QString& signalId) const;

    signalforge::buffer::SignalBufferRegistry* registry_;
    QTableWidget* table_ = nullptr;
};

}  // namespace signalforge::dashboard
