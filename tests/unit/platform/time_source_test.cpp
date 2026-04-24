// tests/unit/platform/time_source_test.cpp
#include "platform/time_source.hpp"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <thread>

using namespace signalforge::platform;
using namespace std::chrono_literals;

TEST_CASE("time_source: monotonicNow is monotonically non-decreasing", "[platform][time_source]") {
    const auto a = monotonicNow();
    const auto b = monotonicNow();
    const auto c = monotonicNow();
    REQUIRE(a <= b);
    REQUIRE(b <= c);
}

TEST_CASE("time_source: monotonicNow advances across sleep", "[platform][time_source]") {
    const auto before = monotonicNow();
    std::this_thread::sleep_for(1ms);
    const auto after = monotonicNow();
    REQUIRE(after > before);
    REQUIRE((after - before) >= 500us);  // Conservative floor under loaded CI
}

TEST_CASE("time_source: wallClockNow returns a realistic epoch time", "[platform][time_source]") {
    const auto t = wallClockNow();
    const auto since_epoch = t.time_since_epoch();
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(since_epoch).count();
    // Sanity: after 2020-01-01 (1577836800) and before 2100-01-01 (4102444800).
    REQUIRE(seconds > 1'577'836'800);
    REQUIRE(seconds < 4'102'444'800);
}

TEST_CASE("time_source: captureOrigin populates both fields", "[platform][time_source]") {
    const auto origin = captureOrigin();
    REQUIRE(origin.monotonic.time_since_epoch().count() > 0);
    REQUIRE(origin.wall.time_since_epoch().count() > 0);
}

TEST_CASE("time_source: captureOrigin monotonic field tracks monotonicNow", "[platform][time_source]") {
    const auto before = monotonicNow();
    const auto origin = captureOrigin();
    const auto after = monotonicNow();
    REQUIRE(origin.monotonic >= before);
    REQUIRE(origin.monotonic <= after);
}

TEST_CASE("time_source: ClockOrigin is value-copyable", "[platform][time_source]") {
    const auto a = captureOrigin();
    ClockOrigin b = a;
    REQUIRE(b.monotonic == a.monotonic);
    REQUIRE(b.wall == a.wall);
}
