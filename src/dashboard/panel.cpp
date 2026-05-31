// src/dashboard/panel.cpp
//
// Panel base class: the header-less card chrome shared by every dashboard
// panel type. An identity strip (signal name + source) sits above a body
// supplied by subclasses. The whole card is draggable (left button); the
// bottom-right grip resizes; right-click opens the config menu.

#include "dashboard/panel.hpp"

#include <QContextMenuEvent>
#include <QEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QResizeEvent>
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

namespace {
/// The source (driver) portion of a signal id ("udp:rig/temp" -> "udp:rig").
QString sourceOf(const QString& signalId) {
    const int slash = signalId.indexOf(QLatin1Char('/'));
    return slash > 0 ? signalId.left(slash) : signalId;
}
}  // namespace

Panel::Panel(PanelConfig config, QWidget* parent) : QFrame(parent), config_(std::move(config)) {
    // Reuse the M16/M17 card styling (QFrame#chartFrame).
    setObjectName(QStringLiteral("chartFrame"));
    setFrameShape(QFrame::NoFrame);
    setCursor(Qt::SizeAllCursor);  // hint: the whole card is the drag handle

    rootLayout_ = new QVBoxLayout(this);
    rootLayout_->setContentsMargins(8, 6, 8, 6);
    rootLayout_->setSpacing(2);

    // In-card identity strip (replaces the old header bar): signal name +
    // its source/driver. Transparent to the mouse so the card stays draggable.
    nameLabel_ = new QLabel(config_.title, this);
    nameLabel_->setProperty("class", QLatin1String("heading"));
    nameLabel_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    rootLayout_->addWidget(nameLabel_);

    sourceLabel_ = new QLabel(this);
    sourceLabel_->setObjectName(QStringLiteral("panelSource"));
    sourceLabel_->setProperty("class", QLatin1String("caption"));
    sourceLabel_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    rootLayout_->addWidget(sourceLabel_);
    updateSourceLabel();

    // Bottom-right resize grip (positioned in resizeEvent).
    grip_ = new QWidget(this);
    grip_->setObjectName(QStringLiteral("panelResizeGrip"));
    grip_->setFixedSize(14, 14);
    grip_->setCursor(Qt::SizeFDiagCursor);
    grip_->setToolTip(tr("Drag to resize"));
    grip_->installEventFilter(this);
}

Panel::~Panel() = default;

void Panel::updateSourceLabel() {
    if (sourceLabel_ == nullptr) {
        return;
    }
    QString text;
    if (config_.signalIds.size() == 1) {
        text = sourceOf(config_.signalIds.first());
    } else if (config_.signalIds.size() > 1) {
        text = tr("%1 signals").arg(config_.signalIds.size());
    } else {
        text = tr("no signal");
    }
    sourceLabel_->setText(text);
}

void Panel::resizeEvent(QResizeEvent* event) {
    QFrame::resizeEvent(event);
    if (grip_ != nullptr) {
        grip_->move(width() - grip_->width() - 2, height() - grip_->height() - 2);
        grip_->raise();
    }
}

void Panel::startDrag(DragMode mode, const QPoint& globalPos) {
    dragMode_ = mode;
    dragStartGlobal_ = globalPos;
    dragStartGeom_ = geometry();
}

void Panel::updateDrag(const QPoint& globalPos) {
    if (dragMode_ == DragMode::None) {
        return;
    }
    const QPoint delta = globalPos - dragStartGlobal_;
    // Propose a geometry; the Dashboard clamps to the surface and pushes
    // overlapping neighbors (or refuses), then drives setUserGeometry.
    QRect proposed = dragStartGeom_;
    if (dragMode_ == DragMode::Move) {
        proposed.moveTopLeft(dragStartGeom_.topLeft() + delta);
    } else {
        proposed.setSize((dragStartGeom_.size() + QSize(delta.x(), delta.y())).expandedTo(QSize(140, 80)));
    }
    Q_EMIT dragProposed(config_.id, proposed);
}

void Panel::endDrag() {
    if (dragMode_ == DragMode::None) {
        return;
    }
    dragMode_ = DragMode::None;
    config_.geometry = geometry();
    Q_EMIT geometryChanged(config_.id);
}

void Panel::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        Q_EMIT selected(config_.id);  // a press selects the panel (then may drag)
        startDrag(DragMode::Move, event->globalPosition().toPoint());
        event->accept();
        return;
    }
    QFrame::mousePressEvent(event);
}

void Panel::mouseMoveEvent(QMouseEvent* event) {
    if (dragMode_ != DragMode::None) {
        updateDrag(event->globalPosition().toPoint());
        event->accept();
        return;
    }
    QFrame::mouseMoveEvent(event);
}

void Panel::mouseReleaseEvent(QMouseEvent* event) {
    if (dragMode_ != DragMode::None) {
        endDrag();
        event->accept();
        return;
    }
    QFrame::mouseReleaseEvent(event);
}

void Panel::contextMenuEvent(QContextMenuEvent* event) {
    Q_EMIT configureRequested(config_.id);
    event->accept();
}

bool Panel::eventFilter(QObject* watched, QEvent* event) {
    // The grip resizes; any other watched widget is a body descendant whose
    // mouse events we route into a card move/right-click so the whole card is
    // draggable/configurable even over children that would consume the event.
    const bool isGrip = (watched == grip_);
    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            startDrag(isGrip ? DragMode::Resize : DragMode::Move, me->globalPosition().toPoint());
            return true;
        }
        break;  // right button → let ContextMenu fire
    }
    case QEvent::MouseMove:
        if (dragMode_ != DragMode::None) {
            updateDrag(static_cast<QMouseEvent*>(event)->globalPosition().toPoint());
            return true;
        }
        break;
    case QEvent::MouseButtonRelease:
        if (dragMode_ != DragMode::None) {
            endDrag();
            return true;
        }
        break;
    case QEvent::ContextMenu:
        Q_EMIT configureRequested(config_.id);
        return true;
    default:
        break;
    }
    return QFrame::eventFilter(watched, event);
}

bool Panel::hasSignal(const QString& signalId) const {
    return config_.signalIds.contains(signalId);
}

void Panel::setUserGeometry(const QRect& geometry) {
    setGeometry(geometry);
    config_.geometry = geometry;
}

void Panel::installDragFilter(QWidget* widget) {
    widget->installEventFilter(this);
    const QObjectList kids = widget->children();
    for (QObject* child : kids) {
        if (auto* childWidget = qobject_cast<QWidget*>(child)) {
            installDragFilter(childWidget);
        }
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
        installDragFilter(body_);  // drag the card from anywhere over the body
    }
}

void Panel::setHeaderTitle(const QString& title) {
    if (nameLabel_ != nullptr) {
        nameLabel_->setText(title);
    }
}

}  // namespace signalforge::dashboard
