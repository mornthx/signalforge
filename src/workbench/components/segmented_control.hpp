// src/workbench/components/segmented_control.hpp
#pragma once

#include <QFrame>
#include <QHash>
#include <QString>

class QAbstractButton;
class QButtonGroup;
class QHBoxLayout;

namespace signalforge::workbench {

/// A horizontal segmented control: a row of exclusive buttons that switch a
/// sub-view within one workbench mode (e.g. Raw / Parsed / Dashboard inside
/// Inspect). The order added is the order shown. Selecting a segment emits
/// `segmentSelected`; the owner swaps the view. Mirrors `ActivityRail`'s API on
/// a horizontal axis — the rail picks the mode, the segmented control picks the
/// view within it.
class SegmentedControl : public QFrame {
    Q_OBJECT

public:
    explicit SegmentedControl(QWidget* parent = nullptr);
    ~SegmentedControl() override;

    /// Append a segment. `id` is the stable key; `label` is shown. The first
    /// segment added becomes the current one.
    void addSegment(const QString& id, const QString& label);

    /// Programmatically select a segment (no `segmentSelected` emission). No-op
    /// for an unknown id.
    void setCurrentSegment(const QString& id);

    [[nodiscard]] QString currentSegment() const {
        return currentId_;
    }
    [[nodiscard]] int segmentCount() const;

    /// The button for `id` (nullptr if unknown) — for interaction tests.
    [[nodiscard]] QAbstractButton* button(const QString& id) const;

Q_SIGNALS:
    /// Emitted when the user picks a segment.
    void segmentSelected(const QString& id);

private:
    QHBoxLayout* layout_ = nullptr;
    QButtonGroup* group_ = nullptr;
    QHash<QString, QAbstractButton*> buttons_;
    QHash<QAbstractButton*, QString> idOf_;
    QString currentId_;
};

}  // namespace signalforge::workbench
