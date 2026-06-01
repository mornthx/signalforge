// src/video/video_view.cpp
#include "video/video_view.hpp"

#include <QColor>
#include <QPaintEvent>
#include <QPainter>
#include <QRect>

namespace signalforge::video {

VideoView::VideoView(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent);  // we fill the whole surface each paint
    setMinimumSize(160, 90);
}

void VideoView::setFrame(const QImage& frame) {
    frame_ = frame;
    update();
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

QSize VideoView::sizeHint() const {
    return {640, 360};
}

void VideoView::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.fillRect(rect(), QColor(0x14, 0x14, 0x16));

    if (frame_.isNull()) {
        p.setPen(QColor(0x88, 0x88, 0x90));
        p.drawText(rect(), Qt::AlignCenter, placeholder_);
        return;
    }

    // Aspect-preserving fit, centered.
    const QSize scaled = frame_.size().scaled(size(), Qt::KeepAspectRatio);
    const QRect target(QPoint((width() - scaled.width()) / 2, (height() - scaled.height()) / 2), scaled);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawImage(target, frame_);
}

}  // namespace signalforge::video
