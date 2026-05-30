// src/workbench/selection_model.cpp

#include "workbench/selection_model.hpp"

namespace signalforge::workbench {

SelectionModel::SelectionModel(QObject* parent) : QObject(parent) {}

void SelectionModel::select(SelectionKind kind, const QString& id) {
    Selection next{kind, id};
    if (next == current_) {
        return;
    }
    current_ = std::move(next);
    Q_EMIT selectionChanged(current_);
}

void SelectionModel::clear() {
    if (current_.isNone()) {
        return;
    }
    current_ = Selection{};
    Q_EMIT selectionChanged(current_);
}

bool SelectionModel::isSelected(SelectionKind kind, const QString& id) const {
    return current_.kind == kind && current_.id == id;
}

}  // namespace signalforge::workbench
