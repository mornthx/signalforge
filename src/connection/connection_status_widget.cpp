// src/connection/connection_status_widget.cpp

#include "connection/connection_status_widget.hpp"

#include "connection/connection.hpp"
#include "connection/connection_manager.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QStyle>

namespace signalforge::connection {

ConnectionStatusWidget::ConnectionStatusWidget(ConnectionManager* manager, QWidget* parent)
    : QWidget(parent), manager_(manager) {
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(4, 0, 4, 0);
    label_ = new QLabel(this);
    label_->setObjectName(QStringLiteral("connectionStatusLabel"));
    label_->setToolTip(tr("Click to open the connection list."));
    row->addWidget(label_);

    if (manager_) {
        connect(manager_, &ConnectionManager::connectionAdded, this, &ConnectionStatusWidget::refresh);
        connect(manager_, &ConnectionManager::connectionRemoved, this, &ConnectionStatusWidget::refresh);
        connect(manager_, &ConnectionManager::connectionStateChanged, this, &ConnectionStatusWidget::refresh);
    }
    refresh();
}

ConnectionStatusWidget::~ConnectionStatusWidget() = default;

const char* ConnectionStatusWidget::classForState(AggregateState s) noexcept {
    switch (s) {
    case AggregateState::Error:
        return "status-error";
    case AggregateState::Connecting:
        return "status-connecting";
    case AggregateState::Connected:
        return "status-connected";
    case AggregateState::Idle:
        return "status-idle";
    }
    return "status-idle";
}

void ConnectionStatusWidget::refresh() {
    if (!manager_) {
        label_->setText(tr("0/0 connected"));
        aggregateState_ = AggregateState::Idle;
        label_->setProperty("class", QLatin1String(classForState(aggregateState_)));
        label_->style()->unpolish(label_);
        label_->style()->polish(label_);
        label_->update();
        return;
    }

    const auto connected = manager_->connectedCount();
    const auto total = manager_->connectionCount();
    const auto errored = manager_->erroredCount();
    if (errored == 0) {
        label_->setText(tr("%1/%2 connected").arg(connected).arg(total));
    } else {
        label_->setText(tr("%1/%2 connected · errors: %3").arg(connected).arg(total).arg(errored));
    }

    // Aggregate-state computation (M17 spec §6.1).
    AggregateState next = AggregateState::Idle;
    if (errored > 0) {
        next = AggregateState::Error;
    } else {
        bool anyTransitioning = false;
        const auto ids = manager_->connectionIds();
        for (const QString& id : ids) {
            const Connection* c = manager_->connection(id);
            if (c == nullptr) {
                continue;
            }
            const auto s = c->state();
            if (s == Connection::State::Connecting || s == Connection::State::Disconnecting) {
                anyTransitioning = true;
                break;
            }
        }
        if (anyTransitioning) {
            next = AggregateState::Connecting;
        } else if (total > 0 && connected == total) {
            next = AggregateState::Connected;
        } else {
            next = AggregateState::Idle;
        }
    }

    if (next != aggregateState_) {
        aggregateState_ = next;
    }
    // Always re-apply the property; polish/unpolish is cheap and
    // covers the case where the QSS stylesheet was reloaded
    // (M16 theme-switch slot) without a state change.
    label_->setProperty("class", QLatin1String(classForState(aggregateState_)));
    label_->style()->unpolish(label_);
    label_->style()->polish(label_);
    label_->update();
}

void ConnectionStatusWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        Q_EMIT clicked();
    }
    QWidget::mousePressEvent(event);
}

}  // namespace signalforge::connection
