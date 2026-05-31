// src/workbench/components/inspector_panel.hpp
#pragma once

#include <QColor>
#include <QString>
#include <QVector>
#include <QWidget>
#include <utility>

class QFormLayout;
class QFrame;
class QLabel;

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
    using Row = std::pair<QString, QString>;  ///< (label, value)

    explicit InspectorPanel(QWidget* parent = nullptr);
    ~InspectorPanel() override;

    InspectorPanel(const InspectorPanel&) = delete;
    InspectorPanel& operator=(const InspectorPanel&) = delete;

    /// Show a titled set of detail rows. A valid `accent` tints the leading
    /// swatch (e.g. a signal's identity colour); an invalid one hides it.
    /// Replaces any prior content and hides the placeholder.
    void showDetails(const QString& title, const QString& subtitle, const QVector<Row>& rows,
                     const QColor& accent = QColor());

    /// Clear the details and show a centered placeholder (nothing selected).
    void showPlaceholder(const QString& message);

    // --- test accessors ---
    [[nodiscard]] QString titleText() const;
    [[nodiscard]] int rowCount() const;
    [[nodiscard]] bool showingPlaceholder() const;

private:
    void clearRows();

    QFrame* swatch_ = nullptr;
    QLabel* titleLabel_ = nullptr;
    QLabel* subtitleLabel_ = nullptr;
    QWidget* header_ = nullptr;
    QWidget* rowsHost_ = nullptr;
    QFormLayout* rowsLayout_ = nullptr;
    QLabel* placeholder_ = nullptr;
    int rowCount_ = 0;
    bool showingPlaceholder_ = false;
};

}  // namespace signalforge::workbench
