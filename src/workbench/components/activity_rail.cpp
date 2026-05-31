// src/workbench/components/activity_rail.cpp

#include "workbench/components/activity_rail.hpp"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QToolButton>
#include <QVBoxLayout>

namespace signalforge::workbench {

ActivityRail::ActivityRail(QWidget* parent) : QFrame(parent) {
    setObjectName(QStringLiteral("activityRail"));
    layout_ = new QVBoxLayout(this);
    layout_->setContentsMargins(4, 8, 4, 8);
    layout_->setSpacing(4);
    layout_->addStretch(1);  // buttons insert above this; rail is top-aligned

    group_ = new QButtonGroup(this);
    group_->setExclusive(true);
    connect(group_, &QButtonGroup::buttonClicked, this, [this](QAbstractButton* button) {
        currentId_ = idOf_.value(button);
        Q_EMIT modeSelected(currentId_);
    });
}

ActivityRail::~ActivityRail() = default;

void ActivityRail::addMode(const QString& id, const QString& label, const QString& glyph) {
    auto* button = new QToolButton(this);
    button->setObjectName(QStringLiteral("railButton"));
    button->setCheckable(true);
    button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    button->setText(glyph.isEmpty() ? label : (glyph + QLatin1Char('\n') + label));
    button->setToolTip(label);
    button->setAutoRaise(true);

    group_->addButton(button);
    layout_->insertWidget(layout_->count() - 1, button);  // above the stretch
    buttons_.insert(id, button);
    idOf_.insert(button, id);

    if (buttons_.size() == 1) {
        button->setChecked(true);
        currentId_ = id;
    }
}

void ActivityRail::setCurrentMode(const QString& id) {
    if (auto* button = buttons_.value(id, nullptr)) {
        button->setChecked(true);
        currentId_ = id;
    }
}

int ActivityRail::modeCount() const {
    return static_cast<int>(buttons_.size());
}

QAbstractButton* ActivityRail::button(const QString& id) const {
    return buttons_.value(id, nullptr);
}

}  // namespace signalforge::workbench
