// tests/unit/buffer/signal_buffer_time_flush_test.cpp
//
// M25 (C1) — time-based publish flush: a slow signal becomes visible to
// readers after ~one inter-sample interval (>= kPublishFlushInterval),
// not after kDefaultPublishCadence (=100) samples; clustered fast pushes
// still publish strictly on the sample-count cadence.

#include "buffer/signal_buffer.hpp"
#include "decode/decoder_interface.hpp"

#include <QString>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <variant>

namespace bm6 = signalforge::buffer;
namespace dm5 = signalforge::decoder;

namespace {

dm5::SignalMetadata makeMeta(QString id) {
    dm5::SignalMetadata meta;
    meta.id = std::move(id);
    meta.type = dm5::SignalType::Double;
    return meta;
}

bm6::SignalBufferConfig wideCfg() {
    bm6::SignalBufferConfig cfg;
    cfg.windowSeconds = 1e9;
    cfg.capSamples = 10'000;
    return cfg;
}

}  // namespace

TEST_CASE("C1: slow signal becomes visible after the time-flush interval", "[buffer][m25][c1]") {
    bm6::SignalBuffer buf(makeMeta(QStringLiteral("m25/slow")), wideCfg());
    const auto t0 = std::chrono::steady_clock::now();

    // First sample only anchors the flush window — not yet published.
    buf.push(t0, dm5::SignalValue{10.0});
    CHECK_FALSE(buf.queryLatestOne().has_value());

    // Second sample 250 ms later (> 200 ms flush interval) → published,
    // long before the 100-sample cadence would have fired.
    buf.push(t0 + std::chrono::milliseconds(250), dm5::SignalValue{20.0});
    const auto latest = buf.queryLatestOne();
    REQUIRE(latest.has_value());
    CHECK(std::get<double>(latest->value) == 20.0);
}

TEST_CASE("C1: clustered fast pushes still publish on the sample cadence", "[buffer][m25][c1]") {
    bm6::SignalBuffer buf(makeMeta(QStringLiteral("m25/fast")), wideCfg());
    const auto t0 = std::chrono::steady_clock::now();

    // 99 samples microseconds apart: under both the 100-sample cadence and
    // the 200 ms time flush → nothing published yet.
    for (int i = 0; i < 99; ++i) {
        buf.push(t0 + std::chrono::microseconds(i), dm5::SignalValue{static_cast<double>(i)});
    }
    CHECK_FALSE(buf.queryLatestOne().has_value());

    // 100th push hits the sample cadence → published.
    buf.push(t0 + std::chrono::microseconds(99), dm5::SignalValue{99.0});
    const auto latest = buf.queryLatestOne();
    REQUIRE(latest.has_value());
    CHECK(std::get<double>(latest->value) == 99.0);
}
