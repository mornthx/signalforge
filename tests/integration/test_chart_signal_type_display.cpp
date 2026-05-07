// tests/integration/test_chart_signal_type_display.cpp
//
// S9 integration test (spec §2.1-12 #5): one signal each of
// Bool / Double / Int64 / QString → addSignal auto-resolves to
// Step / Line / Line / Point per spec §3.4. End-to-end test
// using the registry as the metadata source (vs the unit-level
// chart_signal_lifecycle_test which uses the same code path).

#include "buffer/signal_buffer_registry.hpp"
#include "chart/chart.hpp"
#include "chart/time_axis_manager.hpp"
#include "decode/decoder_interface.hpp"

#include <QGuiApplication>
#include <QString>
#include <catch2/catch_test_macros.hpp>
#include <vector>

namespace ch8 = signalforge::chart;
namespace bm6 = signalforge::buffer;
namespace dm5 = signalforge::decoder;

namespace {

QGuiApplication& app() {
    static int argc = 1;
    static char arg0[] = "test_chart_signal_type_display";
    static char* argv[] = {arg0, nullptr};
    static QGuiApplication instance(argc, argv);
    return instance;
}

dm5::SignalMetadata makeMeta(QString id, dm5::SignalType type) {
    dm5::SignalMetadata m;
    m.id = std::move(id);
    m.type = type;
    return m;
}

}  // namespace

TEST_CASE("S9 integration: each signal type maps to its spec §3.4 display mode",
          "[integration][s9][chart][type_display]") {
    app();
    bm6::SignalBufferRegistry registry;
    std::vector<dm5::SignalMetadata> metas{
        makeMeta(QStringLiteral("d/bool"), dm5::SignalType::Bool),
        makeMeta(QStringLiteral("d/int"), dm5::SignalType::Int64),
        makeMeta(QStringLiteral("d/double"), dm5::SignalType::Double),
        makeMeta(QStringLiteral("d/string"), dm5::SignalType::String),
    };
    registry.onSignalsRegistered(QStringLiteral("d"), metas);

    ch8::TimeAxisManager axis;
    ch8::Chart chart(registry, axis);
    for (const auto& m : metas) {
        chart.addSignal(m.id);
    }

    const auto cfg = chart.config();
    REQUIRE(cfg.signalConfigs.size() == 4);
    REQUIRE(cfg.signalConfigs[0].displayMode == ch8::SignalDisplayMode::Step);   // Bool
    REQUIRE(cfg.signalConfigs[1].displayMode == ch8::SignalDisplayMode::Line);   // Int64
    REQUIRE(cfg.signalConfigs[2].displayMode == ch8::SignalDisplayMode::Line);   // Double
    REQUIRE(cfg.signalConfigs[3].displayMode == ch8::SignalDisplayMode::Point);  // QString
}
