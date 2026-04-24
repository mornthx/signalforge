// tests/unit/pipeline/pipeline_test.cpp
#include "pipeline/frame_sink.hpp"

#include <catch2/catch_test_macros.hpp>

using signalforge::drivers::DriverError;
using signalforge::drivers::DriverErrorCode;
using signalforge::drivers::DriverState;
using signalforge::frame::RawFrame;
using signalforge::pipeline::FrameSink;

namespace {

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
