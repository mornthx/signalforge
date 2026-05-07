// tests/unit/chart/chart_manager_test.cpp
//
// S5 — ChartManager: createChart / removeChart / chart / chartIds /
// activeChartId tracking. yaml save/load (deferred to S7) is exercised
// by chart_config_persistence_test in S7.

#include "buffer/signal_buffer_registry.hpp"
#include "chart/chart_manager.hpp"

#include <QString>
#include <catch2/catch_test_macros.hpp>

namespace ch8 = signalforge::chart;
namespace bm6 = signalforge::buffer;

TEST_CASE("S5: empty manager has no charts and no active id", "[chart][s5][manager]") {
    bm6::SignalBufferRegistry registry;
    ch8::ChartManager manager(registry);
    REQUIRE(manager.chartIds().isEmpty());
    REQUIRE(manager.activeChartId().isEmpty());
    REQUIRE(manager.chart(QStringLiteral("nonexistent")) == nullptr);
}

TEST_CASE("S5: createChart returns a unique id and registers it", "[chart][s5][manager]") {
    bm6::SignalBufferRegistry registry;
    ch8::ChartManager manager(registry);

    const QString id1 = manager.createChart();
    REQUIRE_FALSE(id1.isEmpty());
    REQUIRE(manager.chartIds().size() == 1);
    REQUIRE(manager.chart(id1) != nullptr);
    REQUIRE(manager.activeChartId() == id1);  // first chart becomes active

    const QString id2 = manager.createChart();
    REQUIRE_FALSE(id2.isEmpty());
    REQUIRE(id2 != id1);
    REQUIRE(manager.chartIds().size() == 2);
    REQUIRE(manager.chartIds().at(0) == id1);
    REQUIRE(manager.chartIds().at(1) == id2);
    // Active stays at id1 (first-set; not last-clicked yet).
    REQUIRE(manager.activeChartId() == id1);
}

TEST_CASE("S5: createChart with explicit id honors it when free", "[chart][s5][manager]") {
    bm6::SignalBufferRegistry registry;
    ch8::ChartManager manager(registry);

    ch8::ChartConfig cfg;
    cfg.id = QStringLiteral("my-custom-id");
    cfg.title = QStringLiteral("Custom");
    const QString returnedId = manager.createChart(cfg);
    REQUIRE(returnedId == QStringLiteral("my-custom-id"));
    REQUIRE(manager.chart(QStringLiteral("my-custom-id")) != nullptr);
}

TEST_CASE("S5: createChart with colliding id falls back to generated", "[chart][s5][manager]") {
    bm6::SignalBufferRegistry registry;
    ch8::ChartManager manager(registry);

    ch8::ChartConfig cfg;
    cfg.id = QStringLiteral("dup");
    const QString first = manager.createChart(cfg);
    const QString second = manager.createChart(cfg);
    REQUIRE(first == QStringLiteral("dup"));
    REQUIRE(second != QStringLiteral("dup"));
    REQUIRE(manager.chartIds().size() == 2);
}

TEST_CASE("S5: removeChart drops the entry and updates active id", "[chart][s5][manager]") {
    bm6::SignalBufferRegistry registry;
    ch8::ChartManager manager(registry);
    const QString id1 = manager.createChart();
    const QString id2 = manager.createChart();
    const QString id3 = manager.createChart();
    REQUIRE(manager.activeChartId() == id1);

    REQUIRE(manager.removeChart(id1));
    // active rotates to the new front of chartOrder_.
    REQUIRE(manager.activeChartId() == id2);
    REQUIRE(manager.chartIds().size() == 2);
    REQUIRE(manager.chart(id1) == nullptr);

    REQUIRE_FALSE(manager.removeChart(QStringLiteral("nonexistent")));

    REQUIRE(manager.removeChart(id2));
    REQUIRE(manager.removeChart(id3));
    REQUIRE(manager.chartIds().isEmpty());
    REQUIRE(manager.activeChartId().isEmpty());
}

TEST_CASE("S5: setActiveChartId rotates between known charts", "[chart][s5][manager]") {
    bm6::SignalBufferRegistry registry;
    ch8::ChartManager manager(registry);
    const QString id1 = manager.createChart();
    const QString id2 = manager.createChart();
    REQUIRE(manager.activeChartId() == id1);

    manager.setActiveChartId(id2);
    REQUIRE(manager.activeChartId() == id2);

    // Setting to the same id is a no-op.
    manager.setActiveChartId(id2);
    REQUIRE(manager.activeChartId() == id2);

    // Unknown id is ignored (does not change activeChartId).
    manager.setActiveChartId(QStringLiteral("phantom"));
    REQUIRE(manager.activeChartId() == id2);
}

TEST_CASE("S5: timeAxis() returns the manager-owned shared axis", "[chart][s5][manager]") {
    bm6::SignalBufferRegistry registry;
    ch8::ChartManager manager(registry);
    auto& axis = manager.timeAxis();
    REQUIRE(axis.liveMode());

    // Mutating the axis directly is observable on the same reference.
    axis.pause();
    REQUIRE_FALSE(manager.timeAxis().liveMode());
}
