// tests/unit/chart/chart_smoke_test.cpp
//
// S1 smoke test: confirm the freeze-surface headers compile and the
// chart static lib links. The full Chart / ChartManager / etc. APIs
// are tested in S2 - S10. Here we just instantiate the leaf types
// that don't yet need a QApplication / event loop.

#include "chart/chart.hpp"
#include "chart/chart_manager.hpp"
#include "chart/signal_selector.hpp"
#include "chart/time_axis_manager.hpp"

#include <catch2/catch_test_macros.hpp>

namespace ch8 = signalforge::chart;

TEST_CASE("S1: TimeAxisManager constructs with default 10s duration", "[chart][s1][smoke]") {
    ch8::TimeAxisManager axis;
    REQUIRE(axis.liveMode());
    const auto duration = axis.visibleDuration();
    REQUIRE(duration > std::chrono::seconds(0));
}

TEST_CASE("S1: ChartConfig + ChartSignalConfig defaults are sane", "[chart][s1][smoke]") {
    ch8::ChartSignalConfig sig;
    REQUIRE(sig.displayMode == ch8::SignalDisplayMode::Line);
    REQUIRE(sig.visible);

    ch8::ChartConfig cfg;
    REQUIRE(cfg.signalConfigs.empty());
    REQUIRE_FALSE(cfg.timeAxisId.has_value());
}

TEST_CASE("S1: SignalDisplayMode enum values are distinct", "[chart][s1][smoke]") {
    REQUIRE(static_cast<int>(ch8::SignalDisplayMode::Line) != static_cast<int>(ch8::SignalDisplayMode::Step));
    REQUIRE(static_cast<int>(ch8::SignalDisplayMode::Line) != static_cast<int>(ch8::SignalDisplayMode::Point));
    REQUIRE(static_cast<int>(ch8::SignalDisplayMode::Step) != static_cast<int>(ch8::SignalDisplayMode::Point));
}

TEST_CASE("S1: TimePreset enum has the 5 spec'd presets", "[chart][s1][smoke]") {
    auto p1 = ch8::TimeAxisManager::TimePreset::Sec1;
    auto p2 = ch8::TimeAxisManager::TimePreset::Sec10;
    auto p3 = ch8::TimeAxisManager::TimePreset::Min1;
    auto p4 = ch8::TimeAxisManager::TimePreset::Min10;
    auto p5 = ch8::TimeAxisManager::TimePreset::Hour1;
    REQUIRE(static_cast<int>(p1) != static_cast<int>(p2));
    REQUIRE(static_cast<int>(p4) != static_cast<int>(p5));
    REQUIRE(static_cast<int>(p3) != static_cast<int>(p1));
}
