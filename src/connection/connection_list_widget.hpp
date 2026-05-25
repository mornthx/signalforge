// src/connection/connection_list_widget.hpp
#pragma once

#include "connection/connection.hpp"

#include <QColor>
#include <QStringList>
#include <QWidget>

class QFrame;
class QLabel;
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
///
/// M17 S2 — adds a `QFrame#panelHeader` title row above the list
/// (consumes the M16 `tokens.qss` `QFrame#panelHeader` rule) and
/// renders each row's text colour in the state-appropriate token
/// (`tokens::light::status*`). See
/// `docs/v0.3/widget-styling-guide.md` §4 and §6.2.
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

    /// Test hook: the panel-header `QFrame`. M17 S2 — asserts on
    /// `objectName() == "panelHeader"`.
    [[nodiscard]] QFrame* panelHeader() const noexcept {
        return header_;
    }

    /// M17 S2 — map a `Connection::State` to the corresponding
    /// `tokens::light::status*()` QColor. Public for test parity
    /// against `generated_style_tokens.hpp`.
    [[nodiscard]] static QColor colorForState(Connection::State s);

    /// M19 visual harness: render an existing row as if it were in
    /// `state` without mutating the underlying Connection state machine.
    /// Returns false if the row does not exist.
    [[nodiscard]] bool setVisualStateForTest(const QString& id, Connection::State state);

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
    void applyRowColor(QListWidgetItem* item, Connection::State state);
    [[nodiscard]] QListWidgetItem* findItem(const QString& id) const;
    static QString stateLabel(Connection::State s);
    static QString driverTypeLabel(DriverType t);

    ConnectionManager* manager_ = nullptr;
    QFrame* header_ = nullptr;
    QLabel* headerLabel_ = nullptr;
    QListWidget* list_ = nullptr;
    QPushButton* addBtn_ = nullptr;
    QPushButton* editBtn_ = nullptr;
    QPushButton* removeBtn_ = nullptr;
    QPushButton* connectBtn_ = nullptr;
    QPushButton* disconnectBtn_ = nullptr;
};

}  // namespace signalforge::connection
