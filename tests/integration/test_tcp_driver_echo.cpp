// tests/integration/test_tcp_driver_echo.cpp
#include "drivers/tcp_driver.hpp"
#include "frame/raw_frame.hpp"
#include "tests/integration/echo_server_fixture.hpp"

#include <QByteArray>
#include <QCoreApplication>
#include <QObject>
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
using signalforge::test::EchoServer;

namespace {

class CoreAppHolder {
public:
    CoreAppHolder() {
        signalforge::frame::registerMetatypes();
        signalforge::drivers::registerMetatypes();
        if (QCoreApplication::instance() == nullptr) {
            static int argc = 1;
            static char argv0[] = "test_tcp_driver_echo";
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

TcpConfig cfgFor(quint16 port) {
    TcpConfig cfg;
    cfg.host = QStringLiteral("127.0.0.1");
    cfg.port = port;
    cfg.connectTimeout = std::chrono::milliseconds{2000};
    return cfg;
}

}  // namespace

TEST_CASE("tcp echo: short bidirectional payload round-trip", "[integration][tcp]") {
    EchoServer server;
    REQUIRE(server.listen());
    const quint16 port = server.port();

    TcpDriver d{cfgFor(port)};
    REQUIRE(d.open() == DriverErrorCode::Success);
    REQUIRE(waitForState(d, DriverState::Open));
    REQUIRE(d.start() == DriverErrorCode::Success);
    REQUIRE(waitForState(d, DriverState::Running));

    QByteArray received;
    QObject sink;
    QObject::connect(&d, &DriverInterface::frameReceived, &sink,
                     [&received](const signalforge::frame::RawFrame& f) { received.append(f.payload); });

    const QByteArray msg(100, '\xC3');
    REQUIRE(d.write(msg) == DriverErrorCode::Success);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (received.size() < msg.size() && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
    }

    REQUIRE(received == msg);

    d.close();
    REQUIRE(waitForState(d, DriverState::Idle));
}

TEST_CASE("tcp echo: lossless bulk round-trip (256 KB)", "[integration][tcp]") {
    EchoServer server;
    REQUIRE(server.listen());
    const quint16 port = server.port();

    TcpDriver d{cfgFor(port)};
    REQUIRE(d.open() == DriverErrorCode::Success);
    REQUIRE(waitForState(d, DriverState::Open));
    REQUIRE(d.start() == DriverErrorCode::Success);
    REQUIRE(waitForState(d, DriverState::Running));

    QByteArray received;
    QObject sink;
    QObject::connect(&d, &DriverInterface::frameReceived, &sink,
                     [&received](const signalforge::frame::RawFrame& f) { received.append(f.payload); });

    constexpr int kSize = 256 * 1024;
    QByteArray payload;
    payload.reserve(kSize);
    for (int i = 0; i < kSize; ++i) {
        payload.append(static_cast<char>(i & 0xFF));
    }

    constexpr int kChunk = 4096;
    for (int off = 0; off < kSize; off += kChunk) {
        REQUIRE(d.write(payload.mid(off, kChunk)) == DriverErrorCode::Success);
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while (received.size() < kSize && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }

    REQUIRE(received.size() == kSize);
    REQUIRE(received == payload);

    d.close();
    REQUIRE(waitForState(d, DriverState::Idle));
}

TEST_CASE("tcp echo: peer-side abrupt close triggers ResourceLost", "[integration][tcp]") {
    EchoServer server;
    REQUIRE(server.listen());
    const quint16 port = server.port();

    TcpDriver d{cfgFor(port)};
    QSignalSpy errSpy(&d, &DriverInterface::errorOccurred);

    REQUIRE(d.open() == DriverErrorCode::Success);
    REQUIRE(waitForState(d, DriverState::Open));
    REQUIRE(d.start() == DriverErrorCode::Success);
    REQUIRE(waitForState(d, DriverState::Running));

    // Give the server a moment to actually register the connection
    // before we tear it down.
    const auto settle = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
    while (std::chrono::steady_clock::now() < settle) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }

    server.closeAllClients();

    REQUIRE(waitForState(d, DriverState::Error, 5000));
    REQUIRE(errSpy.count() >= 1);

    bool sawLostOrClose = false;
    for (int i = 0; i < errSpy.count(); ++i) {
        const auto err = qvariant_cast<DriverError>(errSpy.at(i).at(0));
        if (err.code == DriverErrorCode::ResourceLost || err.code == DriverErrorCode::IoFailure) {
            sawLostOrClose = true;
            break;
        }
    }
    REQUIRE(sawLostOrClose);

    d.close();
}
