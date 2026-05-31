// tests/unit/buffer/signal_buffer_registry_test.cpp
//
// S7 — verifies SignalBufferRegistry's lifecycle: registration creates
// SignalBuffers, onSignal routes correctly, unregistration releases
// buffers, budget enforcement (warn / reject) per spec §3.6.

#include "buffer/signal_buffer.hpp"
#include "buffer/signal_buffer_registry.hpp"
#include "decode/decoder_interface.hpp"
#include "observability/metrics.hpp"

#include <QString>
#include <QStringList>
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

}  // namespace

TEST_CASE("S7: registry registers a driver with three signals", "[buffer][s7][registry]") {
    bm6::SignalBufferRegistry registry;
    REQUIRE(registry.signalCount() == 0U);

    const QString driverId = QStringLiteral("s7/driver/three");
    std::vector<dm5::SignalMetadata> metas{
        makeMeta(QStringLiteral("s7/three/bool"), dm5::SignalType::Bool),
        makeMeta(QStringLiteral("s7/three/int64"), dm5::SignalType::Int64),
        makeMeta(QStringLiteral("s7/three/double"), dm5::SignalType::Double),
    };
    registry.onSignalsRegistered(driverId, metas);

    REQUIRE(registry.signalCount() == 3U);
    REQUIRE(registry.bufferFor(QStringLiteral("s7/three/bool")) != nullptr);
    REQUIRE(registry.bufferFor(QStringLiteral("s7/three/int64")) != nullptr);
    REQUIRE(registry.bufferFor(QStringLiteral("s7/three/double")) != nullptr);
    REQUIRE(registry.bufferFor(QStringLiteral("s7/three/missing")) == nullptr);
    REQUIRE(registry.totalMemoryBytes() > 0U);

    auto ids = registry.signalIdsForDriver(driverId);
    REQUIRE(ids.size() == 3);
}

TEST_CASE("S7: onSignal routes to the correct buffer", "[buffer][s7][registry][route]") {
    bm6::SignalBufferRegistry registry;
    const QString driverId = QStringLiteral("s7/driver/route");
    std::vector<dm5::SignalMetadata> metas{
        makeMeta(QStringLiteral("s7/route/double"), dm5::SignalType::Double),
        makeMeta(QStringLiteral("s7/route/int64"), dm5::SignalType::Int64),
    };
    registry.onSignalsRegistered(driverId, metas);

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) {
        registry.onSignal(t0 + std::chrono::microseconds(i), QStringLiteral("s7/route/double"),
                          dm5::SignalValue{static_cast<double>(i)});
    }
    for (int i = 0; i < 50; ++i) {
        registry.onSignal(t0 + std::chrono::microseconds(i), QStringLiteral("s7/route/int64"),
                          dm5::SignalValue{static_cast<std::int64_t>(i)});
    }
    // Unknown signal — silent miss.
    registry.onSignal(t0, QStringLiteral("s7/route/unknown"), dm5::SignalValue{1.0});

    REQUIRE(registry.bufferFor(QStringLiteral("s7/route/double"))->totalSamplesPushed() == 100U);
    REQUIRE(registry.bufferFor(QStringLiteral("s7/route/int64"))->totalSamplesPushed() == 50U);
}

TEST_CASE("S7: unregister releases buffers and budget", "[buffer][s7][registry][unreg]") {
    bm6::SignalBufferRegistry registry;
    const QString driverId = QStringLiteral("s7/driver/unreg");
    std::vector<dm5::SignalMetadata> metas{
        makeMeta(QStringLiteral("s7/unreg/double"), dm5::SignalType::Double),
        makeMeta(QStringLiteral("s7/unreg/int64"), dm5::SignalType::Int64),
    };
    registry.onSignalsRegistered(driverId, metas);
    const std::size_t bytesBefore = registry.totalMemoryBytes();
    REQUIRE(bytesBefore > 0U);
    REQUIRE(registry.signalCount() == 2U);

    registry.onSignalsUnregistered(driverId);
    REQUIRE(registry.signalCount() == 0U);
    REQUIRE(registry.totalMemoryBytes() == 0U);
    REQUIRE(registry.bufferFor(QStringLiteral("s7/unreg/double")) == nullptr);
    REQUIRE(registry.bufferFor(QStringLiteral("s7/unreg/int64")) == nullptr);
    // Idempotent: second call is a no-op.
    registry.onSignalsUnregistered(driverId);
    REQUIRE(registry.signalCount() == 0U);
}

TEST_CASE("S7: budget rejection on registration", "[buffer][s7][registry][budget][reject]") {
    bm6::RegistryConfig cfg;
    cfg.totalBudgetBytes = 1ULL * 1024 * 1024;  // 1 MB — small budget
    cfg.signalDefaults.windowSeconds = 60.0;
    cfg.signalDefaults.estimatedRateHz = 1000.0;  // ~1.6 MB per double signal
    bm6::SignalBufferRegistry registry(cfg);

    auto* rejectedMetric = om2::MetricsRegistry::instance().getOrCreate(QStringLiteral("signal_buffer_budget_rejected"),
                                                                        om2::MetricKind::Counter);
    REQUIRE(rejectedMetric != nullptr);
    const std::int64_t before = rejectedMetric->value();

    std::vector<dm5::SignalMetadata> metas{makeMeta(QStringLiteral("s7/reject/double"), dm5::SignalType::Double)};
    registry.onSignalsRegistered(QStringLiteral("s7/driver/reject"), metas);

    // Estimated > 1 MB → rejected; no buffers allocated.
    REQUIRE(registry.signalCount() == 0U);
    REQUIRE(rejectedMetric->value() - before == 1);
    REQUIRE(registry.bufferFor(QStringLiteral("s7/reject/double")) == nullptr);
}

TEST_CASE("S7: budget soft-warn at 80% threshold", "[buffer][s7][registry][budget][warn]") {
    bm6::RegistryConfig cfg;
    cfg.totalBudgetBytes = 10ULL * 1024 * 1024;   // 10 MB
    cfg.signalDefaults.windowSeconds = 1.0;       // 1 s window
    cfg.signalDefaults.estimatedRateHz = 1000.0;  // 1 kHz → samples / signal = 1000
    bm6::SignalBufferRegistry registry(cfg);

    auto* warnedMetric = om2::MetricsRegistry::instance().getOrCreate(QStringLiteral("signal_buffer_budget_warned"),
                                                                      om2::MetricKind::Counter);
    REQUIRE(warnedMetric != nullptr);
    const std::int64_t before = warnedMetric->value();

    // Per-signal estimate (Double, 1 kHz, 1 s, LOD-enabled):
    //   1000 samples × (8 value + sizeof(time_point)) × 1.22 LOD-factor.
    // To safely cross 80% (8 MB) we register a count high enough that
    // the resulting estimate covers a 10 MB budget's 80% threshold even
    // when sizeof(time_point) is the smaller 8-byte representation.
    constexpr int kSignalCount = 500;
    std::vector<dm5::SignalMetadata> metas;
    metas.reserve(kSignalCount);
    for (int i = 0; i < kSignalCount; ++i) {
        metas.push_back(makeMeta(QString::asprintf("s7/warn/double-%d", i), dm5::SignalType::Double));
    }
    registry.onSignalsRegistered(QStringLiteral("s7/driver/warn"), metas);
    REQUIRE(registry.signalCount() == kSignalCount);
    REQUIRE(warnedMetric->value() - before >= 1);  // at least one warn

    // Second registration that does NOT cross the threshold again: tiny addition.
    std::vector<dm5::SignalMetadata> tiny{makeMeta(QStringLiteral("s7/warn/tiny"), dm5::SignalType::Bool)};
    const std::int64_t between = warnedMetric->value();
    registry.onSignalsRegistered(QStringLiteral("s7/driver/warn-tiny"), tiny);
    REQUIRE(warnedMetric->value() == between);  // no new warn (already past 80%)
}

TEST_CASE("S7: per-driver config overrides", "[buffer][s7][registry][overrides]") {
    bm6::SignalBufferRegistry registry;
    const QString driverId = QStringLiteral("s7/driver/override");
    bm6::SignalBufferConfig override;
    override.windowSeconds = 5.0;
    override.capSamples = 50'000;
    bm6::SignalConfigOverrides overrides;
    overrides[QStringLiteral("s7/override/special")] = override;
    registry.setDriverConfigOverrides(driverId, overrides);

    std::vector<dm5::SignalMetadata> metas{
        makeMeta(QStringLiteral("s7/override/special"), dm5::SignalType::Double),
        makeMeta(QStringLiteral("s7/override/default"), dm5::SignalType::Double),
    };
    registry.onSignalsRegistered(driverId, metas);

    auto* special = registry.bufferFor(QStringLiteral("s7/override/special"));
    auto* defaulted = registry.bufferFor(QStringLiteral("s7/override/default"));
    REQUIRE(special != nullptr);
    REQUIRE(defaulted != nullptr);
    REQUIRE(special->config().windowSeconds == 5.0);
    REQUIRE(defaulted->config().windowSeconds == 60.0);  // registry default
}

TEST_CASE("S7: estimate vs actual within 20% (HALT trigger #6)", "[buffer][s7][registry][estimate]") {
    bm6::SignalBufferConfig cfg;
    cfg.windowSeconds = 1.0;
    cfg.capSamples = 100'000;
    cfg.estimatedRateHz = 1000.0;
    auto meta = makeMeta(QStringLiteral("s7/estimate/double"), dm5::SignalType::Double);

    bm6::SignalBuffer buf(meta, cfg);
    auto t0 = std::chrono::steady_clock::now();
    // Push 1000 samples spaced 1 ms apart (matches the 1 kHz rate hint).
    for (int i = 0; i < 1000; ++i) {
        buf.push(t0 + std::chrono::milliseconds(i), dm5::SignalValue{static_cast<double>(i)});
    }
    const std::size_t actual = buf.memoryBytes();

    // Estimate via the registry's formula. Build a temporary registry that
    // exposes the estimate path indirectly via signalCount + totalMemoryBytes.
    bm6::RegistryConfig regCfg;
    regCfg.signalDefaults = cfg;
    bm6::SignalBufferRegistry registry(regCfg);
    registry.onSignalsRegistered(QStringLiteral("s7/estimate/driver"), {meta});
    const std::size_t estimated = registry.totalMemoryBytes();

    REQUIRE(estimated > 0U);
    REQUIRE(actual > 0U);
    const double ratio = static_cast<double>(actual) / static_cast<double>(estimated);
    // Both sides within 20% of each other.
    REQUIRE(ratio > 0.80);
    REQUIRE(ratio < 1.20);
}

TEST_CASE("S7: SignalValueSink interface conformance", "[buffer][s7][registry][interface]") {
    bm6::SignalBufferRegistry registry;
    dm5::SignalValueSink* sink = &registry;

    std::vector<dm5::SignalMetadata> metas{
        makeMeta(QStringLiteral("s7/iface/double"), dm5::SignalType::Double),
    };
    sink->onSignalsRegistered(QStringLiteral("s7/driver/iface"), metas);
    REQUIRE(registry.signalCount() == 1U);

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) {
        sink->onSignal(t0 + std::chrono::microseconds(i), QStringLiteral("s7/iface/double"),
                       dm5::SignalValue{static_cast<double>(i)});
    }
    REQUIRE(registry.bufferFor(QStringLiteral("s7/iface/double"))->totalSamplesPushed() == 100U);

    sink->onSignalsUnregistered(QStringLiteral("s7/driver/iface"));
    REQUIRE(registry.signalCount() == 0U);
}
