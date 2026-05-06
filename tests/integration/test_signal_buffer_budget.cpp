// tests/integration/test_signal_buffer_budget.cpp
//
// S10 integration test (spec §5.3-5): budget enforcement at registry
// level. Budget = 10 MB; one driver registers ~5 MB worth of signals
// (succeeds), a second driver requests another ~7 MB (rejected).
// Verifies the rejected driver's buffers are not in the registry.

#include "buffer/signal_buffer.hpp"
#include "buffer/signal_buffer_registry.hpp"
#include "decode/decoder_interface.hpp"
#include "observability/metrics.hpp"

#include <QString>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdint>
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

TEST_CASE("S10 integration: budget reject prevents buffer allocation", "[integration][s10][buffer][budget]") {
    using namespace signalforge::decoder;
    using namespace signalforge::buffer;

    RegistryConfig regCfg;
    regCfg.totalBudgetBytes = 10ULL * 1024 * 1024;  // 10 MB
    regCfg.signalDefaults.windowSeconds = 1.0;
    regCfg.signalDefaults.estimatedRateHz = 1000.0;  // ~17.7 KB / Double signal
    SignalBufferRegistry registry(regCfg);
    SignalValueSink* sink = &registry;

    auto* rejectedMetric = signalforge::observability::MetricsRegistry::instance().getOrCreate(
        QStringLiteral("signal_buffer_budget_rejected"), signalforge::observability::MetricKind::Counter);
    REQUIRE(rejectedMetric != nullptr);
    const std::int64_t rejectedBefore = rejectedMetric->value();

    // First driver: ~280 signals × 17.7 KB ≈ 4.96 MB → fits.
    const QString firstDriverId = QStringLiteral("s10/budget/first");
    std::vector<SignalMetadata> firstMetas;
    firstMetas.reserve(280);
    for (int i = 0; i < 280; ++i) {
        firstMetas.push_back(makeMeta(QString::asprintf("s10/budget/first/%d", i), SignalType::Double));
    }
    sink->onSignalsRegistered(firstDriverId, firstMetas);
    REQUIRE(registry.signalCount() == 280U);
    const std::size_t bytesAfterFirst = registry.totalMemoryBytes();
    REQUIRE(bytesAfterFirst > 0U);
    REQUIRE(bytesAfterFirst < regCfg.totalBudgetBytes);

    // Second driver: ~400 signals × 17.7 KB ≈ 7.1 MB. Total would be > 10 MB → rejected.
    const QString secondDriverId = QStringLiteral("s10/budget/second");
    std::vector<SignalMetadata> secondMetas;
    secondMetas.reserve(400);
    for (int i = 0; i < 400; ++i) {
        secondMetas.push_back(makeMeta(QString::asprintf("s10/budget/second/%d", i), SignalType::Double));
    }
    sink->onSignalsRegistered(secondDriverId, secondMetas);

    // Rejected: signal count unchanged, budget unchanged, rejected counter incremented.
    REQUIRE(registry.signalCount() == 280U);
    REQUIRE(registry.totalMemoryBytes() == bytesAfterFirst);
    REQUIRE(rejectedMetric->value() - rejectedBefore == 1);

    // None of the second driver's signals are in the registry.
    REQUIRE(registry.bufferFor(QStringLiteral("s10/budget/second/0")) == nullptr);

    // First driver's signals remain accessible.
    REQUIRE(registry.bufferFor(QStringLiteral("s10/budget/first/0")) != nullptr);

    sink->onSignalsUnregistered(firstDriverId);
    REQUIRE(registry.signalCount() == 0U);
}
