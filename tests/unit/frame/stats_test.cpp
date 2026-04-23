// tests/unit/frame/stats_test.cpp
#include "frame/raw_frame.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace signalforge::frame;

TEST_CASE("RxStats: default-init zeroes all counters", "[frame][stats]") {
    RxStats s;
    REQUIRE(s.framesTotal == 0);
    REQUIRE(s.bytesTotal == 0);
    REQUIRE(s.framesDropped == 0);
    REQUIRE(s.errors == 0);
    REQUIRE(s.queueWatermarkPeak == 0);
    REQUIRE(s.queueWatermarkCurrent == 0);
    REQUIRE(s.lastFrameAt == SteadyTimestamp{});
}

TEST_CASE("TxStats: default-init zeroes all counters", "[frame][stats]") {
    TxStats s;
    REQUIRE(s.framesTotal == 0);
    REQUIRE(s.bytesTotal == 0);
    REQUIRE(s.failures == 0);
    REQUIRE(s.lastFrameAt == SteadyTimestamp{});
}

TEST_CASE("DriverStatistics: default-init has empty Rx and Tx snapshots", "[frame][stats]") {
    DriverStatistics s;
    REQUIRE(s.rx.framesTotal == 0);
    REQUIRE(s.tx.framesTotal == 0);
    REQUIRE(s.snapshotAt == SteadyTimestamp{});
}

TEST_CASE("RxStats: copy + assign preserve field values", "[frame][stats]") {
    RxStats a;
    a.framesTotal = 10;
    a.bytesTotal = 100;
    a.queueWatermarkPeak = 45;
    a.queueWatermarkCurrent = 30;

    RxStats b = a;
    REQUIRE(b.framesTotal == 10);
    REQUIRE(b.bytesTotal == 100);
    REQUIRE(b.queueWatermarkPeak == 45);
    REQUIRE(b.queueWatermarkCurrent == 30);

    RxStats c;
    c = a;
    REQUIRE(c.framesTotal == 10);
}

TEST_CASE("DriverStatistics: composite copy preserves rx and tx", "[frame][stats]") {
    DriverStatistics s;
    s.rx.framesTotal = 7;
    s.tx.framesTotal = 3;
    s.snapshotAt = std::chrono::steady_clock::now();

    DriverStatistics copy = s;
    REQUIRE(copy.rx.framesTotal == 7);
    REQUIRE(copy.tx.framesTotal == 3);
    REQUIRE(copy.snapshotAt == s.snapshotAt);
}
