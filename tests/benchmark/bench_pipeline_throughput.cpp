// tests/benchmark/bench_pipeline_throughput.cpp
//
// Measures the overhead introduced by inserting a FramePipeline
// between a driver and the final sink, versus the direct-signal
// baseline. Runs two UDP localhost scenarios back to back:
//
// 1. Direct: UdpDriver.frameReceived → a lambda counter via Qt
//    signal (connected on the main thread with Qt::QueuedConnection —
//    the typical consumer pattern).
// 2. Pipelined: UdpDriver → FramePipeline → FrameSink counter.
//    Adds the pipeline's QueuedConnection hop + MPSC push/drain +
//    sink fanout.
//
// Per M4 spec §7.3 (HALT trigger 3), the pipeline adds at most 10 %
// overhead vs the direct path; otherwise the benchmark investigates
// per spec §5.5 or HALTs.
//
// JSON lines are emitted to stdout, one per scenario + one summary
// object at the end. `run_baselines.sh` collates them into
// `tests/benchmark/results/M4-baseline.md`.
#include "drivers/udp_driver.hpp"
#include "frame/raw_frame.hpp"
#include "pipeline/frame_pipeline.hpp"
#include "pipeline/frame_sink.hpp"

#include <QByteArray>
#include <QCoreApplication>
#include <QEventLoop>
#include <QHostAddress>
#include <QObject>
#include <QThread>
#include <QUdpSocket>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <memory>

using namespace std::chrono_literals;
using signalforge::drivers::DriverErrorCode;
using signalforge::drivers::DriverInterface;
using signalforge::drivers::DriverState;
using signalforge::drivers::UdpConfig;
using signalforge::drivers::UdpDriver;
using signalforge::frame::RawFrame;
using signalforge::pipeline::FramePipeline;
using signalforge::pipeline::FrameSink;
using signalforge::pipeline::PipelineConfig;

namespace {

constexpr int kDurationSec = 10;
constexpr int kPayloadBytes = 1024;

bool waitForState(DriverInterface& d, DriverState desired, int ms = 3000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (d.state() != desired && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    return d.state() == desired;
}

quint16 pickFreeLocalPort() {
    QUdpSocket probe;
    if (!probe.bind(QHostAddress(QHostAddress::LocalHost), 0))
        return 0;
    const quint16 p = probe.localPort();
    probe.close();
    return p;
}

/// Counter sink for the pipelined scenario.
class CounterSink : public FrameSink {
public:
    void onFrame(const RawFrame&) override {
        frames.fetch_add(1, std::memory_order_relaxed);
    }
    [[nodiscard]] QString sinkName() const override {
        return QStringLiteral("bench-counter");
    }
    std::atomic<std::uint64_t> frames{0};
};

struct ScenarioResult {
    std::uint64_t framesReceived;
    double elapsedSec;
};

/// Drive a pre-wired receiver/producer pair for kDurationSec with the
/// producer spinning as fast as the driver accepts. The receiver's
/// counter is read from the returned result.
ScenarioResult runBenchmark(UdpDriver& producer, std::atomic<std::uint64_t>& rxCounter) {
    const QByteArray dgm(kPayloadBytes, '\xAB');
    const auto start = std::chrono::steady_clock::now();
    const auto deadline = start + std::chrono::seconds(kDurationSec);
    while (std::chrono::steady_clock::now() < deadline) {
        // Burst of writes, then process events briefly. Too much
        // batching starves the receiver's event loop; too little and
        // producer isn't saturating.
        for (int i = 0; i < 64; ++i) {
            producer.write(dgm);
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 0);
    }
    const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    // Final drain so late frames arrive before we sample.
    const auto drainDeadline = std::chrono::steady_clock::now() + 200ms;
    while (std::chrono::steady_clock::now() < drainDeadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    return {rxCounter.load(std::memory_order_relaxed), elapsed};
}

void emitResult(const char* scenario, std::uint64_t frames, double elapsedSec, std::uint64_t bytesTotal) {
    const double framesPerSec = frames / elapsedSec;
    const double bytesPerSec = static_cast<double>(bytesTotal) / elapsedSec;
    std::printf(
        R"({"scenario":"%s","frames_total":%llu,"bytes_total":%llu,"elapsed_sec":%.3f,"frames_per_sec":%.1f,"bytes_per_sec":%.1f})"
        "\n",
        scenario, static_cast<unsigned long long>(frames), static_cast<unsigned long long>(bytesTotal), elapsedSec,
        framesPerSec, bytesPerSec);
    std::fflush(stdout);
}

void emitSummary(double directFps, double pipelinedFps, double overheadPct, const char* verdict) {
    std::printf(
        R"({"scenario":"overhead_summary","direct_frames_per_sec":%.1f,"pipelined_frames_per_sec":%.1f,"overhead_pct":%.2f,"verdict":"%s"})"
        "\n",
        directFps, pipelinedFps, overheadPct, verdict);
    std::fflush(stdout);
}

ScenarioResult benchDirect() {
    const quint16 portRx = pickFreeLocalPort();
    const quint16 portTx = pickFreeLocalPort();
    if (portRx == 0 || portTx == 0)
        return {0, 0.0};

    UdpConfig rxCfg;
    rxCfg.localBindAddress = QStringLiteral("127.0.0.1");
    rxCfg.localBindPort = portRx;

    UdpConfig txCfg;
    txCfg.localBindAddress = QStringLiteral("127.0.0.1");
    txCfg.localBindPort = portTx;
    txCfg.remoteHost = QStringLiteral("127.0.0.1");
    txCfg.remotePort = portRx;

    UdpDriver rx{rxCfg};
    UdpDriver tx{txCfg};

    std::atomic<std::uint64_t> counter{0};
    QObject sink;
    QObject::connect(
        &rx, &DriverInterface::frameReceived, &sink,
        [&counter](const RawFrame&) { counter.fetch_add(1, std::memory_order_relaxed); }, Qt::QueuedConnection);

    if (rx.open() != DriverErrorCode::Success || tx.open() != DriverErrorCode::Success)
        return {0, 0.0};
    waitForState(rx, DriverState::Open);
    waitForState(tx, DriverState::Open);
    if (rx.start() != DriverErrorCode::Success || tx.start() != DriverErrorCode::Success)
        return {0, 0.0};
    waitForState(rx, DriverState::Running);
    waitForState(tx, DriverState::Running);

    ScenarioResult r = runBenchmark(tx, counter);

    rx.close();
    tx.close();
    waitForState(rx, DriverState::Idle);
    waitForState(tx, DriverState::Idle);
    return r;
}

ScenarioResult benchPipelined() {
    const quint16 portRx = pickFreeLocalPort();
    const quint16 portTx = pickFreeLocalPort();
    if (portRx == 0 || portTx == 0)
        return {0, 0.0};

    UdpConfig rxCfg;
    rxCfg.localBindAddress = QStringLiteral("127.0.0.1");
    rxCfg.localBindPort = portRx;

    UdpConfig txCfg;
    txCfg.localBindAddress = QStringLiteral("127.0.0.1");
    txCfg.localBindPort = portTx;
    txCfg.remoteHost = QStringLiteral("127.0.0.1");
    txCfg.remotePort = portRx;

    UdpDriver rx{rxCfg};
    UdpDriver tx{txCfg};

    PipelineConfig cfg;
    cfg.driverId = QStringLiteral("bench:pipelined");
    FramePipeline pipeline(cfg);
    pipeline.attachDriver(&rx);

    auto sink = std::make_shared<CounterSink>();
    pipeline.addSink(sink);

    if (rx.open() != DriverErrorCode::Success || tx.open() != DriverErrorCode::Success)
        return {0, 0.0};
    waitForState(rx, DriverState::Open);
    waitForState(tx, DriverState::Open);
    if (rx.start() != DriverErrorCode::Success || tx.start() != DriverErrorCode::Success)
        return {0, 0.0};
    waitForState(rx, DriverState::Running);
    waitForState(tx, DriverState::Running);

    ScenarioResult r = runBenchmark(tx, sink->frames);

    rx.close();
    tx.close();
    waitForState(rx, DriverState::Idle);
    waitForState(tx, DriverState::Idle);
    return r;
}

}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    signalforge::frame::registerMetatypes();
    signalforge::drivers::registerMetatypes();

    std::fprintf(stderr, "bench_pipeline_throughput: direct scenario (%d s)...\n", kDurationSec);
    const auto direct = benchDirect();
    emitResult("udp_direct", direct.framesReceived, direct.elapsedSec,
               direct.framesReceived * static_cast<std::uint64_t>(kPayloadBytes));

    std::fprintf(stderr, "bench_pipeline_throughput: pipelined scenario (%d s)...\n", kDurationSec);
    const auto pipelined = benchPipelined();
    emitResult("udp_pipelined", pipelined.framesReceived, pipelined.elapsedSec,
               pipelined.framesReceived * static_cast<std::uint64_t>(kPayloadBytes));

    if (direct.framesReceived == 0 || pipelined.framesReceived == 0) {
        emitSummary(0.0, 0.0, 0.0, "benchmark_failed");
        return 1;
    }

    const double directFps = direct.framesReceived / direct.elapsedSec;
    const double pipelinedFps = pipelined.framesReceived / pipelined.elapsedSec;
    const double overheadPct = 100.0 * (directFps - pipelinedFps) / directFps;
    const char* verdict = overheadPct <= 10.0 ? "within_threshold" : "exceeds_threshold";
    emitSummary(directFps, pipelinedFps, overheadPct, verdict);
    return 0;
}
