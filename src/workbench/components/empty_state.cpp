// src/workbench/components/empty_state.cpp

#include "workbench/components/empty_state.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace signalforge::workbench {

EmptyState::EmptyState(QWidget* parent) : QFrame(parent) {
    setObjectName(QStringLiteral("emptyState"));

    auto* outer = new QVBoxLayout(this);
    outer->addStretch(1);

    auto* inner = new QVBoxLayout();
    inner->setContentsMargins(24, 18, 24, 18);
    inner->setSpacing(8);
    inner->setAlignment(Qt::AlignHCenter);

    title_ = new QLabel(this);
    title_->setProperty("class", QLatin1String("display"));
    title_->setAlignment(Qt::AlignHCenter);
    inner->addWidget(title_);

    caption_ = new QLabel(this);
    caption_->setProperty("class", QLatin1String("caption"));
    caption_->setWordWrap(true);
    caption_->setAlignment(Qt::AlignHCenter);
    inner->addWidget(caption_);

    actions_ = new QHBoxLayout();
    actions_->setSpacing(8);
    actions_->addStretch(1);
    actions_->addStretch(1);  // buttons insert between the two stretches → centered
    inner->addLayout(actions_);

    outer->addLayout(inner);
    outer->addStretch(1);
}

EmptyState::~EmptyState() = default;

void EmptyState::setTitle(const QString& title) {
    title_->setText(title);
}

void EmptyState::setCaption(const QString& caption) {
    caption_->setText(caption);
}

QPushButton* EmptyState::addAction(const QString& text, bool primary) {
    auto* button = new QPushButton(text, this);
    if (primary) {
        button->setProperty("class", QLatin1String("primary"));
    }
    actions_->insertWidget(actions_->count() - 1, button);  // before the trailing stretch
    return button;
}

}  // namespace signalforge::workbench
