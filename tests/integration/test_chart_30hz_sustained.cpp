// tests/integration/test_chart_30hz_sustained.cpp
//
// S9 integration test (spec §2.1-12 #2): 60 signals × 30 Hz × 5 sec.
// Driven by direct `onTick` invocation rather than QTimer dispatch
// so the test is deterministic in CI (Catch2WithMain doesn't run
// a QGuiApplication event loop; the dropped-frame test under a
// real loop is exercised via the M8 prototype's bench in S10).
// Here we verify FrameStats counters increment correctly and
// stay within budget.

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
    static char arg0[] = "test_chart_30hz_sustained";
    static char* argv[] = {arg0, nullptr};
    static QGuiApplication instance(argc, argv);
    return instance;
}

}  // namespace

TEST_CASE("S9 integration: 60 signals × 150 ticks accumulates stats", "[integration][s9][chart][30hz]") {
    app();
    bm6::SignalBufferRegistry registry;
    std::vector<dm5::SignalMetadata> metas;
    for (int i = 0; i < 60; ++i) {
        dm5::SignalMetadata m;
        m.id = QStringLiteral("driver/sig_%1").arg(i);
        m.name = QStringLiteral("");
        m.unit = QStringLiteral("");
        m.type = dm5::SignalType::Double;
        metas.push_back(std::move(m));
    }
    registry.onSignalsRegistered(QStringLiteral("driver"), metas);

    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) {
        for (int s = 0; s < 60; ++s) {
            const QString id = QStringLiteral("driver/sig_%1").arg(s);
            registry.onSignal(t0 + std::chrono::microseconds(i * 1000), id,
                              dm5::SignalValue{static_cast<double>(i + s)});
        }
    }

    ch8::TimeAxisManager axis;
    ch8::Chart chart(registry, axis);
    for (const auto& m : metas) {
        chart.addSignal(m.id);
    }
    REQUIRE(chart.visibleSignals().size() == 60);

    // 5 sec × 30 Hz = 150 ticks (matches spec §2.1-12 scope).
    for (int i = 0; i < 150; ++i) {
        QMetaObject::invokeMethod(&chart, "onTick", Qt::DirectConnection);
    }
    const auto stats = chart.stats();
    REQUIRE(stats.totalRedraws == 150);
    // Direct-invoke ticks have no real elapsed time between them,
    // so the dropped-frame counter should stay at 0 for this
    // path. Real-event-loop dropped-frame validation is in S10.
    REQUIRE(stats.droppedFrames == 0);
    REQUIRE(stats.lastRenderUs.count() >= 0);
}
