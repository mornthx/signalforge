// tests/integration/test_chart_global_time_axis.cpp
//
// S9 integration test (spec §2.1-12 #3): three Charts share one
// TimeAxisManager. Pan one chart's view via timeAxis.pan() —
// the manager's emitted `rangeChanged` reaches all charts'
// next ticks (since they all read `timeAxis_->visibleStart() /
// visibleEnd()` on every tick).

#include "buffer/signal_buffer_registry.hpp"
#include "chart/chart.hpp"
#include "chart/time_axis_manager.hpp"
#include "decode/decoder_interface.hpp"

#include <QGuiApplication>
#include <QSignalSpy>
#include <QString>
#include <catch2/catch_test_macros.hpp>
#include <chrono>

namespace ch8 = signalforge::chart;
namespace bm6 = signalforge::buffer;

namespace {

QGuiApplication& app() {
    static int argc = 1;
    static char arg0[] = "test_chart_global_time_axis";
    static char* argv[] = {arg0, nullptr};
    static QGuiApplication instance(argc, argv);
    return instance;
}

}  // namespace

TEST_CASE("S9 integration: 3 charts share one TimeAxisManager", "[integration][s9][chart][axis]") {
    app();
    qRegisterMetaType<bool>("bool");
    bm6::SignalBufferRegistry registry;
    ch8::TimeAxisManager axis;

    ch8::Chart c1(registry, axis);
    ch8::Chart c2(registry, axis);
    ch8::Chart c3(registry, axis);

    // Snapshot the initial visibleEnd from one chart's perspective
    // (all three share the same axis, so visibleEnd is the same).
    const auto endBefore = axis.visibleEnd();

    QSignalSpy rangeSpy(&axis, &ch8::TimeAxisManager::rangeChanged);
    QSignalSpy liveSpy(&axis, &ch8::TimeAxisManager::liveModeChanged);

    // Pan via the axis (as if c1 forwarded a mouse drag). All
    // three charts should observe the same updated range.
    axis.pan(std::chrono::nanoseconds(std::chrono::seconds(-3)));

    REQUIRE(rangeSpy.count() == 1);
    REQUIRE(liveSpy.count() == 1);
    REQUIRE_FALSE(axis.liveMode());

    // visibleEnd jumped backward by ~3 seconds (relative to the
    // initial pre-pan now()).
    const auto endAfter = axis.visibleEnd();
    const auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(endBefore - endAfter).count();
    REQUIRE(delta >= 2900);  // pan went back by 3 sec; tolerance for the now() drift
    REQUIRE(delta <= 3100);
}
