// src/inspect/parsed_signals_view.hpp
#pragma once

#include "query/filter_expr.hpp"
#include "workbench/signal_identity.hpp"

#include <QColor>
#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <chrono>
#include <functional>
#include <utility>
#include <vector>

class QLineEdit;
class QLabel;
class QTableWidget;
class QTimer;
class QToolButton;

namespace signalforge::buffer {
class SignalBufferRegistry;
}

namespace signalforge::inspect {

/// Tier 2 of the workbench (解析数据): a live, zero-config table of every
/// decoded signal — an identity swatch + Name · Trend (mini-sparkline) · Quality
/// · Source · Value · Unit · Rate · Type · Age · Changed · Dashboard marker —
/// with a Wireshark-style display-filter bar on top (the `signalforge_query`
/// engine; `quality`, `rate`, `changed`, `dashboard`, and any signal's own name
/// are filterable fields too). An optional "Group by driver" toggle clusters
/// rows under per-driver header rows.
///
/// This is the default landing surface: it shows the device's decoded output
/// without any configuration, and the dashboard (Tier 3) sits on top of it.
class ParsedSignalsView : public QWidget {
    Q_OBJECT

public:
    explicit ParsedSignalsView(signalforge::buffer::SignalBufferRegistry& registry, QWidget* parent = nullptr);
    ~ParsedSignalsView() override;

    ParsedSignalsView(const ParsedSignalsView&) = delete;
    ParsedSignalsView& operator=(const ParsedSignalsView&) = delete;

    /// Apply a display-filter expression. Invalid expressions are flagged (the
    /// bar turns into an error state) and leave all rows visible.
    void setFilter(const QString& text);

    /// Rebuild rows from the registry (if the signal set changed) and refresh
    /// every row's value + age, then re-apply the active filter.
    void refresh();

    /// Toggle grouping the table by driver (source): rows cluster under a
    /// per-driver header row. Off by default (flat list).
    void setGroupByDriver(bool on);

    // --- identity / dashboard providers (M34 P2) -----------------------------
    // The app layer injects these (the view is in `inspect` and must not depend
    // on the theme or the dashboard). All are optional; absent providers degrade
    // gracefully (no swatch / no quality colour / no dashboard marker).

    /// Resolve a signal's stable identity colour (swatch). Typically resolves a
    /// shared `SignalIdentity` palette index against the active theme.
    void setSignalColorProvider(std::function<QColor(const QString& signalId)> provider);

    /// Resolve a quality's badge colour against the active theme.
    void setQualityColorProvider(std::function<QColor(signalforge::workbench::Quality)> provider);

    /// Report whether a signal is currently on the dashboard (drives the "on
    /// dashboard" marker + the row action's Add/Remove flip).
    void setDashboardMembershipProvider(std::function<bool(const QString& signalId)> provider);

    /// Report whether a signal has a user colour override (drives the row menu's
    /// "Reset colour" entry).
    void setColorOverriddenProvider(std::function<bool(const QString& signalId)> provider);

Q_SIGNALS:
    /// Emitted when the user right-clicks a signal row and picks "Add to
    /// dashboard ▸ <type>". `typeToken` is a `panelTypeName` token
    /// (numeric/state/plot/bar/gauge). The owner routes it to the dashboard.
    void addToDashboardRequested(const QString& signalId, const QString& typeToken);

    /// Emitted when the user picks "Remove from dashboard" on a signal already
    /// shown there. The owner routes it to `Dashboard::removeSignalEverywhere`.
    void removeFromDashboardRequested(const QString& signalId);

    /// Emitted when the selected signal row changes (empty id = selection
    /// cleared). The app routes it to the workbench selection model / inspector.
    void signalSelected(const QString& signalId);

    /// Emitted when the user asks to drill through from a signal to the raw
    /// packets that produced it (double-click a row, or the row menu's "Show
    /// source packets"). The app switches to the Raw tier filtered to the
    /// signal's source — the cross-tier differentiator.
    void drillToSourceRequested(const QString& signalId);

    /// Emitted when the user picks "Set colour…" on a signal row. The app opens
    /// a colour picker and records the per-signal override in the identity SSOT.
    void recolorRequested(const QString& signalId);

    /// Emitted when the user picks "Reset colour" on a signal that has an
    /// override — the app clears it back to the driver default.
    void resetColorRequested(const QString& signalId);

public:
    /// Build the "Add to dashboard ▸ <type>" menu for `signalId` (owned by the
    /// caller). Each action emits `addToDashboardRequested`. Used live (on
    /// right-click) and by interaction tests.
    [[nodiscard]] class QMenu* buildAddToDashboardMenu(const QString& signalId);

    /// Build the row context menu: the "Add to dashboard ▸" submenu when
    /// `onDashboard` is false, or a single "Remove from dashboard" action
    /// (emitting `removeFromDashboardRequested`) when true. Owned by the caller.
    [[nodiscard]] class QMenu* buildRowMenu(const QString& signalId, bool onDashboard);

    /// Build the header context menu: one checkable action per column toggling
    /// its visibility (Name stays pinned). Owned by the caller; used live (on
    /// header right-click) and by interaction tests.
    [[nodiscard]] class QMenu* buildColumnMenu();

    // --- test accessors ---
    [[nodiscard]] int totalRowCount() const;
    [[nodiscard]] int visibleRowCount() const;
    [[nodiscard]] bool filterValid() const {
        return filterValid_;
    }
    [[nodiscard]] QLineEdit* filterEdit() const {
        return filterEdit_;
    }
    [[nodiscard]] QTableWidget* table() const {
        return table_;
    }

private:
    /// Cached per-row field values, fed to the filter engine.
    struct RowData {
        QString id;
        QString name;
        QString source;
        QString unit;
        QString type;
        signalforge::query::FieldValue value;
        QString quality;              ///< quality token (good/stale/uncertain/bad); filterable
        bool onDashboard = false;     ///< whether the signal is currently on the dashboard
        double rateHz = 0.0;          ///< measured update rate (Hz); filterable
        double sinceChangeSec = 0.0;  ///< seconds since the value last changed; filterable as `changed`
        int tableRow = -1;            ///< table row this signal occupies (data rows interleave group headers)
    };

protected:
    /// Refresh immediately when the view becomes visible (e.g. switching to the
    /// Parsed segment) so it isn't stale for up to one timer tick.
    void showEvent(QShowEvent* event) override;

private:
    void rebuild(const QStringList& ids);
    void applyFilter();
    void showRowMenu(const QPoint& pos);
    void showHeaderMenu(const QPoint& pos);

private Q_SLOTS:
    /// Toggle the collapsed state of the driver group whose header is at
    /// `tableRow` (no-op for a non-header row), then re-apply visibility. A slot
    /// so it binds to `cellClicked` directly and is reachable in interaction
    /// tests via `QMetaObject::invokeMethod`.
    void toggleGroupCollapse(int tableRow);

private:
    /// Resolve a table row to its signal id, or an empty string for a
    /// group-header / out-of-range row.
    [[nodiscard]] QString signalIdAtRow(int tableRow) const;

    signalforge::buffer::SignalBufferRegistry* registry_;
    QLineEdit* filterEdit_ = nullptr;
    QLabel* countLabel_ = nullptr;
    QToolButton* groupToggle_ = nullptr;
    QTableWidget* table_ = nullptr;
    QTimer* refreshTimer_ = nullptr;

    QStringList cachedIds_;            ///< signal-id set last laid out (sorted; order-independent set check)
    std::vector<RowData> rows_;        ///< one per signal, in display order
    std::vector<int> tableRowToData_;  ///< table row → index into rows_ (-1 for a group-header row)
    bool groupByDriver_ = false;       ///< cluster rows under per-driver header rows
    bool layoutDirty_ = false;         ///< force a rebuild even if the id set is unchanged (grouping toggled)
    QSet<QString> collapsedDrivers_;   ///< driver sources whose group is collapsed (rows hidden) when grouping

    /// Per-signal value-change tracking for the "Changed" column: id → (last
    /// formatted value, when it last changed). Survives row rebuilds.
    QHash<QString, std::pair<QString, std::chrono::steady_clock::time_point>> changeTracker_;

    signalforge::query::FilterExpr filter_;  ///< active filter (empty = match all)
    bool filterValid_ = true;

    std::function<QColor(const QString&)> signalColorProvider_;
    std::function<QColor(signalforge::workbench::Quality)> qualityColorProvider_;
    std::function<bool(const QString&)> dashboardMembershipProvider_;
    std::function<bool(const QString&)> colorOverriddenProvider_;
};

}  // namespace signalforge::inspect
