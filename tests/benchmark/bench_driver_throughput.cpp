// tests/benchmark/bench_driver_throughput.cpp
//
// Measures sustained receive rate for each concrete driver per spec
// §5.4.1. Each scenario runs for ~`kDurationSec` seconds (default 10),
// writing as fast as write() accepts. Receiver counts bytes (and
// frames) delivered via frameReceived.
//
// Output is a single JSON object per scenario, one scenario per line.
// The driver script at run_baselines.sh parses these into
// results/M3-baseline.md.
//
// No unit-test integration — executed manually or by run_baselines.sh.
#include "drivers/serial_driver.hpp"
#include "drivers/tcp_driver.hpp"
#include "drivers/udp_driver.hpp"
#include "frame/raw_frame.hpp"
#include "tests/integration/echo_server_fixture.hpp"
#include "tests/integration/socat_fixture.hpp"

#include <QByteArray>
#include <QCoreApplication>
#include <QHostAddress>
#include <QObject>
#include <QUdpSocket>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

using namespace std::chrono_literals;
using signalforge::drivers::DriverErrorCode;
using signalforge::drivers::DriverInterface;
using signalforge::drivers::DriverState;

namespace {

constexpr int kDurationSec = 10;

bool waitForState(DriverInterface& d, DriverState desired, int ms = 3000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (d.state() != desired && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    return d.state() == desired;
}

/// Emit one scenario as a JSON line to stdout.
void emitResult(const std::string& scenario, double bytesPerSec, std::uint64_t framesTotal, std::uint64_t bytesTotal,
                std::uint64_t failures) {
    std::printf(R"({"scenario":"%s","bytes_per_sec":%.1f,"frames_total":%llu,"bytes_total":%llu,"failures":%llu})"
                "\n",
                scenario.c_str(), bytesPerSec, static_cast<unsigned long long>(framesTotal),
                static_cast<unsigned long long>(bytesTotal), static_cast<unsigned long long>(failures));
    std::fflush(stdout);
}

void benchTcp() {
    signalforge::test::EchoServer server;
    if (!server.listen()) {
        emitResult("tcp_localhost_echo", 0.0, 0, 0, 1);
        return;
    }
    const quint16 port = server.port();

    signalforge::drivers::TcpConfig cfg;
    cfg.host = QStringLiteral("127.0.0.1");
    cfg.port = port;
    cfg.connectTimeout = 2000ms;

    signalforge::drivers::TcpDriver d{cfg};
    if (d.open() != DriverErrorCode::Success || !waitForState(d, DriverState::Open) ||
        d.start() != DriverErrorCode::Success || !waitForState(d, DriverState::Running)) {
        emitResult("tcp_localhost_echo", 0.0, 0, 0, 1);
        return;
    }

    std::uint64_t bytesReceived = 0;
    std::uint64_t framesReceived = 0;
    QObject sink;
    QObject::connect(&d, &DriverInterface::frameReceived, &sink, [&](const signalforge::frame::RawFrame& f) {
        bytesReceived += static_cast<std::uint64_t>(f.payload.size());
        ++framesReceived;
    });

    const QByteArray chunk(4096, '\xAB');
    const auto start = std::chrono::steady_clock::now();
    const auto deadline = start + std::chrono::seconds(kDurationSec);
    while (std::chrono::steady_clock::now() < deadline) {
        d.write(chunk);
        QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
    }
    // Drain a bit so late echoes arrive.
    const auto drainDeadline = std::chrono::steady_clock::now() + 500ms;
    while (std::chrono::steady_clock::now() < drainDeadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

    const auto stats = d.statistics();
    emitResult("tcp_localhost_echo", static_cast<double>(bytesReceived) / elapsed, framesReceived, bytesReceived,
               stats.tx.failures);

    d.close();
    waitForState(d, DriverState::Idle);
}

void benchUdp() {
    signalforge::drivers::UdpConfig rxCfg;
    rxCfg.localBindAddress = QStringLiteral("127.0.0.1");
    rxCfg.localBindPort = 0;  // OS-assigned

    // Pick a receive port by briefly binding a probe.
    quint16 port = 0;
    {
        QUdpSocket probe;
        probe.bind(QHostAddress(QHostAddress::LocalHost), 0);
        port = probe.localPort();
    }
    rxCfg.localBindPort = port;

    signalforge::drivers::UdpConfig txCfg;
    txCfg.remoteHost = QStringLiteral("127.0.0.1");
    txCfg.remotePort = port;

    signalforge::drivers::UdpDriver rx{rxCfg};
    signalforge::drivers::UdpDriver tx{txCfg};

    if (rx.open() != DriverErrorCode::Success || tx.open() != DriverErrorCode::Success ||
        !waitForState(rx, DriverState::Open) || !waitForState(tx, DriverState::Open) ||
        rx.start() != DriverErrorCode::Success || tx.start() != DriverErrorCode::Success ||
        !waitForState(rx, DriverState::Running) || !waitForState(tx, DriverState::Running)) {
        emitResult("udp_localhost_unicast_1kb", 0.0, 0, 0, 1);
        return;
    }

    std::uint64_t framesReceived = 0;
    std::uint64_t bytesReceived = 0;
    QObject sink;
    QObject::connect(&rx, &DriverInterface::frameReceived, &sink, [&](const signalforge::frame::RawFrame& f) {
        ++framesReceived;
        bytesReceived += static_cast<std::uint64_t>(f.payload.size());
    });

    const QByteArray dgm(1024, '\xCD');
    const auto start = std::chrono::steady_clock::now();
    const auto deadline = start + std::chrono::seconds(kDurationSec);
    while (std::chrono::steady_clock::now() < deadline) {
        tx.write(dgm);
        QCoreApplication::processEvents(QEventLoop::AllEvents, 0);
    }
    const auto drainDeadline = std::chrono::steady_clock::now() + 200ms;
    while (std::chrono::steady_clock::now() < drainDeadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

    const auto stats = tx.statistics();
    emitResult("udp_localhost_unicast_1kb", static_cast<double>(framesReceived) / elapsed, framesReceived,
               bytesReceived, stats.tx.failures);

    rx.close();
    tx.close();
    waitForState(rx, DriverState::Idle);
    waitForState(tx, DriverState::Idle);
}

void benchSerial(qint32 baud, const std::string& label) {
    if (!signalforge::test::SocatVirtualPair::isAvailable()) {
        emitResult(label, 0.0, 0, 0, 1);
        return;
    }
    signalforge::test::SocatVirtualPair pair;

    auto makeCfg = [baud](const QString& dev) {
        signalforge::drivers::SerialConfig c;
        c.device = dev;
        c.baudRate = baud;
        c.dataBits = 8;
        c.parity = QStringLiteral("none");
        c.stopBits = 1;
        c.flowControl = QStringLiteral("none");
        return c;
    };
    signalforge::drivers::SerialDriver a{makeCfg(pair.side0())};
    signalforge::drivers::SerialDriver b{makeCfg(pair.side1())};

    if (a.open() != DriverErrorCode::Success || b.open() != DriverErrorCode::Success ||
        !waitForState(a, DriverState::Open) || !waitForState(b, DriverState::Open) ||
        a.start() != DriverErrorCode::Success || b.start() != DriverErrorCode::Success ||
        !waitForState(a, DriverState::Running) || !waitForState(b, DriverState::Running)) {
        emitResult(label, 0.0, 0, 0, 1);
        return;
    }

    std::uint64_t bytesAtB = 0;
    std::uint64_t framesAtB = 0;
    QObject sink;
    QObject::connect(&b, &DriverInterface::frameReceived, &sink, [&](const signalforge::frame::RawFrame& f) {
        bytesAtB += static_cast<std::uint64_t>(f.payload.size());
        ++framesAtB;
    });

    const QByteArray chunk(256, '\xEE');
    const auto start = std::chrono::steady_clock::now();
    const auto deadline = start + std::chrono::seconds(kDurationSec);
    while (std::chrono::steady_clock::now() < deadline) {
        a.write(chunk);
        QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
    }
    const auto drainDeadline = std::chrono::steady_clock::now() + 500ms;
    while (std::chrono::steady_clock::now() < drainDeadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

    emitResult(label, static_cast<double>(bytesAtB) / elapsed, framesAtB, bytesAtB, a.statistics().tx.failures);

    a.close();
    b.close();
    waitForState(a, DriverState::Idle);
    waitForState(b, DriverState::Idle);
}

}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    signalforge::frame::registerMetatypes();
    signalforge::drivers::registerMetatypes();

    benchTcp();
    benchUdp();
    benchSerial(115200, "serial_115200_socat");
    benchSerial(921600, "serial_921600_socat");
    return 0;
}
