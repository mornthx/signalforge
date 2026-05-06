// tests/integration/test_signal_buffer_concurrent.cpp
//
// S10 integration test (spec §5.3-2): writer thread pushes 1 M Double
// samples through the registry; 4 reader threads call queryLatest at
// ~1 kHz cadence. Verifies no crashes, terminal counts match, and the
// snapshot pattern remains consistent under realistic contention. CI
// debug-asan is the authoritative ASan / TSan gate.

#include "buffer/signal_buffer.hpp"
#include "buffer/signal_buffer_registry.hpp"
#include "decode/decoder_interface.hpp"

#include <QString>
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdint>
#include <thread>
#include <variant>
#include <vector>

namespace {

signalforge::decoder::SignalMetadata makeMeta(QString id, signalforge::decoder::SignalType type) {
    signalforge::decoder::SignalMetadata meta;
    meta.id = std::move(id);
    meta.name = QStringLiteral("Test");
    meta.unit = QStringLiteral("");
    meta.type = type;
    return meta;
}

}  // namespace

TEST_CASE("S10 integration: 1M-sample concurrent writer / 4 readers", "[integration][s10][buffer][concurrent]") {
    using namespace signalforge::decoder;
    using namespace signalforge::buffer;

    RegistryConfig regCfg;
    regCfg.signalDefaults.windowSeconds = 1e9;            // unbounded
    regCfg.signalDefaults.capSamples = 1'500'000;         // safely above 1 M
    regCfg.signalDefaults.estimatedRateHz = 200000;       // 200 kHz hint
    regCfg.totalBudgetBytes = 2ULL * 1024 * 1024 * 1024;  // 2 GB to skip rejection
    SignalBufferRegistry registry(regCfg);
    SignalValueSink* sink = &registry;

    const QString driverId = QStringLiteral("s10/concurrent/driver");
    const QString signalId = QStringLiteral("s10/concurrent/double");
    sink->onSignalsRegistered(driverId, {makeMeta(signalId, SignalType::Double)});
    REQUIRE(registry.signalCount() == 1U);

    constexpr int kPushCount = 1'000'000;

    std::atomic<bool> writerDone{false};
    std::atomic<int> readerErrors{0};
    std::atomic<std::int64_t> readerObservations{0};

    auto writerFn = [&]() {
        const auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < kPushCount; ++i) {
            sink->onSignal(t0 + std::chrono::microseconds(i), signalId, SignalValue{static_cast<double>(i)});
        }
        writerDone.store(true, std::memory_order_release);
    };

    auto readerFn = [&]() {
        int extraRounds = 100;
        while (!writerDone.load(std::memory_order_acquire) || extraRounds-- > 0) {
            auto* buf = registry.bufferFor(signalId);
            if (buf == nullptr) {
                continue;
            }
            const auto latest = buf->queryLatest(100);
            for (const auto& s : latest) {
                if (!std::holds_alternative<double>(s.value)) {
                    readerErrors.fetch_add(1, std::memory_order_relaxed);
                }
                readerObservations.fetch_add(1, std::memory_order_relaxed);
            }
            const auto one = buf->queryLatestOne();
            if (one.has_value() && !std::holds_alternative<double>(one->value)) {
                readerErrors.fetch_add(1, std::memory_order_relaxed);
            }
            std::this_thread::sleep_for(std::chrono::microseconds(1000));  // ~1 kHz cadence
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
    auto* buf = registry.bufferFor(signalId);
    REQUIRE(buf != nullptr);
    REQUIRE(buf->totalSamplesPushed() == static_cast<std::uint64_t>(kPushCount));
    REQUIRE(buf->sampleCount() == static_cast<std::size_t>(kPushCount));
    REQUIRE(readerObservations.load() > 0);  // readers ran

    sink->onSignalsUnregistered(driverId);
    REQUIRE(registry.signalCount() == 0U);
}
