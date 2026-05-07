// src/connection/connection_list_widget.hpp
#pragma once

#include "connection/connection.hpp"

#include <QStringList>
#include <QWidget>

class QListWidget;
class QListWidgetItem;
class QPushButton;

namespace signalforge::connection {

class ConnectionManager;

/// `QListWidget`-based panel showing one row per connection,
/// with per-row Connect / Disconnect / Edit / Remove buttons
/// plus a global Add button. Wires to `ConnectionManager`'s
/// `connectionAdded` / `connectionRemoved` /
/// `connectionStateChanged` signals to keep the view in sync.
///
/// The widget does not own connections — `ConnectionManager`
/// does. The widget emits `editRequested` / `addRequested` so
/// the caller (typically `MainWindow`) can pop a
/// `ConnectionDialog` and feed the result back to the manager.
class ConnectionListWidget : public QWidget {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ConnectionListWidget)

public:
    explicit ConnectionListWidget(ConnectionManager* manager, QWidget* parent = nullptr);
    ~ConnectionListWidget() override;

    /// Currently selected connection id, or empty if none.
    [[nodiscard]] QString currentId() const;

    /// Test hook: the inner list widget. Useful for selecting
    /// rows / counting items in unit tests.
    [[nodiscard]] QListWidget* listWidget() const noexcept {
        return list_;
    }

signals:
    /// Emitted when the user clicks "Add" on the toolbar.
    void addRequested();

    /// Emitted when the user double-clicks a row or clicks "Edit".
    void editRequested(const QString& id);

private slots:
    void onConnectionAdded(const QString& id);
    void onConnectionRemoved(const QString& id);
    void onConnectionStateChanged(const QString& id, signalforge::connection::Connection::State state);
    void onAddClicked();
    void onEditClicked();
    void onRemoveClicked();
    void onConnectClicked();
    void onDisconnectClicked();
    void onItemDoubleClicked(QListWidgetItem* item);

private:
    void rebuild();
    void updateRow(const QString& id);
    [[nodiscard]] QListWidgetItem* findItem(const QString& id) const;
    static QString stateLabel(Connection::State s);
    static QString driverTypeLabel(DriverType t);

    ConnectionManager* manager_ = nullptr;
    QListWidget* list_ = nullptr;
    QPushButton* addBtn_ = nullptr;
    QPushButton* editBtn_ = nullptr;
    QPushButton* removeBtn_ = nullptr;
    QPushButton* connectBtn_ = nullptr;
    QPushButton* disconnectBtn_ = nullptr;
};

}  // namespace signalforge::connection
