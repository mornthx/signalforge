// src/dashboard/panel.cpp
//
// Panel base class: the card chrome (header + remove button) shared by
// every dashboard panel type. The body is supplied by subclasses.

#include "dashboard/panel.hpp"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace signalforge::dashboard {

QString panelTypeName(PanelType type) {
    switch (type) {
    case PanelType::Plot:
        return QStringLiteral("plot");
    case PanelType::Numeric:
        return QStringLiteral("numeric");
    case PanelType::State:
        return QStringLiteral("state");
    case PanelType::Table:
        return QStringLiteral("table");
    }
    return QStringLiteral("numeric");
}

std::optional<PanelType> panelTypeFromName(const QString& name) {
    const QString n = name.trimmed().toLower();
    if (n == QStringLiteral("plot")) {
        return PanelType::Plot;
    }
    if (n == QStringLiteral("numeric")) {
        return PanelType::Numeric;
    }
    if (n == QStringLiteral("state")) {
        return PanelType::State;
    }
    if (n == QStringLiteral("table")) {
        return PanelType::Table;
    }
    return std::nullopt;
}

Panel::Panel(PanelConfig config, QWidget* parent) : QFrame(parent), config_(std::move(config)) {
    // Reuse the M16/M17 panel chrome so panels match existing styling:
    // QFrame#chartFrame body + QFrame#panelHeader header bar.
    setObjectName(QStringLiteral("chartFrame"));
    setFrameShape(QFrame::NoFrame);

    rootLayout_ = new QVBoxLayout(this);
    rootLayout_->setContentsMargins(0, 0, 0, 0);
    rootLayout_->setSpacing(0);

    auto* header = new QFrame(this);
    header->setObjectName(QStringLiteral("panelHeader"));
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(8, 4, 8, 4);

    titleLabel_ = new QLabel(config_.title, header);
    titleLabel_->setProperty("class", QLatin1String("heading"));
    headerLayout->addWidget(titleLabel_);
    headerLayout->addStretch(1);

    removeButton_ = new QPushButton(tr("Remove"), header);
    removeButton_->setToolTip(tr("Remove this panel"));
    headerLayout->addWidget(removeButton_);
    connect(removeButton_, &QPushButton::clicked, this, [this]() { Q_EMIT removeRequested(config_.id); });

    rootLayout_->addWidget(header);
}

Panel::~Panel() = default;

bool Panel::hasSignal(const QString& signalId) const {
    return config_.signalIds.contains(signalId);
}

void Panel::setEditMode(bool on) {
    if (removeButton_ != nullptr) {
        removeButton_->setVisible(on);
    }
}

void Panel::setBody(QWidget* body) {
    if (body_ != nullptr) {
        rootLayout_->removeWidget(body_);
        body_->deleteLater();
    }
    body_ = body;
    if (body_ != nullptr) {
        body_->setParent(this);
        rootLayout_->addWidget(body_, 1);
    }
}

void Panel::setHeaderTitle(const QString& title) {
    if (titleLabel_ != nullptr) {
        titleLabel_->setText(title);
    }
}

}  // namespace signalforge::dashboard
