// tests/benchmark/bench_driver_footprint.cpp
//
// Measures RSS deltas per spec §5.4.3:
// - Baseline RSS
// - Construct driver → RSS delta
// - 100 cycles of open/close → leak indicator
//
// Reads /proc/self/status to get VmRSS (Linux-only; matches the M3
// target platform).
#include "drivers/replay_driver.hpp"
#include "drivers/tcp_driver.hpp"
#include "drivers/udp_driver.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

using signalforge::drivers::DriverErrorCode;
using signalforge::drivers::DriverInterface;
using signalforge::drivers::DriverState;

namespace {

/// Read VmRSS from /proc/self/status in KB. Returns 0 on failure.
long long rssKb() {
    QFile f(QStringLiteral("/proc/self/status"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return 0;
    }
    QTextStream in(&f);
    QString line;
    while (in.readLineInto(&line)) {
        if (line.startsWith(QStringLiteral("VmRSS:"))) {
            const auto parts = line.split(QRegularExpression(QStringLiteral("\\s+")));
            for (const auto& p : parts) {
                bool ok = false;
                const long long v = p.toLongLong(&ok);
                if (ok) {
                    return v;
                }
            }
            return 0;
        }
    }
    return 0;
}

bool waitForState(DriverInterface& d, DriverState desired, int ms = 3000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (d.state() != desired && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    }
    return d.state() == desired;
}

void emitResult(const std::string& scenario, long long baseline_kb, long long post_construct_kb,
                long long after_cycles_kb, long long construct_delta_kb, long long cycle_growth_kb) {
    std::printf(
        R"({"scenario":"%s","baseline_kb":%lld,"post_construct_kb":%lld,"after_100_cycles_kb":%lld,"construct_delta_kb":%lld,"cycle_growth_kb":%lld})"
        "\n",
        scenario.c_str(), baseline_kb, post_construct_kb, after_cycles_kb, construct_delta_kb, cycle_growth_kb);
    std::fflush(stdout);
}

void benchReplay() {
    const long long base = rssKb();
    signalforge::drivers::ReplayConfig cfg;
    cfg.sessionFilePath = QStringLiteral("tests/integration/fixtures/minimal_session.sfreplay");
    auto d = std::make_unique<signalforge::drivers::ReplayDriver>(cfg);
    const long long postCtor = rssKb();
    d.reset();

    // 100 cycles of open/close.
    for (int i = 0; i < 100; ++i) {
        signalforge::drivers::ReplayDriver rd{cfg};
        if (rd.open() != DriverErrorCode::Success)
            break;
        waitForState(rd, DriverState::Open);
        rd.close();
        waitForState(rd, DriverState::Idle);
    }
    const long long afterCycles = rssKb();
    emitResult("replay_footprint", base, postCtor, afterCycles, postCtor - base, afterCycles - postCtor);
}

void benchTcp() {
    const long long base = rssKb();
    signalforge::drivers::TcpConfig cfg;
    cfg.host = QStringLiteral("127.0.0.1");
    cfg.port = 59993;
    cfg.connectTimeout = std::chrono::milliseconds{50};
    auto d = std::make_unique<signalforge::drivers::TcpDriver>(cfg);
    const long long postCtor = rssKb();
    d.reset();

    for (int i = 0; i < 100; ++i) {
        signalforge::drivers::TcpDriver td{cfg};
        td.open();
        waitForState(td, DriverState::Error, 500);  // unreachable — falls to Error
        td.close();
        waitForState(td, DriverState::Idle, 500);
    }
    const long long afterCycles = rssKb();
    emitResult("tcp_footprint", base, postCtor, afterCycles, postCtor - base, afterCycles - postCtor);
}

void benchUdp() {
    const long long base = rssKb();
    signalforge::drivers::UdpConfig cfg;
    cfg.localBindAddress = QStringLiteral("127.0.0.1");
    cfg.localBindPort = 0;
    auto d = std::make_unique<signalforge::drivers::UdpDriver>(cfg);
    const long long postCtor = rssKb();
    d.reset();

    for (int i = 0; i < 100; ++i) {
        signalforge::drivers::UdpDriver ud{cfg};
        ud.open();
        waitForState(ud, DriverState::Open, 500);
        ud.close();
        waitForState(ud, DriverState::Idle, 500);
    }
    const long long afterCycles = rssKb();
    emitResult("udp_footprint", base, postCtor, afterCycles, postCtor - base, afterCycles - postCtor);
}

}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    signalforge::frame::registerMetatypes();
    signalforge::drivers::registerMetatypes();

    benchReplay();
    benchTcp();
    benchUdp();
    return 0;
}
