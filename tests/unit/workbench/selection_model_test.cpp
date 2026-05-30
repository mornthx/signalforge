// tests/unit/workbench/selection_model_test.cpp
//
// M33 S1 — SelectionModel: current selection, change signal (deduped),
// clear, and isSelected.

#include "workbench/selection_model.hpp"

#include <QtTest/QSignalSpy>
#include <catch2/catch_test_macros.hpp>

namespace wb = signalforge::workbench;

TEST_CASE("M33 S1: starts with no selection", "[workbench][m33]") {
    wb::SelectionModel model;
    CHECK(model.current().isNone());
    CHECK(model.current().kind == wb::SelectionKind::None);
}

TEST_CASE("M33 S1: select sets the current and emits once", "[workbench][m33]") {
    wb::SelectionModel model;
    QSignalSpy spy(&model, &wb::SelectionModel::selectionChanged);

    model.select(wb::SelectionKind::Signal, QStringLiteral("udp:rig/temp"));
    CHECK(model.current().kind == wb::SelectionKind::Signal);
    CHECK(model.current().id == QStringLiteral("udp:rig/temp"));
    CHECK(model.isSelected(wb::SelectionKind::Signal, QStringLiteral("udp:rig/temp")));
    CHECK_FALSE(model.isSelected(wb::SelectionKind::Packet, QStringLiteral("udp:rig/temp")));
    REQUIRE(spy.count() == 1);
}

TEST_CASE("M33 S1: re-selecting the same thing does not re-emit", "[workbench][m33]") {
    wb::SelectionModel model;
    model.select(wb::SelectionKind::Signal, QStringLiteral("a"));
    QSignalSpy spy(&model, &wb::SelectionModel::selectionChanged);
    model.select(wb::SelectionKind::Signal, QStringLiteral("a"));  // identical
    CHECK(spy.count() == 0);
    // A different kind with the same id IS a change.
    model.select(wb::SelectionKind::Packet, QStringLiteral("a"));
    CHECK(spy.count() == 1);
}

TEST_CASE("M33 S1: clear resets to None and emits only when needed", "[workbench][m33]") {
    wb::SelectionModel model;
    QSignalSpy spy(&model, &wb::SelectionModel::selectionChanged);
    model.clear();  // already None → no emit
    CHECK(spy.count() == 0);

    model.select(wb::SelectionKind::Widget, QStringLiteral("panel-1"));
    model.clear();
    CHECK(model.current().isNone());
    CHECK(spy.count() == 2);  // select + clear
}
