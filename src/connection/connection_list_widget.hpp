// src/connection/connection_list_widget.hpp
#pragma once

#include <QWidget>

namespace signalforge::connection {

class ConnectionManager;

/// `QListView`-based widget showing one row per connection
/// (driver-type icon + display name + status badge). Provides
/// per-row Connect / Disconnect / Edit / Remove actions plus a
/// global Add button. Wires to `ConnectionManager` signals so the
/// view rebuilds on add/remove/state change.
///
/// S1 declares the public surface; S6 implements the model, the
/// row delegate, and the action wiring.
class ConnectionListWidget : public QWidget {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ConnectionListWidget)

public:
    explicit ConnectionListWidget(ConnectionManager* manager, QWidget* parent = nullptr);
    ~ConnectionListWidget() override;

private:
    ConnectionManager* manager_ = nullptr;
};

}  // namespace signalforge::connection
