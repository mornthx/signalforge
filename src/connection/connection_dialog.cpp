// src/connection/connection_dialog.cpp
//
// S1: stub. S5 implements widgets + validation.

#include "connection/connection_dialog.hpp"

namespace signalforge::connection {

ConnectionDialog::ConnectionDialog(QStringList availableSchemaIds, QWidget* parent)
    : QDialog(parent), availableSchemaIds_(std::move(availableSchemaIds)) {}

ConnectionDialog::~ConnectionDialog() = default;

void ConnectionDialog::setConfig(const ConnectionConfig& /*config*/) {
    // S5 fills in.
}

ConnectionConfig ConnectionDialog::config() const {
    // S5 fills in.
    return {};
}

bool ConnectionDialog::isValid() const {
    // S5 fills in.
    return false;
}

}  // namespace signalforge::connection
