// tests/unit/chart/chart_config_persistence_test.cpp
//
// S7 — ChartManager yaml save/load round-trip + error paths.
// The canonical schema lives at `schemas/charts_v1.yaml`.

#include "buffer/signal_buffer_registry.hpp"
#include "chart/chart_manager.hpp"

#include <QFile>
#include <QString>
#include <QTemporaryDir>
#include <catch2/catch_test_macros.hpp>

namespace ch8 = signalforge::chart;
namespace bm6 = signalforge::buffer;

namespace {

QString writeTempFile(QTemporaryDir& dir, const QString& name, const QString& content) {
    const auto path = dir.filePath(name);
    QFile f(path);
    REQUIRE(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(content.toUtf8());
    return path;
}

}  // namespace

TEST_CASE("S7: yaml save/load round-trip preserves charts and signals", "[chart][s7][persist]") {
    bm6::SignalBufferRegistry registry;
    ch8::ChartManager manager(registry);

    ch8::ChartConfig c1;
    c1.id = QStringLiteral("c1");
    c1.title = QStringLiteral("Chart One");
    {
        ch8::ChartSignalConfig s;
        s.signalId = QStringLiteral("driver-a/voltage");
        s.displayMode = ch8::SignalDisplayMode::Line;
        s.color = QColor("#7AC0FF");
        s.visible = true;
        c1.signalConfigs.push_back(std::move(s));
    }
    {
        ch8::ChartSignalConfig s;
        s.signalId = QStringLiteral("over_temperature");  // derived
        s.displayMode = ch8::SignalDisplayMode::Step;
        s.color = QColor("#FFDA7A");
        s.visible = false;
        c1.signalConfigs.push_back(std::move(s));
    }
    manager.createChart(c1);

    ch8::ChartConfig c2;
    c2.id = QStringLiteral("c2");
    c2.title = QStringLiteral("Chart Two");
    manager.createChart(c2);

    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    const auto path = tmp.filePath(QStringLiteral("charts.yaml"));
    manager.saveConfigToFile(path);
    REQUIRE(QFile::exists(path));

    bm6::SignalBufferRegistry registry2;
    ch8::ChartManager loaded(registry2);
    REQUIRE(loaded.loadConfigFromFile(path));
    REQUIRE(loaded.chartIds().size() == 2);
    REQUIRE(loaded.chartIds().at(0) == QStringLiteral("c1"));
    REQUIRE(loaded.chartIds().at(1) == QStringLiteral("c2"));

    auto* loadedC1 = loaded.chart(QStringLiteral("c1"));
    REQUIRE(loadedC1 != nullptr);
    const auto cfg = loadedC1->config();
    REQUIRE(cfg.title == QStringLiteral("Chart One"));
    REQUIRE(cfg.signalConfigs.size() == 2);
    REQUIRE(cfg.signalConfigs[0].signalId == QStringLiteral("driver-a/voltage"));
    REQUIRE(cfg.signalConfigs[0].displayMode == ch8::SignalDisplayMode::Line);
    REQUIRE(cfg.signalConfigs[0].color.name() == QStringLiteral("#7ac0ff"));
    REQUIRE(cfg.signalConfigs[0].visible);
    REQUIRE(cfg.signalConfigs[1].signalId == QStringLiteral("over_temperature"));
    REQUIRE(cfg.signalConfigs[1].displayMode == ch8::SignalDisplayMode::Step);
    REQUIRE_FALSE(cfg.signalConfigs[1].visible);
}

TEST_CASE("S7: loadConfigFromFile returns false on missing file", "[chart][s7][persist][error]") {
    bm6::SignalBufferRegistry registry;
    ch8::ChartManager manager(registry);
    manager.createChart();
    REQUIRE(manager.chartIds().size() == 1);

    REQUIRE_FALSE(manager.loadConfigFromFile(QStringLiteral("/tmp/__nope__.yaml")));
    // Existing manager state untouched on missing file.
    REQUIRE(manager.chartIds().size() == 1);
}

TEST_CASE("S7: loadConfigFromFile returns false on invalid yaml", "[chart][s7][persist][error]") {
    bm6::SignalBufferRegistry registry;
    ch8::ChartManager manager(registry);

    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    const auto path =
        writeTempFile(tmp, QStringLiteral("bad.yaml"), QStringLiteral("schema_version: 1\ncharts: [bogus: y\n"));
    REQUIRE_FALSE(manager.loadConfigFromFile(path));
}

TEST_CASE("S7: loadConfigFromFile rejects yaml without 'charts' sequence", "[chart][s7][persist][error]") {
    bm6::SignalBufferRegistry registry;
    ch8::ChartManager manager(registry);

    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    const auto path = writeTempFile(tmp, QStringLiteral("nokey.yaml"), QStringLiteral("schema_version: 1\n"));
    REQUIRE_FALSE(manager.loadConfigFromFile(path));
}

TEST_CASE("S7: load replaces existing manager state", "[chart][s7][persist]") {
    bm6::SignalBufferRegistry registry;
    ch8::ChartManager manager(registry);
    manager.createChart();
    manager.createChart();
    REQUIRE(manager.chartIds().size() == 2);

    // Save an empty fresh manager's state, then load into the
    // populated manager. Result: cleared.
    bm6::SignalBufferRegistry registry2;
    ch8::ChartManager fresh(registry2);
    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    const auto path = tmp.filePath(QStringLiteral("empty.yaml"));
    fresh.saveConfigToFile(path);

    REQUIRE(manager.loadConfigFromFile(path));
    REQUIRE(manager.chartIds().isEmpty());
}
