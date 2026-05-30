// tests/unit/inspect/parsed_signals_view_test.cpp
//
// M30 S2 — ParsedSignalsView: row population from the registry and live
// display-filter narrowing, driven by typing into the filter bar.

#include "buffer/signal_buffer_registry.hpp"
#include "decode/decoder_interface.hpp"
#include "inspect/parsed_signals_view.hpp"

#include <QAction>
#include <QApplication>
#include <QLineEdit>
#include <QMenu>
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
