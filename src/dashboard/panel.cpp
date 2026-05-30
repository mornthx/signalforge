// src/dashboard/panel.cpp
//
// Panel base class: the card chrome (header + remove button) shared by
// every dashboard panel type. The body is supplied by subclasses.

#include "dashboard/panel.hpp"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <algorithm>

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
    case PanelType::Bar:
        return QStringLiteral("bar");
    case PanelType::Gauge:
        return QStringLiteral("gauge");
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
    if (n == QStringLiteral("bar")) {
        return PanelType::Bar;
    }
    if (n == QStringLiteral("gauge")) {
        return PanelType::Gauge;
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

    header_ = new QFrame(this);
    header_->setObjectName(QStringLiteral("panelHeader"));
    header_->setCursor(Qt::SizeAllCursor);  // hint: drag the header to move
    auto* headerLayout = new QHBoxLayout(header_);
    headerLayout->setContentsMargins(8, 4, 8, 4);

    titleLabel_ = new QLabel(config_.title, header_);
    titleLabel_->setProperty("class", QLatin1String("heading"));
    // Let header drags pass through the title text to the header drag handle.
    titleLabel_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    headerLayout->addWidget(titleLabel_);
    headerLayout->addStretch(1);

    // Always-visible per-panel config button (⋮). The owning Dashboard
    // builds the menu (change type / assign signals / remove). (M27 #2/#3.)
    configButton_ = new QPushButton(QStringLiteral("⋮"), header_);
    configButton_->setObjectName(QStringLiteral("panelMenuButton"));
    configButton_->setToolTip(tr("Configure this panel"));
    configButton_->setFlat(true);
    configButton_->setFixedWidth(24);
    headerLayout->addWidget(configButton_);
    connect(configButton_, &QPushButton::clicked, this, [this]() { Q_EMIT configureRequested(config_.id); });

    rootLayout_->addWidget(header_);

    // Bottom-right resize grip (M28: free-form drag-resize). Positioned in
    // resizeEvent; drives a resize via eventFilter.
    grip_ = new QWidget(this);
    grip_->setObjectName(QStringLiteral("panelResizeGrip"));
    grip_->setFixedSize(14, 14);
    grip_->setCursor(Qt::SizeFDiagCursor);
    grip_->setToolTip(tr("Drag to resize"));

    header_->installEventFilter(this);
    grip_->installEventFilter(this);
}

Panel::~Panel() = default;

void Panel::resizeEvent(QResizeEvent* event) {
    QFrame::resizeEvent(event);
    if (grip_ != nullptr) {
        grip_->move(width() - grip_->width() - 2, height() - grip_->height() - 2);
        grip_->raise();
    }
}

bool Panel::eventFilter(QObject* watched, QEvent* event) {
    if (watched == header_ || watched == grip_) {
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                dragMode_ = (watched == grip_) ? DragMode::Resize : DragMode::Move;
                dragStartGlobal_ = me->globalPosition().toPoint();
                dragStartGeom_ = geometry();
                return true;
            }
            break;
        }
        case QEvent::MouseMove: {
            if (dragMode_ == DragMode::None) {
                break;
            }
            auto* me = static_cast<QMouseEvent*>(event);
            const QPoint delta = me->globalPosition().toPoint() - dragStartGlobal_;
            if (dragMode_ == DragMode::Move) {
                QPoint np = dragStartGeom_.topLeft() + delta;
                if (auto* p = parentWidget(); p != nullptr) {
                    np.setX(std::clamp(np.x(), 0, std::max(0, p->width() - width())));
                    np.setY(std::clamp(np.y(), 0, std::max(0, p->height() - height())));
                }
                move(np);
            } else {
                QSize ns = dragStartGeom_.size() + QSize(delta.x(), delta.y());
                resize(ns.expandedTo(QSize(140, 80)));
            }
            return true;
        }
        case QEvent::MouseButtonRelease: {
            if (dragMode_ != DragMode::None) {
                dragMode_ = DragMode::None;
                config_.geometry = geometry();
                Q_EMIT geometryChanged(config_.id);
                return true;
            }
            break;
        }
        default:
            break;
        }
    }
    return QFrame::eventFilter(watched, event);
}

bool Panel::hasSignal(const QString& signalId) const {
    return config_.signalIds.contains(signalId);
}

QPushButton* Panel::configButton() const {
    return configButton_;
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
