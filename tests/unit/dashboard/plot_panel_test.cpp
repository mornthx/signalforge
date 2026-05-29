// tests/unit/dashboard/plot_panel_test.cpp
//
// M21 S3 — PlotPanel wraps a legacy chart::Chart: hosts a QQuickWidget,
// delegates add/removeSignal to the chart, is "wide", and does not
// delete the manager-owned chart on destruction.

#include "buffer/signal_buffer_registry.hpp"
#include "chart/chart.hpp"
#include "chart/chart_manager.hpp"
#include "dashboard/panel_types.hpp"
#include "dashboard/plot_panel.hpp"
#include "decode/decoder_interface.hpp"

#include <QApplication>
#include <QQuickWidget>
#include <catch2/catch_test_macros.hpp>
#include <memory>

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

TEST_CASE("S3: PlotPanel hosts a chart and delegates signal add/remove", "[dashboard][s3][plot]") {
    app();
    buf::SignalBufferRegistry reg;
    reg.onSignalsRegistered(QStringLiteral("rig"),
                            {makeMeta(QStringLiteral("rig/temperature"), dec::SignalType::Double)});
    ch::ChartManager manager(reg);
    const QString chartId = manager.createChart();
    auto* chart = manager.chart(chartId);
    REQUIRE(chart != nullptr);

    auto panel = std::make_unique<dash::PlotPanel>(plotCfg(), chart);
    CHECK(panel->isWide());
    CHECK(panel->chart() == chart);
    CHECK(panel->findChild<QQuickWidget*>() != nullptr);

    panel->addSignal(QStringLiteral("rig/temperature"));
    CHECK(panel->hasSignal(QStringLiteral("rig/temperature")));
    CHECK(chart->visibleSignals().contains(QStringLiteral("rig/temperature")));
    // Idempotent.
    panel->addSignal(QStringLiteral("rig/temperature"));
    CHECK(chart->visibleSignals().size() == 1);

    panel->removeSignal(QStringLiteral("rig/temperature"));
    CHECK_FALSE(panel->hasSignal(QStringLiteral("rig/temperature")));
    CHECK(chart->visibleSignals().isEmpty());

    // Destroying the panel must NOT delete the manager-owned chart.
    panel.reset();
    CHECK(manager.chart(chartId) == chart);
}

TEST_CASE("S3: PlotPanel reflects preconfigured signals into the chart", "[dashboard][s3][plot]") {
    app();
    buf::SignalBufferRegistry reg;
    reg.onSignalsRegistered(QStringLiteral("rig"), {makeMeta(QStringLiteral("rig/pressure"), dec::SignalType::Double)});
    ch::ChartManager manager(reg);
    auto* chart = manager.chart(manager.createChart());
    REQUIRE(chart != nullptr);

    auto cfg = plotCfg();
    cfg.signalIds << QStringLiteral("rig/pressure");
    dash::PlotPanel panel(cfg, chart);
    CHECK(panel.hasSignal(QStringLiteral("rig/pressure")));
    CHECK(chart->visibleSignals().contains(QStringLiteral("rig/pressure")));
}
