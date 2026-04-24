// tests/unit/utils/snapshot_test.cpp
#include "utils/snapshot.hpp"

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <thread>
#include <vector>

using namespace signalforge::utils;

TEST_CASE("Snapshot: initial read returns nullptr", "[utils][snapshot]") {
    Snapshot<int> s;
    REQUIRE(s.read() == nullptr);
}

TEST_CASE("Snapshot: publish + read returns published value", "[utils][snapshot]") {
    Snapshot<int> s;
    s.publish(42);
    auto p = s.read();
    REQUIRE(p != nullptr);
    REQUIRE(*p == 42);
}

TEST_CASE("Snapshot: subsequent publish replaces the value for new readers", "[utils][snapshot]") {
    Snapshot<int> s;
    s.publish(1);
    auto oldReader = s.read();
    s.publish(2);

    // Old reader still sees the old value via its shared_ptr.
    REQUIRE(*oldReader == 1);
    // New read sees the new value.
    auto newReader = s.read();
    REQUIRE(*newReader == 2);
    // Both pointers are independently valid.
    REQUIRE(oldReader.get() != newReader.get());
}

TEST_CASE("Snapshot: previous value stays alive until last reader drops", "[utils][snapshot]") {
    struct TrackedDestructor {
        std::atomic<int>* counter;
        int id;
        TrackedDestructor(std::atomic<int>* c, int i) : counter(c), id(i) {}
        TrackedDestructor(const TrackedDestructor&) = delete;
        TrackedDestructor(TrackedDestructor&& other) noexcept : counter(other.counter), id(other.id) {
            other.counter = nullptr;
        }
        ~TrackedDestructor() {
            if (counter != nullptr) {
                counter->fetch_add(1, std::memory_order_relaxed);
            }
        }
    };

    std::atomic<int> destructs{0};
    Snapshot<TrackedDestructor> s;
    s.publish(TrackedDestructor{&destructs, 1});

    auto reader = s.read();  // keep the first value alive
    s.publish(TrackedDestructor{&destructs, 2});

    // The first value should NOT be destructed yet; the reader holds it.
    // (Destructs so far: two temporaries that got moved from during publish,
    //  which have counter reset to nullptr. So destructs should still be 0
    //  for the tracked ones.)
    REQUIRE(destructs.load() == 0);

    reader.reset();  // drop the last owner of the first value
    // Value 1 should now be destructed (plus value 2 is still held by the
    // snapshot's atomic).
    REQUIRE(destructs.load() == 1);

    s.publish(TrackedDestructor{&destructs, 3});  // drop value 2
    REQUIRE(destructs.load() == 2);
}

TEST_CASE("Snapshot: concurrent readers see consistent values (no torn reads)", "[utils][snapshot][stress]") {
    struct Pair {
        int a;
        int b;
    };
    Snapshot<Pair> s;
    s.publish(Pair{0, 0});

    std::atomic<bool> stop{false};
    std::atomic<int> torn{0};

    constexpr int kReaders = 4;
    std::vector<std::thread> readers;
    for (int i = 0; i < kReaders; ++i) {
        readers.emplace_back([&] {
            while (!stop.load(std::memory_order_relaxed)) {
                auto p = s.read();
                if (p && p->a != p->b) {
                    torn.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    // Writer: publish 100k pairs where a == b.
    for (int i = 0; i < 100'000; ++i) {
        s.publish(Pair{i, i});
    }
    stop.store(true, std::memory_order_relaxed);
    for (auto& t : readers) {
        t.join();
    }

    REQUIRE(torn.load() == 0);
}

TEST_CASE("Snapshot: non-trivial value type works", "[utils][snapshot]") {
    Snapshot<std::string> s;
    s.publish(std::string("hello"));
    auto r = s.read();
    REQUIRE(r != nullptr);
    REQUIRE(*r == "hello");

    s.publish(std::string("world"));
    auto r2 = s.read();
    REQUIRE(*r2 == "world");
    // Old reader still valid.
    REQUIRE(*r == "hello");
}
