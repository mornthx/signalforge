// src/workbench/components/status_chip.cpp

#include "workbench/components/status_chip.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>

namespace signalforge::workbench {

StatusChip::StatusChip(QWidget* parent) : QFrame(parent) {
    setObjectName(QStringLiteral("statusChip"));
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 2, 8, 2);
    layout->setSpacing(4);
    label_ = new QLabel(this);
    label_->setObjectName(QStringLiteral("statusChipLabel"));
    layout->addWidget(label_);
    setProperty("state", QString());
}

StatusChip::StatusChip(const QString& text, const QString& state, QWidget* parent) : StatusChip(parent) {
    setText(text);
    setState(state);
}

StatusChip::~StatusChip() = default;

void StatusChip::setText(const QString& text) {
    label_->setText(text);
}

QString StatusChip::text() const {
    return label_->text();
}

void StatusChip::setState(const QString& state) {
    if (state_ == state) {
        return;
    }
    state_ = state;
    setProperty("state", state);
    // Re-polish so a QSS [state="…"] rule re-applies.
    style()->unpolish(this);
    style()->polish(this);
}

}  // namespace signalforge::workbench
