// tests/unit/workbench/components_test.cpp
//
// M34 S1 — component primitives: ActivityRail selection, StatusChip state,
// SectionHeader, EmptyState actions.

#include "workbench/components/activity_rail.hpp"
#include "workbench/components/empty_state.hpp"
#include "workbench/components/section_header.hpp"
#include "workbench/components/status_chip.hpp"

#include <QAbstractButton>
#include <QApplication>
#include <QPushButton>
#include <QtTest/QSignalSpy>
#include <catch2/catch_test_macros.hpp>

namespace wb = signalforge::workbench;

namespace {
QApplication& app() {
    static int argc = 1;
    static char arg0[] = "test";
    static char* argv[] = {arg0, nullptr};
    // Leaked on purpose — see memory qt_xcb_teardown_crash.
    static QApplication* instance = new QApplication(argc, argv);
    return *instance;
}
}  // namespace

TEST_CASE("M34 S1: activity rail selects modes and emits", "[workbench][m34][interaction]") {
    app();
    wb::ActivityRail rail;
    rail.addMode(QStringLiteral("connect"), QStringLiteral("Connect"));
    rail.addMode(QStringLiteral("inspect"), QStringLiteral("Inspect"));
    CHECK(rail.modeCount() == 2);
    CHECK(rail.currentMode() == QStringLiteral("connect"));  // first is current

    QSignalSpy spy(&rail, &wb::ActivityRail::modeSelected);
    rail.button(QStringLiteral("inspect"))->click();
    REQUIRE(spy.count() == 1);
    CHECK(spy.takeFirst().at(0).toString() == QStringLiteral("inspect"));
    CHECK(rail.currentMode() == QStringLiteral("inspect"));

    // Programmatic set does not re-emit.
    rail.setCurrentMode(QStringLiteral("connect"));
    CHECK(rail.currentMode() == QStringLiteral("connect"));
    CHECK(spy.count() == 0);
}

TEST_CASE("M34 S1: status chip carries text and a semantic state", "[workbench][m34]") {
    app();
    wb::StatusChip chip(QStringLiteral("Live"), QStringLiteral("live"));
    CHECK(chip.text() == QStringLiteral("Live"));
    CHECK(chip.state() == QStringLiteral("live"));
    CHECK(chip.property("state").toString() == QStringLiteral("live"));
    chip.setState(QStringLiteral("recording"));
    CHECK(chip.property("state").toString() == QStringLiteral("recording"));
}

TEST_CASE("M34 S1: section header title + caption", "[workbench][m34]") {
    app();
    wb::SectionHeader header(QStringLiteral("Parsed signals"));
    CHECK(header.title() == QStringLiteral("Parsed signals"));
    header.setCaption(QStringLiteral("9 signals"));
    CHECK(header.caption() == QStringLiteral("9 signals"));
}

TEST_CASE("M34 S1: empty state holds copy and returns its actions", "[workbench][m34][interaction]") {
    app();
    wb::EmptyState empty;
    empty.setTitle(QStringLiteral("Start a workflow"));
    empty.setCaption(QStringLiteral("Connect a device."));
    QPushButton* add = empty.addAction(QStringLiteral("Add connection"), /*primary=*/true);
    REQUIRE(add != nullptr);
    CHECK(add->text() == QStringLiteral("Add connection"));
    CHECK(add->property("class").toString() == QStringLiteral("primary"));

    QSignalSpy spy(add, &QPushButton::clicked);
    add->click();
    CHECK(spy.count() == 1);
}
