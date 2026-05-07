// tests/unit/chart/chart_signal_lifecycle_test.cpp
//
// S3 — Chart's public lifecycle API: addSignal / removeSignal /
// setDisplayMode / setSignalVisible / config / setConfig. These
// tests don't need a live Qt event loop; they exercise config_
// state directly. Auto-display-mode resolution is tested with a
// real SignalBufferRegistry pre-populated with one signal of each
// type (per spec §3.4: Bool→Step, Int64/Double→Line, QString→Point).

#include "buffer/signal_buffer_registry.hpp"
#include "chart/chart.hpp"
#include "chart/time_axis_manager.hpp"
#include "decode/decoder_interface.hpp"

#include <QString>
#include <catch2/catch_test_macros.hpp>
#include <vector>

namespace ch8 = signalforge::chart;
namespace bm6 = signalforge::buffer;
namespace dm5 = signalforge::decoder;

namespace {

dm5::SignalMetadata makeMeta(QString id, dm5::SignalType type) {
    dm5::SignalMetadata meta;
    meta.id = std::move(id);
    meta.name = QStringLiteral("test");
    meta.unit = QStringLiteral("");
    meta.type = type;
    return meta;
}

/// Populate the registry with one signal of each variant type;
/// returns the metas (not owned, just for reference).
void populateAllTypes(bm6::SignalBufferRegistry& registry) {
    std::vector<dm5::SignalMetadata> metas{
        makeMeta(QStringLiteral("sig_bool"), dm5::SignalType::Bool),
        makeMeta(QStringLiteral("sig_int"), dm5::SignalType::Int64),
        makeMeta(QStringLiteral("sig_double"), dm5::SignalType::Double),
        makeMeta(QStringLiteral("sig_string"), dm5::SignalType::String),
    };
    registry.onSignalsRegistered(QStringLiteral("test-driver"), metas);
}

}  // namespace

TEST_CASE("S3: addSignal then removeSignal round-trips config()", "[chart][s3][lifecycle]") {
    bm6::SignalBufferRegistry registry;
    populateAllTypes(registry);
    ch8::TimeAxisManager axis;
    ch8::Chart chart(registry, axis);

    REQUIRE(chart.config().signalConfigs.empty());

    chart.addSignal(QStringLiteral("sig_double"));
    REQUIRE(chart.config().signalConfigs.size() == 1);
    REQUIRE(chart.config().signalConfigs[0].signalId == QStringLiteral("sig_double"));
    REQUIRE(chart.visibleSignals().size() == 1);

    chart.removeSignal(QStringLiteral("sig_double"));
    REQUIRE(chart.config().signalConfigs.empty());
    REQUIRE(chart.visibleSignals().empty());
}

TEST_CASE("S3: addSignal is a no-op for an id already present", "[chart][s3][lifecycle]") {
    bm6::SignalBufferRegistry registry;
    populateAllTypes(registry);
    ch8::TimeAxisManager axis;
    ch8::Chart chart(registry, axis);

    chart.addSignal(QStringLiteral("sig_double"));
    chart.addSignal(QStringLiteral("sig_double"));
    chart.addSignal(QStringLiteral("sig_double"));

    REQUIRE(chart.config().signalConfigs.size() == 1);
}

TEST_CASE("S3: addSignal ignores empty id", "[chart][s3][lifecycle]") {
    bm6::SignalBufferRegistry registry;
    ch8::TimeAxisManager axis;
    ch8::Chart chart(registry, axis);

    chart.addSignal(QStringLiteral(""));
    REQUIRE(chart.config().signalConfigs.empty());
}

TEST_CASE("S3: removeSignal is a no-op for a missing id", "[chart][s3][lifecycle]") {
    bm6::SignalBufferRegistry registry;
    ch8::TimeAxisManager axis;
    ch8::Chart chart(registry, axis);

    chart.removeSignal(QStringLiteral("never-added"));
    REQUIRE(chart.config().signalConfigs.empty());
}

TEST_CASE("S3: addSignal auto-selects display mode by signal type", "[chart][s3][lifecycle][auto_mode]") {
    bm6::SignalBufferRegistry registry;
    populateAllTypes(registry);
    ch8::TimeAxisManager axis;
    ch8::Chart chart(registry, axis);

    chart.addSignal(QStringLiteral("sig_bool"));
    chart.addSignal(QStringLiteral("sig_int"));
    chart.addSignal(QStringLiteral("sig_double"));
    chart.addSignal(QStringLiteral("sig_string"));

    const auto& cfg = chart.config().signalConfigs;
    REQUIRE(cfg.size() == 4);
    REQUIRE(cfg[0].displayMode == ch8::SignalDisplayMode::Step);   // Bool
    REQUIRE(cfg[1].displayMode == ch8::SignalDisplayMode::Line);   // Int64
    REQUIRE(cfg[2].displayMode == ch8::SignalDisplayMode::Line);   // Double
    REQUIRE(cfg[3].displayMode == ch8::SignalDisplayMode::Point);  // QString
}

TEST_CASE("S3: addSignal honors explicit display mode override", "[chart][s3][lifecycle][auto_mode]") {
    bm6::SignalBufferRegistry registry;
    populateAllTypes(registry);
    ch8::TimeAxisManager axis;
    ch8::Chart chart(registry, axis);

    // Override Bool's natural Step to Line.
    chart.addSignal(QStringLiteral("sig_bool"), ch8::SignalDisplayMode::Line);
    REQUIRE(chart.config().signalConfigs[0].displayMode == ch8::SignalDisplayMode::Line);
}

TEST_CASE("S3: addSignal falls back to Line when buffer is missing", "[chart][s3][lifecycle][auto_mode]") {
    bm6::SignalBufferRegistry registry;  // Empty registry — no buffers.
    ch8::TimeAxisManager axis;
    ch8::Chart chart(registry, axis);

    chart.addSignal(QStringLiteral("phantom_signal"));
    REQUIRE(chart.config().signalConfigs.size() == 1);
    REQUIRE(chart.config().signalConfigs[0].displayMode == ch8::SignalDisplayMode::Line);
}

TEST_CASE("S3: setDisplayMode updates config", "[chart][s3][lifecycle]") {
    bm6::SignalBufferRegistry registry;
    populateAllTypes(registry);
    ch8::TimeAxisManager axis;
    ch8::Chart chart(registry, axis);
    chart.addSignal(QStringLiteral("sig_double"));

    chart.setDisplayMode(QStringLiteral("sig_double"), ch8::SignalDisplayMode::Step);
    REQUIRE(chart.config().signalConfigs[0].displayMode == ch8::SignalDisplayMode::Step);

    // Setting to the same mode is a no-op (no crash, no change).
    chart.setDisplayMode(QStringLiteral("sig_double"), ch8::SignalDisplayMode::Step);
    REQUIRE(chart.config().signalConfigs[0].displayMode == ch8::SignalDisplayMode::Step);

    // Setting on a missing id is a no-op.
    chart.setDisplayMode(QStringLiteral("nonexistent"), ch8::SignalDisplayMode::Point);
    REQUIRE(chart.config().signalConfigs.size() == 1);
}

TEST_CASE("S3: setSignalVisible toggles without removing", "[chart][s3][lifecycle]") {
    bm6::SignalBufferRegistry registry;
    populateAllTypes(registry);
    ch8::TimeAxisManager axis;
    ch8::Chart chart(registry, axis);
    chart.addSignal(QStringLiteral("sig_int"));
    chart.addSignal(QStringLiteral("sig_double"));

    REQUIRE(chart.visibleSignals().size() == 2);

    chart.setSignalVisible(QStringLiteral("sig_int"), false);
    REQUIRE(chart.config().signalConfigs.size() == 2);  // not removed
    REQUIRE(chart.visibleSignals().size() == 1);
    REQUIRE(chart.visibleSignals().contains(QStringLiteral("sig_double")));

    chart.setSignalVisible(QStringLiteral("sig_int"), true);
    REQUIRE(chart.visibleSignals().size() == 2);
}

TEST_CASE("S3: setConfig replaces all signals", "[chart][s3][lifecycle][config]") {
    bm6::SignalBufferRegistry registry;
    populateAllTypes(registry);
    ch8::TimeAxisManager axis;
    ch8::Chart chart(registry, axis);
    chart.addSignal(QStringLiteral("sig_int"));
    chart.addSignal(QStringLiteral("sig_double"));
    REQUIRE(chart.config().signalConfigs.size() == 2);

    ch8::ChartConfig newCfg;
    newCfg.id = QStringLiteral("c2");
    newCfg.title = QStringLiteral("Replaced");
    ch8::ChartSignalConfig only;
    only.signalId = QStringLiteral("sig_bool");
    only.displayMode = ch8::SignalDisplayMode::Step;
    newCfg.signalConfigs.push_back(std::move(only));

    chart.setConfig(std::move(newCfg));
    const auto cfg = chart.config();
    REQUIRE(cfg.id == QStringLiteral("c2"));
    REQUIRE(cfg.title == QStringLiteral("Replaced"));
    REQUIRE(cfg.signalConfigs.size() == 1);
    REQUIRE(cfg.signalConfigs[0].signalId == QStringLiteral("sig_bool"));
    REQUIRE(cfg.signalConfigs[0].displayMode == ch8::SignalDisplayMode::Step);
    REQUIRE(cfg.signalConfigs[0].color.isValid());  // auto-assigned
}

TEST_CASE("S3: stats() returns zeroed FrameStats before any tick", "[chart][s3][lifecycle][stats]") {
    bm6::SignalBufferRegistry registry;
    ch8::TimeAxisManager axis;
    ch8::Chart chart(registry, axis);

    const auto stats = chart.stats();
    REQUIRE(stats.totalRedraws == 0);
    REQUIRE(stats.droppedFrames == 0);
    REQUIRE(stats.lastRenderUs.count() == 0);
    REQUIRE(stats.peakRenderUs.count() == 0);
    REQUIRE(stats.currentLodLevel == 0);
}
