// tests/unit/video/video_receiver_test.cpp
#include "video/video_receiver.hpp"
#include "video/video_types.hpp"
#include "zvid_test_util.hpp"

#include <QByteArray>
#include <QCoreApplication>
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
#include <cstring>
#include <memory>

using signalforge::video::VideoStats;
using signalforge::video::VideoUdpReceiver;

namespace {

class CoreAppHolder {
public:
    CoreAppHolder() {
        signalforge::video::registerMetatypes();
        if (QCoreApplication::instance() == nullptr) {
            static int argc = 1;
            static char argv0[] = "video_receiver_test";
            static char* argv[] = {argv0, nullptr};
            app_ = std::make_unique<QCoreApplication>(argc, argv);
        }
    }

private:
    std::unique_ptr<QCoreApplication> app_;
};
CoreAppHolder g_app;

bool waitForSpy(QSignalSpy& spy, int minCount, int ms = 3000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (spy.count() < minCount && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(2);
    }
    return spy.count() >= minCount;
}

bool sawTrue(const QSignalSpy& spy) {
    return std::any_of(spy.begin(), spy.end(), [](const QList<QVariant>& call) { return call.at(0).toBool(); });
}

// Wait until runningChanged(true) has been observed since the spy was created.
bool waitBound(QSignalSpy& runningSpy, int ms = 3000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (!sawTrue(runningSpy) && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(2);
    }
    return sawTrue(runningSpy);
}

// Send one frame as chunked datagrams. If skipOffset >= 0, that chunk is omitted (simulated loss).
void sendFrame(QUdpSocket& s, quint16 port, std::uint32_t frameNo, int w, int h, const QByteArray& pixels,
               int chunkBytes, int skipOffset = -1) {
    const auto frameBytes = static_cast<std::uint32_t>(w * h * 3);
    for (int off = 0; off < pixels.size(); off += chunkBytes) {
        if (off == skipOffset) {
            continue;
        }
        const int clen = std::min(chunkBytes, static_cast<int>(pixels.size()) - off);
        const QByteArray chunk = pixels.mid(off, clen);
        const QByteArray dg =
            sfvideotest::makeDatagram(frameNo, static_cast<std::uint32_t>(off), frameBytes,
                                      static_cast<std::uint16_t>(w), static_cast<std::uint16_t>(h), chunk);
        s.writeDatagram(dg, QHostAddress(QHostAddress::LocalHost), port);
    }
}

// Byte-exact compare of a delivered RGB888 image against the source pixel buffer.
bool imageMatches(const QImage& img, int w, int h, const QByteArray& pixels) {
    if (img.width() != w || img.height() != h || img.format() != QImage::Format_RGB888) {
        return false;
    }
    for (int y = 0; y < h; ++y) {
        if (std::memcmp(img.constScanLine(y), pixels.constData() + static_cast<std::ptrdiff_t>(y) * w * 3,
                        static_cast<std::size_t>(w) * 3) != 0) {
            return false;
        }
    }
    return true;
}

}  // namespace

TEST_CASE("VideoUdpReceiver: reassembles and delivers a complete frame byte-exact", "[video][receiver]") {
    constexpr quint16 kPort = 45004;
    VideoUdpReceiver rx(kPort);
    QSignalSpy running(&rx, &VideoUdpReceiver::runningChanged);
    QSignalSpy frames(&rx, &VideoUdpReceiver::frameReady);
    rx.start();
    REQUIRE(waitBound(running));

    QUdpSocket sender;
    const int w = 8, h = 4;
    const QByteArray px = sfvideotest::makeFramePixels(w, h, 1);
    sendFrame(sender, kPort, /*frameNo=*/1, w, h, px, /*chunkBytes=*/24);

    REQUIRE(waitForSpy(frames, 1));
    const auto img = frames.at(0).at(0).value<QImage>();
    CHECK(imageMatches(img, w, h, px));
}

TEST_CASE("VideoUdpReceiver: drops an incomplete frame and delivers the next complete one", "[video][receiver]") {
    constexpr quint16 kPort = 45005;
    VideoUdpReceiver rx(kPort);
    QSignalSpy running(&rx, &VideoUdpReceiver::runningChanged);
    QSignalSpy frames(&rx, &VideoUdpReceiver::frameReady);
    QSignalSpy stats(&rx, &VideoUdpReceiver::statsUpdated);
    rx.start();
    REQUIRE(waitBound(running));

    QUdpSocket sender;
    const int w = 8, h = 4;
    const QByteArray px1 = sfvideotest::makeFramePixels(w, h, 1);
    const QByteArray px2 = sfvideotest::makeFramePixels(w, h, 2);
    sendFrame(sender, kPort, /*frameNo=*/1, w, h, px1, /*chunkBytes=*/24, /*skipOffset=*/24);  // lose one chunk
    sendFrame(sender, kPort, /*frameNo=*/2, w, h, px2, /*chunkBytes=*/24);                     // complete

    REQUIRE(waitForSpy(frames, 1));
    CHECK(frames.count() == 1);  // only the complete frame is delivered
    CHECK(imageMatches(frames.at(0).at(0).value<QImage>(), w, h, px2));

    // A stats sample should eventually report the dropped frame.
    REQUIRE(waitForSpy(stats, 1, 3000));
    const auto last = stats.back().at(0).value<VideoStats>();
    CHECK(last.framesDropped >= 1);
    CHECK(last.framesDelivered >= 1);
}

TEST_CASE("VideoUdpReceiver: ignores foreign-magic datagrams", "[video][receiver]") {
    constexpr quint16 kPort = 45006;
    VideoUdpReceiver rx(kPort);
    QSignalSpy running(&rx, &VideoUdpReceiver::runningChanged);
    QSignalSpy frames(&rx, &VideoUdpReceiver::frameReady);
    rx.start();
    REQUIRE(waitBound(running));

    QUdpSocket sender;
    const int w = 8, h = 4;
    const QByteArray junk =
        sfvideotest::makeDatagram(1, 0, static_cast<std::uint32_t>(w * h * 3), static_cast<std::uint16_t>(w),
                                  static_cast<std::uint16_t>(h), QByteArray(24, '\x01'), /*magic=*/0xDEADBEEF);
    sender.writeDatagram(junk, QHostAddress(QHostAddress::LocalHost), kPort);

    const QByteArray px = sfvideotest::makeFramePixels(w, h, 7);
    sendFrame(sender, kPort, /*frameNo=*/9, w, h, px, /*chunkBytes=*/24);

    REQUIRE(waitForSpy(frames, 1));
    CHECK(frames.count() == 1);
    CHECK(imageMatches(frames.at(0).at(0).value<QImage>(), w, h, px));
}

TEST_CASE("VideoUdpReceiver: reads resolution from the header", "[video][receiver]") {
    constexpr quint16 kPort = 45007;
    VideoUdpReceiver rx(kPort);
    QSignalSpy running(&rx, &VideoUdpReceiver::runningChanged);
    QSignalSpy frames(&rx, &VideoUdpReceiver::frameReady);
    rx.start();
    REQUIRE(waitBound(running));

    QUdpSocket sender;
    const int w = 4, h = 2;
    const QByteArray px = sfvideotest::makeFramePixels(w, h, 3);
    sendFrame(sender, kPort, /*frameNo=*/1, w, h, px, /*chunkBytes=*/12);

    REQUIRE(waitForSpy(frames, 1));
    const auto img = frames.at(0).at(0).value<QImage>();
    CHECK(img.width() == w);
    CHECK(img.height() == h);
}

TEST_CASE("VideoUdpReceiver: rebind switches to a new port", "[video][receiver]") {
    constexpr quint16 kPortA = 45008;
    constexpr quint16 kPortB = 45009;
    VideoUdpReceiver rx(kPortA);
    QSignalSpy running(&rx, &VideoUdpReceiver::runningChanged);
    QSignalSpy frames(&rx, &VideoUdpReceiver::frameReady);
    rx.start();
    REQUIRE(waitBound(running));

    rx.rebind(kPortB);
    CHECK(rx.port() == kPortB);
    // rebind emits runningChanged(false) then (true); wait for the second true.
    REQUIRE(waitForSpy(running, 3));  // initial true, then false, then true

    QUdpSocket sender;
    const int w = 8, h = 4;
    const QByteArray px = sfvideotest::makeFramePixels(w, h, 5);
    sendFrame(sender, kPortB, /*frameNo=*/1, w, h, px, /*chunkBytes=*/24);

    REQUIRE(waitForSpy(frames, 1));
    CHECK(imageMatches(frames.at(0).at(0).value<QImage>(), w, h, px));
}
