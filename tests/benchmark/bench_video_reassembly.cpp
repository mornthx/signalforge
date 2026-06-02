// tests/benchmark/bench_video_reassembly.cpp
//
// Measures sustained video reassembly throughput for the M35 VideoUdpReceiver.
// A sender blasts full 1280x720 RGB24 frames (1920 datagrams of 1440B payload,
// exactly as the board does) at the receiver over loopback for ~kDurationSec;
// the receiver reassembles on its IO thread and delivers complete QImages. We
// report delivered fps / Mbps and the drop count.
//
// End-to-end over a real socket, so the delivered rate is also bounded by the
// host's net.core.rmem_max (the M35 C4 caveat): a low rmem_max shows up as
// drops here exactly as it would in the field.
//
// No unit-test integration — opt-in via -DSIGNALFORGE_BENCHMARKS=ON.
#include "video/video_receiver.hpp"
#include "video/video_types.hpp"

#include <QByteArray>
#include <QCoreApplication>
#include <QHostAddress>
#include <QObject>
#include <QUdpSocket>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

using signalforge::video::VideoStats;
using signalforge::video::VideoUdpReceiver;

namespace {

constexpr int kDurationSec = 5;
constexpr int kWidth = 1280;
constexpr int kHeight = 720;
constexpr int kChunkPayload = 1440;  // board's CHUNK_PIX*bpp
constexpr std::uint32_t kFrameBytes = static_cast<std::uint32_t>(kWidth) * kHeight * 3;

void appendLe16(QByteArray& b, std::uint16_t v) {
    b.append(static_cast<char>(v & 0xFF));
    b.append(static_cast<char>((v >> 8) & 0xFF));
}
void appendLe32(QByteArray& b, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        b.append(static_cast<char>((v >> (8 * i)) & 0xFF));
    }
}

// Precompute one frame's 1920 datagrams (frame number patched per send).
std::vector<QByteArray> buildFrameDatagrams() {
    std::vector<QByteArray> dgs;
    const QByteArray pixels(static_cast<int>(kFrameBytes), '\x40');
    for (std::uint32_t off = 0; off < kFrameBytes; off += kChunkPayload) {
        const std::uint16_t clen =
            static_cast<std::uint16_t>(std::min<std::uint32_t>(kChunkPayload, kFrameBytes - off));
        QByteArray d;
        appendLe32(d, signalforge::video::kZvidMagic);
        appendLe32(d, 0);  // frame number, patched at send time (bytes 4..7)
        appendLe32(d, off);
        appendLe32(d, kFrameBytes);
        appendLe16(d, kWidth);
        appendLe16(d, kHeight);
        appendLe16(d, 3);
        appendLe16(d, clen);
        d.append(pixels.constData() + off, clen);
        dgs.push_back(std::move(d));
    }
    return dgs;
}

void patchFrameNumber(QByteArray& dg, std::uint32_t frame) {
    for (int i = 0; i < 4; ++i) {
        dg.data()[4 + i] = static_cast<char>((frame >> (8 * i)) & 0xFF);
    }
}

}  // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    signalforge::video::registerMetatypes();

    constexpr quint16 kPort = 47004;
    VideoUdpReceiver rx(kPort);

    std::uint64_t delivered = 0;
    VideoStats lastStats;
    QObject ctx;
    QObject::connect(&rx, &VideoUdpReceiver::frameReady, &ctx, [&](const QImage&) { ++delivered; });
    QObject::connect(&rx, &VideoUdpReceiver::statsUpdated, &ctx, [&](const VideoStats& s) { lastStats = s; });

    rx.start();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 200);

    QUdpSocket sender;
    auto dgs = buildFrameDatagrams();
    const QHostAddress dst(QHostAddress::LocalHost);

    const auto start = std::chrono::steady_clock::now();
    std::uint32_t frameNo = 0;
    std::uint64_t sent = 0;
    while (std::chrono::steady_clock::now() - start < std::chrono::seconds(kDurationSec)) {
        ++frameNo;
        for (auto& dg : dgs) {
            patchFrameNumber(dg, frameNo);
            sender.writeDatagram(dg, dst, kPort);
            ++sent;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 1);  // drain delivered, pace lightly
    }
    // Drain remaining deliveries.
    for (int i = 0; i < 20; ++i) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

    const double fps = static_cast<double>(delivered) / secs;
    const double mbps = static_cast<double>(delivered) * kFrameBytes * 8.0 / 1.0e6 / secs;
    std::printf(R"({"scenario":"video_reassembly_loopback","sent_frames":%llu,"delivered_frames":%llu,)"
                R"("dropped_frames":%llu,"fps":%.1f,"mbps":%.1f,"seconds":%.1f})"
                "\n",
                static_cast<unsigned long long>(sent / dgs.size()), static_cast<unsigned long long>(delivered),
                static_cast<unsigned long long>(lastStats.framesDropped), fps, mbps, secs);
    return 0;
}
