// src/workbench/components/section_header.cpp

#include "workbench/components/section_header.hpp"

#include <QHBoxLayout>
#include <QLabel>

namespace signalforge::workbench {

SectionHeader::SectionHeader(const QString& title, QWidget* parent) : QFrame(parent) {
    setObjectName(QStringLiteral("panelHeader"));  // reuse the established header QSS
    layout_ = new QHBoxLayout(this);
    layout_->setContentsMargins(8, 4, 8, 4);
    layout_->setSpacing(8);

    titleLabel_ = new QLabel(title, this);
    titleLabel_->setProperty("class", QLatin1String("heading"));
    layout_->addWidget(titleLabel_);
    layout_->addStretch(1);

    captionLabel_ = new QLabel(this);
    captionLabel_->setProperty("class", QLatin1String("caption"));
    captionLabel_->hide();
    layout_->addWidget(captionLabel_);
}

SectionHeader::~SectionHeader() = default;

void SectionHeader::setTitle(const QString& title) {
    titleLabel_->setText(title);
}

QString SectionHeader::title() const {
    return titleLabel_->text();
}

void SectionHeader::setCaption(const QString& caption) {
    captionLabel_->setText(caption);
    captionLabel_->setVisible(!caption.isEmpty());
}

QString SectionHeader::caption() const {
    return captionLabel_->text();
}

void SectionHeader::addAction(QWidget* widget) {
    if (widget != nullptr) {
        layout_->addWidget(widget);
    }
}

}  // namespace signalforge::workbench
