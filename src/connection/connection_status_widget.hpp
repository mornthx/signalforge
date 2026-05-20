// src/connection/connection_status_widget.hpp
#pragma once

#include <QWidget>

class QLabel;

namespace signalforge::connection {

class ConnectionManager;

/// Compact status-bar widget extending the M8 status bar. Shows
/// "X/N connected" plus an "errors: K" indicator when any
/// connection is in `Error`. Click → emits `clicked()` so callers
/// can open the main connection list / management UI.
///
/// M17 S1 — the inner `QLabel` sets a `class` dynamic property
/// reflecting an aggregate state across all managed connections;
/// `tokens.qss` (M16 S2) renders the label in the corresponding
/// `status-*` colour. See `docs/v0.3/widget-styling-guide.md` §3.
class ConnectionStatusWidget : public QWidget {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ConnectionStatusWidget)

public:
    /// Aggregate state across all managed connections. Drives the
    /// inner label's `class` QSS dynamic property.
    ///
    /// Precedence (highest priority wins) — M17 spec §6.1:
    ///   1. any Error                                  → Error
    ///   2. any Connecting or Disconnecting            → Connecting
    ///   3. N ≥ 1 && all in Connected                  → Connected
    ///   4. otherwise                                  → Idle
    enum class AggregateState : int {
        Idle = 0,
        Connecting = 1,
        Connected = 2,
        Error = 3,
    };

    explicit ConnectionStatusWidget(ConnectionManager* manager, QWidget* parent = nullptr);
    ~ConnectionStatusWidget() override;

    /// Test hook: the inner label widget.
    [[nodiscard]] QLabel* label() const noexcept {
        return label_;
    }

    /// Test hook: the current aggregate state. Updated whenever
    /// `refresh()` runs (i.e., on every connection lifecycle signal).
    [[nodiscard]] AggregateState aggregateState() const noexcept {
        return aggregateState_;
    }

signals:
    /// Emitted when the user clicks the widget. Callers are
    /// expected to surface the connection list / dialog.
    void clicked();

protected:
    void mousePressEvent(QMouseEvent* event) override;

private slots:
    void refresh();

private:
    /// Map an `AggregateState` to the `tokens.qss` class string.
    [[nodiscard]] static const char* classForState(AggregateState s) noexcept;

    ConnectionManager* manager_ = nullptr;
    QLabel* label_ = nullptr;
    AggregateState aggregateState_ = AggregateState::Idle;
};

}  // namespace signalforge::connection
