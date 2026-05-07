// src/connection/connection_status_widget.cpp
//
// S1: stub. S6 implements label/click + wiring.

#include "connection/connection_status_widget.hpp"

namespace signalforge::connection {

ConnectionStatusWidget::ConnectionStatusWidget(ConnectionManager* manager, QWidget* parent)
    : QWidget(parent), manager_(manager) {}

ConnectionStatusWidget::~ConnectionStatusWidget() = default;

}  // namespace signalforge::connection
