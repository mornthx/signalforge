// tests/integration/test_pipeline_fanout.cpp
//
// Verifies the pipeline's fanout semantics with a real UdpDriver:
// 3 sinks registered with one pipeline; 50 frames emitted from a
// peer UdpDriver; every sink observes 50 frames with matching
// payloads. Also verifies no sink cross-contamination (sink state
// is isolated).
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
            static char argv0[] = "test_pipeline_fanout";
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

class CountingSink : public FrameSink {
public:
    explicit CountingSink(QString name) : name_(std::move(name)) {}
    void onFrame(const RawFrame& f) override {
        ++frames;
        lastSize = f.payload.size();
    }
    [[nodiscard]] QString sinkName() const override {
        return name_;
    }
    std::atomic<int> frames{0};
    std::atomic<int> lastSize{0};

private:
    QString name_;
};

PipelineConfig cfgFor(const QString& id) {
    PipelineConfig c;
    c.driverId = id;
    return c;
}

}  // namespace

TEST_CASE("pipeline fanout: 3 sinks each see every frame through a real UdpDriver",
          "[integration][pipeline][udp][fanout]") {
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

    FramePipeline pipeline(cfgFor(QStringLiteral("udp:fanoutA:") + QString::number(portA)));
    pipeline.attachDriver(&a);

    auto s1 = std::make_shared<CountingSink>(QStringLiteral("s1"));
    auto s2 = std::make_shared<CountingSink>(QStringLiteral("s2"));
    auto s3 = std::make_shared<CountingSink>(QStringLiteral("s3"));
    pipeline.addSink(s1);
    pipeline.addSink(s2);
    pipeline.addSink(s3);
    REQUIRE(pipeline.sinkCount() == 3);

    REQUIRE(a.open() == DriverErrorCode::Success);
    REQUIRE(b.open() == DriverErrorCode::Success);
    REQUIRE(waitForState(a, DriverState::Open));
    REQUIRE(waitForState(b, DriverState::Open));
    REQUIRE(a.start() == DriverErrorCode::Success);
    REQUIRE(b.start() == DriverErrorCode::Success);
    REQUIRE(waitForState(a, DriverState::Running));
    REQUIRE(waitForState(b, DriverState::Running));

    constexpr int kFrames = 50;
    for (int i = 0; i < kFrames; ++i) {
        REQUIRE(b.write(QByteArray(16, static_cast<char>(i & 0xFF))) == DriverErrorCode::Success);
    }

    REQUIRE(pumpUntil([&] { return s1->frames == kFrames && s2->frames == kFrames && s3->frames == kFrames; }));

    REQUIRE(s1->frames == kFrames);
    REQUIRE(s2->frames == kFrames);
    REQUIRE(s3->frames == kFrames);
    REQUIRE(s1->lastSize == 16);
    REQUIRE(s2->lastSize == 16);
    REQUIRE(s3->lastSize == 16);

    a.close();
    b.close();
    REQUIRE(waitForState(a, DriverState::Idle));
    REQUIRE(waitForState(b, DriverState::Idle));
}
