// src/video/video_page.cpp
#include "video/video_page.hpp"

#include "observability/logging.hpp"
#include "video/color_panel.hpp"
#include "video/video_view.hpp"

#include <QChar>
#include <QComboBox>
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

    probeLabel_ = new QLabel(controlBar);
    probeLabel_->setStyleSheet(QStringLiteral("color:#8a8a90;"));  // muted readout
    controlBarLayout_->addWidget(probeLabel_);

    controlBarLayout_->addStretch(1);

    hintLabel_ = new QLabel(controlBar);
    hintLabel_->setStyleSheet(QStringLiteral("color:#e6a23c;"));  // amber warning
    hintLabel_->setVisible(false);
    controlBarLayout_->addWidget(hintLabel_);

    elapsedLabel_ = new QLabel(controlBar);
    elapsedLabel_->setStyleSheet(QStringLiteral("color:#f06060;"));  // red REC indicator
    elapsedLabel_->setVisible(false);
    controlBarLayout_->addWidget(elapsedLabel_);

    pauseButton_ = new QToolButton(controlBar);
    pauseButton_->setText(tr("Pause"));
    pauseButton_->setCheckable(true);
    pauseButton_->setEnabled(false);  // enabled once a frame is displayed
    controlBarLayout_->addWidget(pauseButton_);
    connect(pauseButton_, &QToolButton::toggled, this, &VideoPage::onPauseToggled);

    formatCombo_ = new QComboBox(controlBar);
    if (VideoRecorder::ffmpegAvailable()) {
        formatCombo_->addItem(tr("MP4 (H.264)"), static_cast<int>(VideoRecorder::Format::Mp4));
    }
    formatCombo_->addItem(tr("Raw RGB24"), static_cast<int>(VideoRecorder::Format::Raw));
    controlBarLayout_->addWidget(formatCombo_);

    recordButton_ = new QToolButton(controlBar);
    recordButton_->setText(tr("Record"));
    recordButton_->setEnabled(false);  // enabled once a frame is displayed
    controlBarLayout_->addWidget(recordButton_);
    connect(recordButton_, &QToolButton::clicked, this, &VideoPage::onRecordClicked);

    screenshotButton_ = new QToolButton(controlBar);
    screenshotButton_->setText(tr("Screenshot"));
    screenshotButton_->setEnabled(false);  // enabled once a frame is displayed
    controlBarLayout_->addWidget(screenshotButton_);
    connect(screenshotButton_, &QToolButton::clicked, this, &VideoPage::onScreenshotClicked);

    colorButton_ = new QToolButton(controlBar);
    colorButton_->setText(tr("Color"));
    colorButton_->setCheckable(true);
    controlBarLayout_->addWidget(colorButton_);

    statsToggle_ = new QToolButton(controlBar);
    statsToggle_->setText(tr("Stats"));
    statsToggle_->setCheckable(true);
    statsToggle_->setChecked(true);  // matches VideoView's default overlay visibility
    controlBarLayout_->addWidget(statsToggle_);
    connect(statsToggle_, &QToolButton::toggled, this, [this](bool on) { view_->setOverlayVisible(on); });

    root->addWidget(controlBar);
    root->addWidget(view_, 1);
    connect(view_, &VideoView::pixelProbed, this, &VideoPage::onPixelProbed);

    colorPanel_ = new ColorPanel(this);
    colorPanel_->setVisible(false);
    root->addWidget(colorPanel_);
    connect(colorButton_, &QToolButton::toggled, colorPanel_, &QWidget::setVisible);
    connect(colorPanel_, &ColorPanel::paramsChanged, this, &VideoPage::setColorParams);

    recorder_ = new VideoRecorder(this);
    connect(recorder_, &VideoRecorder::recordingStarted, this, &VideoPage::onRecordingStarted);
    connect(recorder_, &VideoRecorder::recordingStopped, this, &VideoPage::onRecordingStopped);
    connect(recorder_, &VideoRecorder::errorOccurred, this,
            [](const QString& msg) { SF_LOG_WARN("video: recorder error: {}", msg.toStdString()); });

    elapsedTimer_ = new QTimer(this);
    elapsedTimer_->setInterval(1000);
    connect(elapsedTimer_, &QTimer::timeout, this, &VideoPage::updateElapsed);

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

QString VideoPage::probeText() const {
    return probeLabel_->text();
}

QToolButton* VideoPage::statsToggleButton() const noexcept {
    return statsToggle_;
}

QToolButton* VideoPage::screenshotButton() const noexcept {
    return screenshotButton_;
}

QToolButton* VideoPage::pauseButton() const noexcept {
    return pauseButton_;
}

bool VideoPage::isPaused() const {
    return view_->isFrozen();
}

QToolButton* VideoPage::recordButton() const noexcept {
    return recordButton_;
}

bool VideoPage::isRecording() const {
    return recorder_->isRecording();
}

bool VideoPage::startRecording(const QString& path, VideoRecorder::Format format) {
    const QImage frame = view_->currentFrame();
    if (frame.isNull()) {
        return false;
    }
    return recorder_->start(path, format, frame.width(), frame.height(), recordFps_ > 0 ? recordFps_ : 25);
}

void VideoPage::stopRecording() {
    recorder_->stop();
}

QToolButton* VideoPage::colorButton() const noexcept {
    return colorButton_;
}

ColorPanel* VideoPage::colorPanel() const noexcept {
    return colorPanel_;
}

ColorParams VideoPage::colorParams() const {
    return corrector_.params();
}

void VideoPage::setColorParams(const ColorParams& params) {
    corrector_.setParams(params);
    if (!lastRawFrame_.isNull()) {
        // Re-render the current frame even if the display is frozen (live preview).
        const bool wasFrozen = view_->isFrozen();
        view_->setFrozen(false);
        view_->setFrame(corrector_.apply(lastRawFrame_));
        view_->setFrozen(wasFrozen);
    }
}

void VideoPage::updateButtonStates() {
    const bool hasFrame = view_->hasFrame();
    screenshotButton_->setEnabled(hasFrame);
    pauseButton_->setEnabled(hasFrame);
    recordButton_->setEnabled(hasFrame || recorder_->isRecording());
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
    // Recording always tracks the live stream, even while the display is frozen.
    if (recorder_->isRecording()) {
        recorder_->writeFrame(recordRaw_ ? frame : corrector_.apply(frame));
    }
    if (!view_->isFrozen()) {
        // Keep the raw frame so colour-param changes can re-render it; the view
        // and screenshot show the corrected image.
        lastRawFrame_ = frame;
        view_->setFrame(corrector_.apply(frame));
    }
    updateButtonStates();
    setStatus(tr("Streaming"));
    if (running_) {
        stallTimer_->start(stallTimeoutMs_);
    }
}

void VideoPage::onRunningChanged(bool running) {
    running_ = running;
    stallTimer_->stop();
    pauseButton_->setChecked(false);  // unfreeze on any stream transition
    view_->setOverlayText(QString());
    hintLabel_->setVisible(false);
    if (!running && recorder_->isRecording()) {
        stopRecording();  // the stream ended — finalize any recording
    }
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
    updateButtonStates();
}

void VideoPage::onStats(const VideoStats& stats) {
    if (stats.fps >= 1.0) {
        recordFps_ = qRound(stats.fps);
    }
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
    updateButtonStates();
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

void VideoPage::onPauseToggled(bool paused) {
    view_->setFrozen(paused);
    pauseButton_->setText(paused ? tr("Resume") : tr("Pause"));
}

void VideoPage::onPixelProbed(const QPoint& imagePos, const QColor& color) {
    probeLabel_->setText(QStringLiteral("(%1,%2) RGB(%3,%4,%5)")
                             .arg(imagePos.x())
                             .arg(imagePos.y())
                             .arg(color.red())
                             .arg(color.green())
                             .arg(color.blue()));
}

void VideoPage::onRecordClicked() {
    if (recorder_->isRecording()) {
        stopRecording();
        return;
    }
    if (!view_->hasFrame()) {
        return;
    }
    const auto format = static_cast<VideoRecorder::Format>(formatCombo_->currentData().toInt());
    const bool mp4 = format == VideoRecorder::Format::Mp4;
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    const QString ext = mp4 ? QStringLiteral(".mp4") : QStringLiteral(".raw");
    const QString suggested =
        (dir.isEmpty() ? QString() : dir + QLatin1Char('/')) + QStringLiteral("signalforge-video-%1%2").arg(stamp, ext);
    const QString filter = mp4 ? tr("MP4 video (*.mp4)") : tr("Raw RGB24 (*.raw)");
    const QString path = QFileDialog::getSaveFileName(this, tr("Record video"), suggested, filter);
    if (path.isEmpty()) {
        return;  // user cancelled
    }
    (void)startRecording(path, format);
}

void VideoPage::onRecordingStarted() {
    recordButton_->setText(tr("Stop"));
    formatCombo_->setEnabled(false);
    recordClock_.start();
    elapsedLabel_->setVisible(true);
    updateElapsed();
    elapsedTimer_->start();
}

void VideoPage::onRecordingStopped(bool ok) {
    recordButton_->setText(tr("Record"));
    formatCombo_->setEnabled(true);
    elapsedTimer_->stop();
    elapsedLabel_->setVisible(false);
    updateButtonStates();
    if (!ok) {
        SF_LOG_WARN("video: recording stopped with an error");
    }
}

void VideoPage::updateElapsed() {
    const qint64 secs = recordClock_.elapsed() / 1000;
    elapsedLabel_->setText(
        QStringLiteral("● REC %1:%2").arg(secs / 60, 2, 10, QLatin1Char('0')).arg(secs % 60, 2, 10, QLatin1Char('0')));
}

void VideoPage::setStatus(const QString& text) {
    statusLabel_->setText(QStringLiteral("● ") + text);
}

}  // namespace signalforge::video
