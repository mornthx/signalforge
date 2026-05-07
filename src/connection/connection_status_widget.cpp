// src/connection/connection_status_widget.cpp

#include "connection/connection_status_widget.hpp"

#include "connection/connection_manager.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>

namespace signalforge::connection {

ConnectionStatusWidget::ConnectionStatusWidget(ConnectionManager* manager, QWidget* parent)
    : QWidget(parent), manager_(manager) {
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(4, 0, 4, 0);
    label_ = new QLabel(this);
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

void ConnectionStatusWidget::refresh() {
    if (!manager_) {
        label_->setText(tr("0/0 connected"));
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
}

void ConnectionStatusWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        Q_EMIT clicked();
    }
    QWidget::mousePressEvent(event);
}

}  // namespace signalforge::connection
