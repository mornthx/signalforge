// tests/unit/session/tee_sink_test.cpp
//
// S9 TeeSignalValueSink unit tests. The fan-out helper feeds
// every onSignal / onSignalsRegistered / onSignalsUnregistered
// callback to every registered downstream sink. Used by
// MainWindow to route decoded signals to both the
// SignalBufferRegistry (M6) and a SessionWriter (M10) without
// modifying the M5 frozen interface.
#include "decode/decoder_interface.hpp"
#include "session/tee_signal_value_sink.hpp"

#include <QString>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <vector>

namespace s = signalforge::session;
namespace d = signalforge::decoder;

namespace {

struct CountingSink : public d::SignalValueSink {
    int signalCount = 0;
    int registrations = 0;
    int unregistrations = 0;
    void onSignal(std::chrono::steady_clock::time_point /*t*/, const QString& /*id*/,
                  const d::SignalValue& /*v*/) override {
        ++signalCount;
    }
    void onSignalsRegistered(const QString& /*driverId*/, const std::vector<d::SignalMetadata>& /*sigs*/) override {
        ++registrations;
    }
    void onSignalsUnregistered(const QString& /*driverId*/) override {
        ++unregistrations;
    }
};

}  // namespace

TEST_CASE("S9: TeeSink fans onSignal to all sinks", "[session][s9][tee]") {
    s::TeeSignalValueSink tee;
    CountingSink a;
    CountingSink b;

    tee.addSink(&a);
    tee.addSink(&b);

    tee.onSignal(std::chrono::steady_clock::time_point{}, QStringLiteral("x"), d::SignalValue{1.0});
    REQUIRE(a.signalCount == 1);
    REQUIRE(b.signalCount == 1);
}

TEST_CASE("S9: TeeSink addSink ignores nullptr and rejects duplicates", "[session][s9][tee]") {
    s::TeeSignalValueSink tee;
    CountingSink a;

    tee.addSink(nullptr);
    REQUIRE(tee.size() == 0);

    tee.addSink(&a);
    tee.addSink(&a);  // duplicate — should be a no-op
    REQUIRE(tee.size() == 1);
}

TEST_CASE("S9: TeeSink removeSink stops fan-out to that sink", "[session][s9][tee]") {
    s::TeeSignalValueSink tee;
    CountingSink a;
    CountingSink b;
    tee.addSink(&a);
    tee.addSink(&b);

    tee.onSignalsRegistered(QStringLiteral("d"), {});
    REQUIRE(a.registrations == 1);
    REQUIRE(b.registrations == 1);

    tee.removeSink(&a);
    REQUIRE(tee.size() == 1);

    tee.onSignalsRegistered(QStringLiteral("d"), {});
    REQUIRE(a.registrations == 1);  // unchanged
    REQUIRE(b.registrations == 2);
}

TEST_CASE("S9: TeeSink fans onSignalsUnregistered too", "[session][s9][tee]") {
    s::TeeSignalValueSink tee;
    CountingSink a;
    tee.addSink(&a);
    tee.onSignalsUnregistered(QStringLiteral("d"));
    REQUIRE(a.unregistrations == 1);
}
