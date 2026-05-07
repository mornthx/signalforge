// src/connection/connection_list_widget.cpp
//
// S1: stub. S6 implements rows + actions.

#include "connection/connection_list_widget.hpp"

namespace signalforge::connection {

ConnectionListWidget::ConnectionListWidget(ConnectionManager* manager, QWidget* parent)
    : QWidget(parent), manager_(manager) {}

ConnectionListWidget::~ConnectionListWidget() = default;

}  // namespace signalforge::connection
