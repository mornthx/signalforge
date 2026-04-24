// tests/unit/utils/spsc_ring_test.cpp
#include "utils/spsc_ring.hpp"

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <vector>

using namespace signalforge::utils;

TEST_CASE("SpscRing: default-empty pop returns nullopt", "[utils][spsc_ring]") {
    SpscRing<int> r(8);
    REQUIRE(r.size() == 0);
    REQUIRE(!r.pop().has_value());
}

TEST_CASE("SpscRing: single push+pop preserves value", "[utils][spsc_ring]") {
    SpscRing<int> r(8);
    REQUIRE(r.push(42));
    REQUIRE(r.size() == 1);
    auto x = r.pop();
    REQUIRE(x.has_value());
    REQUIRE(*x == 42);
    REQUIRE(r.size() == 0);
}

TEST_CASE("SpscRing: FIFO ordering is preserved", "[utils][spsc_ring]") {
    SpscRing<int> r(8);
    for (int i = 0; i < 4; ++i) {
        REQUIRE(r.push(i));
    }
    REQUIRE(r.size() == 4);
    for (int i = 0; i < 4; ++i) {
        auto v = r.pop();
        REQUIRE(v.has_value());
        REQUIRE(*v == i);
    }
    REQUIRE(r.size() == 0);
}

TEST_CASE("SpscRing: fill to capacity then reject further pushes", "[utils][spsc_ring]") {
    SpscRing<int> r(4);  // capacity rounds up to pow-of-2; usable capacity >= 4
    const std::size_t cap = r.capacity();
    REQUIRE(cap >= 4);
    for (std::size_t i = 0; i < cap; ++i) {
        REQUIRE(r.push(static_cast<int>(i)));
    }
    // Ring full: next push fails; existing items still retrievable.
    REQUIRE(!r.push(999));
    REQUIRE(r.size() == cap);

    // Drain.
    for (std::size_t i = 0; i < cap; ++i) {
        auto v = r.pop();
        REQUIRE(v.has_value());
        REQUIRE(*v == static_cast<int>(i));
    }
    REQUIRE(r.size() == 0);
}

TEST_CASE("SpscRing: interleaved push/pop works", "[utils][spsc_ring]") {
    SpscRing<int> r(4);
    for (int i = 0; i < 100; ++i) {
        REQUIRE(r.push(i));
        auto v = r.pop();
        REQUIRE(v.has_value());
        REQUIRE(*v == i);
    }
    REQUIRE(r.size() == 0);
}

TEST_CASE("SpscRing: capacity rounds to next power of two", "[utils][spsc_ring]") {
    // With capacity arg 5, internal usable capacity should be at least 5.
    SpscRing<int> r(5);
    REQUIRE(r.capacity() >= 5);
    // But not gigantic.
    REQUIRE(r.capacity() <= 16);
}

TEST_CASE("SpscRing: peakSize tracks high-water mark", "[utils][spsc_ring]") {
    SpscRing<int> r(8);
    REQUIRE(r.peakSize() == 0);
    REQUIRE(r.push(1));
    REQUIRE(r.push(2));
    REQUIRE(r.push(3));
    REQUIRE(r.peakSize() >= 3);
    (void)r.pop();
    (void)r.pop();
    REQUIRE(r.peakSize() >= 3);  // monotonic
    REQUIRE(r.push(4));
    REQUIRE(r.peakSize() >= 3);
}

TEST_CASE("SpscRing: move-only types work", "[utils][spsc_ring]") {
    SpscRing<std::unique_ptr<int>> r(4);
    auto p = std::make_unique<int>(7);
    REQUIRE(r.push(std::move(p)));
    auto back = r.pop();
    REQUIRE(back.has_value());
    REQUIRE(*back);
    REQUIRE(**back == 7);
}

TEST_CASE("SpscRing: 1M item stress (producer/consumer threads)", "[utils][spsc_ring][stress]") {
    constexpr int kItems = 1'000'000;
    SpscRing<int> r(1024);

    std::thread producer([&] {
        int i = 0;
        while (i < kItems) {
            if (r.push(i)) {
                ++i;
            }
            // else: full; spin until consumer drains
        }
    });

    std::vector<int> received;
    received.reserve(kItems);
    std::thread consumer([&] {
        while (static_cast<int>(received.size()) < kItems) {
            if (auto v = r.pop()) {
                received.push_back(*v);
            }
        }
    });

    producer.join();
    consumer.join();

    REQUIRE(static_cast<int>(received.size()) == kItems);
    // FIFO ordering under single producer / single consumer.
    for (int i = 0; i < kItems; ++i) {
        REQUIRE(received[i] == i);
    }
}
