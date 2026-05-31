// tests/integration/test_signal_buffer_lod.cpp
//
// S10 integration test (spec §5.3-3): 600 000 samples of sine + noise
// at synthetic 1 kHz over a 10-minute window. Query with target=100;
// verify the LOD-decimated output and confirm every level-3 bin's
// [min, max] envelope contains the corresponding raw samples (HALT
// trigger #7 final gate).

#include "buffer/signal_buffer.hpp"
#include "buffer/signal_buffer_registry.hpp"
#include "decode/decoder_interface.hpp"

#include <QString>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cmath>
#include <cstdint>
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

[[nodiscard]] double sample(int i) {
    constexpr double pi = 3.14159265358979323846;
    const double t = static_cast<double>(i) / 1000.0;   // seconds
    const double base = std::sin(2.0 * pi * 0.05 * t);  // 0.05 Hz sine
    const double noise = 0.1 * std::sin(2.0 * pi * 50.0 * t + 1.7);
    return base + noise;
}

}  // namespace

TEST_CASE("S10 integration: LOD level 3 selection and envelope correctness", "[integration][s10][buffer][lod]") {
    using namespace signalforge::decoder;
    using namespace signalforge::buffer;

    RegistryConfig regCfg;
    regCfg.signalDefaults.windowSeconds = 1e9;  // unbounded — keep all 10 minutes
    regCfg.signalDefaults.capSamples = 1'000'000;
    regCfg.signalDefaults.estimatedRateHz = 1000.0;       // 1 kHz hint
    regCfg.totalBudgetBytes = 1ULL * 1024 * 1024 * 1024;  // 1 GB to skip rejection
    SignalBufferRegistry registry(regCfg);
    SignalValueSink* sink = &registry;

    const QString driverId = QStringLiteral("s10/lod/driver");
    const QString signalId = QStringLiteral("s10/lod/double");
    sink->onSignalsRegistered(driverId, {makeMeta(signalId, SignalType::Double)});

    constexpr int kSampleCount = 600'000;  // 10 min × 1 kHz
    const auto t0 = std::chrono::steady_clock::now();
    std::vector<double> rawValues;
    rawValues.reserve(kSampleCount);
    for (int i = 0; i < kSampleCount; ++i) {
        const double v = sample(i);
        rawValues.push_back(v);
        sink->onSignal(t0 + std::chrono::milliseconds(i), signalId, SignalValue{v});
    }
    auto* buf = registry.bufferFor(signalId);
    REQUIRE(buf != nullptr);
    REQUIRE(buf->totalSamplesPushed() == static_cast<std::uint64_t>(kSampleCount));

    // LOD bin counts: 60000 / 600 / 60 (level 1 / 2 / 3).
    REQUIRE(buf->lodBinCount(1) == 60'000U);
    REQUIRE(buf->lodBinCount(2) == 6'000U);
    REQUIRE(buf->lodBinCount(3) == 600U);

    // Query the full 10-minute range with target_count = 100 — picks level 3
    // (density = signal_period / samples_per_pixel = 1ms / 6s = ~1.67e-4).
    const auto rangeEnd = t0 + std::chrono::milliseconds(kSampleCount);
    const auto out = buf->queryRange(t0, rangeEnd, 100);

    // Level 3 has 600 bins → 1200 SignalSamples (min + max per bin).
    // The query may include partial-overlap bins from outside the range too;
    // expect ≥ 1000 and well below the raw-sample count.
    REQUIRE(out.size() >= 1000U);
    REQUIRE(out.size() < static_cast<std::size_t>(kSampleCount / 100));

    // HALT trigger #7 self-check: every level-3 bin's [min, max] must
    // contain every raw sample in that bin's range. The level-3 binSize is
    // 1000, so bin i covers raw indices [1000 i, 1000 i + 1000).
    constexpr int kLodBinSize = 1000;
    const std::size_t numBins = out.size() / 2;  // 2 SignalSamples per bin
    for (std::size_t b = 0; b < numBins; ++b) {
        // M34 P2: the pair is emitted in time order (min/max at their real
        // timestamps), so take the extremes by value, not by position.
        const double v0 = std::get<double>(out[2 * b].value);
        const double v1 = std::get<double>(out[2 * b + 1].value);
        const double minV = std::min(v0, v1);
        const double maxV = std::max(v0, v1);
        REQUIRE(minV <= maxV);
        const std::size_t rawStart = b * kLodBinSize;
        const std::size_t rawEnd = std::min(rawStart + kLodBinSize, static_cast<std::size_t>(kSampleCount));
        for (std::size_t i = rawStart; i < rawEnd; ++i) {
            REQUIRE(rawValues[i] >= minV);
            REQUIRE(rawValues[i] <= maxV);
        }
    }
}
