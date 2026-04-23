// tests/unit/drivers/driver_interface_test.cpp
#include "drivers/driver_interface.hpp"
#include "frame/raw_frame.hpp"
#include "tests/mocks/mock_driver.hpp"

#include <QByteArray>
#include <QSignalSpy>
#include <QVariant>
#include <QtTest/QtTest>
#include <catch2/catch_test_macros.hpp>
#include <type_traits>

using signalforge::drivers::DriverError;
using signalforge::drivers::DriverErrorCode;
using signalforge::drivers::DriverInterface;
using signalforge::drivers::DriverState;
using signalforge::test::MockDriver;

// Compile-time invariants from Q_DISABLE_COPY_MOVE.
static_assert(!std::is_copy_constructible_v<DriverInterface>, "DriverInterface must be non-copyable");
static_assert(!std::is_copy_assignable_v<DriverInterface>, "DriverInterface must be non-copy-assignable");
static_assert(!std::is_move_constructible_v<DriverInterface>, "DriverInterface must be non-movable");
static_assert(!std::is_move_assignable_v<DriverInterface>, "DriverInterface must be non-move-assignable");

TEST_CASE("DriverInterface: default state is Idle", "[drivers][driver_interface]") {
    MockDriver d;
    REQUIRE(d.state() == DriverState::Idle);
    REQUIRE(d.health() == DriverErrorCode::Success);
}

TEST_CASE("DriverInterface: happy-path lifecycle traverses expected states", "[drivers][driver_interface]") {
    MockDriver d;
    QSignalSpy spy(&d, &DriverInterface::stateChanged);

    REQUIRE(d.open() == DriverErrorCode::Success);
    REQUIRE(d.state() == DriverState::Open);

    REQUIRE(d.start() == DriverErrorCode::Success);
    REQUIRE(d.state() == DriverState::Running);

    d.stop();
    REQUIRE(d.state() == DriverState::Open);

    d.close();
    REQUIRE(d.state() == DriverState::Idle);

    // Expected state transitions captured by spy:
    //   Opening, Open, Running, Stopping, Open, Closing, Idle
    REQUIRE(spy.count() == 7);
    REQUIRE(qvariant_cast<DriverState>(spy.at(0).at(0)) == DriverState::Opening);
    REQUIRE(qvariant_cast<DriverState>(spy.at(1).at(0)) == DriverState::Open);
    REQUIRE(qvariant_cast<DriverState>(spy.at(2).at(0)) == DriverState::Running);
    REQUIRE(qvariant_cast<DriverState>(spy.at(3).at(0)) == DriverState::Stopping);
    REQUIRE(qvariant_cast<DriverState>(spy.at(4).at(0)) == DriverState::Open);
    REQUIRE(qvariant_cast<DriverState>(spy.at(5).at(0)) == DriverState::Closing);
    REQUIRE(qvariant_cast<DriverState>(spy.at(6).at(0)) == DriverState::Idle);
}

TEST_CASE("DriverInterface: open() from non-Idle state returns NotConfigured", "[drivers][driver_interface]") {
    MockDriver d;
    REQUIRE(d.open() == DriverErrorCode::Success);
    // Already Open; a second open() rejects.
    REQUIRE(d.open() == DriverErrorCode::NotConfigured);
    REQUIRE(d.state() == DriverState::Open);  // state preserved on rejection
}

TEST_CASE("DriverInterface: start() from non-Open state returns NotConfigured", "[drivers][driver_interface]") {
    MockDriver d;
    // In Idle: start() must fail.
    REQUIRE(d.start() == DriverErrorCode::NotConfigured);
    REQUIRE(d.state() == DriverState::Idle);
}

TEST_CASE("DriverInterface: write() in wrong state returns appropriate error", "[drivers][driver_interface]") {
    MockDriver d;
    QByteArray payload{"hello", 5};

    REQUIRE(d.write(payload) == DriverErrorCode::NotConfigured);
    REQUIRE(d.open() == DriverErrorCode::Success);
    REQUIRE(d.write(payload) == DriverErrorCode::NotConfigured);  // Open but not Running
    REQUIRE(d.start() == DriverErrorCode::Success);
    REQUIRE(d.write(payload) == DriverErrorCode::Success);
}

TEST_CASE("DriverInterface: close() from Idle is a no-op", "[drivers][driver_interface]") {
    MockDriver d;
    QSignalSpy spy(&d, &DriverInterface::stateChanged);
    d.close();  // already Idle
    REQUIRE(spy.count() == 0);
    REQUIRE(d.state() == DriverState::Idle);
}

TEST_CASE("DriverInterface: pre-open failure returns code without transition", "[drivers][driver_interface]") {
    MockDriver d;
    d.setPreOpenFailure(DriverErrorCode::ConfigInvalid);
    QSignalSpy spy(&d, &DriverInterface::stateChanged);
    REQUIRE(d.open() == DriverErrorCode::ConfigInvalid);
    REQUIRE(d.state() == DriverState::Idle);  // no transition
    REQUIRE(spy.count() == 0);
}

TEST_CASE("DriverInterface: injected error transitions to Error state", "[drivers][driver_interface]") {
    MockDriver d;
    REQUIRE(d.open() == DriverErrorCode::Success);
    REQUIRE(d.start() == DriverErrorCode::Success);

    QSignalSpy errSpy(&d, &DriverInterface::errorOccurred);
    QSignalSpy stateSpy(&d, &DriverInterface::stateChanged);
    d.injectError(DriverErrorCode::IoFailure, "simulated");

    REQUIRE(errSpy.count() == 1);
    const auto err = qvariant_cast<DriverError>(errSpy.at(0).at(0));
    REQUIRE(err.code == DriverErrorCode::IoFailure);
    REQUIRE(err.message == "simulated");

    REQUIRE(d.state() == DriverState::Error);
    REQUIRE(d.health() == DriverErrorCode::IoFailure);

    // Recovery path: close() returns to Idle; open() then start() can resume.
    d.close();
    REQUIRE(d.state() == DriverState::Idle);
    REQUIRE(d.open() == DriverErrorCode::Success);
    REQUIRE(d.state() == DriverState::Open);
}

TEST_CASE("DriverInterface: emitFrame delivers RawFrame via signal", "[drivers][driver_interface]") {
    MockDriver d;
    REQUIRE(d.open() == DriverErrorCode::Success);
    REQUIRE(d.start() == DriverErrorCode::Success);

    QSignalSpy spy(&d, &DriverInterface::frameReceived);
    d.emitFrame(QByteArray("abc", 3));
    d.emitFrame(QByteArray("xyz", 3));

    REQUIRE(spy.count() == 2);
    const auto f1 = qvariant_cast<signalforge::frame::RawFrame>(spy.at(0).at(0));
    REQUIRE(f1.sourceId == "mock");
    REQUIRE(f1.payload == QByteArray("abc", 3));
    REQUIRE(f1.sequenceNumber == 1);

    const auto stats = d.statistics();
    REQUIRE(stats.rx.framesTotal == 2);
    REQUIRE(stats.rx.bytesTotal == 6);
}

TEST_CASE("DriverInterface: statistics() after write accumulates tx bytes", "[drivers][driver_interface]") {
    MockDriver d;
    REQUIRE(d.open() == DriverErrorCode::Success);
    REQUIRE(d.start() == DriverErrorCode::Success);

    REQUIRE(d.write(QByteArray("1234", 4)) == DriverErrorCode::Success);
    REQUIRE(d.write(QByteArray("56", 2)) == DriverErrorCode::Success);

    const auto stats = d.statistics();
    REQUIRE(stats.tx.framesTotal == 2);
    REQUIRE(stats.tx.bytesTotal == 6);
}

TEST_CASE("DriverInterface: DriverState / DriverError round-trip through QVariant",
          "[drivers][driver_interface][metatype]") {
    signalforge::drivers::registerMetatypes();
    signalforge::drivers::registerMetatypes();  // idempotent

    const DriverState s = DriverState::Running;
    QVariant vs = QVariant::fromValue(s);
    REQUIRE(vs.canConvert<DriverState>());
    REQUIRE(qvariant_cast<DriverState>(vs) == s);

    DriverError err;
    err.code = DriverErrorCode::BackpressureDrop;
    err.message = "drop";
    err.at = std::chrono::steady_clock::now();
    QVariant ve = QVariant::fromValue(err);
    REQUIRE(ve.canConvert<DriverError>());
    const auto back = qvariant_cast<DriverError>(ve);
    REQUIRE(back.code == err.code);
    REQUIRE(back.message == err.message);
}

TEST_CASE("DriverInterface: DriverState / DriverErrorCode enum values are explicit and stable",
          "[drivers][driver_interface]") {
    REQUIRE(static_cast<int>(DriverState::Idle) == 0);
    REQUIRE(static_cast<int>(DriverState::Opening) == 1);
    REQUIRE(static_cast<int>(DriverState::Open) == 2);
    REQUIRE(static_cast<int>(DriverState::Running) == 3);
    REQUIRE(static_cast<int>(DriverState::Stopping) == 4);
    REQUIRE(static_cast<int>(DriverState::Closing) == 5);
    REQUIRE(static_cast<int>(DriverState::Error) == 6);

    REQUIRE(static_cast<int>(DriverErrorCode::Success) == 0);
    REQUIRE(static_cast<int>(DriverErrorCode::Internal) == 10);
}

TEST_CASE("DriverInterface: DriverError default-init", "[drivers][driver_interface]") {
    DriverError e;
    REQUIRE(e.code == DriverErrorCode::Success);
    REQUIRE(e.message.isEmpty());
}
