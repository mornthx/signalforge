// src/workbench/components/activity_rail.hpp
#pragma once

#include <QFrame>
#include <QHash>
#include <QString>

class QAbstractButton;
class QButtonGroup;
class QVBoxLayout;

namespace signalforge::workbench {

/// The left activity rail: a vertical column of exclusive mode buttons that
/// select the primary workbench mode (Connect / Inspect / …). The order the
/// modes are added is the order shown (mirroring the data flow). Selecting a
/// mode emits `modeSelected`; the owner swaps the content area.
class ActivityRail : public QFrame {
    Q_OBJECT

public:
    explicit ActivityRail(QWidget* parent = nullptr);
    ~ActivityRail() override;

    /// Append a mode. `id` is the stable key; `label` is shown; `glyph` is an
    /// optional leading symbol (a real icon set replaces it later). The first
    /// mode added becomes the current one.
    void addMode(const QString& id, const QString& label, const QString& glyph = QString());

    /// Programmatically select a mode (no `modeSelected` emission — the caller
    /// already knows). No-op for an unknown id.
    void setCurrentMode(const QString& id);

    [[nodiscard]] QString currentMode() const {
        return currentId_;
    }
    [[nodiscard]] int modeCount() const;

    /// The button for `id` (nullptr if unknown) — for interaction tests.
    [[nodiscard]] QAbstractButton* button(const QString& id) const;

Q_SIGNALS:
    /// Emitted when the user picks a mode.
    void modeSelected(const QString& id);

private:
    QVBoxLayout* layout_ = nullptr;
    QButtonGroup* group_ = nullptr;
    QHash<QString, QAbstractButton*> buttons_;
    QHash<QAbstractButton*, QString> idOf_;
    QString currentId_;
};

}  // namespace signalforge::workbench
