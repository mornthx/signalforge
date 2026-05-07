// src/connection/connection_status_widget.hpp
#pragma once

#include <QWidget>

namespace signalforge::connection {

class ConnectionManager;

/// Compact status-bar widget extending the M8 status bar. Shows
/// "X/N connected" plus an error indicator when any connection
/// is in `Error`. Click → emits `clicked()` so callers can open
/// the main connection dialog.
///
/// S1 declares the public surface; S6 implements the labels +
/// click handling and wires to the `ConnectionManager` signals.
class ConnectionStatusWidget : public QWidget {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ConnectionStatusWidget)

public:
    explicit ConnectionStatusWidget(ConnectionManager* manager, QWidget* parent = nullptr);
    ~ConnectionStatusWidget() override;

signals:
    /// Emitted when the user clicks the status widget. Caller is
    /// expected to open the connection management UI.
    void clicked();

private:
    ConnectionManager* manager_ = nullptr;
};

}  // namespace signalforge::connection
