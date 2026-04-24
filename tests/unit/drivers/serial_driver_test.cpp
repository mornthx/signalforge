// tests/unit/drivers/serial_driver_test.cpp
#include "drivers/serial_driver.hpp"
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
using signalforge::drivers::SerialConfig;
using signalforge::drivers::SerialDriver;

namespace {

class CoreAppHolder {
public:
    CoreAppHolder() {
        signalforge::frame::registerMetatypes();
        signalforge::drivers::registerMetatypes();
        if (QCoreApplication::instance() == nullptr) {
            static int argc = 1;
            static char argv0[] = "serial_driver_test";
            static char* argv[] = {argv0, nullptr};
            app_ = std::make_unique<QCoreApplication>(argc, argv);
        }
    }

private:
    std::unique_ptr<QCoreApplication> app_;
};
CoreAppHolder g_app;

bool waitForState(DriverInterface& d, DriverState desired, int ms = 2000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (d.state() != desired && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
    }
    return d.state() == desired;
}

}  // namespace

TEST_CASE("SerialDriver: default state Idle", "[drivers][serial_driver]") {
    SerialConfig cfg;
    cfg.device = QStringLiteral("/dev/nonexistent-for-test");
    SerialDriver d{cfg};
    REQUIRE(d.state() == DriverState::Idle);
    REQUIRE(d.health() == DriverErrorCode::Success);
}

TEST_CASE("SerialDriver: empty device path → ConfigInvalid", "[drivers][serial_driver]") {
    SerialConfig cfg;  // device default-empty
    SerialDriver d{cfg};
    REQUIRE(d.open() == DriverErrorCode::ConfigInvalid);
    REQUIRE(d.state() == DriverState::Idle);
}

TEST_CASE("SerialDriver: invalid baud rate → ConfigInvalid", "[drivers][serial_driver]") {
    SerialConfig cfg;
    cfg.device = QStringLiteral("/dev/null");
    cfg.baudRate = -1;
    SerialDriver d{cfg};
    REQUIRE(d.open() == DriverErrorCode::ConfigInvalid);
    REQUIRE(d.state() == DriverState::Idle);
}

TEST_CASE("SerialDriver: invalid dataBits → ConfigInvalid", "[drivers][serial_driver]") {
    SerialConfig cfg;
    cfg.device = QStringLiteral("/dev/null");
    cfg.dataBits = 9;
    SerialDriver d{cfg};
    REQUIRE(d.open() == DriverErrorCode::ConfigInvalid);
}

TEST_CASE("SerialDriver: invalid stopBits → ConfigInvalid", "[drivers][serial_driver]") {
    SerialConfig cfg;
    cfg.device = QStringLiteral("/dev/null");
    cfg.stopBits = 3;
    SerialDriver d{cfg};
    REQUIRE(d.open() == DriverErrorCode::ConfigInvalid);
}

TEST_CASE("SerialDriver: unknown parity → ConfigInvalid", "[drivers][serial_driver]") {
    SerialConfig cfg;
    cfg.device = QStringLiteral("/dev/null");
    cfg.parity = QStringLiteral("weird");
    SerialDriver d{cfg};
    REQUIRE(d.open() == DriverErrorCode::ConfigInvalid);
}

TEST_CASE("SerialDriver: unknown flow control → ConfigInvalid", "[drivers][serial_driver]") {
    SerialConfig cfg;
    cfg.device = QStringLiteral("/dev/null");
    cfg.flowControl = QStringLiteral("magic");
    SerialDriver d{cfg};
    REQUIRE(d.open() == DriverErrorCode::ConfigInvalid);
}

TEST_CASE("SerialDriver: nonexistent device → Error state async", "[drivers][serial_driver]") {
    SerialConfig cfg;
    cfg.device = QStringLiteral("/dev/signalforge_nope_%1").arg(::getpid());
    SerialDriver d{cfg};
    QSignalSpy errSpy(&d, &DriverInterface::errorOccurred);

    REQUIRE(d.open() == DriverErrorCode::Success);
    REQUIRE(waitForState(d, DriverState::Error));
    REQUIRE(errSpy.count() >= 1);
    const auto err = qvariant_cast<DriverError>(errSpy.at(0).at(0));
    // Either DeviceNotFound (→ ResourceUnavailable) or a generic open
    // failure; both are acceptable per spec §4.8.
    REQUIRE((err.code == DriverErrorCode::ResourceUnavailable || err.code == DriverErrorCode::IoFailure ||
             err.code == DriverErrorCode::PermissionDenied));
    REQUIRE(d.state() == DriverState::Error);
}

TEST_CASE("SerialDriver: write() in wrong state → NotConfigured", "[drivers][serial_driver]") {
    SerialConfig cfg;
    cfg.device = QStringLiteral("/dev/null");
    SerialDriver d{cfg};
    REQUIRE(d.write(QByteArray{"x", 1}) == DriverErrorCode::NotConfigured);  // Idle
}

TEST_CASE("SerialDriver: close()/stop() on Idle are no-ops", "[drivers][serial_driver]") {
    SerialConfig cfg;
    cfg.device = QStringLiteral("/dev/null");
    SerialDriver d{cfg};
    QSignalSpy spy(&d, &DriverInterface::stateChanged);
    d.close();
    d.stop();
    REQUIRE(spy.count() == 0);
    REQUIRE(d.state() == DriverState::Idle);
}

TEST_CASE("SerialDriver: statistics() starts zeroed", "[drivers][serial_driver]") {
    SerialConfig cfg;
    cfg.device = QStringLiteral("/dev/null");
    SerialDriver d{cfg};
    const auto s = d.statistics();
    REQUIRE(s.rx.framesTotal == 0);
    REQUIRE(s.rx.bytesTotal == 0);
    REQUIRE(s.tx.framesTotal == 0);
    REQUIRE(s.tx.bytesTotal == 0);
    REQUIRE(s.tx.failures == 0);
}

TEST_CASE("SerialDriver: config() returns stored config", "[drivers][serial_driver]") {
    SerialConfig cfg;
    cfg.device = QStringLiteral("/dev/ttyUSB0");
    cfg.baudRate = 921600;
    cfg.parity = QStringLiteral("even");
    SerialDriver d{cfg};
    REQUIRE(d.config().device == "/dev/ttyUSB0");
    REQUIRE(d.config().baudRate == 921600);
    REQUIRE(d.config().parity == "even");
}
