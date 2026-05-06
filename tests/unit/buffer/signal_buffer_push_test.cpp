// tests/unit/buffer/signal_buffer_push_test.cpp
//
// S2 — verifies SignalBuffer::push routing, per-variant typed storage,
// and atomic counter wiring.

#include "buffer/signal_buffer.hpp"

#include <QString>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cmath>
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

TEST_CASE("S2: Bool push routes to bit-packed storage", "[buffer][s2][bool]") {
    auto meta = makeMeta(QStringLiteral("s2/bool"), dm5::SignalType::Bool);
    bm6::SignalBuffer buf(meta, {});

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 65; ++i) {
        buf.push(t0 + std::chrono::milliseconds(i), dm5::SignalValue{(i % 2) == 0});
    }

    REQUIRE(buf.sampleCount() == 65U);
    REQUIRE(buf.totalSamplesPushed() == 65U);
    REQUIRE(buf.totalSamplesEvicted() == 0U);
    // Bit-pack: 65 bits spans 2 uint64 words. Plus 65 timestamps.
    REQUIRE(buf.memoryBytes() > 0U);
}

TEST_CASE("S2: Int64 push routes to int64 storage", "[buffer][s2][int64]") {
    auto meta = makeMeta(QStringLiteral("s2/int64"), dm5::SignalType::Int64);
    bm6::SignalBuffer buf(meta, {});

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) {
        buf.push(t0 + std::chrono::milliseconds(i), dm5::SignalValue{static_cast<std::int64_t>(i * 1000)});
    }

    REQUIRE(buf.sampleCount() == 100U);
    REQUIRE(buf.totalSamplesPushed() == 100U);
    REQUIRE(buf.memoryBytes() > 0U);
}

TEST_CASE("S2: Double push routes to double storage and accepts NaN", "[buffer][s2][double]") {
    auto meta = makeMeta(QStringLiteral("s2/double"), dm5::SignalType::Double);
    bm6::SignalBuffer buf(meta, {});

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 50; ++i) {
        buf.push(t0 + std::chrono::milliseconds(i), dm5::SignalValue{static_cast<double>(i) * 0.5});
    }
    // NaN sample is a legitimate value; spec §5.2 says NaN samples are
    // stored (LOD min/max excludes NaN — that's S5).
    buf.push(t0 + std::chrono::milliseconds(60), dm5::SignalValue{std::nan("")});

    REQUIRE(buf.sampleCount() == 51U);
    REQUIRE(buf.totalSamplesPushed() == 51U);
}

TEST_CASE("S2: QString push routes to string storage", "[buffer][s2][string]") {
    auto meta = makeMeta(QStringLiteral("s2/string"), dm5::SignalType::String);
    bm6::SignalBuffer buf(meta, {});

    auto t0 = std::chrono::steady_clock::now();
    buf.push(t0, dm5::SignalValue{QStringLiteral("alpha")});
    buf.push(t0 + std::chrono::milliseconds(1), dm5::SignalValue{QStringLiteral("beta")});
    buf.push(t0 + std::chrono::milliseconds(2), dm5::SignalValue{QStringLiteral("gamma")});

    REQUIRE(buf.sampleCount() == 3U);
    REQUIRE(buf.totalSamplesPushed() == 3U);
    REQUIRE(buf.memoryBytes() > 0U);
}

TEST_CASE("S2: type-mismatched push is silently dropped", "[buffer][s2][mismatch]") {
    auto meta = makeMeta(QStringLiteral("s2/mismatch"), dm5::SignalType::Int64);
    bm6::SignalBuffer buf(meta, {});

    auto t0 = std::chrono::steady_clock::now();
    // Wrong variant — Int64 buffer rejects double / bool / string.
    buf.push(t0, dm5::SignalValue{3.14});
    buf.push(t0 + std::chrono::milliseconds(1), dm5::SignalValue{true});
    buf.push(t0 + std::chrono::milliseconds(2), dm5::SignalValue{QStringLiteral("x")});

    REQUIRE(buf.sampleCount() == 0U);
    REQUIRE(buf.totalSamplesPushed() == 0U);

    // Correct variant succeeds afterwards.
    buf.push(t0 + std::chrono::milliseconds(3), dm5::SignalValue{static_cast<std::int64_t>(42)});
    REQUIRE(buf.sampleCount() == 1U);
    REQUIRE(buf.totalSamplesPushed() == 1U);
}

TEST_CASE("S2: memoryBytes increases monotonically across pushes", "[buffer][s2][memory]") {
    auto meta = makeMeta(QStringLiteral("s2/mem"), dm5::SignalType::Double);
    bm6::SignalBuffer buf(meta, {});

    auto t0 = std::chrono::steady_clock::now();
    std::size_t prev = buf.memoryBytes();
    for (int i = 0; i < 1000; ++i) {
        buf.push(t0 + std::chrono::milliseconds(i), dm5::SignalValue{static_cast<double>(i)});
        const std::size_t now = buf.memoryBytes();
        REQUIRE(now >= prev);  // monotonic; std::vector growth gives steps
        prev = now;
    }
    REQUIRE(buf.sampleCount() == 1000U);
    REQUIRE(buf.memoryBytes() >= 1000U * sizeof(double));
}
