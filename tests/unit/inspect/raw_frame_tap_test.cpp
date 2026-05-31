// tests/unit/inspect/raw_frame_tap_test.cpp
//
// M31 S1 — RawFrameTap: capture, indexing, ring eviction, incremental
// `since`, and a concurrent producer/consumer stress test (the tap is
// written on pipeline threads and read on the UI thread).

#include "frame/raw_frame.hpp"
#include "inspect/raw_frame_tap.hpp"

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <thread>

namespace insp = signalforge::inspect;
namespace frame = signalforge::frame;

namespace {

frame::RawFrame makeFrame(const QString& source, QByteArray payload, std::uint64_t seq) {
    frame::RawFrame f;
    f.sourceId = source;
    f.payload = std::move(payload);
    f.protocolHint = QStringLiteral("udp");
    f.recvAt = std::chrono::steady_clock::now();
    f.sequenceNumber = seq;
    return f;
}

}  // namespace

TEST_CASE("M31 S1: tap captures frames with monotonic indices", "[inspect][m31][tap]") {
    insp::RawFrameTap tap(100);
    CHECK(tap.totalCaptured() == 0);
    CHECK(tap.snapshot().empty());

    tap.onFrame(makeFrame(QStringLiteral("udp:rig"), QByteArray("\x01\x02", 2), 1));
    tap.onFrame(makeFrame(QStringLiteral("udp:rig"), QByteArray("\x03", 1), 2));

    const auto all = tap.snapshot();
    REQUIRE(all.size() == 2);
    CHECK(all[0].index == 1);
    CHECK(all[1].index == 2);
    CHECK(all[0].source == QStringLiteral("udp:rig"));
    CHECK(all[1].payload == QByteArray("\x03", 1));
    CHECK(tap.totalCaptured() == 2);
}

TEST_CASE("M31 S1: ring buffer evicts oldest beyond capacity", "[inspect][m31][tap]") {
    insp::RawFrameTap tap(3);
    for (int i = 1; i <= 5; ++i) {
        tap.onFrame(makeFrame(QStringLiteral("s"), QByteArray(1, char(i)), std::uint64_t(i)));
    }
    const auto all = tap.snapshot();
    REQUIRE(all.size() == 3);       // only the last 3 retained
    CHECK(all.front().index == 3);  // 1 and 2 evicted
    CHECK(all.back().index == 5);
    CHECK(tap.totalCaptured() == 5);  // counter keeps counting evicted
}

TEST_CASE("M31 S1: since() returns only newer frames", "[inspect][m31][tap]") {
    insp::RawFrameTap tap(100);
    for (int i = 1; i <= 4; ++i) {
        tap.onFrame(makeFrame(QStringLiteral("s"), QByteArray(1, char(i)), std::uint64_t(i)));
    }
    const auto recent = tap.since(2);
    REQUIRE(recent.size() == 2);
    CHECK(recent[0].index == 3);
    CHECK(recent[1].index == 4);
    CHECK(tap.since(4).empty());
}

TEST_CASE("M31 S1: clear drops buffered frames, keeps the counter", "[inspect][m31][tap]") {
    insp::RawFrameTap tap(100);
    tap.onFrame(makeFrame(QStringLiteral("s"), QByteArray("x", 1), 1));
    tap.clear();
    CHECK(tap.snapshot().empty());
    CHECK(tap.totalCaptured() == 1);
    tap.onFrame(makeFrame(QStringLiteral("s"), QByteArray("y", 1), 2));
    CHECK(tap.snapshot().front().index == 2);  // index continues
}

TEST_CASE("M31 S1: concurrent producers + reader stay consistent", "[inspect][m31][tap][concurrent]") {
    insp::RawFrameTap tap(10000);
    std::atomic<bool> stop{false};
    constexpr int kProducers = 4;
    constexpr int kPerProducer = 5000;

    std::vector<std::thread> producers;
    producers.reserve(kProducers);
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&tap, p]() {
            for (int i = 0; i < kPerProducer; ++i) {
                tap.onFrame(makeFrame(QStringLiteral("p%1").arg(p), QByteArray(4, char(i & 0xFF)), std::uint64_t(i)));
            }
        });
    }
    // Reader hammers snapshot/since/total concurrently — must never crash or
    // observe a torn frame.
    std::thread reader([&tap, &stop]() {
        std::uint64_t lastSeen = 0;
        while (!stop.load()) {
            const auto recent = tap.since(lastSeen);
            for (const auto& f : recent) {
                lastSeen = std::max(lastSeen, f.index);
            }
            (void)tap.snapshot().size();
        }
    });

    for (auto& t : producers) {
        t.join();
    }
    stop.store(true);
    reader.join();

    CHECK(tap.totalCaptured() == std::uint64_t(kProducers) * kPerProducer);
    // Buffer is capped; indices in the snapshot are strictly increasing.
    const auto all = tap.snapshot();
    REQUIRE(all.size() <= 10000);
    for (std::size_t i = 1; i < all.size(); ++i) {
        CHECK(all[i - 1].index < all[i].index);
    }
}
