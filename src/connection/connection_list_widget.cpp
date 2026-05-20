// src/connection/connection_list_widget.cpp

#include "connection/connection_list_widget.hpp"

#include "app/generated_style_tokens.hpp"
#include "connection/connection_manager.hpp"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>

namespace signalforge::connection {

namespace {

QListWidgetItem* makeItem(const QString& id, const QString& label) {
    auto* item = new QListWidgetItem(label);
    item->setData(Qt::UserRole, id);
    return item;
}

}  // namespace

ConnectionListWidget::ConnectionListWidget(ConnectionManager* manager, QWidget* parent)
    : QWidget(parent), manager_(manager) {
    // M17 S4 — stable objectName for visual-test + AT-SPI tooling.
    setObjectName(QStringLiteral("connectionListPanel"));

    auto* root = new QVBoxLayout(this);

    // M17 S2 — panel header. The QFrame#panelHeader QSS rule in
    // tokens.qss (M16 S2) supplies the background + border-bottom
    // styling automatically.
    header_ = new QFrame(this);
    header_->setObjectName(QStringLiteral("panelHeader"));
    auto* headerLayout = new QHBoxLayout(header_);
    headerLayout->setContentsMargins(8, 4, 8, 4);
    headerLabel_ = new QLabel(tr("Connections"), header_);
    headerLabel_->setProperty("class", QLatin1String("heading"));
    headerLayout->addWidget(headerLabel_);
    headerLayout->addStretch();
    root->addWidget(header_);

    list_ = new QListWidget(this);
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    root->addWidget(list_);

    auto* btnRow = new QHBoxLayout();
    addBtn_ = new QPushButton(tr("Add"), this);
    editBtn_ = new QPushButton(tr("Edit"), this);
    removeBtn_ = new QPushButton(tr("Remove"), this);
    connectBtn_ = new QPushButton(tr("Connect"), this);
    disconnectBtn_ = new QPushButton(tr("Disconnect"), this);
    btnRow->addWidget(addBtn_);
    btnRow->addWidget(editBtn_);
    btnRow->addWidget(removeBtn_);
    btnRow->addStretch();
    btnRow->addWidget(connectBtn_);
    btnRow->addWidget(disconnectBtn_);
    root->addLayout(btnRow);

    connect(addBtn_, &QPushButton::clicked, this, &ConnectionListWidget::onAddClicked);
    connect(editBtn_, &QPushButton::clicked, this, &ConnectionListWidget::onEditClicked);
    connect(removeBtn_, &QPushButton::clicked, this, &ConnectionListWidget::onRemoveClicked);
    connect(connectBtn_, &QPushButton::clicked, this, &ConnectionListWidget::onConnectClicked);
    connect(disconnectBtn_, &QPushButton::clicked, this, &ConnectionListWidget::onDisconnectClicked);
    connect(list_, &QListWidget::itemDoubleClicked, this, &ConnectionListWidget::onItemDoubleClicked);

    if (manager_) {
        connect(manager_, &ConnectionManager::connectionAdded, this, &ConnectionListWidget::onConnectionAdded);
        connect(manager_, &ConnectionManager::connectionRemoved, this, &ConnectionListWidget::onConnectionRemoved);
        connect(manager_, &ConnectionManager::connectionStateChanged, this,
                &ConnectionListWidget::onConnectionStateChanged);
        rebuild();
    }
}

ConnectionListWidget::~ConnectionListWidget() = default;

QColor ConnectionListWidget::colorForState(Connection::State s) {
    using namespace signalforge::tokens::light;
    switch (s) {
    case Connection::State::Idle:
        return statusIdle();
    case Connection::State::Connecting:
        return statusConnecting();
    case Connection::State::Connected:
        return statusConnected();
    case Connection::State::Disconnecting:
        return statusDisconnecting();
    case Connection::State::Error:
        return statusError();
    }
    return statusIdle();
}

void ConnectionListWidget::applyRowColor(QListWidgetItem* item, Connection::State state) {
    if (item == nullptr) {
        return;
    }
    item->setForeground(QBrush(colorForState(state)));
}

QString ConnectionListWidget::currentId() const {
    auto* item = list_->currentItem();
    if (!item) {
        return {};
    }
    return item->data(Qt::UserRole).toString();
}

void ConnectionListWidget::rebuild() {
    list_->clear();
    if (!manager_) {
        return;
    }
    for (const QString& id : manager_->connectionIds()) {
        Connection* c = manager_->connection(id);
        if (!c) {
            continue;
        }
        const QString label = QStringLiteral("%1 [%2] — %3")
                                  .arg(c->config().displayName.isEmpty() ? id : c->config().displayName)
                                  .arg(driverTypeLabel(c->config().driverType))
                                  .arg(stateLabel(c->state()));
        auto* item = makeItem(id, label);
        applyRowColor(item, c->state());
        list_->addItem(item);
    }
}

void ConnectionListWidget::updateRow(const QString& id) {
    auto* item = findItem(id);
    if (!item || !manager_) {
        return;
    }
    Connection* c = manager_->connection(id);
    if (!c) {
        return;
    }
    item->setText(QStringLiteral("%1 [%2] — %3")
                      .arg(c->config().displayName.isEmpty() ? id : c->config().displayName)
                      .arg(driverTypeLabel(c->config().driverType))
                      .arg(stateLabel(c->state())));
    applyRowColor(item, c->state());
}

QListWidgetItem* ConnectionListWidget::findItem(const QString& id) const {
    for (int i = 0; i < list_->count(); ++i) {
        if (list_->item(i)->data(Qt::UserRole).toString() == id) {
            return list_->item(i);
        }
    }
    return nullptr;
}

QString ConnectionListWidget::stateLabel(Connection::State s) {
    switch (s) {
    case Connection::State::Idle:
        return tr("Idle");
    case Connection::State::Connecting:
        return tr("Connecting…");
    case Connection::State::Connected:
        return tr("Connected");
    case Connection::State::Disconnecting:
        return tr("Disconnecting…");
    case Connection::State::Error:
        return tr("Error");
    }
    return QStringLiteral("?");
}

QString ConnectionListWidget::driverTypeLabel(DriverType t) {
    switch (t) {
    case DriverType::Serial:
        return tr("Serial");
    case DriverType::Tcp:
        return tr("TCP");
    case DriverType::Udp:
        return tr("UDP");
    case DriverType::Replay:
        return tr("Replay");
    }
    return QStringLiteral("?");
}

void ConnectionListWidget::onConnectionAdded(const QString& id) {
    if (findItem(id)) {
        return;
    }
    Connection* c = manager_->connection(id);
    if (!c) {
        return;
    }
    const QString label = QStringLiteral("%1 [%2] — %3")
                              .arg(c->config().displayName.isEmpty() ? id : c->config().displayName)
                              .arg(driverTypeLabel(c->config().driverType))
                              .arg(stateLabel(c->state()));
    auto* item = makeItem(id, label);
    applyRowColor(item, c->state());
    list_->addItem(item);
}

void ConnectionListWidget::onConnectionRemoved(const QString& id) {
    auto* item = findItem(id);
    if (!item) {
        return;
    }
    delete list_->takeItem(list_->row(item));
}

void ConnectionListWidget::onConnectionStateChanged(const QString& id, Connection::State /*state*/) {
    updateRow(id);
}

void ConnectionListWidget::onAddClicked() {
    Q_EMIT addRequested();
}

void ConnectionListWidget::onEditClicked() {
    const QString id = currentId();
    if (id.isEmpty()) {
        return;
    }
    Q_EMIT editRequested(id);
}

void ConnectionListWidget::onRemoveClicked() {
    const QString id = currentId();
    if (id.isEmpty() || !manager_) {
        return;
    }
    (void)manager_->removeConnection(id);
}

void ConnectionListWidget::onConnectClicked() {
    const QString id = currentId();
    if (id.isEmpty() || !manager_) {
        return;
    }
    (void)manager_->connectConnection(id);
}

void ConnectionListWidget::onDisconnectClicked() {
    const QString id = currentId();
    if (id.isEmpty() || !manager_) {
        return;
    }
    (void)manager_->disconnectConnection(id);
}

void ConnectionListWidget::onItemDoubleClicked(QListWidgetItem* item) {
    if (!item) {
        return;
    }
    Q_EMIT editRequested(item->data(Qt::UserRole).toString());
}

}  // namespace signalforge::connection
