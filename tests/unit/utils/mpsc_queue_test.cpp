// tests/unit/utils/mpsc_queue_test.cpp
#include "utils/mpsc_queue.hpp"

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <set>
#include <thread>
#include <vector>

using namespace signalforge::utils;

TEST_CASE("MpscQueue: default-empty pop returns nullopt", "[utils][mpsc_queue]") {
    MpscQueue<int> q;
    REQUIRE(!q.pop().has_value());
    REQUIRE(q.sizeApprox() == 0);
}

TEST_CASE("MpscQueue: single-thread push+pop preserves order", "[utils][mpsc_queue]") {
    MpscQueue<int> q;
    for (int i = 0; i < 100; ++i) {
        REQUIRE(q.push(i));
    }
    for (int i = 0; i < 100; ++i) {
        auto v = q.pop();
        REQUIRE(v.has_value());
        REQUIRE(*v == i);
    }
    REQUIRE(!q.pop().has_value());
    REQUIRE(q.sizeApprox() == 0);
}

TEST_CASE("MpscQueue: 8-producer × 100k stress with single consumer", "[utils][mpsc_queue][stress]") {
    constexpr int kProducers = 8;
    constexpr int kPerProducer = 100'000;
    constexpr int kTotal = kProducers * kPerProducer;
    MpscQueue<int> q;

    std::atomic<int> produced{0};
    std::vector<std::thread> producers;
    producers.reserve(kProducers);
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&, p] {
            for (int i = 0; i < kPerProducer; ++i) {
                const int val = p * kPerProducer + i;
                while (!q.push(val)) {
                    // moodycamel's enqueue should not spuriously fail; retry on
                    // extreme allocator pressure, otherwise this never loops.
                    std::this_thread::yield();
                }
                produced.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    std::set<int> received;
    std::thread consumer([&] {
        while (static_cast<int>(received.size()) < kTotal) {
            if (auto v = q.pop()) {
                received.insert(*v);
            }
        }
    });

    for (auto& t : producers) {
        t.join();
    }
    consumer.join();

    REQUIRE(produced.load() == kTotal);
    REQUIRE(static_cast<int>(received.size()) == kTotal);
    // Every value 0..kTotal-1 exactly once (order across producers is not
    // guaranteed; set uniqueness verifies no drops and no duplicates).
    REQUIRE(*received.begin() == 0);
    REQUIRE(*received.rbegin() == kTotal - 1);
}

TEST_CASE("MpscQueue: sizeApprox converges to zero after drain", "[utils][mpsc_queue]") {
    MpscQueue<int> q;
    for (int i = 0; i < 1000; ++i) {
        REQUIRE(q.push(i));
    }
    REQUIRE(q.sizeApprox() > 0);
    while (q.pop().has_value()) {
        // drain
    }
    REQUIRE(q.sizeApprox() == 0);
}

TEST_CASE("MpscQueue: initial capacity hint is accepted", "[utils][mpsc_queue]") {
    MpscQueue<int> q1(16);
    MpscQueue<int> q2(4096);
    MpscQueue<int> q3(1);
    REQUIRE(q1.push(1));
    REQUIRE(q2.push(2));
    REQUIRE(q3.push(3));
    REQUIRE(*q1.pop() == 1);
    REQUIRE(*q2.pop() == 2);
    REQUIRE(*q3.pop() == 3);
}
