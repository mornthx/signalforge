// tests/unit/dashboard/dashboard_test.cpp
//
// M21 S4 — Dashboard: auto-suggest panel creation, dedup on re-add,
// plot panels, and panel/chart removal.

#include "buffer/signal_buffer_registry.hpp"
#include "dashboard/dashboard.hpp"
#include "dashboard/panel.hpp"
#include "dashboard/panel_config_dialog.hpp"
#include "dashboard/panel_types.hpp"
#include "decode/decoder_interface.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QSpinBox>
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
    // Intentionally leaked: destroying QApplication at static-exit runs
    // qt_call_post_routines(), which segfaults under the xcb platform on
    // Qt 6.10 once a QDialog (config dialog) has been created. The process is
    // exiting anyway, so leaking is harmless and avoids the teardown crash.
    static QApplication* instance = new QApplication(argc, argv);
    return *instance;
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

TEST_CASE("M34 P2: refresh rate is configurable and clamped", "[dashboard][m34]") {
    app();
    buf::SignalBufferRegistry reg;
    dash::Dashboard board(reg);
    CHECK(board.refreshRateHz() == 60);  // default

    board.setRefreshRateHz(15);
    CHECK(board.refreshRateHz() == 15);
    board.setRefreshRateHz(30);
    CHECK(board.refreshRateHz() == 30);

    // Clamped to [1, 144].
    board.setRefreshRateHz(0);
    CHECK(board.refreshRateHz() == 1);
    board.setRefreshRateHz(10000);
    CHECK(board.refreshRateHz() == 144);
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

TEST_CASE("M27: panel type/signal changes preserve identity; menu removes", "[dashboard][m27][interaction]") {
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

    // Change the widget type (M32: via the config path), preserving the signal.
    board.setPanelType(p1, dash::PanelType::Plot);
    REQUIRE(board.panel(p1) != nullptr);
    CHECK(board.panel(p1)->type() == dash::PanelType::Plot);
    CHECK(board.panel(p1)->hasSignal(QStringLiteral("rig/temp")));  // signal preserved

    // Assign a second signal to the (multi) plot.
    board.setPanelSignals(p1, {QStringLiteral("rig/temp"), QStringLiteral("rig/alarm")});
    CHECK(board.panel(p1)->hasSignal(QStringLiteral("rig/alarm")));
    CHECK(board.panel(p1)->hasSignal(QStringLiteral("rig/temp")));

    // "Remove panel" — still on the right-click menu.
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

    // Drag the card body by (+120, +60) → the panel moves and becomes user-placed.
    const QRect before = p->geometry();
    const QPoint g0 = p->mapToGlobal(QPoint(20, 10));
    send(p, QEvent::MouseButtonPress, g0);
    send(p, QEvent::MouseMove, g0 + QPoint(120, 60));
    send(p, QEvent::MouseButtonRelease, g0 + QPoint(120, 60));
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

TEST_CASE("M32 S3: config dialog reassigns signal and edits format", "[dashboard][m32][interaction]") {
    app();
    buf::SignalBufferRegistry reg;
    reg.onSignalsRegistered(QStringLiteral("rig"),
                            {
                                makeMeta(QStringLiteral("rig/temp"), dec::SignalType::Double),
                                makeMeta(QStringLiteral("rig/pressure"), dec::SignalType::Double),
                            });
    dash::Dashboard board(reg);
    const QString id = board.addSignal(QStringLiteral("rig/temp"));  // Numeric

    // Open the dialog, switch the bound signal temp→pressure, make it a Gauge
    // with an explicit range/unit/decimals, then apply.
    dash::PanelConfigDialog dlg(board.panel(id)->config(), reg.signalIds());
    for (int i = 0; i < dlg.signalList()->count(); ++i) {
        auto* it = dlg.signalList()->item(i);
        it->setCheckState(it->text() == QStringLiteral("rig/pressure") ? Qt::Checked : Qt::Unchecked);
    }
    dlg.typeCombo()->setCurrentIndex(dlg.typeCombo()->findData(static_cast<int>(dash::PanelType::Gauge)));
    dlg.limitRangeCheck()->setChecked(true);
    dlg.minSpin()->setValue(0.0);
    dlg.maxSpin()->setValue(250.0);
    dlg.unitEdit()->setText(QStringLiteral("kPa"));
    dlg.decimalsSpin()->setValue(1);
    board.applyPanelConfig(id, dlg.result());

    auto* p = board.panel(id);
    REQUIRE(p != nullptr);
    CHECK(p->type() == dash::PanelType::Gauge);
    CHECK(p->hasSignal(QStringLiteral("rig/pressure")));
    CHECK_FALSE(p->hasSignal(QStringLiteral("rig/temp")));
    CHECK(p->config().rangeMin.value_or(-1) == 0.0);
    CHECK(p->config().rangeMax.value_or(-1) == 250.0);
    CHECK(p->config().unitOverride == QStringLiteral("kPa"));
    CHECK(p->config().decimals == 1);
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

TEST_CASE("M29: re-checking a signal restores its last-chosen widget form", "[dashboard][m29][interaction]") {
    app();
    buf::SignalBufferRegistry reg;
    reg.onSignalsRegistered(QStringLiteral("rig"), {makeMeta(QStringLiteral("rig/temp"), dec::SignalType::Double)});
    dash::Dashboard board(reg);

    // Check the signal → defaults to Numeric (suggested for a Double).
    const QString id = board.addSignal(QStringLiteral("rig/temp"));
    REQUIRE(board.panel(id)->type() == dash::PanelType::Numeric);

    // User changes it to a Gauge (config path).
    board.setPanelType(id, dash::PanelType::Gauge);
    REQUIRE(board.panel(id)->type() == dash::PanelType::Gauge);

    // Uncheck → the widget disappears entirely (report 1).
    board.removeSignalEverywhere(QStringLiteral("rig/temp"));
    REQUIRE(board.panelCount() == 0);

    // Re-check → it returns as a Gauge (the remembered form), not the default
    // Numeric (report 2).
    const QString again = board.addSignal(QStringLiteral("rig/temp"));
    CHECK(board.panel(again)->type() == dash::PanelType::Gauge);
}

TEST_CASE("M29: unchecking a plot's last signal removes the empty panel", "[dashboard][m29]") {
    app();
    buf::SignalBufferRegistry reg;
    reg.onSignalsRegistered(QStringLiteral("rig"),
                            {
                                makeMeta(QStringLiteral("rig/temp"), dec::SignalType::Double),
                                makeMeta(QStringLiteral("rig/pressure"), dec::SignalType::Double),
                            });
    dash::Dashboard board(reg);
    const QString plot = board.addPlotPanel({QStringLiteral("rig/temp"), QStringLiteral("rig/pressure")});
    REQUIRE(board.panel(plot)->isMultiSignal());
    REQUIRE(board.panelCount() == 1);

    // Uncheck one signal → the plot is kept with the remaining trace.
    board.removeSignalEverywhere(QStringLiteral("rig/temp"));
    CHECK(board.panelCount() == 1);
    CHECK(board.showsSignal(QStringLiteral("rig/pressure")));

    // Uncheck the last signal → the now-empty plot is removed, not orphaned.
    board.removeSignalEverywhere(QStringLiteral("rig/pressure"));
    CHECK(board.panelCount() == 0);
}

namespace {
/// Synthesize a left-button mouse event at global point `g` on widget `w`.
void sendMouse(QWidget* w, QEvent::Type t, QPoint g) {
    QMouseEvent e(t, QPointF(w->mapFromGlobal(g)), QPointF(g), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(w, &e);
}
}  // namespace

TEST_CASE("M29: dragging a panel into a neighbor pushes it aside within bounds", "[dashboard][m29][interaction]") {
    app();
    buf::SignalBufferRegistry reg;
    reg.onSignalsRegistered(QStringLiteral("rig"), {
                                                       makeMeta(QStringLiteral("rig/a"), dec::SignalType::Double),
                                                       makeMeta(QStringLiteral("rig/b"), dec::SignalType::Double),
                                                   });
    dash::Dashboard board(reg);
    board.resize(900, 400);
    dash::Panel* a = board.panel(board.addSignal(QStringLiteral("rig/a")));
    dash::Panel* b = board.panel(board.addSignal(QStringLiteral("rig/b")));
    const int ax0 = a->geometry().x();
    const QRect gb0 = b->geometry();
    REQUIRE(gb0.x() > ax0);  // auto-placed side by side

    // Drag A rightward into B.
    const QPoint g0 = a->mapToGlobal(QPoint(20, 10));
    sendMouse(a, QEvent::MouseButtonPress, g0);
    sendMouse(a, QEvent::MouseMove, g0 + QPoint(150, 0));
    sendMouse(a, QEvent::MouseButtonRelease, g0 + QPoint(150, 0));

    CHECK(a->geometry().x() == ax0 + 150);                 // A followed the cursor
    CHECK(b->geometry() != gb0);                           // B was pushed aside
    CHECK_FALSE(a->geometry().intersects(b->geometry()));  // no overlap
    // B stayed entirely inside the surface (bounded push — never shoved off).
    CHECK(b->geometry().x() >= 0);
    CHECK(b->geometry().y() >= 0);
    CHECK(b->geometry().right() < 900);
    CHECK(b->geometry().bottom() < 400);
}

TEST_CASE("M29: a push with no room inside the viewport is refused", "[dashboard][m29][interaction]") {
    app();
    buf::SignalBufferRegistry reg;
    reg.onSignalsRegistered(QStringLiteral("rig"), {
                                                       makeMeta(QStringLiteral("rig/a"), dec::SignalType::Double),
                                                       makeMeta(QStringLiteral("rig/b"), dec::SignalType::Double),
                                                       makeMeta(QStringLiteral("rig/c"), dec::SignalType::Double),
                                                   });
    dash::Dashboard board(reg);
    board.resize(880, 170);  // exactly one card-row tall → B cannot be pushed vertically
    dash::Panel* a = board.panel(board.addSignal(QStringLiteral("rig/a")));
    dash::Panel* b = board.panel(board.addSignal(QStringLiteral("rig/b")));
    dash::Panel* c = board.panel(board.addSignal(QStringLiteral("rig/c")));
    const QRect ga0 = a->geometry();
    const QRect gb0 = b->geometry();
    const QRect gc0 = c->geometry();
    REQUIRE(gb0.x() > ga0.x());
    REQUIRE(gc0.x() > gb0.x());

    // Drag A into B: B has nowhere to go (a third panel right, no vertical room)
    // → the whole move is refused, nothing shifts.
    const QPoint g0 = a->mapToGlobal(QPoint(20, 10));
    sendMouse(a, QEvent::MouseButtonPress, g0);
    sendMouse(a, QEvent::MouseMove, g0 + QPoint(200, 0));
    sendMouse(a, QEvent::MouseButtonRelease, g0 + QPoint(200, 0));

    CHECK(a->geometry() == ga0);
    CHECK(b->geometry() == gb0);
    CHECK(c->geometry() == gc0);
}
