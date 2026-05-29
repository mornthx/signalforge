// tests/unit/dashboard/signal_list_panel_test.cpp
//
// M21 S5a — SignalListPanel: tree population/grouping, toggle routes to
// the dashboard, and checkbox state derives from the dashboard (so it
// stays consistent across refresh()).

#include "buffer/signal_buffer_registry.hpp"
#include "chart/chart_manager.hpp"
#include "dashboard/dashboard.hpp"
#include "dashboard/signal_list_panel.hpp"
#include "decode/decoder_interface.hpp"

#include <QApplication>
#include <QTreeWidget>
#include <QTreeWidgetItem>
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

QTreeWidgetItem* findLeaf(QTreeWidget* tree, const QString& signalId) {
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* group = tree->topLevelItem(i);
        for (int j = 0; j < group->childCount(); ++j) {
            QTreeWidgetItem* leaf = group->child(j);
            if (leaf->data(0, Qt::UserRole).toString() == signalId) {
                return leaf;
            }
        }
    }
    return nullptr;
}

}  // namespace

TEST_CASE("S5a: SignalListPanel populates and toggles into the dashboard", "[dashboard][s5][list]") {
    app();
    buf::SignalBufferRegistry reg;
    reg.onSignalsRegistered(QStringLiteral("rig"), {
                                                       makeMeta(QStringLiteral("rig/alarm"), dec::SignalType::Bool),
                                                       makeMeta(QStringLiteral("rig/temp"), dec::SignalType::Double),
                                                   });
    ch::ChartManager manager(reg);
    dash::Dashboard board(reg, manager);
    dash::SignalListPanel list(reg, board);

    auto* tree = list.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    auto* tempLeaf = findLeaf(tree, QStringLiteral("rig/temp"));
    REQUIRE(tempLeaf != nullptr);
    CHECK(tempLeaf->checkState(0) == Qt::Unchecked);

    // Check it → dashboard gains a panel for the signal.
    tempLeaf->setCheckState(0, Qt::Checked);
    CHECK(board.showsSignal(QStringLiteral("rig/temp")));
    CHECK(board.panelCount() == 1);

    // refresh() keeps the box checked because the dashboard is the source
    // of truth (the frozen SignalSelector would have reset it).
    list.refresh();
    auto* tempLeaf2 = findLeaf(list.findChild<QTreeWidget*>(), QStringLiteral("rig/temp"));
    REQUIRE(tempLeaf2 != nullptr);
    CHECK(tempLeaf2->checkState(0) == Qt::Checked);

    // Uncheck → removed from the dashboard.
    tempLeaf2->setCheckState(0, Qt::Unchecked);
    CHECK_FALSE(board.showsSignal(QStringLiteral("rig/temp")));
    CHECK(board.panelCount() == 0);
}

TEST_CASE("S5a: removing a panel unchecks the signal on next refresh", "[dashboard][s5][list]") {
    app();
    buf::SignalBufferRegistry reg;
    reg.onSignalsRegistered(QStringLiteral("rig"), {makeMeta(QStringLiteral("rig/temp"), dec::SignalType::Double)});
    ch::ChartManager manager(reg);
    dash::Dashboard board(reg, manager);
    dash::SignalListPanel list(reg, board);

    const QString panelId = board.addSignal(QStringLiteral("rig/temp"));
    list.refresh();
    CHECK(findLeaf(list.findChild<QTreeWidget*>(), QStringLiteral("rig/temp"))->checkState(0) == Qt::Checked);

    board.removePanel(panelId);  // e.g. user clicked the panel's Remove button
    list.refresh();
    CHECK(findLeaf(list.findChild<QTreeWidget*>(), QStringLiteral("rig/temp"))->checkState(0) == Qt::Unchecked);
}
