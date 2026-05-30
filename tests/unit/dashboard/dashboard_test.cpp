// tests/unit/dashboard/dashboard_test.cpp
//
// M21 S4 — Dashboard: auto-suggest panel creation, dedup on re-add,
// plot panels, and panel/chart removal.

#include "buffer/signal_buffer_registry.hpp"
#include "dashboard/dashboard.hpp"
#include "dashboard/panel.hpp"
#include "dashboard/panel_types.hpp"
#include "decode/decoder_interface.hpp"

#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QMouseEvent>
#include <QtTest/QSignalSpy>
#include <catch2/catch_test_macros.hpp>

namespace {
/// Recursively find a menu action whose text contains `needle`.
QAction* findAction(QMenu* menu, const QString& needle) {
    if (menu == nullptr) {
        return nullptr;
    }
    for (QAction* a : menu->actions()) {
        if (a->text().contains(needle)) {
            return a;
        }
        if (a->menu() != nullptr) {
            if (auto* sub = findAction(a->menu(), needle)) {
                return sub;
            }
        }
    }
    return nullptr;
}
}  // namespace

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

TEST_CASE("M27: panel ⋮ menu changes type, assigns signal, moves, removes", "[dashboard][m27][interaction]") {
    app();
    buf::SignalBufferRegistry reg;
    reg.onSignalsRegistered(QStringLiteral("rig"),
                            {
                                makeMeta(QStringLiteral("rig/temp"), dec::SignalType::Double),
                                makeMeta(QStringLiteral("rig/pressure"), dec::SignalType::Double),
                                makeMeta(QStringLiteral("rig/alarm"), dec::SignalType::Bool),
                            });
    dash::Dashboard board(reg);
    const QString p1 = board.addSignal(QStringLiteral("rig/temp"));      // Numeric (D1)
    const QString p2 = board.addSignal(QStringLiteral("rig/pressure"));  // Numeric
    REQUIRE(board.panel(p1)->type() == dash::PanelType::Numeric);
    CHECK(board.panelIds() == QStringList{p1, p2});

    // "Show as ▸ Plot" — change the widget type, preserving the signal.
    {
        QMenu* m = board.buildPanelMenu(p1);
        QAction* a = findAction(m, QStringLiteral("Plot"));
        REQUIRE(a != nullptr);
        a->trigger();
        delete m;
    }
    REQUIRE(board.panel(p1) != nullptr);
    CHECK(board.panel(p1)->type() == dash::PanelType::Plot);
    CHECK(board.panel(p1)->hasSignal(QStringLiteral("rig/temp")));  // signal preserved

    // "Signals ▸ rig/alarm" — assign a second signal to the (multi) plot.
    {
        QMenu* m = board.buildPanelMenu(p1);
        QAction* a = findAction(m, QStringLiteral("rig/alarm"));
        REQUIRE(a != nullptr);
        a->trigger();
        delete m;
    }
    CHECK(board.panel(p1)->hasSignal(QStringLiteral("rig/alarm")));
    CHECK(board.panel(p1)->hasSignal(QStringLiteral("rig/temp")));

    // "Remove panel" — remove via the menu.
    {
        QMenu* m = board.buildPanelMenu(p2);
        QAction* a = findAction(m, QStringLiteral("Remove panel"));
        REQUIRE(a != nullptr);
        a->trigger();
        delete m;
    }
    CHECK(board.panel(p2) == nullptr);
    CHECK(board.panelCount() == 1);
}

TEST_CASE("M28: drag the header moves a panel; the grip resizes it", "[dashboard][m28][interaction]") {
    app();
    buf::SignalBufferRegistry reg;
    reg.onSignalsRegistered(QStringLiteral("rig"), {makeMeta(QStringLiteral("rig/temp"), dec::SignalType::Double)});
    dash::Dashboard board(reg);
    board.resize(800, 600);  // give the surface real bounds so drags aren't clamped to 0
    const QString id = board.addSignal(QStringLiteral("rig/temp"));
    dash::Panel* p = board.panel(id);
    REQUIRE(p != nullptr);
    CHECK_FALSE(p->userPlaced());  // auto-placed initially

    // Synthesize a mouse stream with explicit global positions (independent of
    // widget realization) — simulates a real drag.
    auto send = [](QWidget* w, QEvent::Type t, QPoint g) {
        QMouseEvent e(t, QPointF(w->mapFromGlobal(g)), QPointF(g), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(w, &e);
    };

    // Drag the header by (+120, +60) → the panel moves and becomes user-placed.
    const QRect before = p->geometry();
    const QPoint g0 = p->header()->mapToGlobal(QPoint(20, 6));
    send(p->header(), QEvent::MouseButtonPress, g0);
    send(p->header(), QEvent::MouseMove, g0 + QPoint(120, 60));
    send(p->header(), QEvent::MouseButtonRelease, g0 + QPoint(120, 60));
    CHECK(p->userPlaced());
    CHECK(p->geometry().topLeft() == before.topLeft() + QPoint(120, 60));

    // Drag the bottom-right grip by (+50, +40) → the panel resizes.
    QSignalSpy spy(p, &dash::Panel::geometryChanged);
    const QSize sz0 = p->size();
    const QPoint h0 = p->resizeGrip()->mapToGlobal(QPoint(7, 7));
    send(p->resizeGrip(), QEvent::MouseButtonPress, h0);
    send(p->resizeGrip(), QEvent::MouseMove, h0 + QPoint(50, 40));
    send(p->resizeGrip(), QEvent::MouseButtonRelease, h0 + QPoint(50, 40));
    CHECK(p->size() == sz0 + QSize(50, 40));
    CHECK(spy.count() >= 1);
}

TEST_CASE("M28: a user-placed geometry survives a type change", "[dashboard][m28]") {
    app();
    buf::SignalBufferRegistry reg;
    reg.onSignalsRegistered(QStringLiteral("rig"), {makeMeta(QStringLiteral("rig/temp"), dec::SignalType::Double)});
    dash::Dashboard board(reg);
    board.resize(800, 600);
    const QString id = board.addSignal(QStringLiteral("rig/temp"));
    // Pin a custom geometry, then change the widget type.
    board.setPanelSignals(id, {QStringLiteral("rig/temp")});  // no-op recreate keeps things stable
    dash::Panel* p = board.panel(id);
    const QRect g(40, 50, 360, 220);
    const QPoint h0 = p->resizeGrip()->mapToGlobal(QPoint(7, 7));
    // simulate move+resize to set a user geometry deterministically
    auto send = [](QWidget* w, QEvent::Type t, QPoint gg) {
        QMouseEvent e(t, QPointF(w->mapFromGlobal(gg)), QPointF(gg), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(w, &e);
    };
    send(p->resizeGrip(), QEvent::MouseButtonPress, h0);
    send(p->resizeGrip(), QEvent::MouseButtonRelease, h0);  // commits current geometry as user-placed
    REQUIRE(p->userPlaced());
    const QRect kept = p->geometry();

    board.setPanelType(id, dash::PanelType::Gauge);
    dash::Panel* fresh = board.panel(id);
    REQUIRE(fresh != nullptr);
    CHECK(fresh->type() == dash::PanelType::Gauge);
    CHECK(fresh->userPlaced());
    CHECK(fresh->geometry() == kept);
}

TEST_CASE("M27: single-signal panel's menu reassigns to one signal", "[dashboard][m27][interaction]") {
    app();
    buf::SignalBufferRegistry reg;
    reg.onSignalsRegistered(QStringLiteral("rig"),
                            {
                                makeMeta(QStringLiteral("rig/temp"), dec::SignalType::Double),
                                makeMeta(QStringLiteral("rig/pressure"), dec::SignalType::Double),
                            });
    dash::Dashboard board(reg);
    const QString id = board.addSignal(QStringLiteral("rig/temp"));  // Numeric
    {
        QMenu* m = board.buildPanelMenu(id);
        findAction(m, QStringLiteral("rig/pressure"))->trigger();  // pick a specific signal
        delete m;
    }
    // Single-signal widget: now shows pressure, not temp.
    CHECK(board.panel(id)->hasSignal(QStringLiteral("rig/pressure")));
    CHECK_FALSE(board.panel(id)->hasSignal(QStringLiteral("rig/temp")));
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
