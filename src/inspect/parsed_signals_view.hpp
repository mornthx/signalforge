// src/inspect/parsed_signals_view.hpp
#pragma once

#include "query/filter_expr.hpp"

#include <QString>
#include <QStringList>
#include <QWidget>
#include <vector>

class QLineEdit;
class QLabel;
class QTableWidget;
class QTimer;

namespace signalforge::buffer {
class SignalBufferRegistry;
}

namespace signalforge::inspect {

/// Tier 2 of the workbench (解析数据): a live, zero-config table of every
/// decoded signal — Name · Source · Value · Unit · Type · Age — with a
/// Wireshark-style display-filter bar on top (the `signalforge_query` engine).
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

Q_SIGNALS:
    /// Emitted when the user right-clicks a signal row and picks "Add to
    /// dashboard ▸ <type>". `typeToken` is a `panelTypeName` token
    /// (numeric/state/plot/bar/gauge). The owner routes it to the dashboard.
    void addToDashboardRequested(const QString& signalId, const QString& typeToken);

public:
    /// Build the "Add to dashboard ▸ <type>" menu for `signalId` (owned by the
    /// caller). Each action emits `addToDashboardRequested`. Used live (on
    /// right-click) and by interaction tests.
    [[nodiscard]] class QMenu* buildAddToDashboardMenu(const QString& signalId);

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
    };

    void rebuild(const QStringList& ids);
    void applyFilter();
    void showRowMenu(const QPoint& pos);

    signalforge::buffer::SignalBufferRegistry* registry_;
    QLineEdit* filterEdit_ = nullptr;
    QLabel* countLabel_ = nullptr;
    QTableWidget* table_ = nullptr;
    QTimer* refreshTimer_ = nullptr;

    QStringList cachedIds_;
    std::vector<RowData> rows_;
    signalforge::query::FilterExpr filter_;  ///< active filter (empty = match all)
    bool filterValid_ = true;
};

}  // namespace signalforge::inspect
