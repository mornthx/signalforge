// tests/unit/chart/time_axis_manager_test.cpp
//
// S2 — full TimeAxisManager state-machine test. Targets ≥ 90%
// line coverage on time_axis_manager.cpp (per plan §S2 acceptance);
// the class is pure logic and easy to cover.

#include "chart/time_axis_manager.hpp"

#include <QSignalSpy>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <thread>

namespace ch8 = signalforge::chart;

using std::chrono::milliseconds;
using std::chrono::nanoseconds;
using std::chrono::seconds;

namespace {

void registerMetaTypes() {
    static bool once = [] {
        qRegisterMetaType<bool>("bool");
        return true;
    }();
    (void)once;
}

}  // namespace

TEST_CASE("S2: default state is live mode with 10s duration", "[chart][s2][axis][default]") {
    registerMetaTypes();
    ch8::TimeAxisManager axis;
    REQUIRE(axis.liveMode());
    REQUIRE(axis.visibleDuration() == nanoseconds(seconds(10)));
}

TEST_CASE("S2: pan from live transitions to paused and emits both signals", "[chart][s2][axis][pan]") {
    registerMetaTypes();
    ch8::TimeAxisManager axis;
    QSignalSpy rangeSpy(&axis, &ch8::TimeAxisManager::rangeChanged);
    QSignalSpy liveSpy(&axis, &ch8::TimeAxisManager::liveModeChanged);

    axis.pan(nanoseconds(seconds(-5)));  // pan into the past
    REQUIRE_FALSE(axis.liveMode());
    REQUIRE(rangeSpy.count() == 1);
    REQUIRE(liveSpy.count() == 1);
    REQUIRE(liveSpy.takeFirst().at(0).toBool() == false);
}

TEST_CASE("S2: pan while paused only emits rangeChanged", "[chart][s2][axis][pan]") {
    registerMetaTypes();
    ch8::TimeAxisManager axis;
    axis.pause();  // already paused

    QSignalSpy rangeSpy(&axis, &ch8::TimeAxisManager::rangeChanged);
    QSignalSpy liveSpy(&axis, &ch8::TimeAxisManager::liveModeChanged);

    const auto endBefore = axis.visibleEnd();
    axis.pan(nanoseconds(seconds(2)));
    const auto endAfter = axis.visibleEnd();

    REQUIRE(rangeSpy.count() == 1);
    REQUIRE(liveSpy.count() == 0);
    REQUIRE((endAfter - endBefore) == nanoseconds(seconds(2)));
}

TEST_CASE("S2: zoom from live transitions to paused and scales duration", "[chart][s2][axis][zoom]") {
    registerMetaTypes();
    ch8::TimeAxisManager axis;
    const auto durBefore = axis.visibleDuration();

    QSignalSpy rangeSpy(&axis, &ch8::TimeAxisManager::rangeChanged);
    QSignalSpy liveSpy(&axis, &ch8::TimeAxisManager::liveModeChanged);

    axis.zoom(2.0, std::chrono::steady_clock::now());

    REQUIRE_FALSE(axis.liveMode());
    REQUIRE(rangeSpy.count() == 1);
    REQUIRE(liveSpy.count() == 1);
    // Duration doubles
    const auto durAfter = axis.visibleDuration();
    REQUIRE(durAfter.count() == durBefore.count() * 2);
}

TEST_CASE("S2: zoom around a reference point preserves the reference's relative position", "[chart][s2][axis][zoom]") {
    registerMetaTypes();
    ch8::TimeAxisManager axis;
    axis.setRange(std::chrono::steady_clock::now() - seconds(20), std::chrono::steady_clock::now() - seconds(0));
    const auto start = axis.visibleStart();
    const auto end = axis.visibleEnd();
    const auto ref = start + std::chrono::duration_cast<nanoseconds>((end - start) / 4);

    axis.zoom(0.5, ref);  // zoom in 2x

    // After zoom, reference's relative position (1/4 from start) is preserved.
    const auto durAfter = axis.visibleDuration();
    const auto startAfter = axis.visibleStart();
    const auto refOffset = ref - startAfter;
    const double relPos = static_cast<double>(refOffset.count()) / static_cast<double>(durAfter.count());
    REQUIRE(relPos > 0.20);
    REQUIRE(relPos < 0.30);
}

TEST_CASE("S2: zoom with non-positive factor is ignored", "[chart][s2][axis][zoom]") {
    registerMetaTypes();
    ch8::TimeAxisManager axis;
    QSignalSpy rangeSpy(&axis, &ch8::TimeAxisManager::rangeChanged);

    axis.zoom(0.0, std::chrono::steady_clock::now());
    axis.zoom(-1.0, std::chrono::steady_clock::now());

    REQUIRE(rangeSpy.count() == 0);
    REQUIRE(axis.liveMode());
}

TEST_CASE("S2: pause then resume round-trips live mode", "[chart][s2][axis][pause]") {
    registerMetaTypes();
    ch8::TimeAxisManager axis;
    QSignalSpy liveSpy(&axis, &ch8::TimeAxisManager::liveModeChanged);

    axis.pause();
    REQUIRE_FALSE(axis.liveMode());
    REQUIRE(liveSpy.count() == 1);
    REQUIRE(liveSpy.takeFirst().at(0).toBool() == false);

    axis.resume();
    REQUIRE(axis.liveMode());
    REQUIRE(liveSpy.count() == 1);
    REQUIRE(liveSpy.takeFirst().at(0).toBool() == true);
}

TEST_CASE("S2: pause while already paused is a no-op", "[chart][s2][axis][pause]") {
    registerMetaTypes();
    ch8::TimeAxisManager axis;
    axis.pause();
    QSignalSpy liveSpy(&axis, &ch8::TimeAxisManager::liveModeChanged);
    QSignalSpy rangeSpy(&axis, &ch8::TimeAxisManager::rangeChanged);

    axis.pause();

    REQUIRE(liveSpy.count() == 0);
    REQUIRE(rangeSpy.count() == 0);
}

TEST_CASE("S2: resume while already live is a no-op", "[chart][s2][axis][pause]") {
    registerMetaTypes();
    ch8::TimeAxisManager axis;
    QSignalSpy liveSpy(&axis, &ch8::TimeAxisManager::liveModeChanged);
    QSignalSpy rangeSpy(&axis, &ch8::TimeAxisManager::rangeChanged);

    axis.resume();

    REQUIRE(liveSpy.count() == 0);
    REQUIRE(rangeSpy.count() == 0);
}

TEST_CASE("S2: setRange transitions to paused with explicit endpoints", "[chart][s2][axis][range]") {
    registerMetaTypes();
    ch8::TimeAxisManager axis;
    const auto start = std::chrono::steady_clock::time_point{} + seconds(100);
    const auto end = std::chrono::steady_clock::time_point{} + seconds(200);

    QSignalSpy liveSpy(&axis, &ch8::TimeAxisManager::liveModeChanged);
    QSignalSpy rangeSpy(&axis, &ch8::TimeAxisManager::rangeChanged);

    axis.setRange(start, end);

    REQUIRE_FALSE(axis.liveMode());
    REQUIRE(rangeSpy.count() == 1);
    REQUIRE(liveSpy.count() == 1);
    REQUIRE(axis.visibleStart() == start);
    REQUIRE(axis.visibleEnd() == end);
    REQUIRE(axis.visibleDuration() == nanoseconds(seconds(100)));
}

TEST_CASE("S2: setRange ignores inverted / empty ranges", "[chart][s2][axis][range]") {
    registerMetaTypes();
    ch8::TimeAxisManager axis;
    QSignalSpy rangeSpy(&axis, &ch8::TimeAxisManager::rangeChanged);

    const auto t = std::chrono::steady_clock::now();
    axis.setRange(t + seconds(10), t);  // inverted
    axis.setRange(t, t);                // empty

    REQUIRE(rangeSpy.count() == 0);
    REQUIRE(axis.liveMode());
}

TEST_CASE("S2: setPreset returns to live mode with the requested duration", "[chart][s2][axis][preset]") {
    registerMetaTypes();
    ch8::TimeAxisManager axis;
    axis.pause();  // start in paused

    QSignalSpy liveSpy(&axis, &ch8::TimeAxisManager::liveModeChanged);
    QSignalSpy rangeSpy(&axis, &ch8::TimeAxisManager::rangeChanged);

    axis.setPreset(ch8::TimeAxisManager::TimePreset::Min10);

    REQUIRE(axis.liveMode());
    REQUIRE(axis.visibleDuration() == nanoseconds(seconds(600)));
    REQUIRE(liveSpy.count() == 1);
    REQUIRE(liveSpy.takeFirst().at(0).toBool() == true);
    REQUIRE(rangeSpy.count() == 1);
}

TEST_CASE("S2: every preset maps to its expected duration", "[chart][s2][axis][preset]") {
    registerMetaTypes();
    using TP = ch8::TimeAxisManager::TimePreset;
    ch8::TimeAxisManager axis;

    axis.setPreset(TP::Sec1);
    REQUIRE(axis.visibleDuration() == nanoseconds(seconds(1)));
    axis.setPreset(TP::Sec10);
    REQUIRE(axis.visibleDuration() == nanoseconds(seconds(10)));
    axis.setPreset(TP::Min1);
    REQUIRE(axis.visibleDuration() == nanoseconds(seconds(60)));
    axis.setPreset(TP::Min10);
    REQUIRE(axis.visibleDuration() == nanoseconds(seconds(600)));
    axis.setPreset(TP::Hour1);
    REQUIRE(axis.visibleDuration() == nanoseconds(seconds(3600)));
}

TEST_CASE("S2: visibleStart in live mode tracks now()", "[chart][s2][axis][live]") {
    registerMetaTypes();
    ch8::TimeAxisManager axis;
    // visibleStart() and visibleEnd() each call now() in live mode,
    // so their difference is duration ± a few hundred ns of jitter
    // (the time it takes for the second call). Allow ±1 ms tolerance.
    const auto end = axis.visibleEnd();
    const auto start = axis.visibleStart();
    const auto observed = end - start;
    const auto expected = axis.visibleDuration();
    const auto delta = (observed > expected) ? (observed - expected) : (expected - observed);
    REQUIRE(delta < nanoseconds(milliseconds(1)));

    // Wait a small amount; visibleEnd should advance.
    std::this_thread::sleep_for(milliseconds(2));
    const auto end2 = axis.visibleEnd();
    REQUIRE(end2 > end);
}
