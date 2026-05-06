// tests/integration/test_signal_buffer_round_trip.cpp
//
// S10 integration test (spec §5.3-1): register one driver with three
// signals (Bool / Int64 / Double); push 1000 samples per signal via the
// SignalValueSink interface; queryRange and verify every sample comes
// back with the correct variant alternative and value.

#include "buffer/signal_buffer.hpp"
#include "buffer/signal_buffer_registry.hpp"
#include "decode/decoder_interface.hpp"

#include <QString>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdint>
#include <variant>

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

TEST_CASE("S10 integration: 3-signal round trip via registry", "[integration][s10][buffer][round_trip]") {
    using namespace signalforge::decoder;
    using namespace signalforge::buffer;

    SignalBufferRegistry registry;
    SignalValueSink* sink = &registry;

    const QString driverId = QStringLiteral("s10/round_trip/driver");
    const QString boolId = QStringLiteral("s10/round_trip/bool");
    const QString int64Id = QStringLiteral("s10/round_trip/int64");
    const QString doubleId = QStringLiteral("s10/round_trip/double");
    sink->onSignalsRegistered(driverId, {makeMeta(boolId, SignalType::Bool), makeMeta(int64Id, SignalType::Int64),
                                         makeMeta(doubleId, SignalType::Double)});

    REQUIRE(registry.signalCount() == 3U);

    constexpr int kPerSignal = 1000;
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kPerSignal; ++i) {
        const auto t = t0 + std::chrono::microseconds(i);
        sink->onSignal(t, boolId, SignalValue{(i % 2) == 0});
        sink->onSignal(t, int64Id, SignalValue{static_cast<std::int64_t>(i * 7)});
        sink->onSignal(t, doubleId, SignalValue{static_cast<double>(i) * 0.5});
    }

    auto* boolBuf = registry.bufferFor(boolId);
    auto* int64Buf = registry.bufferFor(int64Id);
    auto* doubleBuf = registry.bufferFor(doubleId);
    REQUIRE(boolBuf != nullptr);
    REQUIRE(int64Buf != nullptr);
    REQUIRE(doubleBuf != nullptr);

    REQUIRE(boolBuf->totalSamplesPushed() == static_cast<std::uint64_t>(kPerSignal));
    REQUIRE(int64Buf->totalSamplesPushed() == static_cast<std::uint64_t>(kPerSignal));
    REQUIRE(doubleBuf->totalSamplesPushed() == static_cast<std::uint64_t>(kPerSignal));

    const auto rangeEnd = t0 + std::chrono::seconds(1);
    const auto boolSamples = boolBuf->queryRange(t0, rangeEnd, 0);
    const auto int64Samples = int64Buf->queryRange(t0, rangeEnd, 0);
    const auto doubleSamples = doubleBuf->queryRange(t0, rangeEnd, 0);

    REQUIRE(boolSamples.size() == static_cast<std::size_t>(kPerSignal));
    REQUIRE(int64Samples.size() == static_cast<std::size_t>(kPerSignal));
    REQUIRE(doubleSamples.size() == static_cast<std::size_t>(kPerSignal));

    for (int i = 0; i < kPerSignal; ++i) {
        REQUIRE(std::get<bool>(boolSamples[i].value) == ((i % 2) == 0));
        REQUIRE(std::get<std::int64_t>(int64Samples[i].value) == static_cast<std::int64_t>(i * 7));
        REQUIRE(std::get<double>(doubleSamples[i].value) == static_cast<double>(i) * 0.5);
    }

    sink->onSignalsUnregistered(driverId);
    REQUIRE(registry.signalCount() == 0U);
}
