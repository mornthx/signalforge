// tests/unit/dashboard/plot_panel_test.cpp
//
// M23 — PlotPanel hosts a PlotView, delegates add/removeSignal, is wide
// + multi-signal, and reflects preconfigured signals.

#include "buffer/signal_buffer_registry.hpp"
#include "chart/time_axis_manager.hpp"
#include "dashboard/panel_types.hpp"
#include "dashboard/plot_panel.hpp"
#include "dashboard/plot_view.hpp"
#include "decode/decoder_interface.hpp"

#include <QApplication>
#include <catch2/catch_test_macros.hpp>

namespace dash = signalforge::dashboard;
namespace ch = signalforge::chart;
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

dec::SignalMetadata makeMeta(QString id, dec::SignalType type) {
    dec::SignalMetadata meta;
    meta.id = std::move(id);
    meta.type = type;
    return meta;
}

dash::PanelConfig plotCfg() {
    dash::PanelConfig cfg;
    cfg.id = QStringLiteral("panel-plot-1");
    cfg.type = dash::PanelType::Plot;
    return cfg;
}

}  // namespace

TEST_CASE("M23: PlotPanel hosts a PlotView and delegates add/remove", "[dashboard][m23][plot]") {
    app();
    buf::SignalBufferRegistry reg;
    reg.onSignalsRegistered(QStringLiteral("rig"),
                            {makeMeta(QStringLiteral("rig/temperature"), dec::SignalType::Double)});
    ch::TimeAxisManager axis;

    dash::PlotPanel panel(plotCfg(), reg, axis);
    CHECK(panel.isWide());
    CHECK(panel.isMultiSignal());
    REQUIRE(panel.view() != nullptr);

    panel.addSignal(QStringLiteral("rig/temperature"));
    CHECK(panel.hasSignal(QStringLiteral("rig/temperature")));
    CHECK(panel.view()->hasSignal(QStringLiteral("rig/temperature")));
    panel.addSignal(QStringLiteral("rig/temperature"));  // idempotent
    CHECK(panel.view()->signalIds().size() == 1);

    panel.removeSignal(QStringLiteral("rig/temperature"));
    CHECK_FALSE(panel.hasSignal(QStringLiteral("rig/temperature")));
    CHECK(panel.view()->signalIds().isEmpty());
}

TEST_CASE("M23: PlotPanel reflects preconfigured signals into the view", "[dashboard][m23][plot]") {
    app();
    buf::SignalBufferRegistry reg;
    reg.onSignalsRegistered(QStringLiteral("rig"), {makeMeta(QStringLiteral("rig/pressure"), dec::SignalType::Double)});
    ch::TimeAxisManager axis;

    auto cfg = plotCfg();
    cfg.signalIds << QStringLiteral("rig/pressure");
    dash::PlotPanel panel(cfg, reg, axis);
    CHECK(panel.hasSignal(QStringLiteral("rig/pressure")));
    CHECK(panel.view()->hasSignal(QStringLiteral("rig/pressure")));
}
