// src/workbench/components/inspector_panel.hpp
#pragma once

#include <QColor>
#include <QString>
#include <QVector>
#include <QWidget>
#include <functional>
#include <utility>

class QAbstractButton;
class QFormLayout;
class QFrame;
class QHBoxLayout;
class QLabel;
class QVBoxLayout;

namespace signalforge::workbench {

/// The right-hand inspector body: the details of the current selection — a
/// signal's stats, a dissected packet field, a dashboard panel's config, etc.
/// A dumb display the app layer feeds; it owns no data and no selection logic.
///
/// Mounted into `WorkbenchFrame`'s inspector slot. Shows a placeholder until
/// something is selected.
class InspectorPanel : public QWidget {
    Q_OBJECT

public:
    using Row = std::pair<QString, QString>;                   ///< (label, value)
    using Action = std::pair<QString, std::function<void()>>;  ///< (button label, on-click)

    explicit InspectorPanel(QWidget* parent = nullptr);
    ~InspectorPanel() override;

    InspectorPanel(const InspectorPanel&) = delete;
    InspectorPanel& operator=(const InspectorPanel&) = delete;

    /// Show a titled set of detail rows. A valid `accent` tints the leading
    /// swatch (e.g. a signal's identity colour); an invalid one hides it.
    /// Replaces any prior content and hides the placeholder.
    void showDetails(const QString& title, const QString& subtitle, const QVector<Row>& rows,
                     const QColor& accent = QColor());

    /// Host a caller-built editable body (e.g. a dashboard panel's property
    /// form) below the detail rows. Passing nullptr clears it. The panel takes
    /// ownership; replaced/cleared on the next call, `showDetails`, or
    /// `showPlaceholder`.
    void setContent(QWidget* body);

    /// Clear the details and show a centered placeholder (nothing selected).
    void showPlaceholder(const QString& message);

    /// Set the action buttons shown below the details (an empty list clears the
    /// area). Each button invokes its callback when clicked. Cleared by
    /// `showPlaceholder`. Lets the app attach selection-specific actions (set
    /// colour, add to dashboard, …) without coupling this component to signals.
    void setActions(const QVector<Action>& actions);

    // --- test accessors ---
    [[nodiscard]] QString titleText() const;
    [[nodiscard]] int rowCount() const;
    [[nodiscard]] int actionCount() const;
    [[nodiscard]] QAbstractButton* actionButton(const QString& label) const;
    [[nodiscard]] bool hasContent() const;
    [[nodiscard]] bool showingPlaceholder() const;

Q_SIGNALS:
    /// Emitted when the user clicks the inspector's close (×) button. The app
    /// hides the inspector slot (the sidebar is dismissible).
    void closeRequested();

private:
    void clearRows();
    void clearActions();

    QFrame* swatch_ = nullptr;
    QLabel* titleLabel_ = nullptr;
    QLabel* subtitleLabel_ = nullptr;
    QWidget* header_ = nullptr;
    QWidget* rowsHost_ = nullptr;
    QFormLayout* rowsLayout_ = nullptr;
    QWidget* contentHost_ = nullptr;
    QVBoxLayout* contentLayout_ = nullptr;
    QWidget* content_ = nullptr;
    QWidget* actionsHost_ = nullptr;
    QHBoxLayout* actionsLayout_ = nullptr;
    QLabel* placeholder_ = nullptr;
    int rowCount_ = 0;
    bool showingPlaceholder_ = false;
};

}  // namespace signalforge::workbench
