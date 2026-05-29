// tests/unit/dashboard/table_panel_test.cpp
//
// M22 S1 — TablePanel: one row per signal (Signal/Value/Unit/Updated),
// value + age from queryLatestOne, add/removeSignal row management.

#include "buffer/signal_buffer_registry.hpp"
#include "dashboard/panel_types.hpp"
#include "dashboard/table_panel.hpp"
#include "decode/decoder_interface.hpp"

#include <QApplication>
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
    meta.unit = std::move(unit);
    meta.type = type;
    return meta;
}

// M6 buffer publishes to readers every 100 samples (see panels_test.cpp).
void pushUntilVisible(buf::SignalBufferRegistry& reg, const QString& id, const dec::SignalValue& v) {
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) {
        reg.onSignal(t0 + std::chrono::microseconds(i), id, v);
    }
}

dash::PanelConfig tableCfg(const QStringList& ids) {
    dash::PanelConfig cfg;
    cfg.id = QStringLiteral("panel-table-1");
    cfg.type = dash::PanelType::Table;
    cfg.signalIds = ids;
    return cfg;
}

}  // namespace

TEST_CASE("S1: TablePanel shows a row per signal with live values", "[dashboard][m22][table]") {
    app();
    buf::SignalBufferRegistry reg;
    reg.onSignalsRegistered(
        QStringLiteral("rig"),
        {
            makeMeta(QStringLiteral("rig/temperature"), dec::SignalType::Double, QStringLiteral("°C")),
            makeMeta(QStringLiteral("rig/alarm"), dec::SignalType::Bool, QString()),
            makeMeta(QStringLiteral("rig/fsm"), dec::SignalType::String, QString()),
        });

    dash::TablePanel panel(
        tableCfg({QStringLiteral("rig/temperature"), QStringLiteral("rig/alarm"), QStringLiteral("rig/fsm")}), reg);
    CHECK(panel.isMultiSignal());
    CHECK(panel.isWide());
    CHECK(panel.rowCount() == 3);
    CHECK(panel.valueTextFor(QStringLiteral("rig/temperature")) == QStringLiteral("—"));  // no data yet

    pushUntilVisible(reg, QStringLiteral("rig/temperature"), dec::SignalValue{24.7});
    pushUntilVisible(reg, QStringLiteral("rig/fsm"), dec::SignalValue{QStringLiteral("RUNNING")});
    panel.refresh();
    CHECK(panel.valueTextFor(QStringLiteral("rig/temperature")) == QStringLiteral("24.700"));
    CHECK(panel.valueTextFor(QStringLiteral("rig/fsm")) == QStringLiteral("RUNNING"));
    CHECK(panel.valueTextFor(QStringLiteral("rig/alarm")) == QStringLiteral("—"));  // no data pushed
}

TEST_CASE("S1: TablePanel add/removeSignal manages rows", "[dashboard][m22][table]") {
    app();
    buf::SignalBufferRegistry reg;
    reg.onSignalsRegistered(QStringLiteral("rig"),
                            {makeMeta(QStringLiteral("rig/temperature"), dec::SignalType::Double, QString())});

    dash::TablePanel panel(tableCfg({}), reg);
    CHECK(panel.rowCount() == 0);

    panel.addSignal(QStringLiteral("rig/temperature"));
    CHECK(panel.rowCount() == 1);
    CHECK(panel.hasSignal(QStringLiteral("rig/temperature")));
    // Idempotent.
    panel.addSignal(QStringLiteral("rig/temperature"));
    CHECK(panel.rowCount() == 1);

    panel.removeSignal(QStringLiteral("rig/temperature"));
    CHECK(panel.rowCount() == 0);
    CHECK_FALSE(panel.hasSignal(QStringLiteral("rig/temperature")));
}
