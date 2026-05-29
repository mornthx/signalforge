// tests/unit/dashboard/dashboard_test.cpp
//
// M21 S4 — Dashboard: auto-suggest panel creation, dedup on re-add,
// plot panels, and panel/chart removal.

#include "buffer/signal_buffer_registry.hpp"
#include "dashboard/dashboard.hpp"
#include "dashboard/panel.hpp"
#include "dashboard/panel_types.hpp"
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

}  // namespace

TEST_CASE("S4: addSignal auto-suggests panel type and dedups", "[dashboard][s4]") {
    app();
    buf::SignalBufferRegistry reg;
    reg.onSignalsRegistered(QStringLiteral("rig"), {
                                                       makeMeta(QStringLiteral("rig/alarm"), dec::SignalType::Bool),
                                                       makeMeta(QStringLiteral("rig/temp"), dec::SignalType::Double),
                                                   });
    dash::Dashboard board(reg);
    CHECK(board.panelCount() == 0);

    const QString boolPanel = board.addSignal(QStringLiteral("rig/alarm"));
    REQUIRE(board.panelCount() == 1);
    REQUIRE(board.panel(boolPanel) != nullptr);
    CHECK(board.panel(boolPanel)->type() == dash::PanelType::State);

    const QString numPanel = board.addSignal(QStringLiteral("rig/temp"));
    CHECK(board.panelCount() == 2);
    CHECK(board.panel(numPanel)->type() == dash::PanelType::Numeric);

    // Re-adding an already-shown signal returns the same panel, no growth.
    const QString again = board.addSignal(QStringLiteral("rig/alarm"));
    CHECK(again == boolPanel);
    CHECK(board.panelCount() == 2);
}

TEST_CASE("S4: addPlotPanel creates a wide plot", "[dashboard][s4][plot]") {
    app();
    buf::SignalBufferRegistry reg;
    dash::Dashboard board(reg);

    const QString plotId = board.addPlotPanel();
    REQUIRE(board.panel(plotId) != nullptr);
    CHECK(board.panel(plotId)->type() == dash::PanelType::Plot);
    CHECK(board.panel(plotId)->isWide());

    board.removePanel(plotId);
    CHECK(board.panelCount() == 0);
}

TEST_CASE("M22: addTablePanel creates a wide table hosting N signals", "[dashboard][m22][table]") {
    app();
    buf::SignalBufferRegistry reg;
    reg.onSignalsRegistered(QStringLiteral("rig"), {
                                                       makeMeta(QStringLiteral("rig/temp"), dec::SignalType::Double),
                                                       makeMeta(QStringLiteral("rig/alarm"), dec::SignalType::Bool),
                                                   });
    dash::Dashboard board(reg);

    const QString tableId = board.addTablePanel({QStringLiteral("rig/temp"), QStringLiteral("rig/alarm")});
    auto* p = board.panel(tableId);
    REQUIRE(p != nullptr);
    CHECK(p->type() == dash::PanelType::Table);
    CHECK(p->isWide());
    CHECK(p->isMultiSignal());
    CHECK(board.showsSignal(QStringLiteral("rig/temp")));
    CHECK(board.showsSignal(QStringLiteral("rig/alarm")));

    // Unticking a tabled signal drops its row, not the whole panel.
    board.removeSignalEverywhere(QStringLiteral("rig/temp"));
    CHECK(board.panelCount() == 1);
    CHECK_FALSE(board.showsSignal(QStringLiteral("rig/temp")));
    CHECK(board.showsSignal(QStringLiteral("rig/alarm")));
}

TEST_CASE("M24: addBarPanel / addGaugePanel create meter panels", "[dashboard][m24]") {
    app();
    buf::SignalBufferRegistry reg;
    reg.onSignalsRegistered(QStringLiteral("rig"), {makeMeta(QStringLiteral("rig/level"), dec::SignalType::Double)});
    dash::Dashboard board(reg);

    const QString barId = board.addBarPanel(QStringLiteral("rig/level"));
    REQUIRE(board.panel(barId) != nullptr);
    CHECK(board.panel(barId)->type() == dash::PanelType::Bar);
    CHECK(board.showsSignal(QStringLiteral("rig/level")));

    const QString gaugeId = board.addGaugePanel(QStringLiteral("rig/level"));
    CHECK(board.panel(gaugeId)->type() == dash::PanelType::Gauge);
    CHECK(board.panelCount() == 2);
}

TEST_CASE("S4: removeSignalEverywhere drops single-signal cards", "[dashboard][s4]") {
    app();
    buf::SignalBufferRegistry reg;
    reg.onSignalsRegistered(QStringLiteral("rig"), {makeMeta(QStringLiteral("rig/alarm"), dec::SignalType::Bool)});
    dash::Dashboard board(reg);

    board.addSignal(QStringLiteral("rig/alarm"));
    REQUIRE(board.panelCount() == 1);
    board.removeSignalEverywhere(QStringLiteral("rig/alarm"));
    CHECK(board.panelCount() == 0);
}
