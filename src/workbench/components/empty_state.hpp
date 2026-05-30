// src/workbench/components/empty_state.hpp
#pragma once

#include <QFrame>
#include <QString>

class QHBoxLayout;
class QLabel;
class QPushButton;
class QVBoxLayout;

namespace signalforge::workbench {

/// A centered empty/onboarding state: a title, a caption, and optional action
/// buttons. Reused for the app-level "connect a device" onboarding and for any
/// per-context empty view, so they all read the same.
class EmptyState : public QFrame {
    Q_OBJECT

public:
    explicit EmptyState(QWidget* parent = nullptr);
    ~EmptyState() override;

    void setTitle(const QString& title);
    void setCaption(const QString& caption);

    /// Add an action button (returned so the caller can connect it). `primary`
    /// tags it for accent styling.
    QPushButton* addAction(const QString& text, bool primary = false);

private:
    QLabel* title_ = nullptr;
    QLabel* caption_ = nullptr;
    QHBoxLayout* actions_ = nullptr;
};

}  // namespace signalforge::workbench
