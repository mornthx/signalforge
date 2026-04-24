// tests/integration/test_serial_driver_loopback.cpp
#include "drivers/serial_driver.hpp"
#include "frame/raw_frame.hpp"
#include "tests/integration/socat_fixture.hpp"

#include <QByteArray>
#include <QCoreApplication>
#include <QObject>
#include <QSignalSpy>
#include <QtTest/QtTest>
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <memory>

using signalforge::drivers::DriverError;
using signalforge::drivers::DriverErrorCode;
using signalforge::drivers::DriverInterface;
using signalforge::drivers::DriverState;
using signalforge::drivers::SerialConfig;
using signalforge::drivers::SerialDriver;
using signalforge::test::SocatVirtualPair;

namespace {

class CoreAppHolder {
public:
    CoreAppHolder() {
        signalforge::frame::registerMetatypes();
        signalforge::drivers::registerMetatypes();
        if (QCoreApplication::instance() == nullptr) {
            static int argc = 1;
            static char argv0[] = "test_serial_driver_loopback";
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

SerialConfig cfgFor(const QString& device) {
    SerialConfig cfg;
    cfg.device = device;
    cfg.baudRate = 115200;
    cfg.dataBits = 8;
    cfg.parity = QStringLiteral("none");
    cfg.stopBits = 1;
    cfg.flowControl = QStringLiteral("none");
    return cfg;
}

}  // namespace

TEST_CASE("serial loopback: bidirectional short payload via socat pair", "[integration][serial][socat]") {
    if (!SocatVirtualPair::isAvailable()) {
        SKIP("socat executable not found on PATH; install socat to run this test");
    }
    SocatVirtualPair pair;

    SerialDriver a{cfgFor(pair.side0())};
    SerialDriver b{cfgFor(pair.side1())};

    REQUIRE(a.open() == DriverErrorCode::Success);
    REQUIRE(b.open() == DriverErrorCode::Success);
    REQUIRE(waitForState(a, DriverState::Open));
    REQUIRE(waitForState(b, DriverState::Open));
    REQUIRE(a.start() == DriverErrorCode::Success);
    REQUIRE(b.start() == DriverErrorCode::Success);
    REQUIRE(waitForState(a, DriverState::Running));
    REQUIRE(waitForState(b, DriverState::Running));

    QByteArray receivedAtA;
    QByteArray receivedAtB;
    QObject sinkA;
    QObject sinkB;
    QObject::connect(&a, &DriverInterface::frameReceived, &sinkA,
                     [&receivedAtA](const signalforge::frame::RawFrame& f) { receivedAtA.append(f.payload); });
    QObject::connect(&b, &DriverInterface::frameReceived, &sinkB,
                     [&receivedAtB](const signalforge::frame::RawFrame& f) { receivedAtB.append(f.payload); });

    const QByteArray msgAtoB(100, '\xAA');
    const QByteArray msgBtoA(100, '\x55');
    REQUIRE(a.write(msgAtoB) == DriverErrorCode::Success);
    REQUIRE(b.write(msgBtoA) == DriverErrorCode::Success);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while ((receivedAtA.size() < msgBtoA.size() || receivedAtB.size() < msgAtoB.size()) &&
           std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
    }

    REQUIRE(receivedAtB == msgAtoB);
    REQUIRE(receivedAtA == msgBtoA);

    a.close();
    b.close();
    REQUIRE(waitForState(a, DriverState::Idle));
    REQUIRE(waitForState(b, DriverState::Idle));
}

TEST_CASE("serial loopback: lossless bulk transfer (256 KB A→B)", "[integration][serial][socat]") {
    if (!SocatVirtualPair::isAvailable()) {
        SKIP("socat executable not found on PATH; install socat to run this test");
    }
    SocatVirtualPair pair;

    SerialDriver a{cfgFor(pair.side0())};
    SerialDriver b{cfgFor(pair.side1())};

    REQUIRE(a.open() == DriverErrorCode::Success);
    REQUIRE(b.open() == DriverErrorCode::Success);
    REQUIRE(waitForState(a, DriverState::Open));
    REQUIRE(waitForState(b, DriverState::Open));
    REQUIRE(a.start() == DriverErrorCode::Success);
    REQUIRE(b.start() == DriverErrorCode::Success);
    REQUIRE(waitForState(a, DriverState::Running));
    REQUIRE(waitForState(b, DriverState::Running));

    QByteArray received;
    QObject sink;
    QObject::connect(&b, &DriverInterface::frameReceived, &sink,
                     [&received](const signalforge::frame::RawFrame& f) { received.append(f.payload); });

    // Build a deterministic payload so we can verify order.
    constexpr int kSize = 256 * 1024;
    QByteArray payload;
    payload.reserve(kSize);
    for (int i = 0; i < kSize; ++i) {
        payload.append(static_cast<char>(i & 0xFF));
    }

    // Write in chunks so the kernel/socat pipe doesn't overflow.
    constexpr int kChunk = 4096;
    for (int off = 0; off < kSize; off += kChunk) {
        REQUIRE(a.write(payload.mid(off, kChunk)) == DriverErrorCode::Success);
        // Pump events frequently so the receive side drains in parallel.
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while (received.size() < kSize && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }

    REQUIRE(received.size() == kSize);
    REQUIRE(received == payload);

    a.close();
    b.close();
}

TEST_CASE("serial loopback: mid-run socat disconnect triggers error", "[integration][serial][socat]") {
    if (!SocatVirtualPair::isAvailable()) {
        SKIP("socat executable not found on PATH; install socat to run this test");
    }
    auto pair = std::make_unique<SocatVirtualPair>();

    SerialDriver a{cfgFor(pair->side0())};
    SerialDriver b{cfgFor(pair->side1())};

    REQUIRE(a.open() == DriverErrorCode::Success);
    REQUIRE(b.open() == DriverErrorCode::Success);
    REQUIRE(waitForState(a, DriverState::Open));
    REQUIRE(waitForState(b, DriverState::Open));
    REQUIRE(a.start() == DriverErrorCode::Success);
    REQUIRE(b.start() == DriverErrorCode::Success);
    REQUIRE(waitForState(a, DriverState::Running));
    REQUIRE(waitForState(b, DriverState::Running));

    QSignalSpy errA(&a, &DriverInterface::errorOccurred);
    QSignalSpy errB(&b, &DriverInterface::errorOccurred);

    pair->killNow();

    // Kicking the drivers with a tiny write may help Qt notice the
    // disconnect on some kernel configurations; otherwise the readyRead
    // cycle will surface the error on its own.
    (void)a.write(QByteArray{"x", 1});

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while ((a.state() != DriverState::Error || b.state() != DriverState::Error) &&
           std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }

    REQUIRE(a.state() == DriverState::Error);
    REQUIRE(b.state() == DriverState::Error);
    REQUIRE(errA.count() >= 1);
    REQUIRE(errB.count() >= 1);

    a.close();
    b.close();
}
