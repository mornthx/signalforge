// src/video/video_recorder.hpp
#pragma once

#include <QObject>
#include <QString>
#include <QtGlobal>
#include <memory>

class QFile;
class QImage;
class QProcess;

namespace signalforge::video {

/// Records the live RGB24 video stream to disk.
///
/// Two formats:
///  - `Mp4`: pipes raw RGB24 frames into the `ffmpeg` CLI (`QProcess`) which
///    encodes H.264/MP4. ffmpeg is an external **runtime tool**, not a linked
///    dependency; if it is missing, MP4 recording is unavailable (the caller
///    falls back to `Raw`).
///  - `Raw`: writes the RGB24 frames verbatim to a `.raw` file (no dependency,
///    large; playable with e.g. `ffplay -f rawvideo -pix_fmt rgb24 -s WxH`).
///
/// Frames whose dimensions differ from the dimensions fixed at `start()` are
/// skipped. Lives on the GUI thread; `writeFrame` is called from the same
/// thread that delivers frames to the UI.
class VideoRecorder : public QObject {
    Q_OBJECT

public:
    enum class Format { Mp4, Raw };

    explicit VideoRecorder(QObject* parent = nullptr);
    ~VideoRecorder() override;

    /// Whether the `ffmpeg` CLI is on PATH (required for `Mp4`).
    [[nodiscard]] static bool ffmpegAvailable();

    /// Begin recording `width`x`height` RGB24 frames at `fps` to `path`.
    /// Returns false if already recording, the arguments are invalid, or
    /// (`Mp4`) ffmpeg is unavailable / fails to start.
    [[nodiscard]] bool start(const QString& path, Format format, int width, int height, int fps);

    /// Append one frame (skipped if its size ≠ the recording size).
    void writeFrame(const QImage& frame);

    /// Stop recording, flushing and closing the sink. Emits `recordingStopped`.
    void stop();

    [[nodiscard]] bool isRecording() const noexcept;
    [[nodiscard]] Format format() const noexcept;
    [[nodiscard]] quint64 framesWritten() const noexcept;

signals:
    void recordingStarted();
    void recordingStopped(bool ok);
    void errorOccurred(const QString& message);

private:
    bool recording_ = false;
    Format format_ = Format::Mp4;
    int width_ = 0;
    int height_ = 0;
    quint64 framesWritten_ = 0;
    std::unique_ptr<QProcess> ffmpeg_;  ///< Mp4 sink
    std::unique_ptr<QFile> rawFile_;    ///< Raw sink
};

}  // namespace signalforge::video
