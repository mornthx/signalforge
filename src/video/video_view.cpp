// src/video/video_view.cpp
#include "video/video_view.hpp"

#include <QColor>
#include <QFontMetrics>
#include <QLatin1Char>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QRect>
#include <QStringList>
#include <QWheelEvent>
#include <QtGlobal>
#include <algorithm>

namespace signalforge::video {

VideoView::VideoView(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent);  // we fill the whole surface each paint
    setMinimumSize(160, 90);
    setMouseTracking(true);  // hover pixel-probe without a button held
}

void VideoView::setFrame(const QImage& frame) {
    if (frozen_) {
        return;  // keep the frozen image
    }
    frame_ = frame;
    update();
}

void VideoView::setFrozen(bool frozen) {
    frozen_ = frozen;
}

bool VideoView::isFrozen() const noexcept {
    return frozen_;
}

void VideoView::clearFrame() {
    if (frame_.isNull()) {
        return;
    }
    frame_ = QImage();
    update();
}

QImage VideoView::currentFrame() const {
    return frame_;
}

bool VideoView::hasFrame() const noexcept {
    return !frame_.isNull();
}

void VideoView::setPlaceholderText(const QString& text) {
    placeholder_ = text;
    if (frame_.isNull()) {
        update();
    }
}

QString VideoView::placeholderText() const {
    return placeholder_;
}

void VideoView::setOverlayText(const QString& text) {
    if (overlay_ == text) {
        return;
    }
    overlay_ = text;
    if (overlayVisible_) {
        update();
    }
}

QString VideoView::overlayText() const {
    return overlay_;
}

void VideoView::setOverlayVisible(bool visible) {
    if (overlayVisible_ == visible) {
        return;
    }
    overlayVisible_ = visible;
    update();
}

bool VideoView::isOverlayVisible() const noexcept {
    return overlayVisible_;
}

QSize VideoView::sizeHint() const {
    return {640, 360};
}

void VideoView::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.fillRect(rect(), QColor(0x14, 0x14, 0x16));

    if (frame_.isNull()) {
        p.setPen(QColor(0x88, 0x88, 0x90));
        p.drawText(rect(), Qt::AlignCenter, placeholder_);
    } else {
        // Aspect-preserving fit (zoom=1) or zoomed/panned view.
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.drawImage(targetRect(), frame_);
    }

    if (overlayVisible_ && !overlay_.isEmpty()) {
        paintOverlay(p);
    }
}

void VideoView::paintOverlay(QPainter& p) {
    const QStringList lines = overlay_.split(QLatin1Char('\n'));
    const QFontMetrics fm(font());
    int textW = 0;
    for (const QString& line : lines) {
        textW = std::max(textW, fm.horizontalAdvance(line));
    }
    const int lineH = fm.height();
    const QRect box(8, 8, textW + 16, lineH * static_cast<int>(lines.size()) + 10);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 150));
    p.drawRoundedRect(box, 4, 4);
    p.setPen(QColor(0xE6, 0xE6, 0xE6));
    int y = box.top() + 5 + fm.ascent();
    for (const QString& line : lines) {
        p.drawText(box.left() + 8, y, line);
        y += lineH;
    }
}

void VideoView::resetView() {
    zoom_ = 1.0;
    pan_ = {0.0, 0.0};
    update();
}

double VideoView::zoom() const noexcept {
    return zoom_;
}

double VideoView::fitScale() const {
    if (frame_.isNull() || frame_.width() <= 0 || frame_.height() <= 0 || width() <= 0 || height() <= 0) {
        return 1.0;
    }
    return std::min(static_cast<double>(width()) / frame_.width(), static_cast<double>(height()) / frame_.height());
}

double VideoView::currentScale() const {
    return fitScale() * zoom_;
}

QRectF VideoView::targetRect() const {
    const double s = currentScale();
    const double dw = frame_.width() * s;
    const double dh = frame_.height() * s;
    const double left = (width() - dw) / 2.0 + pan_.x();
    const double top = (height() - dh) / 2.0 + pan_.y();
    return {left, top, dw, dh};
}

QPoint VideoView::mapToImage(const QPoint& widgetPos) const {
    if (frame_.isNull()) {
        return {-1, -1};
    }
    const QRectF tr = targetRect();
    if (!tr.contains(QPointF(widgetPos))) {
        return {-1, -1};
    }
    const double s = currentScale();
    if (s <= 0.0) {
        return {-1, -1};
    }
    const int ix = std::clamp(static_cast<int>((widgetPos.x() - tr.left()) / s), 0, frame_.width() - 1);
    const int iy = std::clamp(static_cast<int>((widgetPos.y() - tr.top()) / s), 0, frame_.height() - 1);
    return {ix, iy};
}

void VideoView::wheelEvent(QWheelEvent* event) {
    if (frame_.isNull()) {
        QWidget::wheelEvent(event);
        return;
    }
    const QPointF cursor = event->position();
    const QPoint imgUnderCursor = mapToImage(cursor.toPoint());
    const double factor = event->angleDelta().y() > 0 ? 1.25 : 0.8;
    const double newZoom = std::clamp(zoom_ * factor, 1.0, 16.0);
    if (qFuzzyCompare(newZoom, zoom_)) {
        event->accept();
        return;
    }
    zoom_ = newZoom;
    if (qFuzzyCompare(zoom_, 1.0)) {
        pan_ = {0.0, 0.0};  // back to fit — no pan
    } else if (imgUnderCursor.x() >= 0) {
        // Keep the same image point under the cursor.
        const double s = currentScale();
        const double dw = frame_.width() * s;
        const double dh = frame_.height() * s;
        pan_.setX((cursor.x() - imgUnderCursor.x() * s) - (width() - dw) / 2.0);
        pan_.setY((cursor.y() - imgUnderCursor.y() * s) - (height() - dh) / 2.0);
    }
    update();
    event->accept();
}

void VideoView::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && zoom_ > 1.0) {
        dragging_ = true;
        lastDragPos_ = event->position();
        setCursor(Qt::ClosedHandCursor);
    }
    QWidget::mousePressEvent(event);
}

void VideoView::mouseMoveEvent(QMouseEvent* event) {
    if (dragging_) {
        pan_ += event->position() - lastDragPos_;
        lastDragPos_ = event->position();
        update();
    } else {
        const QPoint img = mapToImage(event->position().toPoint());
        if (img.x() >= 0) {
            emit pixelProbed(img, frame_.pixelColor(img));
        }
    }
    QWidget::mouseMoveEvent(event);
}

void VideoView::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && dragging_) {
        dragging_ = false;
        unsetCursor();
    }
    QWidget::mouseReleaseEvent(event);
}

void VideoView::mouseDoubleClickEvent(QMouseEvent* event) {
    resetView();  // double-click resets zoom/pan to fit
    QWidget::mouseDoubleClickEvent(event);
}

}  // namespace signalforge::video
