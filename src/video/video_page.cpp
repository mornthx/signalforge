// src/video/video_page.cpp
#include "video/video_page.hpp"

#include "video/video_view.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>

namespace signalforge::video {

VideoPage::VideoPage(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Control bar — status on the left; later phases add buttons on the right.
    auto* controlBar = new QWidget(this);
    controlBar->setObjectName(QStringLiteral("videoControlBar"));
    controlBarLayout_ = new QHBoxLayout(controlBar);
    controlBarLayout_->setContentsMargins(8, 4, 8, 4);
    controlBarLayout_->setSpacing(8);
    statusLabel_ = new QLabel(controlBar);
    controlBarLayout_->addWidget(statusLabel_);
    controlBarLayout_->addStretch(1);
    root->addWidget(controlBar);

    view_ = new VideoView(this);
    root->addWidget(view_, 1);

    stallTimer_ = new QTimer(this);
    stallTimer_->setSingleShot(true);
    connect(stallTimer_, &QTimer::timeout, this, &VideoPage::onStallTimeout);

    setStatus(tr("Disabled"));
    view_->setPlaceholderText(tr("Video stream disabled — enable it on a UDP connection."));
}

VideoView* VideoPage::view() const noexcept {
    return view_;
}

QHBoxLayout* VideoPage::controlBarLayout() const noexcept {
    return controlBarLayout_;
}

QString VideoPage::statusText() const {
    return statusLabel_->text();
}

void VideoPage::setStallTimeoutMs(int ms) {
    stallTimeoutMs_ = ms;
}

void VideoPage::onFrameReady(const QImage& frame) {
    view_->setFrame(frame);
    setStatus(tr("Streaming"));
    if (running_) {
        stallTimer_->start(stallTimeoutMs_);
    }
}

void VideoPage::onRunningChanged(bool running) {
    running_ = running;
    stallTimer_->stop();
    if (running) {
        view_->clearFrame();
        view_->setPlaceholderText(tr("Waiting for video stream…"));
        setStatus(tr("Waiting…"));
    } else {
        view_->clearFrame();
        view_->setPlaceholderText(tr("Video stream disabled — enable it on a UDP connection."));
        setStatus(tr("Disabled"));
    }
}

void VideoPage::onError(const QString& message) {
    view_->clearFrame();
    view_->setPlaceholderText(tr("Video error: %1").arg(message));
    setStatus(tr("Error"));
}

void VideoPage::onStallTimeout() {
    if (!running_) {
        return;
    }
    view_->clearFrame();
    view_->setPlaceholderText(tr("Video stream stalled — waiting for frames…"));
    setStatus(tr("Stalled"));
}

void VideoPage::setStatus(const QString& text) {
    statusLabel_->setText(QStringLiteral("● ") + text);
}

}  // namespace signalforge::video
