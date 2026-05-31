// src/workbench/components/segmented_control.cpp

#include "workbench/components/segmented_control.hpp"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QToolButton>

namespace signalforge::workbench {

SegmentedControl::SegmentedControl(QWidget* parent) : QFrame(parent) {
    setObjectName(QStringLiteral("segmentedControl"));
    layout_ = new QHBoxLayout(this);
    layout_->setContentsMargins(0, 0, 0, 0);
    layout_->setSpacing(4);
    layout_->addStretch(1);  // segments insert before this; row is left-aligned

    group_ = new QButtonGroup(this);
    group_->setExclusive(true);
    connect(group_, &QButtonGroup::buttonClicked, this, [this](QAbstractButton* button) {
        currentId_ = idOf_.value(button);
        Q_EMIT segmentSelected(currentId_);
    });
}

SegmentedControl::~SegmentedControl() = default;

void SegmentedControl::addSegment(const QString& id, const QString& label) {
    auto* button = new QToolButton(this);
    button->setObjectName(QStringLiteral("segmentButton"));
    button->setCheckable(true);
    button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    button->setText(label);
    button->setAutoRaise(true);

    group_->addButton(button);
    layout_->insertWidget(layout_->count() - 1, button);  // before the trailing stretch
    buttons_.insert(id, button);
    idOf_.insert(button, id);

    if (buttons_.size() == 1) {
        button->setChecked(true);
        currentId_ = id;
    }
}

void SegmentedControl::setCurrentSegment(const QString& id) {
    if (auto* button = buttons_.value(id, nullptr)) {
        button->setChecked(true);
        currentId_ = id;
    }
}

int SegmentedControl::segmentCount() const {
    return static_cast<int>(buttons_.size());
}

QAbstractButton* SegmentedControl::button(const QString& id) const {
    return buttons_.value(id, nullptr);
}

}  // namespace signalforge::workbench
