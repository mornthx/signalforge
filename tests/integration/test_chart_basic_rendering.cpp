// tests/integration/test_chart_basic_rendering.cpp
//
// S9 integration test (spec §2.1-12 #1): instantiate Chart, attach
// 5 signals, push data into the registry, drive ticks, verify
// FrameStats counters increment and visibleSignals reflects
// attached signals. The actual paint result is verified by the
// SG-node lifecycle test (chart_signal_lifecycle_test, S3).

#include "buffer/signal_buffer_registry.hpp"
#include "chart/chart.hpp"
#include "chart/time_axis_manager.hpp"
#include "decode/decoder_interface.hpp"

#include <QGuiApplication>
#include <QMetaObject>
#include <QString>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <vector>

namespace ch8 = signalforge::chart;
namespace bm6 = signalforge::buffer;
namespace dm5 = signalforge::decoder;

namespace {

QGuiApplication& app() {
    static int argc = 1;
    static char arg0[] = "test_chart_basic_rendering";
    static char* argv[] = {arg0, nullptr};
    static QGuiApplication instance(argc, argv);
    return instance;
}

dm5::SignalMetadata makeMeta(QString id, dm5::SignalType type) {
    dm5::SignalMetadata m;
    m.id = std::move(id);
    m.name = QStringLiteral("");
    m.unit = QStringLiteral("");
    m.type = type;
    return m;
}

void invokeTick(ch8::Chart& chart) {
    QMetaObject::invokeMethod(&chart, "onTick", Qt::DirectConnection);
}

}  // namespace

TEST_CASE("S9 integration: 5 signals attached + ticks drive FrameStats", "[integration][s9][chart][basic]") {
    app();
    bm6::SignalBufferRegistry registry;
    std::vector<dm5::SignalMetadata> metas{
        makeMeta(QStringLiteral("d/s0"), dm5::SignalType::Double),
        makeMeta(QStringLiteral("d/s1"), dm5::SignalType::Double),
        makeMeta(QStringLiteral("d/s2"), dm5::SignalType::Int64),
        makeMeta(QStringLiteral("d/s3"), dm5::SignalType::Bool),
        makeMeta(QStringLiteral("d/s4"), dm5::SignalType::Double),
    };
    registry.onSignalsRegistered(QStringLiteral("d"), metas);

    // Push 100 samples per signal so queryLatestOne / queryRange
    // resolve under the M6 publish cadence.
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) {
        registry.onSignal(t0 + std::chrono::microseconds(i * 1000), QStringLiteral("d/s0"),
                          dm5::SignalValue{static_cast<double>(i)});
        registry.onSignal(t0 + std::chrono::microseconds(i * 1000), QStringLiteral("d/s4"),
                          dm5::SignalValue{static_cast<double>(i) * 0.1});
    }

    ch8::TimeAxisManager axis;
    ch8::Chart chart(registry, axis);
    for (const auto& m : metas) {
        chart.addSignal(m.id);
    }
    REQUIRE(chart.visibleSignals().size() == 5);

    for (int i = 0; i < 10; ++i) {
        invokeTick(chart);
    }
    const auto stats = chart.stats();
    REQUIRE(stats.totalRedraws == 10);
}
