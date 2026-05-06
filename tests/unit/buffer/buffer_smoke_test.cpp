// tests/unit/buffer/buffer_smoke_test.cpp
//
// S1 placeholder. Verifies that the M6 freeze-surface headers compile,
// that default configs are sensible, and that a minimal SignalBuffer
// constructs and destructs cleanly.
//
// Per-type tests, eviction, snapshot publish, LOD, and registry behaviour
// land in S2-S9 with dedicated test files (see plan.md §2 S9).

#include "buffer/signal_buffer.hpp"
#include "buffer/signal_buffer_registry.hpp"
#include "decode/decoder_interface.hpp"

#include <catch2/catch_test_macros.hpp>

namespace bm6 = signalforge::buffer;
namespace dm5 = signalforge::decoder;

TEST_CASE("S1: default SignalBufferConfig has sane defaults", "[buffer][smoke]") {
    bm6::SignalBufferConfig cfg;
    REQUIRE(cfg.windowSeconds == 60.0);
    REQUIRE(cfg.capSamples == 1'000'000U);
    REQUIRE_FALSE(cfg.lodEnabled.has_value());
    REQUIRE_FALSE(cfg.estimatedRateHz.has_value());
}

TEST_CASE("S1: default RegistryConfig has sane defaults", "[buffer][smoke]") {
    bm6::RegistryConfig cfg;
    REQUIRE(cfg.totalBudgetBytes == 256ULL * 1024 * 1024);
    REQUIRE(cfg.rejectOnBudgetExceeded == true);
    REQUIRE(cfg.signalDefaults.windowSeconds == 60.0);
}

TEST_CASE("S1: SignalBuffer constructs and reports zero counters", "[buffer][smoke]") {
    dm5::SignalMetadata meta;
    meta.id = QStringLiteral("test/sig");
    meta.name = QStringLiteral("Test Signal");
    meta.unit = QStringLiteral("V");
    meta.type = dm5::SignalType::Double;

    bm6::SignalBufferConfig cfg;
    cfg.windowSeconds = 1.0;

    bm6::SignalBuffer buf(meta, cfg);

    REQUIRE(buf.metadata().id == meta.id);
    REQUIRE(buf.config().windowSeconds == 1.0);
    REQUIRE(buf.totalSamplesPushed() == 0U);
    REQUIRE(buf.totalSamplesEvicted() == 0U);
    REQUIRE(buf.sampleCount() == 0U);
    REQUIRE_FALSE(buf.queryLatestOne().has_value());
    REQUIRE(buf.queryLatest(10).empty());
}

TEST_CASE("S1: empty SignalBufferRegistry has zero signals", "[buffer][smoke]") {
    bm6::SignalBufferRegistry registry;

    REQUIRE(registry.signalCount() == 0U);
    REQUIRE(registry.totalMemoryBytes() == 0U);
    REQUIRE(registry.totalBudgetBytes() == 256ULL * 1024 * 1024);
    REQUIRE(registry.signalIds().isEmpty());
    REQUIRE(registry.bufferFor(QStringLiteral("nonexistent")) == nullptr);

    auto report = registry.memoryUsage();
    REQUIRE(report.totalBytes == 0U);
    REQUIRE(report.budgetBytes == 256ULL * 1024 * 1024);
    REQUIRE(report.drivers.empty());
}

TEST_CASE("S1: SignalBufferRegistry is a SignalValueSink", "[buffer][smoke]") {
    bm6::SignalBufferRegistry registry;
    dm5::SignalValueSink* sink = &registry;
    // S1: stub onSignal must be callable without crashing on unknown signalId.
    sink->onSignal(std::chrono::steady_clock::now(), QStringLiteral("unknown/sig"), dm5::SignalValue{1.0});
    SUCCEED();
}
