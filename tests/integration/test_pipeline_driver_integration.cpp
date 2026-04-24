// tests/integration/test_pipeline_driver_integration.cpp
//
// Integration of FramePipeline + real drivers (ReplayDriver skeleton +
// two localhost UdpDrivers). Exercises the Qt::QueuedConnection hop
// from a driver's IO thread into the pipeline worker, sink fanout
// across a real-thread boundary, and clean lifecycle teardown.
#include "drivers/replay_driver.hpp"
#include "drivers/udp_driver.hpp"
#include "frame/raw_frame.hpp"
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
#include <vector>

using signalforge::drivers::DriverError;
using signalforge::drivers::DriverErrorCode;
using signalforge::drivers::DriverInterface;
using signalforge::drivers::DriverState;
using signalforge::drivers::ReplayConfig;
using signalforge::drivers::ReplayDriver;
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
            static char argv0[] = "test_pipeline_driver_integration";
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

template <typename Pred> bool pumpUntil(Pred&& pred, int timeoutMs = 3000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (!pred() && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(5);
    }
    return pred();
}

class RecordingSink : public FrameSink {
public:
    explicit RecordingSink(QString name) : name_(std::move(name)) {}
    void onFrame(const RawFrame& f) override {
        ++frames;
        (void)f;
    }
    void onLifecycle(DriverState s) override {
        ++lifecycle;
        // Atomic flags are thread-safe across the pipeline-worker /
        // main-thread boundary without needing a mutex.
        switch (s) {
        case DriverState::Opening:
            sawOpening = true;
            break;
        case DriverState::Open:
            sawOpen = true;
            break;
        case DriverState::Running:
            sawRunning = true;
            break;
        case DriverState::Stopping:
            sawStopping = true;
            break;
        case DriverState::Closing:
            sawClosing = true;
            break;
        case DriverState::Idle:
            sawIdle = true;
            break;
        case DriverState::Error:
            break;
        }
    }
    [[nodiscard]] QString sinkName() const override {
        return name_;
    }

    std::atomic<int> frames{0};
    std::atomic<int> lifecycle{0};
    std::atomic<bool> sawOpening{false};
    std::atomic<bool> sawOpen{false};
    std::atomic<bool> sawRunning{false};
    std::atomic<bool> sawStopping{false};
    std::atomic<bool> sawClosing{false};
    std::atomic<bool> sawIdle{false};

private:
    QString name_;
};

PipelineConfig cfgFor(const QString& id) {
    PipelineConfig c;
    c.driverId = id;
    return c;
}

}  // namespace

TEST_CASE("pipeline integration: ReplayDriver skeleton drives lifecycle onto a sink",
          "[integration][pipeline][replay]") {
    ReplayConfig rc;
    rc.sessionFilePath = QStringLiteral(SIGNALFORGE_FIXTURES_DIR) + QStringLiteral("/minimal_session.sfreplay");
    ReplayDriver driver(rc);

    FramePipeline pipeline(cfgFor(QStringLiteral("replay:integration")));
    pipeline.attachDriver(&driver);

    auto sink = std::make_shared<RecordingSink>(QStringLiteral("lifecycle-sink"));
    pipeline.addSink(sink);

    REQUIRE(driver.open() == DriverErrorCode::Success);
    REQUIRE(waitForState(driver, DriverState::Open));
    REQUIRE(driver.start() == DriverErrorCode::Success);
    REQUIRE(waitForState(driver, DriverState::Running));
    driver.stop();
    REQUIRE(waitForState(driver, DriverState::Open));
    driver.close();
    REQUIRE(waitForState(driver, DriverState::Idle));

    // Lifecycle events are queued through the pipeline thread; wait for
    // all the expected transitions to land on the sink rather than a
    // fixed count (which varies between drivers).
    REQUIRE(pumpUntil([&] {
        return sink->sawOpening.load() && sink->sawOpen.load() && sink->sawRunning.load() && sink->sawIdle.load();
    }));

    // Skeleton emits no frames.
    REQUIRE(sink->frames == 0);
}

TEST_CASE("pipeline integration: two UdpDrivers deliver frames through a pipeline sink",
          "[integration][pipeline][udp]") {
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

    FramePipeline pipelineA(cfgFor(QStringLiteral("udp:integA:") + QString::number(portA)));
    pipelineA.attachDriver(&a);
    auto sinkA = std::make_shared<RecordingSink>(QStringLiteral("sink-a"));
    pipelineA.addSink(sinkA);

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
        // Driver B writes; pipeline on driver A receives.
        REQUIRE(b.write(QByteArray(8, static_cast<char>(i & 0xFF))) == DriverErrorCode::Success);
    }

    REQUIRE(pumpUntil([&] { return sinkA->frames.load() >= kFrames; }, 5000));

    const auto stats = pipelineA.stats();
    REQUIRE(stats.framesReceived >= kFrames);
    REQUIRE(stats.framesDropped == 0);

    a.close();
    b.close();
    REQUIRE(waitForState(a, DriverState::Idle));
    REQUIRE(waitForState(b, DriverState::Idle));
}
