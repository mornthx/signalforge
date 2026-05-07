// src/chart/signal_selector.hpp
#pragma once

#include "buffer/signal_buffer_registry.hpp"
#include "chart/chart_manager.hpp"

#include <QString>
#include <QWidget>
#include <memory>

namespace signalforge::chart {

/// Tree widget that lists every signal in the
/// `SignalBufferRegistry`, grouped by `SignalMetadata::driverId`
/// with a special "Derived" group for signals whose driver id is
/// `expression-engine` (M7's virtual driver id, decision M8.2).
///
/// Each row is a checkbox + signal id + unit. Toggling a checkbox
/// adds/removes the signal in the manager's active chart. A filter
/// line-edit at the top narrows by signal-id substring.
///
/// Observes `signalsRegistered` / `signalsUnregistered` Qt signals
/// from the registry to keep the tree in sync as decoders register
/// or unregister signals mid-session.
///
/// Threading: lives on the main thread. The registry's signals are
/// emitted on the writer thread but Qt's queued connections route
/// them to this widget's slot on the main thread.
///
/// Freeze scope: the public API of this class is frozen at M8 close.
/// See spec §6.1.
class SignalSelector : public QWidget {
    Q_OBJECT

public:
    explicit SignalSelector(signalforge::buffer::SignalBufferRegistry& registry, ChartManager& manager,
                            QWidget* parent = nullptr);
    ~SignalSelector() override;

    SignalSelector(const SignalSelector&) = delete;
    SignalSelector& operator=(const SignalSelector&) = delete;
    SignalSelector(SignalSelector&&) = delete;
    SignalSelector& operator=(SignalSelector&&) = delete;

    /// Programmatically apply a filter substring (matches signal id).
    void setFilter(const QString& substring);

    /// Currently-applied filter substring.
    [[nodiscard]] QString filter() const;

    /// Re-read the registry and rebuild the tree. Call this after
    /// `SignalBufferRegistry::onSignalsRegistered` /
    /// `onSignalsUnregistered` (the M6 registry is not a `QObject`
    /// and doesn't emit Qt signals — see `.claude/M8-concerns.md`
    /// C2). Driver-startup code in MainWindow (S8) connects this
    /// to its decoder-registration completion path.
    void refresh();

Q_SIGNALS:
    /// Emitted when the user toggles a signal's checkbox. Default
    /// behavior wires this to the active chart's
    /// `addSignal` / `removeSignal`. Tests may also observe.
    void signalToggled(const QString& signalId, bool checked);

private:
    signalforge::buffer::SignalBufferRegistry* registry_;
    ChartManager* manager_;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace signalforge::chart
