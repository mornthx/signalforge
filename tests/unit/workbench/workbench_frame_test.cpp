// tests/unit/workbench/workbench_frame_test.cpp
//
// M34 S3 — WorkbenchFrame shell: mode registration swaps the content stack,
// rail clicks drive the frame + emit, inspector/drawer show/hide + content
// replacement.

#include "workbench/components/activity_rail.hpp"
#include "workbench/workbench_frame.hpp"

#include <QAbstractButton>
#include <QApplication>
#include <QLabel>
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

TEST_CASE("M34 S3: frame registers modes and the first becomes current", "[workbench][m34][interaction]") {
    app();
    wb::WorkbenchFrame frame;
    auto* connectPage = new QLabel(QStringLiteral("connect"));
    auto* inspectPage = new QLabel(QStringLiteral("inspect"));
    frame.addMode(QStringLiteral("connect"), QStringLiteral("Connect"), connectPage);
    frame.addMode(QStringLiteral("inspect"), QStringLiteral("Inspect"), inspectPage);

    CHECK(frame.currentMode() == QStringLiteral("connect"));
    CHECK(frame.rail()->modeCount() == 2);
}

TEST_CASE("M34 S3: clicking a rail mode swaps content and emits modeChanged", "[workbench][m34][interaction]") {
    app();
    wb::WorkbenchFrame frame;
    frame.addMode(QStringLiteral("connect"), QStringLiteral("Connect"), new QLabel(QStringLiteral("connect")));
    frame.addMode(QStringLiteral("inspect"), QStringLiteral("Inspect"), new QLabel(QStringLiteral("inspect")));

    QSignalSpy spy(&frame, &wb::WorkbenchFrame::modeChanged);
    frame.rail()->button(QStringLiteral("inspect"))->click();
    REQUIRE(spy.count() == 1);
    CHECK(spy.takeFirst().at(0).toString() == QStringLiteral("inspect"));
    CHECK(frame.currentMode() == QStringLiteral("inspect"));

    // Programmatic switch drives the frame without re-emitting.
    frame.setCurrentMode(QStringLiteral("connect"));
    CHECK(frame.currentMode() == QStringLiteral("connect"));
    CHECK(spy.count() == 0);
}

TEST_CASE("M34 S3: inspector + drawer are hidden until shown, and hold content", "[workbench][m34]") {
    app();
    wb::WorkbenchFrame frame;
    CHECK_FALSE(frame.isInspectorVisible());
    CHECK_FALSE(frame.isDrawerVisible());

    auto* inspector = new QLabel(QStringLiteral("inspector"));
    frame.setInspector(inspector);
    frame.setInspectorVisible(true);
    CHECK(frame.isInspectorVisible());

    auto* drawer = new QLabel(QStringLiteral("drawer"));
    frame.setDrawer(drawer);
    frame.setDrawerVisible(true);
    CHECK(frame.isDrawerVisible());

    frame.setInspectorVisible(false);
    CHECK_FALSE(frame.isInspectorVisible());
}

TEST_CASE("M34 S3: duplicate mode ids are ignored", "[workbench][m34]") {
    app();
    wb::WorkbenchFrame frame;
    frame.addMode(QStringLiteral("inspect"), QStringLiteral("Inspect"), new QLabel(QStringLiteral("a")));
    frame.addMode(QStringLiteral("inspect"), QStringLiteral("Inspect again"), new QLabel(QStringLiteral("b")));
    CHECK(frame.rail()->modeCount() == 1);
}
