// src/video/video_page.cpp
#include "video/video_page.hpp"

#include "observability/logging.hpp"
#include "video/video_view.hpp"

#include <QChar>
#include <QDateTime>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QStandardPaths>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <cstdint>

namespace signalforge::video {

VideoPage::VideoPage(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // The view is created first so control-bar wiring can reference it safely.
    view_ = new VideoView(this);

    // Control bar: status (left) · rmem hint + Stats toggle (right).
    auto* controlBar = new QWidget(this);
    controlBar->setObjectName(QStringLiteral("videoControlBar"));
    controlBarLayout_ = new QHBoxLayout(controlBar);
    controlBarLayout_->setContentsMargins(8, 4, 8, 4);
    controlBarLayout_->setSpacing(8);

    statusLabel_ = new QLabel(controlBar);
    controlBarLayout_->addWidget(statusLabel_);
    controlBarLayout_->addStretch(1);

    hintLabel_ = new QLabel(controlBar);
    hintLabel_->setStyleSheet(QStringLiteral("color:#e6a23c;"));  // amber warning
    hintLabel_->setVisible(false);
    controlBarLayout_->addWidget(hintLabel_);

    screenshotButton_ = new QToolButton(controlBar);
    screenshotButton_->setText(tr("Screenshot"));
    screenshotButton_->setEnabled(false);  // enabled once a frame is displayed
    controlBarLayout_->addWidget(screenshotButton_);
    connect(screenshotButton_, &QToolButton::clicked, this, &VideoPage::onScreenshotClicked);

    statsToggle_ = new QToolButton(controlBar);
    statsToggle_->setText(tr("Stats"));
    statsToggle_->setCheckable(true);
    statsToggle_->setChecked(true);  // matches VideoView's default overlay visibility
    controlBarLayout_->addWidget(statsToggle_);
    connect(statsToggle_, &QToolButton::toggled, this, [this](bool on) { view_->setOverlayVisible(on); });

    root->addWidget(controlBar);
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

QToolButton* VideoPage::statsToggleButton() const noexcept {
    return statsToggle_;
}

QToolButton* VideoPage::screenshotButton() const noexcept {
    return screenshotButton_;
}

bool VideoPage::saveScreenshot(const QString& path) const {
    const QImage frame = view_->currentFrame();
    if (frame.isNull() || path.isEmpty()) {
        return false;
    }
    if (!frame.save(path, "PNG")) {
        SF_LOG_WARN("video: screenshot save failed: {}", path.toStdString());
        return false;
    }
    SF_LOG_INFO("video: screenshot saved: {}", path.toStdString());
    return true;
}

bool VideoPage::isHintVisible() const {
    // isHidden() reflects the explicit show/hide state regardless of whether the
    // page's window is realized (isVisible() would be false in an unshown tree).
    return !hintLabel_->isHidden();
}

void VideoPage::setStallTimeoutMs(int ms) {
    stallTimeoutMs_ = ms;
}

void VideoPage::onFrameReady(const QImage& frame) {
    view_->setFrame(frame);
    screenshotButton_->setEnabled(true);
    setStatus(tr("Streaming"));
    if (running_) {
        stallTimer_->start(stallTimeoutMs_);
    }
}

void VideoPage::onRunningChanged(bool running) {
    running_ = running;
    stallTimer_->stop();
    view_->setOverlayText(QString());
    hintLabel_->setVisible(false);
    screenshotButton_->setEnabled(false);
    lastDelivered_ = 0;
    lastDropped_ = 0;
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

void VideoPage::onStats(const VideoStats& stats) {
    const QString res = (stats.width > 0 && stats.height > 0)
                            ? QStringLiteral("%1×%2").arg(stats.width).arg(stats.height)
                            : QStringLiteral("—");
    view_->setOverlayText(QStringLiteral("%1 · %2 fps · %3 Mbps · dropped %4")
                              .arg(res)
                              .arg(QString::number(stats.fps, 'f', 1))
                              .arg(QString::number(stats.mbps, 'f', 1))
                              .arg(static_cast<qulonglong>(stats.framesDropped)));

    // High-packet-loss hint, computed over the gap since the last sample. Guard
    // against the receiver's counter reset on rebind (counters go backwards).
    if (stats.framesDropped < lastDropped_ || stats.framesDelivered < lastDelivered_) {
        hintLabel_->setVisible(false);
    } else {
        const std::uint64_t dropped = stats.framesDropped - lastDropped_;
        const std::uint64_t delivered = stats.framesDelivered - lastDelivered_;
        const std::uint64_t total = dropped + delivered;
        const bool highLoss = total > 0 && static_cast<double>(dropped) / static_cast<double>(total) > 0.05;
        if (highLoss) {
            hintLabel_->setText(tr("⚠ High packet loss — raise host net.core.rmem_max"));
        }
        hintLabel_->setVisible(highLoss);
    }
    lastDropped_ = stats.framesDropped;
    lastDelivered_ = stats.framesDelivered;
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
    screenshotButton_->setEnabled(false);
    view_->setPlaceholderText(tr("Video stream stalled — waiting for frames…"));
    setStatus(tr("Stalled"));
}

void VideoPage::onScreenshotClicked() {
    if (!view_->hasFrame()) {
        return;
    }
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    const QString suggested =
        (dir.isEmpty() ? QString() : dir + QLatin1Char('/')) + QStringLiteral("signalforge-video-%1.png").arg(stamp);
    const QString path = QFileDialog::getSaveFileName(this, tr("Save screenshot"), suggested, tr("PNG image (*.png)"));
    if (path.isEmpty()) {
        return;  // user cancelled
    }
    (void)saveScreenshot(path);
}

void VideoPage::setStatus(const QString& text) {
    statusLabel_->setText(QStringLiteral("● ") + text);
}

}  // namespace signalforge::video
