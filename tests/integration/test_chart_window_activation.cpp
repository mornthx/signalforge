// tests/integration/test_chart_window_activation.cpp
//
// S9 integration test (spec §2.1-12 #6): verify Chart and
// ChartManager can be instantiated under a QGuiApplication
// without crashing (the window-activation path itself is in
// MainWindow::showEvent — covered by S8 main_window
// integration; full activation requires a window manager,
// which CI doesn't provide). This test confirms the API
// path doesn't crash and that ChartManager wires up correctly.

#include "buffer/signal_buffer_registry.hpp"
#include "chart/chart.hpp"
#include "chart/chart_manager.hpp"
#include "chart/time_axis_manager.hpp"

#include <QGuiApplication>
#include <QQuickWindow>
#include <catch2/catch_test_macros.hpp>

namespace ch8 = signalforge::chart;
namespace bm6 = signalforge::buffer;

namespace {

QGuiApplication& app() {
    static int argc = 1;
    static char arg0[] = "test_chart_window_activation";
    static char* argv[] = {arg0, nullptr};
    static QGuiApplication instance(argc, argv);
    return instance;
}

}  // namespace

TEST_CASE("S9 integration: ChartManager + Chart construct cleanly under QGuiApplication",
          "[integration][s9][chart][activation]") {
    app();
    bm6::SignalBufferRegistry registry;
    ch8::ChartManager manager(registry);
    REQUIRE(manager.chartIds().isEmpty());

    const QString id = manager.createChart();
    REQUIRE_FALSE(id.isEmpty());
    auto* chart = manager.chart(id);
    REQUIRE(chart != nullptr);
    REQUIRE(chart->stats().totalRedraws == 0);

    // The window-activation contract (showEvent + raise +
    // requestActivate) lives in MainWindow and is exercised
    // end-to-end during the M8 prototype bench (S10) and the
    // 1-hour soak (S11). This headless test confirms the
    // construction pipeline + add/remove flow works cleanly
    // under a QGuiApplication without crashing.
    REQUIRE(manager.removeChart(id));
    REQUIRE(manager.chartIds().isEmpty());
}
