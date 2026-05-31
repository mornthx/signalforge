// tests/unit/inspect/parsed_signals_view_test.cpp
//
// M30 S2 — ParsedSignalsView: row population from the registry and live
// display-filter narrowing, driven by typing into the filter bar.

#include "buffer/signal_buffer_registry.hpp"
#include "decode/decoder_interface.hpp"
#include "inspect/parsed_signals_view.hpp"
#include "workbench/signal_identity.hpp"

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QLineEdit>
#include <QMenu>
#include <QPolygonF>
#include <QSet>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QtTest/QSignalSpy>
#include <catch2/catch_test_macros.hpp>
#include <chrono>

namespace insp = signalforge::inspect;
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

// The M6 buffer publishes to readers only every 100 samples; push a full
// cadence so queryLatestOne returns `v`.
void pushUntilVisible(buf::SignalBufferRegistry& reg, const QString& id, const dec::SignalValue& v) {
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) {
        reg.onSignal(t0 + std::chrono::microseconds(i), id, v);
    }
}

// Column indices mirror ParsedSignalsView's internal layout (M34 P2/P3):
// Name · Trend · Quality · Source · Value · Unit · Rate · Type · Age · Changed · Dashboard.
constexpr int kColName = 0;
constexpr int kColTrend = 1;
constexpr int kColQuality = 2;
constexpr int kColRate = 6;
constexpr int kColChanged = 9;
constexpr int kColDash = 10;
constexpr int kSparkPolyRole = Qt::UserRole + 1;

int rowOf(QTableWidget* table, const QString& id) {
    for (int r = 0; r < table->rowCount(); ++r) {
        if (table->item(r, kColName)->data(Qt::UserRole).toString() == id) {
            return r;
        }
    }
    return -1;
}

}  // namespace

TEST_CASE("M30 S2: parsed view lists every signal, no filter", "[inspect][m30]") {
    app();
    buf::SignalBufferRegistry reg;
    reg.onSignalsRegistered(
        QStringLiteral("rig"),
        {
            makeMeta(QStringLiteral("rig/temp"), dec::SignalType::Double, QStringLiteral("C")),
            makeMeta(QStringLiteral("rig/pressure"), dec::SignalType::Double, QStringLiteral("kPa")),
            makeMeta(QStringLiteral("rig/alarm"), dec::SignalType::Bool, QString()),
        });
    insp::ParsedSignalsView view(reg);
    view.refresh();
    CHECK(view.totalRowCount() == 3);
    CHECK(view.visibleRowCount() == 3);
    CHECK(view.filterValid());
}

TEST_CASE("M30 S2: typing a filter narrows the visible rows", "[inspect][m30][interaction]") {
    app();
    buf::SignalBufferRegistry reg;
    reg.onSignalsRegistered(
        QStringLiteral("rig"),
        {
            makeMeta(QStringLiteral("rig/temp"), dec::SignalType::Double, QStringLiteral("C")),
            makeMeta(QStringLiteral("rig/pressure"), dec::SignalType::Double, QStringLiteral("kPa")),
            makeMeta(QStringLiteral("rig/alarm"), dec::SignalType::Bool, QString()),
        });
    pushUntilVisible(reg, QStringLiteral("rig/temp"), 25.0);
    pushUntilVisible(reg, QStringLiteral("rig/pressure"), 101.0);
    pushUntilVisible(reg, QStringLiteral("rig/alarm"), true);

    insp::ParsedSignalsView view(reg);
    view.refresh();
    REQUIRE(view.totalRowCount() == 3);

    // Filter by a static field (unit).
    view.filterEdit()->setText(QStringLiteral("unit == kPa"));
    CHECK(view.filterValid());
    CHECK(view.visibleRowCount() == 1);

    // Filter by live value.
    view.filterEdit()->setText(QStringLiteral("value > 50"));
    CHECK(view.visibleRowCount() == 1);  // only pressure (101)

    // Filter by source — all three share the rig driver.
    view.filterEdit()->setText(QStringLiteral("source == rig"));
    CHECK(view.visibleRowCount() == 3);

    // Substring on the (derived) name.
    view.filterEdit()->setText(QStringLiteral("name contains temp"));
    CHECK(view.visibleRowCount() == 1);

    // Clearing restores all rows.
    view.filterEdit()->setText(QString());
    CHECK(view.visibleRowCount() == 3);
}

TEST_CASE("M30 S2: an invalid filter is flagged and leaves all rows visible", "[inspect][m30][interaction]") {
    app();
    buf::SignalBufferRegistry reg;
    reg.onSignalsRegistered(
        QStringLiteral("rig"),
        {
            makeMeta(QStringLiteral("rig/temp"), dec::SignalType::Double, QStringLiteral("C")),
            makeMeta(QStringLiteral("rig/pressure"), dec::SignalType::Double, QStringLiteral("kPa")),
        });
    insp::ParsedSignalsView view(reg);
    view.refresh();

    view.filterEdit()->setText(QStringLiteral("value =="));  // missing value → parse error
    CHECK_FALSE(view.filterValid());
    CHECK(view.visibleRowCount() == 2);  // invalid filter does not hide anything
}

TEST_CASE("M34 P3: group-by-driver inserts header rows; signals cluster by source", "[inspect][m34][interaction]") {
    app();
    buf::SignalBufferRegistry reg;
    reg.onSignalsRegistered(QStringLiteral("rigA"),
                            {makeMeta(QStringLiteral("rigA/temp"), dec::SignalType::Double, QStringLiteral("C"))});
    reg.onSignalsRegistered(QStringLiteral("rigB"),
                            {makeMeta(QStringLiteral("rigB/flow"), dec::SignalType::Double, QStringLiteral("L"))});

    insp::ParsedSignalsView view(reg);
    view.refresh();
    auto* t = view.table();

    // Flat: two data rows, no header rows.
    REQUIRE(view.totalRowCount() == 2);
    CHECK(t->rowCount() == 2);

    // Grouped: two driver headers + two data rows = four table rows; the data
    // count (totalRowCount) is unchanged.
    view.setGroupByDriver(true);
    CHECK(view.totalRowCount() == 2);
    CHECK(t->rowCount() == 4);
    CHECK(view.visibleRowCount() == 2);  // headers excluded from the signal count

    // A filter that selects only rigA's signal also hides rigB's now-empty header.
    view.filterEdit()->setText(QStringLiteral("source == rigA"));
    CHECK(view.visibleRowCount() == 1);
    int visibleTableRows = 0;
    for (int r = 0; r < t->rowCount(); ++r) {
        if (!t->isRowHidden(r)) {
            ++visibleTableRows;
        }
    }
    CHECK(visibleTableRows == 2);  // rigA header + rigA/temp row only

    // Toggling back off returns to a flat two-row table.
    view.filterEdit()->setText(QString());
    view.setGroupByDriver(false);
    CHECK(t->rowCount() == 2);
}

TEST_CASE("M34 P3: the Changed column reports time since the value last moved", "[inspect][m34][interaction]") {
    app();
    buf::SignalBufferRegistry reg;
    reg.onSignalsRegistered(QStringLiteral("rig"),
                            {makeMeta(QStringLiteral("rig/temp"), dec::SignalType::Double, QStringLiteral("C"))});
    pushUntilVisible(reg, QStringLiteral("rig/temp"), 25.0);

    insp::ParsedSignalsView view(reg);
    view.refresh();
    auto* t = view.table();
    const int row = rowOf(t, QStringLiteral("rig/temp"));
    REQUIRE(row >= 0);

    // A value with data shows a Changed duration (not the no-data dash).
    CHECK(t->item(row, kColChanged)->text() != QStringLiteral("—"));

    // `changed` is a filterable field (seconds since the value last moved).
    view.filterEdit()->setText(QStringLiteral("changed >= 0"));
    CHECK(view.filterValid());
    CHECK(view.visibleRowCount() == 1);
}

TEST_CASE("M34 P3: a signal name acts as a filter field (parity with Raw)", "[inspect][m34][interaction]") {
    app();
    buf::SignalBufferRegistry reg;
    reg.onSignalsRegistered(
        QStringLiteral("rig"),
        {
            makeMeta(QStringLiteral("rig/temp"), dec::SignalType::Double, QStringLiteral("C")),
            makeMeta(QStringLiteral("rig/pressure"), dec::SignalType::Double, QStringLiteral("kPa")),
        });
    pushUntilVisible(reg, QStringLiteral("rig/temp"), 25.0);
    pushUntilVisible(reg, QStringLiteral("rig/pressure"), 101.0);

    insp::ParsedSignalsView view(reg);
    view.refresh();
    REQUIRE(view.totalRowCount() == 2);

    // `temp` resolves to the temp row's value; pressure row reports it absent.
    view.filterEdit()->setText(QStringLiteral("temp > 20"));
    CHECK(view.filterValid());
    CHECK(view.visibleRowCount() == 1);

    // A threshold the temp value fails hides its row entirely.
    view.filterEdit()->setText(QStringLiteral("temp > 30"));
    CHECK(view.visibleRowCount() == 0);

    // Each signal name narrows to its own row.
    view.filterEdit()->setText(QStringLiteral("pressure > 100"));
    CHECK(view.visibleRowCount() == 1);

    view.filterEdit()->setText(QString());
    CHECK(view.visibleRowCount() == 2);
}

TEST_CASE("M32 S1: 'Add to dashboard' menu emits a typed promote request", "[inspect][m32][interaction]") {
    app();
    buf::SignalBufferRegistry reg;
    reg.onSignalsRegistered(QStringLiteral("rig"),
                            {makeMeta(QStringLiteral("rig/pressure"), dec::SignalType::Double, QStringLiteral("kPa"))});
    insp::ParsedSignalsView view(reg);
    view.refresh();

    QSignalSpy spy(&view, &insp::ParsedSignalsView::addToDashboardRequested);
    QMenu* menu = view.buildAddToDashboardMenu(QStringLiteral("rig/pressure"));
    // Find the "Gauge" action recursively and trigger it.
    QAction* gauge = nullptr;
    for (QAction* a : menu->findChildren<QAction*>()) {
        if (a->text() == QStringLiteral("Gauge")) {
            gauge = a;
            break;
        }
    }
    REQUIRE(gauge != nullptr);
    gauge->trigger();
    delete menu;

    REQUIRE(spy.count() == 1);
    const auto args = spy.takeFirst();
    CHECK(args.at(0).toString() == QStringLiteral("rig/pressure"));
    CHECK(args.at(1).toString() == QStringLiteral("gauge"));
}

TEST_CASE("M34 P2: parsed view shows quality, swatch, and the on-dashboard marker", "[inspect][m34][interaction]") {
    app();
    buf::SignalBufferRegistry reg;
    reg.onSignalsRegistered(
        QStringLiteral("rig"),
        {
            makeMeta(QStringLiteral("rig/temp"), dec::SignalType::Double, QStringLiteral("C")),
            makeMeta(QStringLiteral("rig/pressure"), dec::SignalType::Double, QStringLiteral("kPa")),
        });
    pushUntilVisible(reg, QStringLiteral("rig/temp"), 25.0);
    pushUntilVisible(reg, QStringLiteral("rig/pressure"), 101.0);

    insp::ParsedSignalsView view(reg);
    bool colorAsked = false;
    view.setSignalColorProvider([&colorAsked](const QString&) {
        colorAsked = true;
        return QColor(Qt::red);
    });
    view.setQualityColorProvider([](signalforge::workbench::Quality) { return QColor(Qt::green); });
    QSet<QString> onDash{QStringLiteral("rig/temp")};
    view.setDashboardMembershipProvider([&onDash](const QString& id) { return onDash.contains(id); });
    view.refresh();

    auto* t = view.table();
    const int rTemp = rowOf(t, QStringLiteral("rig/temp"));
    const int rPress = rowOf(t, QStringLiteral("rig/pressure"));
    REQUIRE(rTemp >= 0);
    REQUIRE(rPress >= 0);

    // Identity swatch: the provider is consulted and a colour decoration is set.
    CHECK(colorAsked);
    CHECK(t->item(rTemp, kColName)->data(Qt::DecorationRole).isValid());

    // Quality: a fresh push reads as "good".
    CHECK(t->item(rTemp, kColQuality)->text() == QStringLiteral("good"));

    // On-dashboard marker reflects membership.
    CHECK(t->item(rTemp, kColDash)->text() == QStringLiteral("● on"));
    CHECK(t->item(rPress, kColDash)->text().isEmpty());

    // Rate column populated and a sparkline polygon built from recent samples.
    CHECK(t->item(rTemp, kColRate)->text() != QStringLiteral("—"));
    CHECK(t->item(rTemp, kColTrend)->data(kSparkPolyRole).value<QPolygonF>().size() >= 2);

    // Both new fields are filterable (Wireshark-style display filter).
    view.filterEdit()->setText(QStringLiteral("quality == good"));
    CHECK(view.visibleRowCount() == 2);
    view.filterEdit()->setText(QStringLiteral("dashboard == true"));
    CHECK(view.visibleRowCount() == 1);
    view.filterEdit()->setText(QString());
    CHECK(view.visibleRowCount() == 2);
}

TEST_CASE("M34 P2: the row menu flips between Add and Remove", "[inspect][m34][interaction]") {
    app();
    buf::SignalBufferRegistry reg;
    reg.onSignalsRegistered(QStringLiteral("rig"),
                            {makeMeta(QStringLiteral("rig/pressure"), dec::SignalType::Double, QStringLiteral("kPa"))});
    insp::ParsedSignalsView view(reg);
    view.refresh();

    // Not on dashboard → the "Add to dashboard ▸" submenu (no Remove action).
    {
        QMenu* menu = view.buildRowMenu(QStringLiteral("rig/pressure"), /*onDashboard=*/false);
        bool hasRemove = false;
        bool hasAdd = false;
        for (QAction* a : menu->findChildren<QAction*>()) {
            hasRemove = hasRemove || a->text() == QStringLiteral("Remove from dashboard");
            hasAdd = hasAdd || a->text() == QStringLiteral("Gauge");
        }
        CHECK_FALSE(hasRemove);
        CHECK(hasAdd);
        delete menu;
    }

    // On dashboard → a single "Remove from dashboard" action that emits.
    {
        QSignalSpy spy(&view, &insp::ParsedSignalsView::removeFromDashboardRequested);
        QMenu* menu = view.buildRowMenu(QStringLiteral("rig/pressure"), /*onDashboard=*/true);
        QAction* remove = nullptr;
        for (QAction* a : menu->findChildren<QAction*>()) {
            if (a->text() == QStringLiteral("Remove from dashboard")) {
                remove = a;
            }
        }
        REQUIRE(remove != nullptr);
        remove->trigger();
        delete menu;
        REQUIRE(spy.count() == 1);
        CHECK(spy.takeFirst().at(0).toString() == QStringLiteral("rig/pressure"));
    }
}
