// tests/integration/test_chart_lod_selection.cpp
//
// S9 integration test (spec §2.1-12 #4): push 100k samples (the
// M8-prototype's 600k baseline is overkill for unit-test wall
// time on CI; 100k still spans LOD 0 / 1 / 2 / 3 thresholds
// per M6 §4.5). Set the visible window to 4 zoom levels and
// verify queryRange returns appropriately decimated counts via
// the chart's stats.

#include "buffer/signal_buffer.hpp"
#include "buffer/signal_buffer_registry.hpp"
#include "chart/chart.hpp"
#include "chart/time_axis_manager.hpp"
#include "decode/decoder_interface.hpp"

#include <QGuiApplication>
#include <QMetaObject>
#include <QString>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cmath>

namespace ch8 = signalforge::chart;
namespace bm6 = signalforge::buffer;
namespace dm5 = signalforge::decoder;

namespace {

QGuiApplication& app() {
    static int argc = 1;
    static char arg0[] = "test_chart_lod_selection";
    static char* argv[] = {arg0, nullptr};
    static QGuiApplication instance(argc, argv);
    return instance;
}

}  // namespace

TEST_CASE("S9 integration: queryRange across 4 zoom levels returns ≤ target each", "[integration][s9][chart][lod]") {
    app();
    bm6::SignalBufferRegistry registry;
    dm5::SignalMetadata m;
    m.id = QStringLiteral("d/lod_sig");
    m.name = QStringLiteral("");
    m.unit = QStringLiteral("");
    m.type = dm5::SignalType::Double;
    registry.onSignalsRegistered(QStringLiteral("d"), {m});

    constexpr int kSamples = 100'000;
    constexpr double kSampleHz = 1000.0;
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kSamples; ++i) {
        const double y = std::sin(i * 0.01);
        registry.onSignal(t0 + std::chrono::microseconds(static_cast<std::int64_t>(i * 1e6 / kSampleHz)),
                          QStringLiteral("d/lod_sig"), dm5::SignalValue{y});
    }

    auto* buf = registry.bufferFor(QStringLiteral("d/lod_sig"));
    REQUIRE(buf != nullptr);

    constexpr std::size_t kTargetBins = 1920;
    const auto end = t0 + std::chrono::microseconds(static_cast<std::int64_t>(kSamples * 1e6 / kSampleHz));

    // 4 zoom levels: 1 sec / 10 sec / 1 min / full.
    const auto zoom1s = buf->queryRange(end - std::chrono::seconds(1), end, kTargetBins);
    const auto zoom10s = buf->queryRange(end - std::chrono::seconds(10), end, kTargetBins);
    const auto zoom60s = buf->queryRange(end - std::chrono::seconds(60), end, kTargetBins);
    const auto zoomFull = buf->queryRange(t0, end, kTargetBins);

    // The LOD pipeline returns at most `kTargetBins * 2` samples
    // (LOD bins are 2 samples each: min @ t_start, max @ t_end —
    // M6 §4.5 design).
    REQUIRE(zoom1s.size() <= kTargetBins * 2);
    REQUIRE(zoom10s.size() <= kTargetBins * 2);
    REQUIRE(zoom60s.size() <= kTargetBins * 2);
    REQUIRE(zoomFull.size() <= kTargetBins * 2);

    // 1 sec at 1 kHz = 1000 samples ≤ 2*target → raw is selected,
    // so the result is ~1000 samples.
    REQUIRE(zoom1s.size() <= 1100);
    // Full window (retained samples, target=1920). M34 P2 count-based
    // selection picks the FINEST level that fits 2*target — LOD 2
    // (bin 100) here → ~1200 samples. It fills a good share of the
    // pixel budget instead of collapsing to ~200 (the pre-fix bug),
    // stays within 2*target, and is clearly decimated below the raw count.
    REQUIRE(zoomFull.size() >= kTargetBins / 2U);
    REQUIRE(zoomFull.size() < kSamples / 10U);
}
