// tests/unit/pipeline/pipeline_test.cpp
#include "observability/metrics.hpp"
#include "pipeline/frame_pipeline.hpp"
#include "pipeline/frame_sink.hpp"
#include "pipeline/pipeline_manager.hpp"
#include "tests/mocks/mock_driver.hpp"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QThread>
#include <QtTest/QtTest>
#include <algorithm>
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <vector>

using signalforge::drivers::DriverError;
using signalforge::drivers::DriverErrorCode;
using signalforge::drivers::DriverState;
using signalforge::frame::RawFrame;
using signalforge::pipeline::FramePipeline;
using signalforge::pipeline::FrameSink;
using signalforge::pipeline::PipelineConfig;
using signalforge::pipeline::PipelineManager;

namespace {

class CoreAppHolder {
public:
    CoreAppHolder() {
        signalforge::frame::registerMetatypes();
        signalforge::drivers::registerMetatypes();
        if (QCoreApplication::instance() == nullptr) {
            static int argc = 1;
            static char argv0[] = "pipeline_test";
            static char* argv[] = {argv0, nullptr};
            app_ = std::make_unique<QCoreApplication>(argc, argv);
        }
    }

private:
    std::unique_ptr<QCoreApplication> app_;
};
CoreAppHolder g_app;

/// Concrete FrameSink used to exercise the interface. The default
/// implementations of onError and onLifecycle must be non-throwing no-ops;
/// sinkName must be returned as specified.
class TestSink : public FrameSink {
public:
    void onFrame(const RawFrame&) override {
        ++frames;
    }
    [[nodiscard]] QString sinkName() const override {
        return QStringLiteral("test-sink");
    }

    int frames = 0;
};

PipelineConfig cfgWithId(const QString& id) {
    PipelineConfig c;
    c.driverId = id;
    return c;
}

}  // namespace

TEST_CASE("FrameSink: concrete subclass can be constructed and queried", "[pipeline][frame_sink]") {
    TestSink sink;
    REQUIRE(sink.sinkName() == "test-sink");
}

TEST_CASE("FrameSink: default onError is a non-throwing no-op", "[pipeline][frame_sink]") {
    TestSink sink;
    DriverError err;
    err.code = DriverErrorCode::IoFailure;
    err.message = QStringLiteral("synthetic");
    REQUIRE_NOTHROW(sink.onError(err));
}

TEST_CASE("FrameSink: default onLifecycle is a non-throwing no-op", "[pipeline][frame_sink]") {
    TestSink sink;
    REQUIRE_NOTHROW(sink.onLifecycle(DriverState::Opening));
    REQUIRE_NOTHROW(sink.onLifecycle(DriverState::Running));
    REQUIRE_NOTHROW(sink.onLifecycle(DriverState::Idle));
}

TEST_CASE("FrameSink: onFrame override is callable", "[pipeline][frame_sink]") {
    TestSink sink;
    RawFrame frame;
    frame.payload = QByteArray("x", 1);
    sink.onFrame(frame);
    sink.onFrame(frame);
    REQUIRE(sink.frames == 2);
}

TEST_CASE("FramePipeline: construction starts a worker thread", "[pipeline][frame_pipeline]") {
    FramePipeline pipeline(cfgWithId(QStringLiteral("test:driver")));
    REQUIRE(pipeline.driverId() == "test:driver");
    REQUIRE(pipeline.sinkCount() == 0);
}

TEST_CASE("FramePipeline: addSink / removeSink round-trip", "[pipeline][frame_pipeline]") {
    FramePipeline pipeline(cfgWithId(QStringLiteral("test:round-trip")));
    auto sink = std::make_shared<TestSink>();
    REQUIRE(pipeline.sinkCount() == 0);
    pipeline.addSink(sink);
    REQUIRE(pipeline.sinkCount() == 1);
    pipeline.removeSink(sink);
    REQUIRE(pipeline.sinkCount() == 0);
}

TEST_CASE("FramePipeline: addSink is idempotent", "[pipeline][frame_pipeline]") {
    FramePipeline pipeline(cfgWithId(QStringLiteral("test:idem")));
    auto sink = std::make_shared<TestSink>();
    pipeline.addSink(sink);
    pipeline.addSink(sink);
    REQUIRE(pipeline.sinkCount() == 1);
}

TEST_CASE("FramePipeline: addSink(nullptr) is ignored", "[pipeline][frame_pipeline]") {
    FramePipeline pipeline(cfgWithId(QStringLiteral("test:null-sink")));
    pipeline.addSink(nullptr);
    REQUIRE(pipeline.sinkCount() == 0);
}

TEST_CASE("FramePipeline: removeSink on unregistered sink is a no-op", "[pipeline][frame_pipeline]") {
    FramePipeline pipeline(cfgWithId(QStringLiteral("test:noop-remove")));
    auto sink = std::make_shared<TestSink>();
    pipeline.removeSink(sink);  // never added
    REQUIRE(pipeline.sinkCount() == 0);
}

TEST_CASE("FramePipeline: multiple distinct sinks coexist", "[pipeline][frame_pipeline]") {
    FramePipeline pipeline(cfgWithId(QStringLiteral("test:multi")));
    auto a = std::make_shared<TestSink>();
    auto b = std::make_shared<TestSink>();
    auto c = std::make_shared<TestSink>();
    pipeline.addSink(a);
    pipeline.addSink(b);
    pipeline.addSink(c);
    REQUIRE(pipeline.sinkCount() == 3);
    pipeline.removeSink(b);
    REQUIRE(pipeline.sinkCount() == 2);
}

TEST_CASE("FramePipeline: stats() returns zeroed counters in S2 skeleton", "[pipeline][frame_pipeline]") {
    FramePipeline pipeline(cfgWithId(QStringLiteral("test:stats")));
    const auto s = pipeline.stats();
    REQUIRE(s.framesReceived == 0);
    REQUIRE(s.framesDropped == 0);
    REQUIRE(s.errorsForwarded == 0);
    REQUIRE(s.ingressDepthCurrent == 0);
}

namespace {

/// Counting sink. Records every onFrame / onError / onLifecycle call plus
/// the thread the callback ran on. Thread-safe counters.
class CountingSink : public FrameSink {
public:
    explicit CountingSink(QString name) : name_(std::move(name)) {}
    void onFrame(const RawFrame& f) override {
        ++frames;
        lastPayload = f.payload;
        lastThread = QThread::currentThread();
    }
    void onError(const DriverError&) override {
        ++errors;
    }
    void onLifecycle(DriverState s) override {
        ++lifecycle;
        lastState = s;
    }
    [[nodiscard]] QString sinkName() const override {
        return name_;
    }

    std::atomic<int> frames{0};
    std::atomic<int> errors{0};
    std::atomic<int> lifecycle{0};
    QByteArray lastPayload;
    DriverState lastState = DriverState::Idle;
    QThread* lastThread = nullptr;

private:
    QString name_;
};

/// Sink that always throws from onFrame. Used to verify exception
/// isolation.
class ThrowingSink : public FrameSink {
public:
    void onFrame(const RawFrame&) override {
        ++seen;
        throw std::runtime_error("synthetic sink failure");
    }
    [[nodiscard]] QString sinkName() const override {
        return QStringLiteral("throwing-sink");
    }
    std::atomic<int> seen{0};
};

/// Wait until predicate holds or timeout, pumping the main thread's event
/// loop so queued signal deliveries (to objects on the main thread) land.
/// The pipeline worker thread drives its own event loop independently.
template <typename Pred> bool pumpUntil(Pred&& pred, int timeoutMs = 2000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (!pred() && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(5);
    }
    return pred();
}

}  // namespace

TEST_CASE("FramePipeline: frameReceived fans out to all registered sinks", "[pipeline][frame_pipeline]") {
    signalforge::test::MockDriver driver;
    FramePipeline pipeline(cfgWithId(QStringLiteral("test:fanout")));
    pipeline.attachDriver(&driver);

    auto a = std::make_shared<CountingSink>(QStringLiteral("sink-a"));
    auto b = std::make_shared<CountingSink>(QStringLiteral("sink-b"));
    auto c = std::make_shared<CountingSink>(QStringLiteral("sink-c"));
    pipeline.addSink(a);
    pipeline.addSink(b);
    pipeline.addSink(c);

    driver.emitFrame(QByteArray("abc", 3));

    REQUIRE(pumpUntil([&] { return a->frames == 1 && b->frames == 1 && c->frames == 1; }));
    REQUIRE(a->lastPayload == QByteArray("abc", 3));
    REQUIRE(b->lastPayload == QByteArray("abc", 3));
    REQUIRE(c->lastPayload == QByteArray("abc", 3));

    const auto s = pipeline.stats();
    REQUIRE(s.framesReceived == 1);
    REQUIRE(s.framesDropped == 0);
}

TEST_CASE("FramePipeline: errorOccurred fans out to all registered sinks", "[pipeline][frame_pipeline]") {
    signalforge::test::MockDriver driver;
    FramePipeline pipeline(cfgWithId(QStringLiteral("test:errors")));
    pipeline.attachDriver(&driver);

    auto a = std::make_shared<CountingSink>(QStringLiteral("sink-a"));
    auto b = std::make_shared<CountingSink>(QStringLiteral("sink-b"));
    pipeline.addSink(a);
    pipeline.addSink(b);

    driver.injectError(DriverErrorCode::IoFailure, QStringLiteral("synthetic"));

    REQUIRE(pumpUntil([&] { return a->errors == 1 && b->errors == 1; }));
    REQUIRE(pipeline.stats().errorsForwarded == 1);
}

TEST_CASE("FramePipeline: stateChanged fans out to all registered sinks", "[pipeline][frame_pipeline]") {
    signalforge::test::MockDriver driver;
    FramePipeline pipeline(cfgWithId(QStringLiteral("test:state")));
    pipeline.attachDriver(&driver);

    auto sink = std::make_shared<CountingSink>(QStringLiteral("sink"));
    pipeline.addSink(sink);

    // open() drives Idle → Opening → Open (two transitions).
    REQUIRE(driver.open() == DriverErrorCode::Success);
    REQUIRE(pumpUntil([&] { return sink->lifecycle.load() >= 2; }));
    REQUIRE(sink->lastState == DriverState::Open);
}

TEST_CASE("FramePipeline: throwing sink does not break fanout to other sinks", "[pipeline][frame_pipeline]") {
    signalforge::test::MockDriver driver;
    FramePipeline pipeline(cfgWithId(QStringLiteral("test:isolation")));
    pipeline.attachDriver(&driver);

    auto before = std::make_shared<CountingSink>(QStringLiteral("before"));
    auto boom = std::make_shared<ThrowingSink>();
    auto after = std::make_shared<CountingSink>(QStringLiteral("after"));
    pipeline.addSink(before);
    pipeline.addSink(boom);
    pipeline.addSink(after);

    driver.emitFrame(QByteArray("1", 1));
    REQUIRE(pumpUntil([&] { return before->frames == 1 && after->frames == 1; }));
    REQUIRE(boom->seen == 1);

    // Second frame after a sink has thrown — pipeline must still deliver.
    driver.emitFrame(QByteArray("2", 1));
    REQUIRE(pumpUntil([&] { return before->frames == 2 && after->frames == 2; }));
}

namespace {

/// Sink that ignores frames; used when we want to drive the pipeline
/// without tracking fanout in the test.
class NullSink : public FrameSink {
public:
    void onFrame(const RawFrame&) override {}
    [[nodiscard]] QString sinkName() const override {
        return QStringLiteral("null");
    }
};

PipelineConfig cfgWithCapacity(const QString& id, std::uint32_t cap) {
    PipelineConfig c;
    c.driverId = id;
    c.ingressCapacity = cap;
    return c;
}

/// Drop-counter sink: blocks-up onFrame to force the ingress queue to
/// overflow when we spam-enqueue. Uses an atomic flag for release.
class BlockingSink : public FrameSink {
public:
    void onFrame(const RawFrame&) override {
        while (!release.load(std::memory_order_acquire)) {
            QThread::msleep(1);
        }
    }
    [[nodiscard]] QString sinkName() const override {
        return QStringLiteral("blocking");
    }
    std::atomic<bool> release{false};
};

}  // namespace

TEST_CASE("FramePipeline: 5 per-driver metrics appear in registry at construction",
          "[pipeline][frame_pipeline][metrics]") {
    auto& reg = signalforge::observability::MetricsRegistry::instance();
    reg.clearForTesting();
    const QString driverId = QStringLiteral("udp:127.0.0.1:9000");
    FramePipeline pipeline(cfgWithId(driverId));

    auto names = reg.metricNames();
    const auto has = [&](const QString& n) { return std::find(names.begin(), names.end(), n) != names.end(); };
    REQUIRE(has(QStringLiteral("pipeline_frames_received_") + driverId));
    REQUIRE(has(QStringLiteral("pipeline_frames_dropped_") + driverId));
    REQUIRE(has(QStringLiteral("pipeline_errors_forwarded_") + driverId));
    REQUIRE(has(QStringLiteral("pipeline_ingress_watermark_") + driverId));
    REQUIRE(has(QStringLiteral("pipeline_ingress_depth_peak_") + driverId));
}

TEST_CASE("FramePipeline: metrics counters track frames and errors", "[pipeline][frame_pipeline][metrics]") {
    auto& reg = signalforge::observability::MetricsRegistry::instance();
    reg.clearForTesting();
    const QString driverId = QStringLiteral("udp:metrics");
    signalforge::test::MockDriver driver;
    FramePipeline pipeline(cfgWithId(driverId));
    pipeline.attachDriver(&driver);
    pipeline.addSink(std::make_shared<NullSink>());

    driver.emitFrame(QByteArray("a", 1));
    driver.emitFrame(QByteArray("bb", 2));
    driver.injectError(DriverErrorCode::IoFailure, QStringLiteral("x"));

    REQUIRE(pumpUntil([&] { return pipeline.stats().framesReceived == 2; }));
    REQUIRE(pumpUntil([&] { return pipeline.stats().errorsForwarded == 1; }));

    auto* framesRx = reg.getOrCreate(QStringLiteral("pipeline_frames_received_") + driverId,
                                     signalforge::observability::MetricKind::Counter);
    auto* errors = reg.getOrCreate(QStringLiteral("pipeline_errors_forwarded_") + driverId,
                                   signalforge::observability::MetricKind::Counter);
    REQUIRE(framesRx->value() == 2);
    REQUIRE(errors->value() == 1);
}

TEST_CASE("FramePipeline: ingress capacity exceeded drops frame and bumps framesDropped",
          "[pipeline][frame_pipeline][backpressure]") {
    // capacity=0 makes every frame hit the cap check; directly exercises
    // the drop path without relying on sink-slow queue build-up (which is
    // hard to trigger with inline drain because the worker event queue,
    // not MpscQueue, is where frames pile up when a sink blocks).
    signalforge::test::MockDriver driver;
    auto& reg = signalforge::observability::MetricsRegistry::instance();
    reg.clearForTesting();
    const QString driverId = QStringLiteral("udp:drop");
    FramePipeline pipeline(cfgWithCapacity(driverId, 0));
    pipeline.attachDriver(&driver);
    pipeline.addSink(std::make_shared<NullSink>());

    driver.emitFrame(QByteArray("a", 1));
    driver.emitFrame(QByteArray("b", 1));
    driver.emitFrame(QByteArray("c", 1));

    REQUIRE(pumpUntil([&] { return pipeline.stats().framesDropped >= 3; }, 2000));
    REQUIRE(pipeline.stats().framesReceived == 0);

    auto* dropsMetric = reg.getOrCreate(QStringLiteral("pipeline_frames_dropped_") + driverId,
                                        signalforge::observability::MetricKind::Counter);
    REQUIRE(dropsMetric->value() >= 3);
}

TEST_CASE("FramePipeline: ingress depth peak tracks monotonically", "[pipeline][frame_pipeline][backpressure]") {
    signalforge::test::MockDriver driver;
    FramePipeline pipeline(cfgWithCapacity(QStringLiteral("udp:peak"), 64));
    pipeline.attachDriver(&driver);
    auto slow = std::make_shared<BlockingSink>();
    pipeline.addSink(slow);

    for (int i = 0; i < 5; ++i) {
        driver.emitFrame(QByteArray(4, 'p'));
    }
    // Wait until we've recorded a peak >= 1 (at minimum one frame
    // enqueued before the worker's first onFrame finishes).
    REQUIRE(pumpUntil([&] { return pipeline.stats().ingressDepthPeak >= 1; }, 2000));

    const auto peakBefore = pipeline.stats().ingressDepthPeak;
    slow->release.store(true, std::memory_order_release);
    REQUIRE(peakBefore >= 1);
    REQUIRE(peakBefore <= 64);
}

TEST_CASE("FramePipeline: resetBackpressureStats zeroes framesDropped and peak",
          "[pipeline][frame_pipeline][backpressure]") {
    signalforge::test::MockDriver driver;
    FramePipeline pipeline(cfgWithCapacity(QStringLiteral("udp:reset"), 0));
    pipeline.attachDriver(&driver);
    pipeline.addSink(std::make_shared<NullSink>());

    driver.emitFrame(QByteArray("a", 1));
    driver.emitFrame(QByteArray("b", 1));
    REQUIRE(pumpUntil([&] { return pipeline.stats().framesDropped >= 2; }, 2000));

    pipeline.resetBackpressureStats();
    // resetBackpressureStats hits framesDropped and ingressDepthPeak
    // synchronously; WatermarkTracker::reset is dispatched to the
    // worker. Both atomics will be zero immediately even if the worker-
    // side reset hasn't yet run.
    const auto after = pipeline.stats();
    REQUIRE(after.framesDropped == 0);
    REQUIRE(after.ingressDepthPeak == 0);
}

TEST_CASE("FramePipeline: sink callbacks execute on the pipeline thread, not the caller",
          "[pipeline][frame_pipeline]") {
    signalforge::test::MockDriver driver;
    FramePipeline pipeline(cfgWithId(QStringLiteral("test:affinity")));
    pipeline.attachDriver(&driver);

    auto sink = std::make_shared<CountingSink>(QStringLiteral("sink"));
    pipeline.addSink(sink);

    QThread* mainThread = QThread::currentThread();
    driver.emitFrame(QByteArray("x", 1));
    REQUIRE(pumpUntil([&] { return sink->frames == 1; }));
    REQUIRE(sink->lastThread != nullptr);
    REQUIRE(sink->lastThread != mainThread);
}

namespace {

PipelineConfig cfgFor(const QString& id) {
    PipelineConfig c;
    c.driverId = id;
    return c;
}

}  // namespace

TEST_CASE("PipelineManager: attach creates a pipeline and bumps count", "[pipeline][pipeline_manager]") {
    signalforge::test::MockDriver driver;
    PipelineManager manager;
    REQUIRE(manager.pipelineCount() == 0);

    auto* p = manager.attach(&driver, cfgFor(QStringLiteral("tcp:127.0.0.1:9001")));
    REQUIRE(p != nullptr);
    REQUIRE(manager.pipelineCount() == 1);
    REQUIRE(manager.pipelineFor(QStringLiteral("tcp:127.0.0.1:9001")) == p);

    manager.detach(QStringLiteral("tcp:127.0.0.1:9001"));
    REQUIRE(manager.pipelineCount() == 0);
    REQUIRE(manager.pipelineFor(QStringLiteral("tcp:127.0.0.1:9001")) == nullptr);
}

TEST_CASE("PipelineManager: duplicate driverId is refused with nullptr", "[pipeline][pipeline_manager]") {
    signalforge::test::MockDriver driverA;
    signalforge::test::MockDriver driverB;
    PipelineManager manager;

    REQUIRE(manager.attach(&driverA, cfgFor(QStringLiteral("udp:dup"))) != nullptr);
    REQUIRE(manager.attach(&driverB, cfgFor(QStringLiteral("udp:dup"))) == nullptr);
    REQUIRE(manager.pipelineCount() == 1);

    manager.detach(QStringLiteral("udp:dup"));
}

TEST_CASE("PipelineManager: attach(nullptr) and empty driverId are refused", "[pipeline][pipeline_manager]") {
    signalforge::test::MockDriver driver;
    PipelineManager manager;

    REQUIRE(manager.attach(nullptr, cfgFor(QStringLiteral("abc"))) == nullptr);
    REQUIRE(manager.attach(&driver, cfgFor(QString())) == nullptr);
    REQUIRE(manager.pipelineCount() == 0);
}

TEST_CASE("PipelineManager: detach on unknown driverId is a no-op", "[pipeline][pipeline_manager]") {
    PipelineManager manager;
    manager.detach(QStringLiteral("never-attached"));
    REQUIRE(manager.pipelineCount() == 0);
}

TEST_CASE("PipelineManager: driverIds enumerates attached pipelines", "[pipeline][pipeline_manager]") {
    signalforge::test::MockDriver a, b, c;
    PipelineManager manager;
    (void)manager.attach(&a, cfgFor(QStringLiteral("serial:/tmp/ttyV0")));
    (void)manager.attach(&b, cfgFor(QStringLiteral("tcp:localhost:9000")));
    (void)manager.attach(&c, cfgFor(QStringLiteral("replay:fixture.sfreplay")));

    auto ids = manager.driverIds();
    std::sort(ids.begin(), ids.end());
    REQUIRE(ids.size() == 3);
    REQUIRE(ids[0] == "replay:fixture.sfreplay");
    REQUIRE(ids[1] == "serial:/tmp/ttyV0");
    REQUIRE(ids[2] == "tcp:localhost:9000");
}

TEST_CASE("PipelineManager: pipelineAttached and pipelineDetached signals fire", "[pipeline][pipeline_manager]") {
    signalforge::test::MockDriver driver;
    PipelineManager manager;

    QSignalSpy attachedSpy(&manager, &PipelineManager::pipelineAttached);
    QSignalSpy detachedSpy(&manager, &PipelineManager::pipelineDetached);

    auto* p = manager.attach(&driver, cfgFor(QStringLiteral("udp:signal")));
    REQUIRE(p != nullptr);
    REQUIRE(attachedSpy.count() == 1);
    REQUIRE(attachedSpy.at(0).at(0).toString() == "udp:signal");
    REQUIRE(qvariant_cast<FramePipeline*>(attachedSpy.at(0).at(1)) == p);

    manager.detach(QStringLiteral("udp:signal"));
    REQUIRE(detachedSpy.count() == 1);
    REQUIRE(detachedSpy.at(0).at(0).toString() == "udp:signal");
}

TEST_CASE("PipelineManager: destructor destroys all attached pipelines", "[pipeline][pipeline_manager]") {
    signalforge::test::MockDriver a, b;
    {
        PipelineManager manager;
        (void)manager.attach(&a, cfgFor(QStringLiteral("x1")));
        (void)manager.attach(&b, cfgFor(QStringLiteral("x2")));
        REQUIRE(manager.pipelineCount() == 2);
    }
    // Manager out of scope: its destructor joins both pipeline threads
    // within the 500 ms budget. The MockDrivers survive; reaching this
    // line without hang or crash is the test.
    SUCCEED("manager destructor completed cleanly");
}
