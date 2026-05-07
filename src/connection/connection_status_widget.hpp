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
class ConnectionStatusWidget : public QWidget {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ConnectionStatusWidget)

public:
    explicit ConnectionStatusWidget(ConnectionManager* manager, QWidget* parent = nullptr);
    ~ConnectionStatusWidget() override;

    /// Test hook: the inner label widget.
    [[nodiscard]] QLabel* label() const noexcept {
        return label_;
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
    ConnectionManager* manager_ = nullptr;
    QLabel* label_ = nullptr;
};

}  // namespace signalforge::connection
