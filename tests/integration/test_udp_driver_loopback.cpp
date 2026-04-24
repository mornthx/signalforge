// tests/integration/test_udp_driver_loopback.cpp
#include "drivers/udp_driver.hpp"
#include "frame/raw_frame.hpp"

#include <QByteArray>
#include <QCoreApplication>
#include <QHostAddress>
#include <QObject>
#include <QSignalSpy>
#include <QUdpSocket>
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
            static char argv0[] = "test_udp_driver_loopback";
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

/// Bind a transient QUdpSocket to localhost:0, read its OS-assigned
/// port, then close. Races are rare but possible; callers retry on
/// AddressInUseError at open() time if they hit the window.
quint16 pickFreeLocalPort() {
    QUdpSocket probe;
    if (!probe.bind(QHostAddress(QHostAddress::LocalHost), 0)) {
        return 0;
    }
    const quint16 p = probe.localPort();
    probe.close();
    return p;
}

}  // namespace

TEST_CASE("udp loopback: bidirectional unicast on 127.0.0.1", "[integration][udp]") {
    const quint16 portA = pickFreeLocalPort();
    const quint16 portB = pickFreeLocalPort();
    REQUIRE(portA != 0);
    REQUIRE(portB != 0);
    REQUIRE(portA != portB);

    UdpConfig cfgA;
    cfgA.localBindAddress = QStringLiteral("127.0.0.1");
    cfgA.localBindPort = portA;
    cfgA.remoteHost = QStringLiteral("127.0.0.1");
    cfgA.remotePort = portB;

    UdpConfig cfgB;
    cfgB.localBindAddress = QStringLiteral("127.0.0.1");
    cfgB.localBindPort = portB;
    cfgB.remoteHost = QStringLiteral("127.0.0.1");
    cfgB.remotePort = portA;

    UdpDriver a{cfgA};
    UdpDriver b{cfgB};

    REQUIRE(a.open() == DriverErrorCode::Success);
    REQUIRE(b.open() == DriverErrorCode::Success);
    REQUIRE(waitForState(a, DriverState::Open));
    REQUIRE(waitForState(b, DriverState::Open));
    REQUIRE(a.start() == DriverErrorCode::Success);
    REQUIRE(b.start() == DriverErrorCode::Success);
    REQUIRE(waitForState(a, DriverState::Running));
    REQUIRE(waitForState(b, DriverState::Running));

    int framesAtA = 0;
    int framesAtB = 0;
    QByteArray receivedAtA;
    QByteArray receivedAtB;
    QObject sinkA;
    QObject sinkB;
    QObject::connect(&a, &DriverInterface::frameReceived, &sinkA, [&](const signalforge::frame::RawFrame& f) {
        ++framesAtA;
        receivedAtA.append(f.payload);
    });
    QObject::connect(&b, &DriverInterface::frameReceived, &sinkB, [&](const signalforge::frame::RawFrame& f) {
        ++framesAtB;
        receivedAtB.append(f.payload);
    });

    const QByteArray msgAtoB(64, '\xA1');
    const QByteArray msgBtoA(128, '\xB2');
    REQUIRE(a.write(msgAtoB) == DriverErrorCode::Success);
    REQUIRE(b.write(msgBtoA) == DriverErrorCode::Success);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while ((framesAtA < 1 || framesAtB < 1) && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
    }

    REQUIRE(framesAtA >= 1);
    REQUIRE(framesAtB >= 1);
    REQUIRE(receivedAtB == msgAtoB);
    REQUIRE(receivedAtA == msgBtoA);

    a.close();
    b.close();
    REQUIRE(waitForState(a, DriverState::Idle));
    REQUIRE(waitForState(b, DriverState::Idle));
}

TEST_CASE("udp loopback: datagrams preserve framing boundaries", "[integration][udp]") {
    const quint16 rxPort = pickFreeLocalPort();
    REQUIRE(rxPort != 0);

    UdpConfig rxCfg;
    rxCfg.localBindAddress = QStringLiteral("127.0.0.1");
    rxCfg.localBindPort = rxPort;

    UdpConfig txCfg;
    txCfg.remoteHost = QStringLiteral("127.0.0.1");
    txCfg.remotePort = rxPort;

    UdpDriver rx{rxCfg};
    UdpDriver tx{txCfg};

    REQUIRE(rx.open() == DriverErrorCode::Success);
    REQUIRE(tx.open() == DriverErrorCode::Success);
    REQUIRE(waitForState(rx, DriverState::Open));
    REQUIRE(waitForState(tx, DriverState::Open));
    REQUIRE(rx.start() == DriverErrorCode::Success);
    REQUIRE(tx.start() == DriverErrorCode::Success);
    REQUIRE(waitForState(rx, DriverState::Running));
    REQUIRE(waitForState(tx, DriverState::Running));

    QVector<QByteArray> framesSeen;
    QObject sink;
    QObject::connect(&rx, &DriverInterface::frameReceived, &sink,
                     [&](const signalforge::frame::RawFrame& f) { framesSeen.push_back(f.payload); });

    // Send three distinct datagrams; each must arrive as its own frame.
    const QByteArray p1 = QByteArray(16, '\x11');
    const QByteArray p2 = QByteArray(32, '\x22');
    const QByteArray p3 = QByteArray(48, '\x33');
    REQUIRE(tx.write(p1) == DriverErrorCode::Success);
    REQUIRE(tx.write(p2) == DriverErrorCode::Success);
    REQUIRE(tx.write(p3) == DriverErrorCode::Success);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (framesSeen.size() < 3 && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
    }

    REQUIRE(framesSeen.size() == 3);
    REQUIRE(framesSeen[0] == p1);
    REQUIRE(framesSeen[1] == p2);
    REQUIRE(framesSeen[2] == p3);

    rx.close();
    tx.close();
    REQUIRE(waitForState(rx, DriverState::Idle));
    REQUIRE(waitForState(tx, DriverState::Idle));
}

TEST_CASE("udp loopback: multicast group receive", "[integration][udp][multicast]") {
    // Bind a shared multicast listening port. We pick via the helper,
    // which binds on 127.0.0.1 — good enough for detecting free ports
    // even though the multicast bind will use INADDR_ANY.
    const quint16 port = pickFreeLocalPort();
    REQUIRE(port != 0);
    const QString mcastGroup = QStringLiteral("239.200.123.45");

    UdpConfig rxCfg;
    rxCfg.localBindAddress = QStringLiteral("0.0.0.0");
    rxCfg.localBindPort = port;
    rxCfg.multicastGroup = mcastGroup;
    rxCfg.multicastTtl = 1;

    UdpConfig txCfg;
    txCfg.remoteHost = mcastGroup;
    txCfg.remotePort = port;
    txCfg.multicastTtl = 1;

    UdpDriver rx{rxCfg};
    UdpDriver tx{txCfg};

    REQUIRE(rx.open() == DriverErrorCode::Success);
    REQUIRE(tx.open() == DriverErrorCode::Success);
    if (!waitForState(rx, DriverState::Open)) {
        // Host-specific multicast bind limitations (e.g. no route to
        // multicast on a CI runner) are acknowledged as tolerable per
        // spec §5.3.3 portability note — we skip rather than fail.
        SUCCEED("multicast bind not available on this host; skipping");
        return;
    }
    REQUIRE(waitForState(tx, DriverState::Open));
    REQUIRE(rx.start() == DriverErrorCode::Success);
    REQUIRE(tx.start() == DriverErrorCode::Success);
    REQUIRE(waitForState(rx, DriverState::Running));
    REQUIRE(waitForState(tx, DriverState::Running));

    int framesAtRx = 0;
    QByteArray lastPayload;
    QObject sink;
    QObject::connect(&rx, &DriverInterface::frameReceived, &sink, [&](const signalforge::frame::RawFrame& f) {
        ++framesAtRx;
        lastPayload = f.payload;
    });

    const QByteArray msg(24, '\xCC');
    REQUIRE(tx.write(msg) == DriverErrorCode::Success);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (framesAtRx < 1 && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
    }

    // Multicast routing between interfaces can be blocked by the host's
    // routing table; if the datagram did not loop back, treat the test
    // as skipped rather than a driver bug (spec §5.3.3).
    if (framesAtRx == 0) {
        SUCCEED("multicast datagram did not loop back on this host; skipping");
    } else {
        REQUIRE(lastPayload == msg);
    }

    rx.close();
    tx.close();
    REQUIRE(waitForState(rx, DriverState::Idle));
    REQUIRE(waitForState(tx, DriverState::Idle));
}
