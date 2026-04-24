// tests/integration/test_driver_error_paths.cpp
//
// Concentrated error-injection coverage per M3-plan S9 and spec §3.5.
// The scenarios specifically targeted here (not already covered in
// test_serial_driver_loopback, test_tcp_driver_echo,
// test_udp_driver_loopback, or the unit tests) are:
//
//   1. Rapid open/close cycles (100 iterations) — no leaks, clean
//      lifecycle every time.
//   2. open() → close() immediately before Open reached — state
//      machine ends in Idle, no crash.
//   3. write() after the driver has transitioned to Error — should
//      refuse with NotConfigured (not crash, not enqueue).
//   4. close() after Error — returns to Idle.
//   5. stop() without start() — no-op, no signal emission.
//
// Overlap with existing tests:
//   - Mid-run Serial disconnect: test_serial_driver_loopback.cpp case 3.
//   - Mid-run TCP disconnect: test_tcp_driver_echo.cpp case 3.
//   - start() without open(): already exercised by per-driver unit tests.
#include "drivers/replay_driver.hpp"
#include "drivers/tcp_driver.hpp"
#include "frame/raw_frame.hpp"

#include <QByteArray>
#include <QCoreApplication>
#include <QSignalSpy>
#include <QtTest/QtTest>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <memory>

using signalforge::drivers::DriverError;
using signalforge::drivers::DriverErrorCode;
using signalforge::drivers::DriverInterface;
using signalforge::drivers::DriverState;
using signalforge::drivers::ReplayConfig;
using signalforge::drivers::ReplayDriver;
using signalforge::drivers::TcpConfig;
using signalforge::drivers::TcpDriver;

namespace {

class CoreAppHolder {
public:
    CoreAppHolder() {
        signalforge::frame::registerMetatypes();
        signalforge::drivers::registerMetatypes();
        if (QCoreApplication::instance() == nullptr) {
            static int argc = 1;
            static char argv0[] = "test_driver_error_paths";
            static char* argv[] = {argv0, nullptr};
            app_ = std::make_unique<QCoreApplication>(argc, argv);
        }
    }

private:
    std::unique_ptr<QCoreApplication> app_;
};
CoreAppHolder g_app;

bool waitForState(DriverInterface& d, DriverState desired, int ms = 3000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (d.state() != desired && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
    }
    return d.state() == desired;
}

ReplayConfig replayCfg() {
    ReplayConfig c;
    c.sessionFilePath = QStringLiteral(SIGNALFORGE_FIXTURES_DIR) + QStringLiteral("/minimal_session.sfreplay");
    return c;
}

TcpConfig unreachableTcpCfg() {
    TcpConfig c;
    c.host = QStringLiteral("127.0.0.1");
    c.port = 59992;  // picked high, unlikely to be bound
    c.connectTimeout = std::chrono::milliseconds{300};
    return c;
}

}  // namespace

TEST_CASE("error paths: 100 rapid open/close cycles leave driver Idle", "[integration][errors][replay]") {
    for (int i = 0; i < 100; ++i) {
        ReplayDriver d{replayCfg()};
        REQUIRE(d.open() == DriverErrorCode::Success);
        REQUIRE(waitForState(d, DriverState::Open));
        d.close();
        REQUIRE(waitForState(d, DriverState::Idle));
    }
}

TEST_CASE("error paths: close() issued before Open completes ends in Idle", "[integration][errors][replay]") {
    ReplayDriver d{replayCfg()};
    REQUIRE(d.open() == DriverErrorCode::Success);
    // Do NOT wait for Open; issue close() while the worker is still
    // processing the open() request on the IO thread.
    d.close();
    // Regardless of whether the worker managed to complete open first,
    // the driver must converge to Idle, not hang or crash.
    REQUIRE(waitForState(d, DriverState::Idle, 3000));
}

TEST_CASE("error paths: write() after Error returns NotConfigured", "[integration][errors][tcp]") {
    TcpDriver d{unreachableTcpCfg()};
    QSignalSpy errSpy(&d, &DriverInterface::errorOccurred);
    REQUIRE(d.open() == DriverErrorCode::Success);
    REQUIRE(waitForState(d, DriverState::Error, 3000));
    REQUIRE(errSpy.count() >= 1);

    // In Error state the write must be rejected synchronously; no
    // worker invocation, no payload queued.
    REQUIRE(d.write(QByteArray{"x", 1}) == DriverErrorCode::NotConfigured);

    d.close();
    REQUIRE(waitForState(d, DriverState::Idle, 3000));
}

TEST_CASE("error paths: close() after Error transitions back to Idle", "[integration][errors][tcp]") {
    TcpDriver d{unreachableTcpCfg()};
    REQUIRE(d.open() == DriverErrorCode::Success);
    REQUIRE(waitForState(d, DriverState::Error, 3000));

    d.close();
    REQUIRE(waitForState(d, DriverState::Idle, 3000));
}

TEST_CASE("error paths: stop() without start() is a silent no-op", "[integration][errors][replay]") {
    ReplayDriver d{replayCfg()};
    REQUIRE(d.open() == DriverErrorCode::Success);
    REQUIRE(waitForState(d, DriverState::Open));

    QSignalSpy stateSpy(&d, &DriverInterface::stateChanged);
    d.stop();  // from Open, not Running — should do nothing

    // Pump a little to catch any spurious transition.
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    REQUIRE(stateSpy.count() == 0);
    REQUIRE(d.state() == DriverState::Open);

    d.close();
    REQUIRE(waitForState(d, DriverState::Idle));
}

TEST_CASE("error paths: start() without open() returns NotConfigured", "[integration][errors][replay]") {
    ReplayDriver d{replayCfg()};
    REQUIRE(d.start() == DriverErrorCode::NotConfigured);
    REQUIRE(d.state() == DriverState::Idle);
}
