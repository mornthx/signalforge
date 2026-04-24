// tests/unit/drivers/udp_driver_test.cpp
#include "drivers/udp_driver.hpp"
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
using signalforge::drivers::UdpConfig;
using signalforge::drivers::UdpDriver;

namespace {

class CoreAppHolder {
public:
    CoreAppHolder() {
        signalforge::frame::registerMetatypes();
        signalforge::drivers::registerMetatypes();
        if (QCoreApplication::instance() == nullptr) {
            static int argc = 1;
            static char argv0[] = "udp_driver_test";
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

}  // namespace

TEST_CASE("UdpDriver: default state Idle", "[drivers][udp_driver]") {
    UdpConfig cfg;
    cfg.remoteHost = QStringLiteral("127.0.0.1");
    cfg.remotePort = 5555;
    UdpDriver d{cfg};
    REQUIRE(d.state() == DriverState::Idle);
    REQUIRE(d.health() == DriverErrorCode::Success);
}

TEST_CASE("UdpDriver: neither bind nor remote → ConfigInvalid", "[drivers][udp_driver]") {
    UdpConfig cfg;  // all defaults: 0.0.0.0:0 bind-intent absent, no remote
    UdpDriver d{cfg};
    REQUIRE(d.open() == DriverErrorCode::ConfigInvalid);
    REQUIRE(d.state() == DriverState::Idle);
}

TEST_CASE("UdpDriver: remoteHost without remotePort → ConfigInvalid", "[drivers][udp_driver]") {
    UdpConfig cfg;
    cfg.remoteHost = QStringLiteral("127.0.0.1");
    cfg.remotePort = 0;
    UdpDriver d{cfg};
    REQUIRE(d.open() == DriverErrorCode::ConfigInvalid);
}

TEST_CASE("UdpDriver: invalid multicast group → ConfigInvalid", "[drivers][udp_driver]") {
    UdpConfig cfg;
    cfg.localBindAddress = QStringLiteral("0.0.0.0");
    cfg.localBindPort = 0;
    cfg.multicastGroup = QStringLiteral("127.0.0.1");  // not in multicast range
    cfg.remoteHost = QStringLiteral("224.0.0.1");
    cfg.remotePort = 5555;
    UdpDriver d{cfg};
    REQUIRE(d.open() == DriverErrorCode::ConfigInvalid);
}

TEST_CASE("UdpDriver: bind-only config opens successfully", "[drivers][udp_driver]") {
    UdpConfig cfg;
    cfg.localBindAddress = QStringLiteral("127.0.0.1");
    cfg.localBindPort = 0;  // OS-assigned, but localBindAddress triggers bind-intent
    UdpDriver d{cfg};
    REQUIRE(d.open() == DriverErrorCode::Success);
    REQUIRE(waitForState(d, DriverState::Open));
    d.close();
    REQUIRE(waitForState(d, DriverState::Idle));
}

TEST_CASE("UdpDriver: send-only config opens successfully", "[drivers][udp_driver]") {
    UdpConfig cfg;
    cfg.remoteHost = QStringLiteral("127.0.0.1");
    cfg.remotePort = 5555;
    UdpDriver d{cfg};
    REQUIRE(d.open() == DriverErrorCode::Success);
    REQUIRE(waitForState(d, DriverState::Open));
    d.close();
    REQUIRE(waitForState(d, DriverState::Idle));
}

TEST_CASE("UdpDriver: write() with empty remoteHost → ConfigInvalid", "[drivers][udp_driver]") {
    UdpConfig cfg;
    cfg.localBindAddress = QStringLiteral("127.0.0.1");
    cfg.localBindPort = 0;
    UdpDriver d{cfg};
    REQUIRE(d.write(QByteArray{"x", 1}) == DriverErrorCode::ConfigInvalid);
}

TEST_CASE("UdpDriver: write() in wrong state → NotConfigured", "[drivers][udp_driver]") {
    UdpConfig cfg;
    cfg.remoteHost = QStringLiteral("127.0.0.1");
    cfg.remotePort = 5555;
    UdpDriver d{cfg};
    REQUIRE(d.write(QByteArray{"x", 1}) == DriverErrorCode::NotConfigured);
}

TEST_CASE("UdpDriver: close()/stop() on Idle are no-ops", "[drivers][udp_driver]") {
    UdpConfig cfg;
    cfg.remoteHost = QStringLiteral("127.0.0.1");
    cfg.remotePort = 5555;
    UdpDriver d{cfg};
    QSignalSpy spy(&d, &DriverInterface::stateChanged);
    d.close();
    d.stop();
    REQUIRE(spy.count() == 0);
    REQUIRE(d.state() == DriverState::Idle);
}

TEST_CASE("UdpDriver: statistics() starts zeroed", "[drivers][udp_driver]") {
    UdpConfig cfg;
    cfg.remoteHost = QStringLiteral("127.0.0.1");
    cfg.remotePort = 5555;
    UdpDriver d{cfg};
    const auto s = d.statistics();
    REQUIRE(s.rx.framesTotal == 0);
    REQUIRE(s.tx.framesTotal == 0);
}

TEST_CASE("UdpDriver: config() accessor", "[drivers][udp_driver]") {
    UdpConfig cfg;
    cfg.localBindAddress = QStringLiteral("127.0.0.1");
    cfg.localBindPort = 12345;
    cfg.remoteHost = QStringLiteral("example.com");
    cfg.remotePort = 443;
    cfg.multicastGroup = QStringLiteral("224.0.0.99");
    cfg.multicastTtl = 4;
    UdpDriver d{cfg};
    REQUIRE(d.config().localBindAddress == "127.0.0.1");
    REQUIRE(d.config().localBindPort == 12345);
    REQUIRE(d.config().remoteHost == "example.com");
    REQUIRE(d.config().remotePort == 443);
    REQUIRE(d.config().multicastGroup == "224.0.0.99");
    REQUIRE(d.config().multicastTtl == 4u);
}
