// tests/unit/buffer/signal_buffer_query_test.cpp
//
// S6 — verifies the reader-side query API: queryRange, queryLatest,
// queryLatestOne. All queries operate on the most recently published
// segment; tests force enough pushes (≥ 100) to trigger one publish
// before querying.

#include "buffer/signal_buffer.hpp"

#include <QString>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdint>
#include <variant>

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

TEST_CASE("S6: Double queryRange round-trip with target=0", "[buffer][s6][query][double]") {
    auto meta = makeMeta(QStringLiteral("s6/query/double"), dm5::SignalType::Double);
    bm6::SignalBufferConfig cfg;
    cfg.windowSeconds = 1e9;
    cfg.capSamples = 10'000;
    bm6::SignalBuffer buf(meta, cfg);

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) {
        buf.push(t0 + std::chrono::microseconds(i), dm5::SignalValue{static_cast<double>(i)});
    }
    // 100th push triggered publish #1 with the first 100 samples.

    auto samples = buf.queryRange(t0, t0 + std::chrono::milliseconds(1), 0);
    REQUIRE(samples.size() == 100U);
    REQUIRE(samples.front().timestamp == t0);
    REQUIRE(std::get<double>(samples.front().value) == 0.0);
    REQUIRE(std::get<double>(samples.back().value) == 99.0);
}

TEST_CASE("S6: Int64 queryRange filters by time window", "[buffer][s6][query][int64]") {
    auto meta = makeMeta(QStringLiteral("s6/query/int64"), dm5::SignalType::Int64);
    bm6::SignalBufferConfig cfg;
    cfg.windowSeconds = 1e9;
    cfg.capSamples = 10'000;
    bm6::SignalBuffer buf(meta, cfg);

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 200; ++i) {
        buf.push(t0 + std::chrono::microseconds(i * 1000),  // 1ms apart
                 dm5::SignalValue{static_cast<std::int64_t>(i)});
    }
    // 200th push: 2 publishes occurred (at #100 and #200).

    // Query the middle 50 samples.
    auto samples = buf.queryRange(t0 + std::chrono::microseconds(50'000),  // i=50
                                  t0 + std::chrono::microseconds(99'000),  // i=99
                                  0);
    REQUIRE(samples.size() == 50U);
    REQUIRE(std::get<std::int64_t>(samples.front().value) == 50);
    REQUIRE(std::get<std::int64_t>(samples.back().value) == 99);
}

TEST_CASE("S6: queryRange with target>0 selects LOD on dense data", "[buffer][s6][query][lod]") {
    auto meta = makeMeta(QStringLiteral("s6/query/lod"), dm5::SignalType::Double);
    bm6::SignalBufferConfig cfg;
    cfg.windowSeconds = 1e9;
    cfg.capSamples = 100'000;
    cfg.estimatedRateHz = 1000.0;  // 1 kHz
    bm6::SignalBuffer buf(meta, cfg);

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 1000; ++i) {
        // 1 ms spacing matches the 1 kHz rate hint.
        buf.push(t0 + std::chrono::milliseconds(i), dm5::SignalValue{static_cast<double>(i)});
    }
    // 1000 pushes → publishes at 100, 200, ..., 1000 (10 publishes).
    // LOD: level 1 has 100 bins, level 2 has 10, level 3 has 1.

    // M34 P2: level selection is count-based — ratio = samples / target.
    // 1000 samples in window, target 100 → ratio 10 → level 1 (bin 10) →
    // 100 bins → 200 SignalSamples (min @ t_start, max @ t_end per bin).
    const auto samples = buf.queryRange(t0, t0 + std::chrono::milliseconds(1000), 100);
    REQUIRE(samples.size() == 200U);
    REQUIRE(std::get<double>(samples.front().value) == 0.0);   // global min (bin 0)
    REQUIRE(std::get<double>(samples.back().value) == 999.0);  // global max (last bin)

    // target = 1000 → ratio 1 → raw → all 1000 in-window samples.
    const auto samples2 = buf.queryRange(t0, t0 + std::chrono::milliseconds(1000), 1000);
    REQUIRE(samples2.size() == 1000U);
    REQUIRE(std::get<double>(samples2.front().value) == 0.0);
    REQUIRE(std::get<double>(samples2.back().value) == 999.0);
}

TEST_CASE("M34 P2: LOD emits min/max at their real timestamps (no sawtooth)", "[buffer][m34][query][lod]") {
    auto meta = makeMeta(QStringLiteral("m34/lod/order"), dm5::SignalType::Double);
    bm6::SignalBufferConfig cfg;
    cfg.windowSeconds = 1e9;
    cfg.capSamples = 100'000;
    bm6::SignalBuffer buf(meta, cfg);

    // Each level-1 bin (10 samples) falls 9 → 0, so within every bin the MAX
    // occurs at the earlier timestamp and the MIN at the later one.
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) {
        buf.push(t0 + std::chrono::milliseconds(i), dm5::SignalValue{static_cast<double>(9 - (i % 10))});
    }

    // target 10 → ratio forces level 1 (bin 10) → 10 bins × 2 points.
    const auto s = buf.queryRange(t0, t0 + std::chrono::milliseconds(100), 10);
    REQUIRE(s.size() == 20U);
    // Time-ordered envelope: the first bin emits its max (9 @ earlier t) before
    // its min (0 @ later t). The pre-fix fixed-position emit gave 0 then 9 — the
    // inversion that drew a sawtooth.
    REQUIRE(std::get<double>(s[0].value) == 9.0);
    REQUIRE(std::get<double>(s[1].value) == 0.0);
    REQUIRE(s[0].timestamp < s[1].timestamp);
}

TEST_CASE("S6: Double queryLatest returns tail in chronological order", "[buffer][s6][query][latest]") {
    auto meta = makeMeta(QStringLiteral("s6/query/latest"), dm5::SignalType::Double);
    bm6::SignalBufferConfig cfg;
    cfg.windowSeconds = 1e9;
    cfg.capSamples = 10'000;
    bm6::SignalBuffer buf(meta, cfg);

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) {
        buf.push(t0 + std::chrono::microseconds(i), dm5::SignalValue{static_cast<double>(i)});
    }
    // Publish #1 captures all 100 samples.

    auto samples = buf.queryLatest(20);
    REQUIRE(samples.size() == 20U);
    REQUIRE(std::get<double>(samples.front().value) == 80.0);
    REQUIRE(std::get<double>(samples.back().value) == 99.0);
}

TEST_CASE("S6: queryLatestOne returns most recent with non-negative age", "[buffer][s6][query][one]") {
    auto meta = makeMeta(QStringLiteral("s6/query/one"), dm5::SignalType::Int64);
    bm6::SignalBufferConfig cfg;
    cfg.capSamples = 10'000;
    bm6::SignalBuffer buf(meta, cfg);

    REQUIRE_FALSE(buf.queryLatestOne().has_value());

    // Use past timestamps so age is unambiguously positive.
    auto now = std::chrono::steady_clock::now();
    auto base = now - std::chrono::seconds(1);
    for (int i = 0; i < 100; ++i) {
        buf.push(base + std::chrono::microseconds(i), dm5::SignalValue{static_cast<std::int64_t>(i)});
    }

    auto lv = buf.queryLatestOne();
    REQUIRE(lv.has_value());
    REQUIRE(std::get<std::int64_t>(lv->value) == 99);
    REQUIRE(lv->timestamp == base + std::chrono::microseconds(99));
    REQUIRE(lv->age.count() > 0);
}

TEST_CASE("S6: Bool queryRange decodes bit-packed storage", "[buffer][s6][query][bool]") {
    auto meta = makeMeta(QStringLiteral("s6/query/bool"), dm5::SignalType::Bool);
    bm6::SignalBufferConfig cfg;
    cfg.windowSeconds = 1e9;
    cfg.capSamples = 10'000;
    bm6::SignalBuffer buf(meta, cfg);

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) {
        buf.push(t0 + std::chrono::microseconds(i), dm5::SignalValue{(i % 2) == 0});
    }

    auto samples = buf.queryRange(t0, t0 + std::chrono::milliseconds(1), 0);
    REQUIRE(samples.size() == 100U);
    for (std::size_t i = 0; i < 100; ++i) {
        const bool expected = (i % 2) == 0;
        REQUIRE(std::get<bool>(samples[i].value) == expected);
    }
}

TEST_CASE("S6: QString queryRange round-trip", "[buffer][s6][query][string]") {
    auto meta = makeMeta(QStringLiteral("s6/query/string"), dm5::SignalType::String);
    bm6::SignalBufferConfig cfg;
    cfg.windowSeconds = 1e9;
    cfg.capSamples = 10'000;
    bm6::SignalBuffer buf(meta, cfg);

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) {
        buf.push(t0 + std::chrono::microseconds(i), dm5::SignalValue{QString::number(i)});
    }

    auto samples = buf.queryRange(t0, t0 + std::chrono::milliseconds(1), 0);
    REQUIRE(samples.size() == 100U);
    REQUIRE(std::get<QString>(samples.front().value) == QStringLiteral("0"));
    REQUIRE(std::get<QString>(samples.back().value) == QStringLiteral("99"));
}

TEST_CASE("S6: queryRange returns empty when no segment published", "[buffer][s6][query][empty]") {
    auto meta = makeMeta(QStringLiteral("s6/query/empty"), dm5::SignalType::Double);
    bm6::SignalBuffer buf(meta, {});

    auto t0 = std::chrono::steady_clock::now();
    // Push only 50 samples — no publish yet (cadence=100).
    for (int i = 0; i < 50; ++i) {
        buf.push(t0 + std::chrono::microseconds(i), dm5::SignalValue{static_cast<double>(i)});
    }

    auto samples = buf.queryRange(t0, t0 + std::chrono::seconds(1), 0);
    REQUIRE(samples.empty());

    auto latest = buf.queryLatest(10);
    REQUIRE(latest.empty());

    auto one = buf.queryLatestOne();
    REQUIRE_FALSE(one.has_value());
}

TEST_CASE("S6: LOD query covers raw range envelope (HALT trigger #7)", "[buffer][s6][query][envelope]") {
    auto meta = makeMeta(QStringLiteral("s6/query/envelope"), dm5::SignalType::Double);
    bm6::SignalBufferConfig cfg;
    cfg.windowSeconds = 1e9;
    cfg.capSamples = 100'000;
    cfg.estimatedRateHz = 1000.0;
    bm6::SignalBuffer buf(meta, cfg);

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 1000; ++i) {
        buf.push(t0 + std::chrono::milliseconds(i), dm5::SignalValue{static_cast<double>(i)});
    }

    // Force level 3 (M34 P2 count-based: ratio = 1000/target ≥ 1000 ⇒ target 1):
    // 1 bin covering samples 0..999, min=0, max=999.
    auto samples = buf.queryRange(t0, t0 + std::chrono::milliseconds(1000), 1);
    REQUIRE(samples.size() == 2U);
    REQUIRE(std::get<double>(samples[0].value) == 0.0);
    REQUIRE(std::get<double>(samples[1].value) == 999.0);

    // Cross-check by raw query of the same range — every sample must lie
    // within the envelope [0, 999].
    auto raw = buf.queryRange(t0, t0 + std::chrono::milliseconds(1000), 0);
    REQUIRE(raw.size() == 1000U);
    for (const auto& s : raw) {
        const double v = std::get<double>(s.value);
        REQUIRE(v >= 0.0);
        REQUIRE(v <= 999.0);
    }
}
