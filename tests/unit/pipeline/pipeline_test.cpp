// tests/unit/pipeline/pipeline_test.cpp
#include "pipeline/frame_pipeline.hpp"
#include "pipeline/frame_sink.hpp"

#include <QCoreApplication>
#include <catch2/catch_test_macros.hpp>
#include <memory>

using signalforge::drivers::DriverError;
using signalforge::drivers::DriverErrorCode;
using signalforge::drivers::DriverState;
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
