// tests/unit/drivers/tcp_driver_test.cpp
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
            static char argv0[] = "tcp_driver_test";
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

TEST_CASE("TcpDriver: default state Idle", "[drivers][tcp_driver]") {
    TcpConfig cfg;
    cfg.host = QStringLiteral("127.0.0.1");
    cfg.port = 12345;
    TcpDriver d{cfg};
    REQUIRE(d.state() == DriverState::Idle);
    REQUIRE(d.health() == DriverErrorCode::Success);
}

TEST_CASE("TcpDriver: empty host → ConfigInvalid", "[drivers][tcp_driver]") {
    TcpConfig cfg;  // host default-empty
    cfg.port = 1234;
    TcpDriver d{cfg};
    REQUIRE(d.open() == DriverErrorCode::ConfigInvalid);
    REQUIRE(d.state() == DriverState::Idle);
}

TEST_CASE("TcpDriver: port=0 → ConfigInvalid", "[drivers][tcp_driver]") {
    TcpConfig cfg;
    cfg.host = QStringLiteral("127.0.0.1");
    cfg.port = 0;
    TcpDriver d{cfg};
    REQUIRE(d.open() == DriverErrorCode::ConfigInvalid);
    REQUIRE(d.state() == DriverState::Idle);
}

TEST_CASE("TcpDriver: non-positive timeout → ConfigInvalid", "[drivers][tcp_driver]") {
    TcpConfig cfg;
    cfg.host = QStringLiteral("127.0.0.1");
    cfg.port = 1234;
    cfg.connectTimeout = std::chrono::milliseconds{0};
    TcpDriver d{cfg};
    REQUIRE(d.open() == DriverErrorCode::ConfigInvalid);
}

TEST_CASE("TcpDriver: unreachable port → Error async", "[drivers][tcp_driver]") {
    TcpConfig cfg;
    cfg.host = QStringLiteral("127.0.0.1");
    // Use a port very unlikely to be listening.
    cfg.port = 59991;
    cfg.connectTimeout = std::chrono::milliseconds{500};
    TcpDriver d{cfg};
    QSignalSpy errSpy(&d, &DriverInterface::errorOccurred);
    REQUIRE(d.open() == DriverErrorCode::Success);
    REQUIRE(waitForState(d, DriverState::Error));
    REQUIRE(errSpy.count() >= 1);
    const auto err = qvariant_cast<DriverError>(errSpy.at(0).at(0));
    // ConnectionRefused is the typical response on 127.0.0.1 to an
    // unbound port, mapping to ResourceUnavailable. Host-specific
    // variants (Timeout) are also acceptable.
    REQUIRE((err.code == DriverErrorCode::ResourceUnavailable || err.code == DriverErrorCode::Timeout ||
             err.code == DriverErrorCode::IoFailure));
}

TEST_CASE("TcpDriver: write() in wrong state → NotConfigured", "[drivers][tcp_driver]") {
    TcpConfig cfg;
    cfg.host = QStringLiteral("127.0.0.1");
    cfg.port = 1234;
    TcpDriver d{cfg};
    REQUIRE(d.write(QByteArray{"x", 1}) == DriverErrorCode::NotConfigured);
}

TEST_CASE("TcpDriver: close()/stop() on Idle are no-ops", "[drivers][tcp_driver]") {
    TcpConfig cfg;
    cfg.host = QStringLiteral("127.0.0.1");
    cfg.port = 1234;
    TcpDriver d{cfg};
    QSignalSpy spy(&d, &DriverInterface::stateChanged);
    d.close();
    d.stop();
    REQUIRE(spy.count() == 0);
    REQUIRE(d.state() == DriverState::Idle);
}

TEST_CASE("TcpDriver: statistics() starts zeroed", "[drivers][tcp_driver]") {
    TcpConfig cfg;
    cfg.host = QStringLiteral("127.0.0.1");
    cfg.port = 1234;
    TcpDriver d{cfg};
    const auto s = d.statistics();
    REQUIRE(s.rx.framesTotal == 0);
    REQUIRE(s.tx.framesTotal == 0);
}

TEST_CASE("TcpDriver: config() accessor", "[drivers][tcp_driver]") {
    TcpConfig cfg;
    cfg.host = QStringLiteral("example.com");
    cfg.port = 443;
    cfg.connectTimeout = std::chrono::milliseconds{1234};
    TcpDriver d{cfg};
    REQUIRE(d.config().host == "example.com");
    REQUIRE(d.config().port == 443);
    REQUIRE(d.config().connectTimeout == std::chrono::milliseconds{1234});
}
