// src/dashboard/panel.hpp
#pragma once

#include "dashboard/panel_types.hpp"

#include <QFrame>
#include <QString>
#include <QStringList>

class QLabel;
class QPushButton;
class QVBoxLayout;

namespace signalforge::dashboard {

/// Base class for a dashboard panel: a card with a header (title +
/// remove button) and a subclass-provided body. Concrete panel types
/// (Numeric / State / Plot) derive from this and call `setBody`.
///
/// A `Panel` is a plain `QWidget` (no QML scene) except `PlotPanel`,
/// which embeds a `QQuickWidget` in its body. Lives on the main thread.
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

    /// Release any externally-owned resource before the panel is deleted
    /// (PlotPanel detaches its manager-owned chart). Base no-op.
    virtual void detachChart() {}

    /// Show or hide editing affordances (the remove button).
    void setEditMode(bool on);

Q_SIGNALS:
    /// Emitted when the user clicks this panel's remove button.
    void removeRequested(const QString& panelId);

protected:
    /// Install the subclass's content widget below the header. Replaces
    /// any previously-set body.
    void setBody(QWidget* body);

    /// Update the header title text shown to the user.
    void setHeaderTitle(const QString& title);

    PanelConfig config_;

private:
    QVBoxLayout* rootLayout_ = nullptr;
    QLabel* titleLabel_ = nullptr;
    QPushButton* removeButton_ = nullptr;
    QWidget* body_ = nullptr;
};

}  // namespace signalforge::dashboard
