// src/dashboard/dashboard.hpp
#pragma once

#include "dashboard/panel_types.hpp"

#include <QHash>
#include <QStringList>
#include <QWidget>
#include <memory>

class QGridLayout;
class QTimer;

namespace signalforge::buffer {
class SignalBufferRegistry;
}
namespace signalforge::chart {
class TimeAxisManager;
}

namespace signalforge::dashboard {

class Panel;

/// The central workbench surface: a reflowing grid of heterogeneous
/// panels (Numeric / State / Plot). Replaces the legacy vertical stack
/// of identical charts. Small cards pack into rows of `columns`; Plot
/// panels take a full row.
///
/// A single ~15 Hz timer drives every panel's `refresh()` (Plot panels
/// self-drive their own redraw, so their refresh is a no-op). Plot
/// panels are backed by charts created in the injected `ChartManager`.
class Dashboard : public QWidget {
    Q_OBJECT

public:
    explicit Dashboard(signalforge::buffer::SignalBufferRegistry& registry, QWidget* parent = nullptr);
    ~Dashboard() override;

    Dashboard(const Dashboard&) = delete;
    Dashboard& operator=(const Dashboard&) = delete;

    /// Create a panel from an explicit config. Returns the panel id
    /// (generated if `config.id` is empty).
    QString addPanel(PanelConfig config);

    /// Ensure `signalId` is shown somewhere: if a panel already hosts
    /// it, returns that panel's id; otherwise creates a panel using the
    /// auto-suggested type for the signal. Returns the hosting panel id.
    QString addSignal(const QString& signalId);

    /// Add a Plot panel (empty by default; optionally pre-bound to
    /// `signalIds`). The toolbar "+ Plot" uses the empty form.
    QString addPlotPanel(const QStringList& signalIds = {});

    /// Add a Table panel listing the current value of each signal in
    /// `signalIds` (the toolbar "+ Table" affordance).
    QString addTablePanel(const QStringList& signalIds);

    /// Remove `signalId` from every panel; single-signal cards that
    /// become empty are removed, Plot panels are kept.
    void removeSignalEverywhere(const QString& signalId);

    /// Remove a panel by id (and its backing chart, for Plot panels).
    void removePanel(const QString& panelId);

    /// Panel ids in layout order.
    [[nodiscard]] QStringList panelIds() const {
        return panelOrder_;
    }

    /// Number of live panels.
    [[nodiscard]] int panelCount() const {
        return static_cast<int>(panelOrder_.size());
    }

    /// Look up a panel by id (nullptr if absent).
    [[nodiscard]] Panel* panel(const QString& panelId) const;

    /// Whether any panel currently hosts `signalId`. Used by the signal
    /// list to derive checkbox state from the dashboard (source of truth).
    [[nodiscard]] bool showsSignal(const QString& signalId) const;

    /// Toggle editing affordances (remove buttons) across all panels.
    void setEditMode(bool on);

    /// Re-read all panels' data immediately (also invoked by the timer).
    void refreshAll();

    /// Shared time axis driving every plot panel (toolbar Live/preset).
    [[nodiscard]] signalforge::chart::TimeAxisManager& timeAxis();

Q_SIGNALS:
    /// Emitted whenever a panel is added or removed.
    void panelsChanged();

private:
    void relayout();
    [[nodiscard]] QString nextPanelId();

    signalforge::buffer::SignalBufferRegistry* registry_;
    std::unique_ptr<signalforge::chart::TimeAxisManager> timeAxis_;
    QGridLayout* grid_ = nullptr;
    QTimer* refreshTimer_ = nullptr;

    QHash<QString, Panel*> panels_;  ///< id -> panel (Qt-parented to this).
    QStringList panelOrder_;         ///< layout order.
    int columns_ = 3;                ///< small-card columns per row.
    int nextPanelSuffix_ = 1;
    bool editMode_ = false;
};

}  // namespace signalforge::dashboard
