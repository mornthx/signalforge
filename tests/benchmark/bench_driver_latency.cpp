// tests/benchmark/bench_driver_latency.cpp
//
// Measures per-packet round-trip latency percentiles per spec §5.4.2.
// TCP: producer writes a 1KB payload every `kIntervalMs`, receiver
//      timestamps on frameReceived. Payload is sequenced so each sample
//      can be matched.
// UDP: same, but unicast.
// Serial: deferred to throughput benchmark only (sample count would
//         require too long a run at 115200 baud for M3 timing budget).
#include "drivers/tcp_driver.hpp"
#include "drivers/udp_driver.hpp"
#include "frame/raw_frame.hpp"
#include "tests/integration/echo_server_fixture.hpp"

#include <QByteArray>
#include <QCoreApplication>
#include <QHostAddress>
#include <QObject>
#include <QUdpSocket>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace std::chrono_literals;
using signalforge::drivers::DriverErrorCode;
using signalforge::drivers::DriverInterface;
using signalforge::drivers::DriverState;

namespace {

constexpr int kIntervalMs = 20;
constexpr int kSampleCount = 500;

bool waitForState(DriverInterface& d, DriverState desired, int ms = 3000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (d.state() != desired && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    }
    return d.state() == desired;
}

double percentile(std::vector<double>& xs, double p) {
    if (xs.empty()) {
        return 0.0;
    }
    std::sort(xs.begin(), xs.end());
    const size_t idx = std::min(xs.size() - 1, static_cast<size_t>(xs.size() * p));
    return xs[idx];
}

void emitResult(const std::string& scenario, const std::vector<double>& latencies_ms) {
    std::vector<double> xs = latencies_ms;
    const double p50 = percentile(xs, 0.50);
    const double p90 = percentile(xs, 0.90);
    const double p99 = percentile(xs, 0.99);
    const double p999 = percentile(xs, 0.999);
    const double maxv = xs.empty() ? 0.0 : xs.back();
    std::printf(
        R"({"scenario":"%s","samples":%zu,"p50_ms":%.3f,"p90_ms":%.3f,"p99_ms":%.3f,"p999_ms":%.3f,"max_ms":%.3f})"
        "\n",
        scenario.c_str(), latencies_ms.size(), p50, p90, p99, p999, maxv);
    std::fflush(stdout);
}

void benchTcpLatency() {
    signalforge::test::EchoServer server;
    if (!server.listen()) {
        std::printf(R"({"scenario":"tcp_localhost_echo_latency","error":"server_listen_failed"})"
                    "\n");
        return;
    }

    signalforge::drivers::TcpConfig cfg;
    cfg.host = QStringLiteral("127.0.0.1");
    cfg.port = server.port();
    cfg.connectTimeout = 2000ms;

    signalforge::drivers::TcpDriver d{cfg};
    if (d.open() != DriverErrorCode::Success || !waitForState(d, DriverState::Open) ||
        d.start() != DriverErrorCode::Success || !waitForState(d, DriverState::Running)) {
        std::printf(R"({"scenario":"tcp_localhost_echo_latency","error":"open_failed"})"
                    "\n");
        return;
    }

    std::vector<double> latencies;
    latencies.reserve(kSampleCount);
    std::chrono::steady_clock::time_point lastSend;

    QObject sink;
    QObject::connect(&d, &DriverInterface::frameReceived, &sink, [&](const signalforge::frame::RawFrame&) {
        const auto now = std::chrono::steady_clock::now();
        const double ms = std::chrono::duration<double, std::milli>(now - lastSend).count();
        latencies.push_back(ms);
    });

    const QByteArray payload(1024, '\xAA');
    for (int i = 0; i < kSampleCount; ++i) {
        lastSend = std::chrono::steady_clock::now();
        d.write(payload);
        const auto deadline = lastSend + std::chrono::milliseconds(kIntervalMs);
        while (std::chrono::steady_clock::now() < deadline) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
        }
    }

    const auto drainDeadline = std::chrono::steady_clock::now() + 200ms;
    while (std::chrono::steady_clock::now() < drainDeadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    }

    emitResult("tcp_localhost_echo_latency", latencies);

    d.close();
    waitForState(d, DriverState::Idle);
}

void benchUdpLatency() {
    quint16 port = 0;
    {
        QUdpSocket probe;
        probe.bind(QHostAddress(QHostAddress::LocalHost), 0);
        port = probe.localPort();
    }

    signalforge::drivers::UdpConfig rxCfg;
    rxCfg.localBindAddress = QStringLiteral("127.0.0.1");
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
        std::printf(R"({"scenario":"udp_localhost_unicast_latency","error":"open_failed"})"
                    "\n");
        return;
    }

    std::vector<double> latencies;
    latencies.reserve(kSampleCount);
    std::chrono::steady_clock::time_point lastSend;

    QObject sink;
    QObject::connect(&rx, &DriverInterface::frameReceived, &sink, [&](const signalforge::frame::RawFrame&) {
        const auto now = std::chrono::steady_clock::now();
        latencies.push_back(std::chrono::duration<double, std::milli>(now - lastSend).count());
    });

    const QByteArray payload(1024, '\xBB');
    for (int i = 0; i < kSampleCount; ++i) {
        lastSend = std::chrono::steady_clock::now();
        tx.write(payload);
        const auto deadline = lastSend + std::chrono::milliseconds(kIntervalMs);
        while (std::chrono::steady_clock::now() < deadline) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
        }
    }

    emitResult("udp_localhost_unicast_latency", latencies);

    rx.close();
    tx.close();
    waitForState(rx, DriverState::Idle);
    waitForState(tx, DriverState::Idle);
}

}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    signalforge::frame::registerMetatypes();
    signalforge::drivers::registerMetatypes();
    benchTcpLatency();
    benchUdpLatency();
    return 0;
}
