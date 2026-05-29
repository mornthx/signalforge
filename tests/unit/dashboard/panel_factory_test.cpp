// tests/unit/dashboard/panel_factory_test.cpp
//
// M21 S1 — panel type suggestion, type-name round-trip, PanelConfig
// defaults, and the Panel base chrome (header title + remove signal).

#include "dashboard/panel.hpp"
#include "dashboard/panel_factory.hpp"
#include "dashboard/panel_types.hpp"
#include "decode/decoder_interface.hpp"

#include <QApplication>
#include <QPushButton>
#include <QSignalSpy>
#include <QtTest/QTest>
#include <catch2/catch_test_macros.hpp>

namespace dash = signalforge::dashboard;
namespace dec = signalforge::decoder;

namespace {

/// Lazy single QApplication for this binary (Catch2WithMain makes none).
QApplication& app() {
    static int argc = 1;
    static char arg0[] = "test";
    static char* argv[] = {arg0, nullptr};
    static QApplication instance(argc, argv);
    return instance;
}

}  // namespace

TEST_CASE("S1: suggestPanelType maps signal types", "[dashboard][s1][factory]") {
    CHECK(dash::suggestPanelType(dec::SignalType::Bool) == dash::PanelType::State);
    CHECK(dash::suggestPanelType(dec::SignalType::String) == dash::PanelType::State);
    CHECK(dash::suggestPanelType(dec::SignalType::Int64) == dash::PanelType::Numeric);
    CHECK(dash::suggestPanelType(dec::SignalType::Double) == dash::PanelType::Numeric);
}

TEST_CASE("S1: panelTypeName round-trips", "[dashboard][s1][factory]") {
    for (auto t : {dash::PanelType::Plot, dash::PanelType::Numeric, dash::PanelType::State}) {
        const auto name = dash::panelTypeName(t);
        const auto back = dash::panelTypeFromName(name);
        REQUIRE(back.has_value());
        CHECK(*back == t);
    }
    CHECK_FALSE(dash::panelTypeFromName(QStringLiteral("bogus")).has_value());
    // Case / whitespace tolerant.
    CHECK(dash::panelTypeFromName(QStringLiteral("  PLOT ")) == dash::PanelType::Plot);
}

TEST_CASE("S1: PanelConfig zero-config defaults", "[dashboard][s1][config]") {
    dash::PanelConfig cfg;
    CHECK(cfg.type == dash::PanelType::Numeric);
    CHECK(cfg.signalIds.isEmpty());
    CHECK_FALSE(cfg.rangeMin.has_value());
    CHECK_FALSE(cfg.rangeMax.has_value());
    CHECK(cfg.unitOverride.isEmpty());
    CHECK(cfg.decimals == 3);
}

TEST_CASE("S1: Panel exposes config and its ⋮ button emits configureRequested", "[dashboard][s1][panel]") {
    app();
    dash::PanelConfig cfg;
    cfg.id = QStringLiteral("panel-7");
    cfg.type = dash::PanelType::Numeric;
    cfg.title = QStringLiteral("My panel");
    cfg.signalIds << QStringLiteral("udp:rig/temperature");

    dash::Panel panel(cfg);
    CHECK(panel.id() == QStringLiteral("panel-7"));
    CHECK(panel.type() == dash::PanelType::Numeric);
    CHECK(panel.hasSignal(QStringLiteral("udp:rig/temperature")));
    CHECK_FALSE(panel.hasSignal(QStringLiteral("udp:rig/pressure")));
    CHECK_FALSE(panel.isWide());

    // Simulate a real click on the always-visible ⋮ config button.
    QSignalSpy spy(&panel, &dash::Panel::configureRequested);
    auto* btn = panel.configButton();
    REQUIRE(btn != nullptr);
    CHECK(btn->isVisibleTo(&panel));
    QTest::mouseClick(btn, Qt::LeftButton);
    REQUIRE(spy.count() == 1);
    CHECK(spy.takeFirst().at(0).toString() == QStringLiteral("panel-7"));
}
