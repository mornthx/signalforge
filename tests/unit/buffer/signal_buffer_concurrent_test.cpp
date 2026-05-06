// tests/unit/buffer/signal_buffer_concurrent_test.cpp
//
// S9 — single-writer / multi-reader concurrency stress for the
// SignalBuffer snapshot pattern. CI debug-asan runs this test as the
// authoritative gate for HALT trigger #5 (data race detection).

#include "buffer/signal_buffer.hpp"

#include <QString>
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdint>
#include <thread>
#include <variant>
#include <vector>

namespace bm6 = signalforge::buffer;
namespace dm5 = signalforge::decoder;

namespace {

dm5::SignalMetadata makeMeta(QString id, dm5::SignalType type) {
    dm5::SignalMetadata meta;
    meta.id = std::move(id);
    meta.name = QStringLiteral("Test");
    meta.unit = QStringLiteral("");
    meta.type = type;
    return meta;
}

}  // namespace

TEST_CASE("S9: 1 writer + 4 readers, 100k Double pushes, snapshot consistency", "[buffer][s9][concurrent]") {
    auto meta = makeMeta(QStringLiteral("s9/concurrent/double"), dm5::SignalType::Double);
    bm6::SignalBufferConfig cfg;
    cfg.windowSeconds = 1e9;    // unbounded — keep all samples
    cfg.capSamples = 200'000;   // safely above the push count
    cfg.estimatedRateHz = 1e6;  // hint for LOD level selection
    bm6::SignalBuffer buf(meta, cfg);

    constexpr int kPushCount = 100'000;

    std::atomic<bool> writerDone{false};
    std::atomic<int> readerErrors{0};
    std::atomic<std::int64_t> readerObservations{0};

    auto writerFn = [&]() {
        const auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < kPushCount; ++i) {
            buf.push(t0 + std::chrono::microseconds(i), dm5::SignalValue{static_cast<double>(i)});
        }
        writerDone.store(true, std::memory_order_release);
    };

    auto readerFn = [&]() {
        // Run while the writer is still pushing and a few extra rounds afterwards.
        int extraRounds = 50;
        while (!writerDone.load(std::memory_order_acquire) || extraRounds-- > 0) {
            // Short, simple queries to maximize contention.
            const auto latest = buf.queryLatest(100);
            for (const auto& s : latest) {
                if (!std::holds_alternative<double>(s.value)) {
                    readerErrors.fetch_add(1, std::memory_order_relaxed);
                }
                readerObservations.fetch_add(1, std::memory_order_relaxed);
            }
            const auto one = buf.queryLatestOne();
            if (one.has_value()) {
                if (!std::holds_alternative<double>(one->value)) {
                    readerErrors.fetch_add(1, std::memory_order_relaxed);
                }
                if (one->timestamp.time_since_epoch().count() == 0) {
                    readerErrors.fetch_add(1, std::memory_order_relaxed);
                }
            }
            // queryRange over the full window — exercises both raw and (via LOD)
            // the level-selection branch.
            const auto rangeStart = std::chrono::steady_clock::now() - std::chrono::seconds(10);
            const auto rangeEnd = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            const auto ranged = buf.queryRange(rangeStart, rangeEnd, 1000);
            for (const auto& s : ranged) {
                if (!std::holds_alternative<double>(s.value)) {
                    readerErrors.fetch_add(1, std::memory_order_relaxed);
                }
            }
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    };

    std::jthread writer(writerFn);
    std::vector<std::jthread> readers;
    readers.reserve(4);
    for (int i = 0; i < 4; ++i) {
        readers.emplace_back(readerFn);
    }
    writer.join();
    for (auto& r : readers) {
        r.join();
    }

    REQUIRE(readerErrors.load() == 0);
    REQUIRE(buf.totalSamplesPushed() == static_cast<std::uint64_t>(kPushCount));
    REQUIRE(buf.sampleCount() == static_cast<std::size_t>(kPushCount));
    REQUIRE(readerObservations.load() > 0);  // readers actually ran

    // Final snapshot post-join.
    auto final = buf.queryLatest(1000);
    REQUIRE(final.size() == 1000U);
    REQUIRE(std::get<double>(final.back().value) == static_cast<double>(kPushCount - 1));
}
