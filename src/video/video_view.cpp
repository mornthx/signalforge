// src/video/video_view.cpp
#include "video/video_view.hpp"

#include <QColor>
#include <QFontMetrics>
#include <QLatin1Char>
#include <QPaintEvent>
#include <QPainter>
#include <QRect>
#include <QStringList>
#include <algorithm>

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
        // Aspect-preserving fit, centered.
        const QSize scaled = frame_.size().scaled(size(), Qt::KeepAspectRatio);
        const QRect target(QPoint((width() - scaled.width()) / 2, (height() - scaled.height()) / 2), scaled);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.drawImage(target, frame_);
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

}  // namespace signalforge::video
