// src/dashboard/panel_config_dialog.hpp
#pragma once

#include "dashboard/panel_types.hpp"

#include <QDialog>
#include <QStringList>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QListWidget;
class QSpinBox;

namespace signalforge::dashboard {

/// Modal editor for a panel's full `PanelConfig`: widget type, bound signals,
/// title, value range, unit override, and decimals. Replaces the per-panel
/// right-click "Show as ▸ / Signals ▸" submenus with one richer surface.
///
/// (The owner chose a modal over architecture §7.3 #4's right-hand inspector;
/// recorded as a deviation in M32-done.md.)
class PanelConfigDialog : public QDialog {
    Q_OBJECT

public:
    PanelConfigDialog(const PanelConfig& initial, const QStringList& availableSignals, QWidget* parent = nullptr);
    ~PanelConfigDialog() override;

    /// The edited config (valid after the dialog is accepted). Preserves the
    /// original id and geometry.
    [[nodiscard]] PanelConfig result() const;

    // --- test accessors ---
    [[nodiscard]] QComboBox* typeCombo() const {
        return typeCombo_;
    }
    [[nodiscard]] QListWidget* signalList() const {
        return signalList_;
    }
    [[nodiscard]] QLineEdit* titleEdit() const {
        return titleEdit_;
    }
    [[nodiscard]] QCheckBox* limitRangeCheck() const {
        return limitRangeCheck_;
    }
    [[nodiscard]] QDoubleSpinBox* minSpin() const {
        return minSpin_;
    }
    [[nodiscard]] QDoubleSpinBox* maxSpin() const {
        return maxSpin_;
    }
    [[nodiscard]] QLineEdit* unitEdit() const {
        return unitEdit_;
    }
    [[nodiscard]] QSpinBox* decimalsSpin() const {
        return decimalsSpin_;
    }

private:
    PanelConfig initial_;
    QComboBox* typeCombo_ = nullptr;
    QListWidget* signalList_ = nullptr;
    QLineEdit* titleEdit_ = nullptr;
    QCheckBox* limitRangeCheck_ = nullptr;
    QDoubleSpinBox* minSpin_ = nullptr;
    QDoubleSpinBox* maxSpin_ = nullptr;
    QLineEdit* unitEdit_ = nullptr;
    QSpinBox* decimalsSpin_ = nullptr;
};

}  // namespace signalforge::dashboard
