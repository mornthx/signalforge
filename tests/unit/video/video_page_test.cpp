// tests/unit/video/video_page_test.cpp
#include "video/video_page.hpp"
#include "video/video_receiver.hpp"
#include "video/video_view.hpp"
#include "zvid_test_util.hpp"

#include <QApplication>
#include <QByteArray>
#include <QColor>
#include <QHostAddress>
#include <QImage>
#include <QSignalSpy>
#include <QThread>
#include <QUdpSocket>
#include <QtTest/QtTest>
#include <algorithm>
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
