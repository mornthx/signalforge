// src/video/video_recorder.cpp
#include "video/video_recorder.hpp"

#include "observability/logging.hpp"

#include <QFile>
#include <QIODevice>
#include <QImage>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>

namespace signalforge::video {

namespace {
constexpr int kRgb24Bpp = 3;
constexpr int kFfmpegStartTimeoutMs = 3000;
constexpr int kFfmpegFinishTimeoutMs = 10000;
}  // namespace

VideoRecorder::VideoRecorder(QObject* parent) : QObject(parent) {}

VideoRecorder::~VideoRecorder() {
    if (recording_) {
        stop();
    }
}

bool VideoRecorder::ffmpegAvailable() {
    return !QStandardPaths::findExecutable(QStringLiteral("ffmpeg")).isEmpty();
}

bool VideoRecorder::start(const QString& path, Format format, int width, int height, int fps) {
    if (recording_) {
        return false;
    }
    if (path.isEmpty() || width <= 0 || height <= 0 || fps <= 0) {
        return false;
    }

    width_ = width;
    height_ = height;
    format_ = format;
    framesWritten_ = 0;

    if (format == Format::Mp4) {
        if (!ffmpegAvailable()) {
            emit errorOccurred(QStringLiteral("ffmpeg not found — MP4 recording unavailable"));
            return false;
        }
        ffmpeg_ = std::make_unique<QProcess>();
        const QStringList args = {QStringLiteral("-loglevel"), QStringLiteral("error"),
                                  QStringLiteral("-f"),        QStringLiteral("rawvideo"),
                                  QStringLiteral("-pix_fmt"),  QStringLiteral("rgb24"),
                                  QStringLiteral("-s"),        QStringLiteral("%1x%2").arg(width).arg(height),
                                  QStringLiteral("-r"),        QString::number(fps),
                                  QStringLiteral("-i"),        QStringLiteral("-"),
                                  QStringLiteral("-c:v"),      QStringLiteral("libx264"),
                                  QStringLiteral("-pix_fmt"),  QStringLiteral("yuv420p"),
                                  QStringLiteral("-y"),        path};
        ffmpeg_->start(QStringLiteral("ffmpeg"), args);
        if (!ffmpeg_->waitForStarted(kFfmpegStartTimeoutMs)) {
            emit errorOccurred(QStringLiteral("ffmpeg failed to start: %1").arg(ffmpeg_->errorString()));
            ffmpeg_.reset();
            return false;
        }
    } else {
        rawFile_ = std::make_unique<QFile>(path);
        if (!rawFile_->open(QIODevice::WriteOnly)) {
            emit errorOccurred(QStringLiteral("cannot open '%1' for writing: %2").arg(path, rawFile_->errorString()));
            rawFile_.reset();
            return false;
        }
    }

    recording_ = true;
    SF_LOG_INFO("video: recording started ({}x{} @ {}fps, {}) -> {}", width, height, fps,
                format == Format::Mp4 ? "mp4" : "raw", path.toStdString());
    emit recordingStarted();
    return true;
}

void VideoRecorder::writeFrame(const QImage& frame) {
    if (!recording_) {
        return;
    }
    if (frame.width() != width_ || frame.height() != height_) {
        return;  // resolution changed mid-recording — skip
    }
    const QImage img = frame.format() == QImage::Format_RGB888 ? frame : frame.convertToFormat(QImage::Format_RGB888);

    QIODevice* sink =
        format_ == Format::Mp4 ? static_cast<QIODevice*>(ffmpeg_.get()) : static_cast<QIODevice*>(rawFile_.get());
    if (sink == nullptr) {
        return;
    }
    // Write row-by-row to drop QImage's 32-bit scanline padding (ffmpeg/raw
    // expect tightly-packed width*3 bytes per row).
    const qint64 rowBytes = static_cast<qint64>(width_) * kRgb24Bpp;
    for (int y = 0; y < height_; ++y) {
        if (sink->write(reinterpret_cast<const char*>(img.constScanLine(y)), rowBytes) != rowBytes) {
            emit errorOccurred(QStringLiteral("write failed while recording"));
            return;
        }
    }
    ++framesWritten_;
}

void VideoRecorder::stop() {
    if (!recording_) {
        return;
    }
    recording_ = false;
    bool ok = true;

    if (format_ == Format::Mp4 && ffmpeg_) {
        ffmpeg_->closeWriteChannel();
        if (!ffmpeg_->waitForFinished(kFfmpegFinishTimeoutMs)) {
            SF_LOG_WARN("video: ffmpeg did not finish in time; killing");
            ffmpeg_->kill();
            ffmpeg_->waitForFinished(2000);
            ok = false;
        } else {
            ok = ffmpeg_->exitStatus() == QProcess::NormalExit && ffmpeg_->exitCode() == 0;
        }
        ffmpeg_.reset();
    } else if (rawFile_) {
        rawFile_->close();
        ok = rawFile_->error() == QFile::NoError;
        rawFile_.reset();
    }

    SF_LOG_INFO("video: recording stopped ({} frames, ok={})", framesWritten_, ok);
    emit recordingStopped(ok);
}

bool VideoRecorder::isRecording() const noexcept {
    return recording_;
}

VideoRecorder::Format VideoRecorder::format() const noexcept {
    return format_;
}

quint64 VideoRecorder::framesWritten() const noexcept {
    return framesWritten_;
}

}  // namespace signalforge::video
