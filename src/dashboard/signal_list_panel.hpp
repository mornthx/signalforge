// src/dashboard/signal_list_panel.hpp
#pragma once

#include <QString>
#include <QWidget>
#include <map>

class QLabel;
class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

namespace signalforge::buffer {
class SignalBufferRegistry;
}

namespace signalforge::dashboard {

class Dashboard;

/// Dashboard-aware signal picker: a checkable tree of every registered
/// signal, grouped by driver id, with a filter. Checking a signal calls
/// `Dashboard::addSignal` (auto-suggesting a panel type); unchecking
/// calls `Dashboard::removeSignalEverywhere`. Crucially, checkbox state
/// is derived from the dashboard on every `refresh()`, so the list,
/// panel Remove buttons, and the dashboard never drift apart.
///
/// This is the dashboard-era replacement for chart::SignalSelector (which
/// is frozen at M8 and routes to a single active chart). The two are
/// parallel; the old one is no longer mounted.
class SignalListPanel : public QWidget {
    Q_OBJECT

public:
    SignalListPanel(signalforge::buffer::SignalBufferRegistry& registry, Dashboard& dashboard,
                    QWidget* parent = nullptr);
    ~SignalListPanel() override;

    SignalListPanel(const SignalListPanel&) = delete;
    SignalListPanel& operator=(const SignalListPanel&) = delete;

    /// Rebuild the tree from the registry and derive checkbox state from
    /// the dashboard. Safe to call repeatedly (e.g. on a refresh timer).
    void refresh();

    /// Apply a filter substring (matches signal id).
    void setFilter(const QString& substring);

Q_SIGNALS:
    /// Emitted after a user toggle has been routed to the dashboard.
    void signalToggled(const QString& signalId, bool checked);

private:
    signalforge::buffer::SignalBufferRegistry* registry_;
    Dashboard* dashboard_;
    QLineEdit* filterEdit_ = nullptr;
    QLabel* countLabel_ = nullptr;
    QTreeWidget* tree_ = nullptr;
    QString filter_;
};

}  // namespace signalforge::dashboard
