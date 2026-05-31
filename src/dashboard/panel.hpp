// src/dashboard/panel.hpp
#pragma once

#include "dashboard/panel_types.hpp"

#include <QFrame>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QStringList>

class QContextMenuEvent;
class QEvent;
class QLabel;
class QMouseEvent;
class QResizeEvent;
class QVBoxLayout;
class QWidget;

namespace signalforge::dashboard {

/// Base class for a dashboard panel: a header-less card showing its signal's
/// identity (name + source) above a subclass-provided body. Concrete panel
/// types (Numeric / State / Plot / …) derive from this and call `setBody`.
///
/// Interaction (M29 S3): the whole card is the drag handle — press the left
/// button anywhere and drag to move; drag the bottom-right grip to resize;
/// right-click anywhere to open the configuration menu. There is no header bar
/// and no visible config button.
///
/// A `Panel` is a plain `QWidget` (no QML scene). Lives on the main thread.
class Panel : public QFrame {
    Q_OBJECT

public:
    explicit Panel(PanelConfig config, QWidget* parent = nullptr);
    ~Panel() override;

    Panel(const Panel&) = delete;
    Panel& operator=(const Panel&) = delete;
    Panel(Panel&&) = delete;
    Panel& operator=(Panel&&) = delete;

    /// Snapshot of this panel's configuration.
    [[nodiscard]] const PanelConfig& config() const noexcept {
        return config_;
    }

    /// Stable panel id.
    [[nodiscard]] QString id() const {
        return config_.id;
    }

    /// Widget kind.
    [[nodiscard]] PanelType type() const noexcept {
        return config_.type;
    }

    /// Bound signal ids (in binding order).
    [[nodiscard]] QStringList signalIds() const {
        return config_.signalIds;
    }

    /// Whether this panel currently hosts `signalId`.
    [[nodiscard]] bool hasSignal(const QString& signalId) const;

    /// True if the panel should occupy a full grid row (Plot). Small
    /// cards (Numeric / State) return false. Overridden by `PlotPanel`.
    [[nodiscard]] virtual bool isWide() const {
        return false;
    }

    /// Pull fresh data from the source and update the view. Base no-op;
    /// driven by the owning `Dashboard`'s refresh tick.
    virtual void refresh() {}

    /// True for panels that host more than one signal (Plot, Table). The
    /// Dashboard uses this to decide whether unticking a signal removes a
    /// row (multi-signal) or the whole card (single-signal). Default false.
    [[nodiscard]] virtual bool isMultiSignal() const {
        return false;
    }

    /// Add / remove a signal. Meaningful only for multi-signal panels;
    /// base is a no-op (single-signal cards bind their signal at construction).
    virtual void addSignal(const QString& signalId) {}
    virtual void removeSignal(const QString& signalId) {}

    /// Inject the shared signal→identity-colour provider so the panel's trace /
    /// bar / value renders in the signal's identity colour (parity with the
    /// Parsed swatch). Base is a no-op (Numeric / State / Table show no colour);
    /// Plot and Meter panels override it.
    virtual void setSignalColorProvider(SignalColorProvider provider) {}

    /// The bottom-right resize grip — for interaction tests that simulate a
    /// drag-resize. (Move is simulated by sending mouse events to the panel.)
    [[nodiscard]] QWidget* resizeGrip() const {
        return grip_;
    }

    /// Whether the panel has a user-set free-form geometry (vs. auto-placed).
    [[nodiscard]] bool userPlaced() const {
        return config_.geometry.isValid();
    }

    /// Apply a geometry chosen by the owning Dashboard's drag/push resolution:
    /// sets the widget geometry and records it as the user-placed geometry.
    void setUserGeometry(const QRect& geometry);

Q_SIGNALS:
    /// Emitted when the user presses the card (a click, or the start of a
    /// drag) — the Dashboard forwards it so the right inspector can show this
    /// panel's editable properties.
    void selected(const QString& panelId);

    /// Emitted when the user right-clicks the card. The owning Dashboard
    /// responds by showing the per-panel configuration menu at the cursor.
    void configureRequested(const QString& panelId);

    /// Emitted continuously while the user drags the card (move) or grip
    /// (resize), carrying the proposed new geometry in parent coordinates
    /// (unclamped). The Dashboard resolves bounds + neighbor collisions and
    /// calls `setUserGeometry` on the affected panels.
    void dragProposed(const QString& panelId, const QRect& proposed);

    /// Emitted after the user drag-moves or drag-resizes the panel; the new
    /// geometry has already been written into `config().geometry`.
    void geometryChanged(const QString& panelId);

protected:
    /// Install the subclass's content widget below the identity strip.
    /// Replaces any previously-set body.
    void setBody(QWidget* body);

    /// Update the signal-name text shown at the top of the card.
    void setHeaderTitle(const QString& title);

    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

    PanelConfig config_;

private:
    enum class DragMode { None, Move, Resize };

    /// Recursively route a body subtree's mouse events to this panel so the
    /// card is draggable even over children that would otherwise consume them.
    void installDragFilter(QWidget* widget);
    /// Refresh the source/caption line from the bound signal ids.
    void updateSourceLabel();

    void startDrag(DragMode mode, const QPoint& globalPos);
    void updateDrag(const QPoint& globalPos);
    void endDrag();

    QVBoxLayout* rootLayout_ = nullptr;
    QLabel* nameLabel_ = nullptr;
    QLabel* sourceLabel_ = nullptr;
    QWidget* body_ = nullptr;
    QWidget* grip_ = nullptr;

    DragMode dragMode_ = DragMode::None;
    QPoint dragStartGlobal_;
    QRect dragStartGeom_;
};

}  // namespace signalforge::dashboard
