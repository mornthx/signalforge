// src/workbench/components/section_header.hpp
#pragma once

#include <QFrame>
#include <QString>

class QHBoxLayout;
class QLabel;

namespace signalforge::workbench {

/// A consistent panel/section header: a title on the left, an optional caption
/// (e.g. a count) on the right, and a slot for trailing action widgets. Replaces
/// the ad-hoc per-panel headers so every surface reads the same.
class SectionHeader : public QFrame {
    Q_OBJECT

public:
    explicit SectionHeader(const QString& title = QString(), QWidget* parent = nullptr);
    ~SectionHeader() override;

    void setTitle(const QString& title);
    [[nodiscard]] QString title() const;

    /// Right-aligned secondary text (e.g. "9 signals"). Empty hides it.
    void setCaption(const QString& caption);
    [[nodiscard]] QString caption() const;

    /// Add a trailing action widget (button, filter, …) to the right edge.
    void addAction(QWidget* widget);

private:
    QHBoxLayout* layout_ = nullptr;
    QLabel* titleLabel_ = nullptr;
    QLabel* captionLabel_ = nullptr;
};

}  // namespace signalforge::workbench
