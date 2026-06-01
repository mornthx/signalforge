// tests/unit/video/video_page_test.cpp
#include "video/color_panel.hpp"
#include "video/video_page.hpp"
#include "video/video_receiver.hpp"
#include "video/video_view.hpp"
#include "zvid_test_util.hpp"

#include <QApplication>
#include <QByteArray>
#include <QColor>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QImage>
#include <QMouseEvent>
#include <QSignalSpy>
#include <QThread>
#include <QToolButton>
#include <QUdpSocket>
#include <QWheelEvent>
#include <QtTest/QtTest>
#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdint>
#include <functional>

using signalforge::video::VideoPage;
using signalforge::video::VideoUdpReceiver;
using signalforge::video::VideoView;

namespace {

QApplication& app() {
    static int argc = 1;
    static char arg0[] = "video_page_test";
    static char* argv[] = {arg0, nullptr};
    // Leaked on purpose — see memory qt_xcb_teardown_crash.
    static QApplication* instance = new QApplication(argc, argv);
    return *instance;
}

bool waitFor(const std::function<bool()>& pred, int ms = 3000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (!pred() && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(2);
    }
    return pred();
}

QImage solidFrame(int w, int h, std::uint8_t v) {
    QImage img(w, h, QImage::Format_RGB888);
    img.fill(QColor(v, v, v));
    return img;
}

void sendFrame(QUdpSocket& s, quint16 port, std::uint32_t frameNo, int w, int h, const QByteArray& pixels,
               int chunkBytes) {
    const auto frameBytes = static_cast<std::uint32_t>(w * h * 3);
    for (int off = 0; off < pixels.size(); off += chunkBytes) {
        const int clen = std::min(chunkBytes, static_cast<int>(pixels.size()) - off);
        const QByteArray dg = sfvideotest::makeDatagram(frameNo, static_cast<std::uint32_t>(off), frameBytes,
                                                        static_cast<std::uint16_t>(w), static_cast<std::uint16_t>(h),
                                                        pixels.mid(off, clen));
        s.writeDatagram(dg, QHostAddress(QHostAddress::LocalHost), port);
    }
}

}  // namespace

TEST_CASE("VideoView: setFrame / clearFrame / placeholder", "[video][page][interaction]") {
    app();
    VideoView view;
    CHECK_FALSE(view.hasFrame());

    view.setFrame(solidFrame(16, 9, 200));
    CHECK(view.hasFrame());
    CHECK(view.currentFrame().size() == QSize(16, 9));

    view.clearFrame();
    CHECK_FALSE(view.hasFrame());

    view.setPlaceholderText(QStringLiteral("hello"));
    CHECK(view.placeholderText() == QStringLiteral("hello"));
}

TEST_CASE("VideoPage: starts Disabled with no frame", "[video][page][interaction]") {
    app();
    VideoPage page;
    CHECK(page.statusText().contains(QStringLiteral("Disabled")));
    CHECK_FALSE(page.view()->hasFrame());
}

TEST_CASE("VideoPage: running → waiting, then frame → streaming", "[video][page][interaction]") {
    app();
    VideoPage page;

    page.onRunningChanged(true);
    CHECK(page.statusText().contains(QStringLiteral("Waiting")));
    CHECK_FALSE(page.view()->hasFrame());

    page.onFrameReady(solidFrame(32, 18, 120));
    CHECK(page.statusText().contains(QStringLiteral("Streaming")));
    CHECK(page.view()->hasFrame());

    page.onRunningChanged(false);
    CHECK(page.statusText().contains(QStringLiteral("Disabled")));
    CHECK_FALSE(page.view()->hasFrame());
}

TEST_CASE("VideoPage: stall watchdog flips to Stalled when frames stop", "[video][page][interaction]") {
    app();
    VideoPage page;
    page.setStallTimeoutMs(80);
    page.onRunningChanged(true);
    page.onFrameReady(solidFrame(8, 8, 90));
    REQUIRE(page.statusText().contains(QStringLiteral("Streaming")));

    REQUIRE(waitFor([&] { return page.statusText().contains(QStringLiteral("Stalled")); }, 2000));
    CHECK_FALSE(page.view()->hasFrame());
}

TEST_CASE("VideoPage: stats overlay reflects the latest sample", "[video][page][interaction]") {
    app();
    VideoPage page;
    page.onRunningChanged(true);
    signalforge::video::VideoStats s;
    s.fps = 25.4;
    s.mbps = 568.3;
    s.framesDelivered = 100;
    s.framesDropped = 2;
    s.width = 1280;
    s.height = 720;
    page.onStats(s);
    const QString ov = page.view()->overlayText();
    CHECK(ov.contains(QStringLiteral("1280×720")));
    CHECK(ov.contains(QStringLiteral("25.4")));
    CHECK(ov.contains(QStringLiteral("568.3")));
}

TEST_CASE("VideoPage: Stats toggle controls overlay visibility", "[video][page][interaction]") {
    app();
    VideoPage page;
    CHECK(page.view()->isOverlayVisible());  // default on
    page.statsToggleButton()->click();       // off
    CHECK_FALSE(page.view()->isOverlayVisible());
    page.statsToggleButton()->click();  // on
    CHECK(page.view()->isOverlayVisible());
}

TEST_CASE("VideoPage: rmem hint appears under high packet loss", "[video][page][interaction]") {
    app();
    VideoPage page;
    page.onRunningChanged(true);

    signalforge::video::VideoStats a;  // baseline
    a.framesDelivered = 100;
    a.framesDropped = 0;
    page.onStats(a);
    CHECK_FALSE(page.isHintVisible());

    signalforge::video::VideoStats b;  // window: 20 dropped / 100 total = 20% loss
    b.framesDelivered = 180;
    b.framesDropped = 20;
    page.onStats(b);
    CHECK(page.isHintVisible());

    signalforge::video::VideoStats c;  // window: 0 dropped → clears
    c.framesDelivered = 280;
    c.framesDropped = 20;
    page.onStats(c);
    CHECK_FALSE(page.isHintVisible());
}

TEST_CASE("VideoPage: screenshot button gating + PNG save round-trip", "[video][page][interaction]") {
    app();
    VideoPage page;
    CHECK_FALSE(page.screenshotButton()->isEnabled());  // no frame yet

    page.onRunningChanged(true);
    CHECK_FALSE(page.screenshotButton()->isEnabled());  // bound, still no frame

    page.onFrameReady(solidFrame(12, 10, 77));
    CHECK(page.screenshotButton()->isEnabled());

    const QString path = QDir::tempPath() + QStringLiteral("/sf_video_shot_test.png");
    QFile::remove(path);
    CHECK(page.saveScreenshot(path));
    const QImage back(path);
    CHECK(back.size() == QSize(12, 10));
    QFile::remove(path);

    page.onRunningChanged(false);
    CHECK_FALSE(page.screenshotButton()->isEnabled());
    CHECK_FALSE(page.saveScreenshot(path));  // no frame → false
}

TEST_CASE("VideoPage: record lifecycle (raw) via seam", "[video][page][interaction]") {
    app();
    VideoPage page;
    page.onRunningChanged(true);
    CHECK_FALSE(page.recordButton()->isEnabled());  // no frame yet

    page.onFrameReady(solidFrame(6, 4, 30));
    CHECK(page.recordButton()->isEnabled());
    CHECK_FALSE(page.isRecording());

    const QString path = QDir::tempPath() + QStringLiteral("/sf_page_rec.raw");
    QFile::remove(path);
    REQUIRE(page.startRecording(path, signalforge::video::VideoRecorder::Format::Raw));
    CHECK(page.isRecording());
    CHECK(page.recordButton()->text() == QStringLiteral("Stop"));

    // Frames are written only via onFrameReady while recording.
    page.onFrameReady(solidFrame(6, 4, 40));
    page.onFrameReady(solidFrame(6, 4, 50));
    page.stopRecording();

    CHECK_FALSE(page.isRecording());
    CHECK(page.recordButton()->text() == QStringLiteral("Record"));
    CHECK(QFileInfo(path).size() == 2LL * 6 * 4 * 3);  // two frames written post-start
    QFile::remove(path);
}

TEST_CASE("VideoPage: stream stop finalizes an active recording", "[video][page][interaction]") {
    app();
    VideoPage page;
    page.onRunningChanged(true);
    page.onFrameReady(solidFrame(6, 4, 30));
    const QString path = QDir::tempPath() + QStringLiteral("/sf_page_rec2.raw");
    QFile::remove(path);
    REQUIRE(page.startRecording(path, signalforge::video::VideoRecorder::Format::Raw));
    CHECK(page.isRecording());
    page.onRunningChanged(false);  // stream drops
    CHECK_FALSE(page.isRecording());
    QFile::remove(path);
}

TEST_CASE("VideoPage: pause freezes the display while frames keep flowing", "[video][page][interaction]") {
    app();
    VideoPage page;
    page.onRunningChanged(true);
    page.onFrameReady(solidFrame(6, 4, 10));
    CHECK(page.pauseButton()->isEnabled());
    CHECK_FALSE(page.isPaused());

    page.pauseButton()->click();  // pause
    CHECK(page.isPaused());
    CHECK(page.pauseButton()->text() == QStringLiteral("Resume"));

    page.onFrameReady(solidFrame(6, 4, 200));  // ignored by the view while frozen
    CHECK(page.view()->currentFrame().pixelColor(0, 0) == QColor(10, 10, 10));

    page.pauseButton()->click();  // resume
    CHECK_FALSE(page.isPaused());
    CHECK(page.pauseButton()->text() == QStringLiteral("Pause"));
    page.onFrameReady(solidFrame(6, 4, 99));
    CHECK(page.view()->currentFrame().pixelColor(0, 0) == QColor(99, 99, 99));
}

TEST_CASE("VideoPage: recording continues while paused", "[video][page][interaction]") {
    app();
    VideoPage page;
    page.onRunningChanged(true);
    page.onFrameReady(solidFrame(6, 4, 10));
    const QString path = QDir::tempPath() + QStringLiteral("/sf_pause_rec.raw");
    QFile::remove(path);
    REQUIRE(page.startRecording(path, signalforge::video::VideoRecorder::Format::Raw));
    page.pauseButton()->click();              // freeze display
    page.onFrameReady(solidFrame(6, 4, 20));  // view frozen, but recorder still fed
    page.onFrameReady(solidFrame(6, 4, 30));
    page.stopRecording();
    CHECK(QFileInfo(path).size() == 2LL * 6 * 4 * 3);  // two frames recorded despite pause
    QFile::remove(path);
}

TEST_CASE("VideoView: maps widget coords to image pixels at fit", "[video][view][interaction]") {
    app();
    VideoView view;
    view.resize(200, 100);
    QImage f(100, 50, QImage::Format_RGB888);
    f.fill(QColor(0, 0, 0));
    view.setFrame(f);
    // fit scale = min(200/100, 100/50) = 2; the image fills the widget exactly.
    CHECK(view.mapToImage(QPoint(0, 0)) == QPoint(0, 0));
    CHECK(view.mapToImage(QPoint(100, 50)) == QPoint(50, 25));
    CHECK(view.mapToImage(QPoint(199, 99)) == QPoint(99, 49));
}

TEST_CASE("VideoView: wheel zooms in; resetView returns to fit", "[video][view][interaction]") {
    app();
    VideoView view;
    view.resize(200, 100);
    QImage f(100, 50, QImage::Format_RGB888);
    f.fill(QColor(0, 0, 0));
    view.setFrame(f);
    CHECK(view.zoom() == 1.0);

    QWheelEvent we(QPointF(100, 50), QPointF(100, 50), QPoint(0, 0), QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
                   Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(&view, &we);
    CHECK(view.zoom() > 1.0);

    view.resetView();
    CHECK(view.zoom() == 1.0);
}

TEST_CASE("VideoView: hover emits pixelProbed with the source pixel colour", "[video][view][interaction]") {
    app();
    VideoView view;
    view.resize(200, 100);
    QImage f(100, 50, QImage::Format_RGB888);
    f.fill(QColor(33, 66, 99));
    view.setFrame(f);

    QSignalSpy spy(&view, &VideoView::pixelProbed);
    QMouseEvent me(QEvent::MouseMove, QPointF(100, 50), QPointF(100, 50), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&view, &me);

    REQUIRE(spy.count() >= 1);
    const auto last = spy.takeLast();
    CHECK(last.at(0).toPoint() == QPoint(50, 25));
    CHECK(last.at(1).value<QColor>() == QColor(33, 66, 99));
}

TEST_CASE("VideoPage: pixel probe updates the readout", "[video][page][interaction]") {
    app();
    VideoPage page;
    page.onRunningChanged(true);
    QImage f(100, 50, QImage::Format_RGB888);
    f.fill(QColor(10, 20, 30));
    page.onFrameReady(f);
    page.view()->resize(200, 100);

    QMouseEvent me(QEvent::MouseMove, QPointF(100, 50), QPointF(100, 50), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(page.view(), &me);
    CHECK(page.probeText().contains(QStringLiteral("RGB(10,20,30)")));
}

TEST_CASE("VideoPage: color correction transforms the displayed frame", "[video][page][interaction]") {
    app();
    VideoPage page;
    page.onRunningChanged(true);
    page.onFrameReady(solidFrame(4, 4, 100));  // raw grey 100
    CHECK(page.view()->currentFrame().pixelColor(0, 0) == QColor(100, 100, 100));

    signalforge::video::ColorParams p;
    p.rGain = 2.0;
    page.setColorParams(p);
    const QColor c = page.view()->currentFrame().pixelColor(0, 0);
    CHECK(c.red() == 200);    // 100 * 2
    CHECK(c.green() == 100);  // unchanged
    CHECK(c.blue() == 100);

    page.onFrameReady(solidFrame(4, 4, 50));  // next frame stays corrected
    const QColor c2 = page.view()->currentFrame().pixelColor(0, 0);
    CHECK(c2.red() == 100);  // 50 * 2
    CHECK(c2.green() == 50);
}

TEST_CASE("VideoPage: Color button toggles the slider panel; panel drives params", "[video][page][interaction]") {
    app();
    VideoPage page;
    CHECK(page.colorPanel()->isHidden());  // hidden by default
    page.colorButton()->click();
    CHECK_FALSE(page.colorPanel()->isHidden());

    signalforge::video::ColorParams p;
    p.gGain = 1.5;
    page.colorPanel()->setParams(p);  // panel → page corrector
    CHECK(page.colorParams().gGain == 1.5);
}

TEST_CASE("VideoPage: color preset save/load round-trip", "[video][page][interaction]") {
    app();
    VideoPage page;
    signalforge::video::ColorParams p;
    p.rGain = 1.7;
    p.saturation = 0.5;
    page.setColorParams(p);

    const QString path = QDir::tempPath() + QStringLiteral("/sf_color_preset.json");
    QFile::remove(path);
    REQUIRE(page.saveColorPreset(path));

    page.setColorParams(signalforge::video::ColorParams{});  // reset to identity
    REQUIRE(page.colorParams().isIdentity());

    REQUIRE(page.loadColorPreset(path));
    CHECK(page.colorParams().rGain == Catch::Approx(1.7));
    CHECK(page.colorParams().saturation == Catch::Approx(0.5));
    QFile::remove(path);
}

TEST_CASE("VideoPage: loadColorPreset rejects a missing file", "[video][page][interaction]") {
    app();
    VideoPage page;
    CHECK_FALSE(page.loadColorPreset(QDir::tempPath() + QStringLiteral("/sf_no_such_preset.json")));
}

TEST_CASE("VideoPage: record source routes raw vs corrected pixels", "[video][page][interaction]") {
    app();
    VideoPage page;
    page.onRunningChanged(true);
    signalforge::video::ColorParams p;
    p.rGain = 2.0;  // red doubled
    page.setColorParams(p);
    page.onFrameReady(solidFrame(2, 2, 100));  // raw R=100; corrected R=200

    // Corrected source (default).
    page.setRecordSource(false);
    CHECK_FALSE(page.recordRaw());
    const QString cpath = QDir::tempPath() + QStringLiteral("/sf_rec_corrected.raw");
    QFile::remove(cpath);
    REQUIRE(page.startRecording(cpath, signalforge::video::VideoRecorder::Format::Raw));
    page.onFrameReady(solidFrame(2, 2, 100));
    page.stopRecording();
    QFile cf(cpath);
    REQUIRE(cf.open(QIODevice::ReadOnly));
    const QByteArray cb = cf.readAll();
    cf.close();
    REQUIRE(cb.size() == 2 * 2 * 3);
    CHECK(static_cast<unsigned char>(cb[0]) == 200);  // corrected red
    CHECK(static_cast<unsigned char>(cb[1]) == 100);  // green unchanged
    QFile::remove(cpath);

    // Raw source.
    page.setRecordSource(true);
    CHECK(page.recordRaw());
    const QString rpath = QDir::tempPath() + QStringLiteral("/sf_rec_raw.raw");
    QFile::remove(rpath);
    REQUIRE(page.startRecording(rpath, signalforge::video::VideoRecorder::Format::Raw));
    page.onFrameReady(solidFrame(2, 2, 100));
    page.stopRecording();
    QFile rf(rpath);
    REQUIRE(rf.open(QIODevice::ReadOnly));
    const QByteArray rb = rf.readAll();
    rf.close();
    CHECK(static_cast<unsigned char>(rb[0]) == 100);  // raw red (uncorrected)
    QFile::remove(rpath);
}

TEST_CASE("VideoPage: end-to-end receiver → page wiring", "[video][page][interaction]") {
    app();
    signalforge::video::registerMetatypes();
    constexpr quint16 kPort = 45014;

    VideoUdpReceiver rx(kPort);
    VideoPage page;
    QObject::connect(&rx, &VideoUdpReceiver::frameReady, &page, &VideoPage::onFrameReady);
    QObject::connect(&rx, &VideoUdpReceiver::runningChanged, &page, &VideoPage::onRunningChanged);

    rx.start();
    REQUIRE(waitFor([&] { return page.statusText().contains(QStringLiteral("Waiting")); }));

    QUdpSocket sender;
    const int w = 8, h = 4;
    sendFrame(sender, kPort, 1, w, h, sfvideotest::makeFramePixels(w, h, 4), 24);

    REQUIRE(waitFor([&] { return page.view()->hasFrame(); }));
    CHECK(page.statusText().contains(QStringLiteral("Streaming")));
    CHECK(page.view()->currentFrame().size() == QSize(w, h));
}
