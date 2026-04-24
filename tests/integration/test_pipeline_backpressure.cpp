// tests/integration/test_pipeline_backpressure.cpp
//
// Exercises the pipeline metrics under realistic driver → sink flow.
//
// Design note: with inline drain (current implementation), a slow sink
// does NOT cause the MpscQueue watermark to cross 80% under normal
// driver emission — pending frames pile in Qt's worker-thread event
// queue rather than in the MpscQueue, because every `enqueueFrame` slot
// invocation drains the MPSC to empty before returning. This is an
// explicit latency-vs-backpressure-observability trade-off documented
// in `.claude/M4-progress.md` (S7 entry). Watermark threshold crossing
// is still exercised at the unit level via `WatermarkTracker::observe`
// in M2's backpressure_test and via the capacity=0 drop path in S4.
//
// What this integration test verifies:
// - Slow sink does not lose frames under burst load.
// - Per-driver metrics tick correctly for received frames.
// - No false drops when capacity is large and producer is slower than
//   the sink's steady-state rate.
#include "drivers/udp_driver.hpp"
#include "frame/raw_frame.hpp"
#include "observability/metrics.hpp"
#include "pipeline/frame_pipeline.hpp"
#include "pipeline/frame_sink.hpp"

#include <QByteArray>
#include <QCoreApplication>
#include <QHostAddress>
#include <QThread>
#include <QUdpSocket>
#include <QtTest/QtTest>
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <memory>

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

class CoreAppHolder {
public:
    CoreAppHolder() {
        signalforge::frame::registerMetatypes();
        signalforge::drivers::registerMetatypes();
        if (QCoreApplication::instance() == nullptr) {
            static int argc = 1;
            static char argv0[] = "test_pipeline_backpressure";
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

template <typename Pred> bool pumpUntil(Pred&& pred, int timeoutMs = 5000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (!pred() && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(5);
    }
    return pred();
}

/// Sink that sleeps briefly on each frame to simulate real processing
/// cost. 1 ms per frame means 1000 frames costs ~1 s — tuned so the
/// test stays quick but sink-work is non-trivial.
class SlowSink : public FrameSink {
public:
    void onFrame(const RawFrame&) override {
        ++frames;
        QThread::msleep(1);
    }
    [[nodiscard]] QString sinkName() const override {
        return QStringLiteral("slow-sink");
    }
    std::atomic<int> frames{0};
};

}  // namespace

TEST_CASE("pipeline backpressure: slow sink receives every frame from a burst",
          "[integration][pipeline][udp][backpressure]") {
    const quint16 portA = pickFreeLocalPort();
    const quint16 portB = pickFreeLocalPort();
    REQUIRE(portA != 0);
    REQUIRE(portB != 0);

    UdpConfig cfgA;
    cfgA.localBindAddress = QStringLiteral("127.0.0.1");
    cfgA.localBindPort = portA;

    UdpConfig cfgB;
    cfgB.localBindAddress = QStringLiteral("127.0.0.1");
    cfgB.localBindPort = portB;
    cfgB.remoteHost = QStringLiteral("127.0.0.1");
    cfgB.remotePort = portA;

    UdpDriver a{cfgA};
    UdpDriver b{cfgB};

    auto& reg = signalforge::observability::MetricsRegistry::instance();
    reg.clearForTesting();
    const QString driverId = QStringLiteral("udp:backpressure:") + QString::number(portA);
    PipelineConfig cfg;
    cfg.driverId = driverId;
    cfg.ingressCapacity = 10000;  // comfortably large so drops should not happen
    FramePipeline pipeline(cfg);
    pipeline.attachDriver(&a);

    auto slow = std::make_shared<SlowSink>();
    pipeline.addSink(slow);

    REQUIRE(a.open() == DriverErrorCode::Success);
    REQUIRE(b.open() == DriverErrorCode::Success);
    REQUIRE(waitForState(a, DriverState::Open));
    REQUIRE(waitForState(b, DriverState::Open));
    REQUIRE(a.start() == DriverErrorCode::Success);
    REQUIRE(b.start() == DriverErrorCode::Success);
    REQUIRE(waitForState(a, DriverState::Running));
    REQUIRE(waitForState(b, DriverState::Running));

    constexpr int kFrames = 100;
    for (int i = 0; i < kFrames; ++i) {
        REQUIRE(b.write(QByteArray(32, static_cast<char>(i & 0xFF))) == DriverErrorCode::Success);
    }

    REQUIRE(pumpUntil([&] { return slow->frames.load() >= kFrames; }, 5000));

    const auto stats = pipeline.stats();
    REQUIRE(stats.framesReceived >= kFrames);
    REQUIRE(stats.framesDropped == 0);

    // Verify the received counter metric matches.
    auto* rxMetric = reg.getOrCreate(QStringLiteral("pipeline_frames_received_") + driverId,
                                     signalforge::observability::MetricKind::Counter);
    REQUIRE(rxMetric->value() >= kFrames);

    a.close();
    b.close();
    REQUIRE(waitForState(a, DriverState::Idle));
    REQUIRE(waitForState(b, DriverState::Idle));
}
