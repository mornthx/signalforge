// tests/unit/buffer/signal_buffer_publish_test.cpp
//
// S4 — verifies the writer's snapshot publish cadence (default 100
// pushes per publish; spec §3.5 / §4.4).
//
// The publish event is observed indirectly via the
// `signal_buffer_publishes_<id>` counter metric registered by each
// SignalBuffer. Direct segment access is exercised in S6 once the
// query API consumes the published segment.

#include "buffer/signal_buffer.hpp"
#include "observability/metrics.hpp"

#include <QString>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdint>

namespace bm6 = signalforge::buffer;
namespace dm5 = signalforge::decoder;
namespace om2 = signalforge::observability;

namespace {

dm5::SignalMetadata makeMeta(QString id, dm5::SignalType type) {
    dm5::SignalMetadata meta;
    meta.id = std::move(id);
    meta.name = QStringLiteral("Test");
    meta.unit = QStringLiteral("");
    meta.type = type;
    return meta;
}

[[nodiscard]] om2::Metric* publishesMetricFor(const QString& signalId) {
    const QString name = QStringLiteral("signal_buffer_publishes_") + signalId;
    return om2::MetricsRegistry::instance().getOrCreate(name, om2::MetricKind::Counter);
}

}  // namespace

TEST_CASE("S4: Double publishes every 100 pushes", "[buffer][s4][publish][double]") {
    auto meta = makeMeta(QStringLiteral("s4/double"), dm5::SignalType::Double);
    bm6::SignalBuffer buf(meta, {});
    auto* metric = publishesMetricFor(meta.id);
    REQUIRE(metric != nullptr);
    const std::int64_t before = metric->value();

    auto t0 = std::chrono::steady_clock::now();
    // 99 pushes — no publish yet.
    for (int i = 0; i < 99; ++i) {
        buf.push(t0 + std::chrono::microseconds(i), dm5::SignalValue{static_cast<double>(i)});
    }
    REQUIRE(metric->value() - before == 0);

    // 100th push triggers publish #1.
    buf.push(t0 + std::chrono::microseconds(99), dm5::SignalValue{99.0});
    REQUIRE(metric->value() - before == 1);

    // Push another 100 — publish #2.
    for (int i = 100; i < 200; ++i) {
        buf.push(t0 + std::chrono::microseconds(i), dm5::SignalValue{static_cast<double>(i)});
    }
    REQUIRE(metric->value() - before == 2);

    // And another 100 — publish #3.
    for (int i = 200; i < 300; ++i) {
        buf.push(t0 + std::chrono::microseconds(i), dm5::SignalValue{static_cast<double>(i)});
    }
    REQUIRE(metric->value() - before == 3);
}

TEST_CASE("S4: Int64 publishes every 100 pushes", "[buffer][s4][publish][int64]") {
    auto meta = makeMeta(QStringLiteral("s4/int64"), dm5::SignalType::Int64);
    bm6::SignalBuffer buf(meta, {});
    auto* metric = publishesMetricFor(meta.id);
    REQUIRE(metric != nullptr);
    const std::int64_t before = metric->value();

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 1000; ++i) {
        buf.push(t0 + std::chrono::microseconds(i), dm5::SignalValue{static_cast<std::int64_t>(i)});
    }
    REQUIRE(metric->value() - before == 10);
}

TEST_CASE("S4: Bool publishes every 100 pushes", "[buffer][s4][publish][bool]") {
    auto meta = makeMeta(QStringLiteral("s4/bool"), dm5::SignalType::Bool);
    bm6::SignalBuffer buf(meta, {});
    auto* metric = publishesMetricFor(meta.id);
    REQUIRE(metric != nullptr);
    const std::int64_t before = metric->value();

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 250; ++i) {
        buf.push(t0 + std::chrono::microseconds(i), dm5::SignalValue{(i % 2) == 0});
    }
    // Two complete cadences fired (at 100 and 200); the trailing 50 are
    // buffered for the next publish.
    REQUIRE(metric->value() - before == 2);
}

TEST_CASE("S4: QString publishes every 100 pushes", "[buffer][s4][publish][string]") {
    auto meta = makeMeta(QStringLiteral("s4/string"), dm5::SignalType::String);
    bm6::SignalBuffer buf(meta, {});
    auto* metric = publishesMetricFor(meta.id);
    REQUIRE(metric != nullptr);
    const std::int64_t before = metric->value();

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) {
        buf.push(t0 + std::chrono::microseconds(i), dm5::SignalValue{QString::number(i)});
    }
    REQUIRE(metric->value() - before == 1);
}

TEST_CASE("S4: failed pushes do not advance publish cadence", "[buffer][s4][publish][mismatch]") {
    auto meta = makeMeta(QStringLiteral("s4/mismatch"), dm5::SignalType::Int64);
    bm6::SignalBuffer buf(meta, {});
    auto* metric = publishesMetricFor(meta.id);
    REQUIRE(metric != nullptr);
    const std::int64_t before = metric->value();

    auto t0 = std::chrono::steady_clock::now();
    // 1000 type-mismatched pushes — no successful pushes, no publishes.
    for (int i = 0; i < 1000; ++i) {
        buf.push(t0 + std::chrono::microseconds(i), dm5::SignalValue{3.14});  // wrong type
    }
    REQUIRE(metric->value() - before == 0);
    REQUIRE(buf.totalSamplesPushed() == 0U);

    // Now 100 valid pushes → one publish.
    for (int i = 0; i < 100; ++i) {
        buf.push(t0 + std::chrono::microseconds(1000 + i), dm5::SignalValue{static_cast<std::int64_t>(i)});
    }
    REQUIRE(metric->value() - before == 1);
}
