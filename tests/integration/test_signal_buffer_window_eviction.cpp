// tests/integration/test_signal_buffer_window_eviction.cpp
//
// S10 integration test (spec §5.3-4): registry-level time-window
// eviction. Register a signal with windowSeconds=1.0; push 2000 samples
// spread over 2 seconds at 1 kHz; verify roughly 1000 retained and
// 1000 evicted.

#include "buffer/signal_buffer.hpp"
#include "buffer/signal_buffer_registry.hpp"
#include "decode/decoder_interface.hpp"

#include <QString>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdint>

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

TEST_CASE("S10 integration: time-window eviction at registry level", "[integration][s10][buffer][window]") {
    using namespace signalforge::decoder;
    using namespace signalforge::buffer;

    RegistryConfig regCfg;
    regCfg.signalDefaults.windowSeconds = 1.0;   // 1-second window
    regCfg.signalDefaults.capSamples = 100'000;  // not the constraint
    regCfg.signalDefaults.estimatedRateHz = 1000.0;
    SignalBufferRegistry registry(regCfg);
    SignalValueSink* sink = &registry;

    const QString driverId = QStringLiteral("s10/window/driver");
    const QString signalId = QStringLiteral("s10/window/double");
    sink->onSignalsRegistered(driverId, {makeMeta(signalId, SignalType::Double)});

    const auto t0 = std::chrono::steady_clock::now();
    constexpr int kSampleCount = 2000;
    for (int i = 0; i < kSampleCount; ++i) {
        sink->onSignal(t0 + std::chrono::milliseconds(i), signalId, SignalValue{static_cast<double>(i)});
    }

    auto* buf = registry.bufferFor(signalId);
    REQUIRE(buf != nullptr);
    REQUIRE(buf->totalSamplesPushed() == static_cast<std::uint64_t>(kSampleCount));

    const auto retained = buf->sampleCount();
    REQUIRE(retained >= 998U);
    REQUIRE(retained <= 1002U);
    REQUIRE(buf->totalSamplesEvicted() == kSampleCount - retained);

    // The retained samples should be the most recent ones.
    auto* bufRaw = registry.bufferFor(signalId);
    auto latest10 = bufRaw->queryLatest(10);
    REQUIRE(latest10.size() == 10U);
    REQUIRE(std::get<double>(latest10.back().value) == static_cast<double>(kSampleCount - 1));
}
