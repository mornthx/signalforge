// tests/unit/dashboard/panels_test.cpp
//
// M21 S2 — NumericPanel + StatePanel: value/unit formatting, observed
// min/max tracking, unit override, decimals, and bool/string state.

#include "buffer/signal_buffer_registry.hpp"
#include "dashboard/numeric_panel.hpp"
#include "dashboard/panel_types.hpp"
#include "dashboard/state_panel.hpp"
#include "decode/decoder_interface.hpp"

#include <QApplication>
#include <QLabel>
#include <catch2/catch_test_macros.hpp>
#include <chrono>

namespace dash = signalforge::dashboard;
namespace buf = signalforge::buffer;
namespace dec = signalforge::decoder;

namespace {

QApplication& app() {
    static int argc = 1;
    static char arg0[] = "test";
    static char* argv[] = {arg0, nullptr};
    static QApplication instance(argc, argv);
    return instance;
}

dec::SignalMetadata makeMeta(QString id, dec::SignalType type, QString unit) {
    dec::SignalMetadata meta;
    meta.id = std::move(id);
    meta.name = QStringLiteral("");
    meta.unit = std::move(unit);
    meta.type = type;
    return meta;
}

// The M6 buffer publishes to readers only every kDefaultPublishCadence
// (=100) samples; a single push is not visible via queryLatestOne until
// the segment publishes. Push a full cadence of the same value so the
// published "latest" is `v`. (Production rates make this latency small;
// the slow-signal case is logged in M21-concerns.md.)
constexpr int kPublishCadence = 100;

void pushUntilVisible(buf::SignalBufferRegistry& reg, const QString& id, const dec::SignalValue& v) {
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kPublishCadence; ++i) {
        reg.onSignal(t0 + std::chrono::microseconds(i), id, v);
    }
}

dash::PanelConfig cfgFor(dash::PanelType type, const QString& signalId) {
    dash::PanelConfig cfg;
    cfg.id = QStringLiteral("panel-1");
    cfg.type = type;
    cfg.signalIds << signalId;
    return cfg;
}

}  // namespace

TEST_CASE("S2: NumericPanel shows value, unit, observed range", "[dashboard][s2][numeric]") {
    app();
    buf::SignalBufferRegistry reg;
    const QString id = QStringLiteral("rig/temperature");
    reg.onSignalsRegistered(QStringLiteral("rig"), {makeMeta(id, dec::SignalType::Double, QStringLiteral("°C"))});

    dash::NumericPanel panel(cfgFor(dash::PanelType::Numeric, id), reg);
    CHECK(panel.valueText() == QStringLiteral("—"));  // no data yet

    pushUntilVisible(reg, id, dec::SignalValue{24.7});
    panel.refresh();
    CHECK(panel.valueText() == QStringLiteral("24.700"));  // default 3 decimals
    CHECK(panel.unitText() == QStringLiteral("°C"));
    REQUIRE(panel.observedMin().has_value());
    CHECK(*panel.observedMin() == 24.7);
    CHECK(*panel.observedMax() == 24.7);

    pushUntilVisible(reg, id, dec::SignalValue{20.0});
    panel.refresh();
    pushUntilVisible(reg, id, dec::SignalValue{30.0});
    panel.refresh();
    CHECK(panel.valueText() == QStringLiteral("30.000"));
    CHECK(*panel.observedMin() == 20.0);
    CHECK(*panel.observedMax() == 30.0);
}

TEST_CASE("S2: NumericPanel honors unitOverride and decimals", "[dashboard][s2][numeric]") {
    app();
    buf::SignalBufferRegistry reg;
    const QString id = QStringLiteral("rig/temperature");
    reg.onSignalsRegistered(QStringLiteral("rig"), {makeMeta(id, dec::SignalType::Double, QStringLiteral("°C"))});

    auto cfg = cfgFor(dash::PanelType::Numeric, id);
    cfg.unitOverride = QStringLiteral("K");
    cfg.decimals = 1;
    dash::NumericPanel panel(cfg, reg);
    pushUntilVisible(reg, id, dec::SignalValue{24.75});
    panel.refresh();
    CHECK(panel.valueText() == QStringLiteral("24.8"));  // 1 decimal, rounded
    CHECK(panel.unitText() == QStringLiteral("K"));
}

TEST_CASE("S2: StatePanel reflects boolean state", "[dashboard][s2][state]") {
    app();
    buf::SignalBufferRegistry reg;
    const QString id = QStringLiteral("rig/alarm");
    reg.onSignalsRegistered(QStringLiteral("rig"), {makeMeta(id, dec::SignalType::Bool, QString())});

    dash::StatePanel panel(cfgFor(dash::PanelType::State, id), reg);
    auto* indicator = panel.findChild<QLabel*>(QStringLiteral("stateIndicator"));
    REQUIRE(indicator != nullptr);

    pushUntilVisible(reg, id, dec::SignalValue{true});
    panel.refresh();
    CHECK(panel.isActive());
    CHECK(panel.stateText() == QStringLiteral("true"));
    CHECK(indicator->text() == QStringLiteral("●"));

    pushUntilVisible(reg, id, dec::SignalValue{false});
    panel.refresh();
    CHECK_FALSE(panel.isActive());
    CHECK(panel.stateText() == QStringLiteral("false"));
    CHECK(indicator->text() == QStringLiteral("○"));
}

TEST_CASE("S2: StatePanel shows string state verbatim", "[dashboard][s2][state]") {
    app();
    buf::SignalBufferRegistry reg;
    const QString id = QStringLiteral("rig/fsm");
    reg.onSignalsRegistered(QStringLiteral("rig"), {makeMeta(id, dec::SignalType::String, QString())});

    dash::StatePanel panel(cfgFor(dash::PanelType::State, id), reg);
    pushUntilVisible(reg, id, dec::SignalValue{QStringLiteral("RUNNING")});
    panel.refresh();
    CHECK(panel.isActive());
    CHECK(panel.stateText() == QStringLiteral("RUNNING"));

    pushUntilVisible(reg, id, dec::SignalValue{QStringLiteral("false")});
    panel.refresh();
    CHECK_FALSE(panel.isActive());
    CHECK(panel.stateText() == QStringLiteral("false"));
}
