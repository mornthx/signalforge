// tests/unit/frame/backpressure_test.cpp
#include "frame/backpressure.hpp"

#include <QString>
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <vector>

using namespace signalforge::frame;

namespace {

const QString kQueueName = QStringLiteral("qtest");

}

TEST_CASE("WatermarkTracker: default-state peakPct is zero", "[frame][backpressure]") {
    WatermarkTracker t(100);
    REQUIRE(t.peakPct() == 0);
}

TEST_CASE("WatermarkTracker: below high threshold yields no signal", "[frame][backpressure]") {
    WatermarkTracker t(100, 80, 60);
    for (std::uint64_t d : {0, 10, 40, 50, 79}) {
        REQUIRE(!t.observe(d, kQueueName).has_value());
    }
    REQUIRE(t.peakPct() == 79);
}

TEST_CASE("WatermarkTracker: crossing high threshold emits QueueFilling once", "[frame][backpressure]") {
    WatermarkTracker t(100, 80, 60);
    REQUIRE(!t.observe(50, kQueueName).has_value());
    auto sig = t.observe(80, kQueueName);
    REQUIRE(sig.has_value());
    REQUIRE(sig->reason == BackpressureReason::QueueFilling);
    REQUIRE(sig->watermarkPct == 80);
    REQUIRE(sig->currentDepth == 80);
    REQUIRE(sig->capacity == 100);
    REQUIRE(sig->queueName == kQueueName);

    // Subsequent observes above 80 do not re-emit Filling.
    REQUIRE(!t.observe(81, kQueueName).has_value());
    REQUIRE(!t.observe(82, kQueueName).has_value());
    REQUIRE(!t.observe(99, kQueueName).has_value());
}

TEST_CASE("WatermarkTracker: at capacity emits QueueFull", "[frame][backpressure]") {
    WatermarkTracker t(100, 80, 60);
    auto sig = t.observe(100, kQueueName);
    REQUIRE(sig.has_value());
    REQUIRE(sig->reason == BackpressureReason::QueueFull);
    REQUIRE(sig->watermarkPct == 100);
    REQUIRE(sig->currentDepth == 100);

    // Full continues to fire (signals active drop decisions).
    auto sig2 = t.observe(100, kQueueName);
    REQUIRE(sig2.has_value());
    REQUIRE(sig2->reason == BackpressureReason::QueueFull);
}

TEST_CASE("WatermarkTracker: recovering below 60 emits QueueRecovered once", "[frame][backpressure]") {
    WatermarkTracker t(100, 80, 60);
    // Climb above high.
    (void)t.observe(50, kQueueName);
    (void)t.observe(80, kQueueName);
    // Sit in hysteresis band [60, 80) — no signal.
    REQUIRE(!t.observe(75, kQueueName).has_value());
    REQUIRE(!t.observe(65, kQueueName).has_value());
    REQUIRE(!t.observe(60, kQueueName).has_value());

    // Drop below 60 → Recovered (once).
    auto rec = t.observe(59, kQueueName);
    REQUIRE(rec.has_value());
    REQUIRE(rec->reason == BackpressureReason::QueueRecovered);

    // Subsequent low observes do not re-emit.
    REQUIRE(!t.observe(50, kQueueName).has_value());
    REQUIRE(!t.observe(10, kQueueName).has_value());
}

TEST_CASE("WatermarkTracker: re-entering high after recovery emits Filling again", "[frame][backpressure]") {
    WatermarkTracker t(100, 80, 60);
    (void)t.observe(80, kQueueName);  // Filling #1
    (void)t.observe(50, kQueueName);  // Recovered #1
    auto again = t.observe(85, kQueueName);
    REQUIRE(again.has_value());
    REQUIRE(again->reason == BackpressureReason::QueueFilling);
}

TEST_CASE("WatermarkTracker: capacity 0 never signals", "[frame][backpressure]") {
    WatermarkTracker t(0);
    REQUIRE(!t.observe(0, kQueueName).has_value());
    REQUIRE(!t.observe(1000, kQueueName).has_value());
    REQUIRE(t.peakPct() == 0);
}

TEST_CASE("WatermarkTracker: peakPct tracks maximum observed", "[frame][backpressure]") {
    WatermarkTracker t(100);
    (void)t.observe(30, kQueueName);
    REQUIRE(t.peakPct() == 30);
    (void)t.observe(10, kQueueName);
    REQUIRE(t.peakPct() == 30);
    (void)t.observe(55, kQueueName);
    REQUIRE(t.peakPct() == 55);
}

TEST_CASE("WatermarkTracker: reset clears peak and aboveHigh state", "[frame][backpressure]") {
    WatermarkTracker t(100, 80, 60);
    (void)t.observe(90, kQueueName);
    REQUIRE(t.peakPct() == 90);
    t.reset();
    REQUIRE(t.peakPct() == 0);
    // After reset, we can cross-high again and receive a Filling signal.
    auto sig = t.observe(85, kQueueName);
    REQUIRE(sig.has_value());
    REQUIRE(sig->reason == BackpressureReason::QueueFilling);
}

TEST_CASE("WatermarkTracker: concurrent observe emits at most one Filling across threads", "[frame][backpressure]") {
    constexpr std::uint64_t kCap = 1000;
    WatermarkTracker t(kCap, 80, 60);
    constexpr int kThreads = 8;
    std::atomic<int> fillingCount{0};

    std::vector<std::thread> workers;
    workers.reserve(kThreads);
    for (int i = 0; i < kThreads; ++i) {
        workers.emplace_back([&] {
            // Each thread observes 800 (exactly at high). Only one CAS wins.
            if (auto s = t.observe(800, kQueueName); s && s->reason == BackpressureReason::QueueFilling) {
                fillingCount.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (auto& w : workers) {
        w.join();
    }
    REQUIRE(fillingCount.load() == 1);
}

TEST_CASE("WatermarkTracker: BackpressureSignal field consistency", "[frame][backpressure]") {
    WatermarkTracker t(200, 80, 60);
    auto sig = t.observe(160, kQueueName);
    REQUIRE(sig.has_value());
    REQUIRE(sig->watermarkPct == 80);
    REQUIRE(sig->currentDepth == 160);
    REQUIRE(sig->capacity == 200);
    REQUIRE(sig->queueName == kQueueName);
    REQUIRE(sig->at.time_since_epoch().count() > 0);
}

TEST_CASE("BackpressureSignal: default-init values", "[frame][backpressure]") {
    BackpressureSignal s;
    REQUIRE(s.reason == BackpressureReason::QueueFilling);
    REQUIRE(s.watermarkPct == 0);
    REQUIRE(s.currentDepth == 0);
    REQUIRE(s.capacity == 0);
}
