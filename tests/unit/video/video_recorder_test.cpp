// tests/unit/video/video_recorder_test.cpp
#include "video/video_recorder.hpp"

#include <QApplication>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSignalSpy>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <memory>

using signalforge::video::VideoRecorder;

namespace {

QApplication& app() {
    static int argc = 1;
    static char arg0[] = "video_recorder_test";
    static char* argv[] = {arg0, nullptr};
    // Leaked on purpose — see memory qt_xcb_teardown_crash. (QProcess wants an
    // event loop / Q*Application; QApplication is already used by sibling tests.)
    static QApplication* instance = new QApplication(argc, argv);
    return *instance;
}

QImage solid(int w, int h, std::uint8_t v) {
    QImage img(w, h, QImage::Format_RGB888);
    img.fill(QColor(v, v, v));
    return img;
}

}  // namespace

TEST_CASE("VideoRecorder: raw recording writes tightly-packed RGB24", "[video][recorder]") {
    app();
    const int w = 6, h = 4, fps = 25, frames = 5;
    const QString path = QDir::tempPath() + QStringLiteral("/sf_rec_test.raw");
    QFile::remove(path);

    VideoRecorder rec;
    REQUIRE(rec.start(path, VideoRecorder::Format::Raw, w, h, fps));
    CHECK(rec.isRecording());
    for (int i = 0; i < frames; ++i) {
        rec.writeFrame(solid(w, h, static_cast<std::uint8_t>(10 * i)));
    }
    // A wrong-size frame is skipped, not written.
    rec.writeFrame(solid(w + 1, h, 99));
    rec.stop();

    CHECK_FALSE(rec.isRecording());
    CHECK(rec.framesWritten() == static_cast<quint64>(frames));
    CHECK(QFileInfo(path).size() == static_cast<qint64>(frames) * w * h * 3);
    QFile::remove(path);
}

TEST_CASE("VideoRecorder: start rejects bad args and double-start", "[video][recorder]") {
    app();
    const QString path = QDir::tempPath() + QStringLiteral("/sf_rec_bad.raw");
    QFile::remove(path);
    VideoRecorder rec;
    CHECK_FALSE(rec.start(QString(), VideoRecorder::Format::Raw, 4, 4, 25));  // empty path
    CHECK_FALSE(rec.start(path, VideoRecorder::Format::Raw, 0, 4, 25));       // bad width
    CHECK_FALSE(rec.start(path, VideoRecorder::Format::Raw, 4, 4, 0));        // bad fps

    REQUIRE(rec.start(path, VideoRecorder::Format::Raw, 4, 4, 25));
    CHECK_FALSE(rec.start(path, VideoRecorder::Format::Raw, 4, 4, 25));  // already recording
    rec.stop();
    QFile::remove(path);
}

TEST_CASE("VideoRecorder: signals fire on start/stop", "[video][recorder]") {
    app();
    const QString path = QDir::tempPath() + QStringLiteral("/sf_rec_sig.raw");
    QFile::remove(path);
    VideoRecorder rec;
    QSignalSpy started(&rec, &VideoRecorder::recordingStarted);
    QSignalSpy stopped(&rec, &VideoRecorder::recordingStopped);
    REQUIRE(rec.start(path, VideoRecorder::Format::Raw, 4, 4, 25));
    rec.writeFrame(solid(4, 4, 50));
    rec.stop();
    CHECK(started.count() == 1);
    REQUIRE(stopped.count() == 1);
    CHECK(stopped.takeFirst().at(0).toBool());  // ok == true
    QFile::remove(path);
}

TEST_CASE("VideoRecorder: MP4 via ffmpeg (only when ffmpeg is installed)", "[video][recorder][ffmpeg]") {
    app();
    if (!VideoRecorder::ffmpegAvailable()) {
        SUCCEED("ffmpeg not installed — MP4 path skipped");
        return;
    }
    const int w = 16, h = 16, fps = 25;
    const QString path = QDir::tempPath() + QStringLiteral("/sf_rec_test.mp4");
    QFile::remove(path);

    VideoRecorder rec;
    REQUIRE(rec.start(path, VideoRecorder::Format::Mp4, w, h, fps));
    for (int i = 0; i < 10; ++i) {
        rec.writeFrame(solid(w, h, static_cast<std::uint8_t>(20 * i)));
    }
    rec.stop();
    CHECK_FALSE(rec.isRecording());
    CHECK(rec.framesWritten() == 10u);
    REQUIRE(QFileInfo(path).exists());
    CHECK(QFileInfo(path).size() > 0);  // ffmpeg produced an MP4
    QFile::remove(path);
}
