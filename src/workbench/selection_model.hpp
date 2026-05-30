// src/workbench/selection_model.hpp
#pragma once

#include <QObject>
#include <QString>

namespace signalforge::workbench {

/// What kind of thing is currently selected across the workbench.
enum class SelectionKind {
    None,
    Signal,       ///< a decoded signal (id = signalId)
    Packet,       ///< a raw frame (id = capture index, as text)
    MessageType,  ///< a decoded message type (id = message/schema id)
    Widget,       ///< a dashboard panel (id = panelId)
};

/// The app-wide current selection: a kind + a stable string id. String ids keep
/// this decoupled from concrete view/data types.
struct Selection {
    SelectionKind kind = SelectionKind::None;
    QString id;

    [[nodiscard]] bool isNone() const noexcept {
        return kind == SelectionKind::None;
    }
    bool operator==(const Selection& other) const = default;
};

/// One app-wide "current selection" observed by every view. Selecting a signal
/// in one tier highlights it in the others and feeds the inspector; this is the
/// backbone of cross-tier highlight + drill-through (UI redesign Phase 0).
///
/// Pure main-thread `QObject`; no UI, no data dependencies.
class SelectionModel : public QObject {
    Q_OBJECT

public:
    explicit SelectionModel(QObject* parent = nullptr);

    /// The current selection (None until something is selected).
    [[nodiscard]] const Selection& current() const noexcept {
        return current_;
    }

    /// Set the current selection. Emits `selectionChanged` only if it differs
    /// from the current one (idempotent).
    void select(SelectionKind kind, const QString& id);

    /// Clear the selection (back to None). Emits only if something was selected.
    void clear();

    /// Whether `(kind, id)` is exactly the current selection.
    [[nodiscard]] bool isSelected(SelectionKind kind, const QString& id) const;

Q_SIGNALS:
    /// Emitted whenever the current selection changes.
    void selectionChanged(const Selection& selection);

private:
    Selection current_;
};

}  // namespace signalforge::workbench
