// tests/unit/chart/signal_selector_tree_test.cpp
//
// S6 — SignalSelector tree population, filter, refresh. The
// itemChanged checkbox handler requires a Qt event loop and a
// realized widget; that path is exercised in S9
// `test_signal_selector_tree_population.cpp`. Here we cover the
// tree-building + filter + refresh logic that doesn't need the
// event loop.

#include "buffer/signal_buffer_registry.hpp"
#include "chart/chart_manager.hpp"
#include "chart/signal_selector.hpp"
#include "decode/decoder_interface.hpp"

#include <QApplication>
#include <QString>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <vector>

namespace ch8 = signalforge::chart;
namespace bm6 = signalforge::buffer;
namespace dm5 = signalforge::decoder;

namespace {

dm5::SignalMetadata makeMeta(QString id, dm5::SignalType type) {
    dm5::SignalMetadata meta;
    meta.id = std::move(id);
    meta.name = QStringLiteral("");
    meta.unit = QStringLiteral("");
    meta.type = type;
    return meta;
}

/// Lazy-init a single QApplication for this test binary so widgets
/// can be constructed. Catch2WithMain doesn't create one for us.
QApplication& app() {
    static int argc = 1;
    static char arg0[] = "test";
    static char* argv[] = {arg0, nullptr};
    static QApplication instance(argc, argv);
    return instance;
}

QTreeWidget* findTree(ch8::SignalSelector& selector) {
    return selector.findChild<QTreeWidget*>();
}

std::vector<QString> topLevelLabels(QTreeWidget* tree) {
    std::vector<QString> out;
    if (tree == nullptr) {
        return out;
    }
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        out.push_back(tree->topLevelItem(i)->text(0));
    }
    return out;
}

}  // namespace

TEST_CASE("S6: empty registry produces empty tree", "[chart][s6][selector]") {
    app();
    bm6::SignalBufferRegistry registry;
    ch8::ChartManager manager(registry);
    ch8::SignalSelector selector(registry, manager);
    auto* tree = findTree(selector);
    REQUIRE(tree != nullptr);
    REQUIRE(tree->topLevelItemCount() == 0);
}

TEST_CASE("S6: signals from one driver populate one Driver group", "[chart][s6][selector]") {
    app();
    bm6::SignalBufferRegistry registry;
    std::vector<dm5::SignalMetadata> metas{
        makeMeta(QStringLiteral("serial-driver-1/voltage"), dm5::SignalType::Double),
        makeMeta(QStringLiteral("serial-driver-1/current"), dm5::SignalType::Double),
    };
    registry.onSignalsRegistered(QStringLiteral("serial-driver-1"), metas);

    ch8::ChartManager manager(registry);
    ch8::SignalSelector selector(registry, manager);
    auto* tree = findTree(selector);
    REQUIRE(tree->topLevelItemCount() == 1);
    REQUIRE(tree->topLevelItem(0)->text(0) == QStringLiteral("Driver: serial-driver-1"));
    REQUIRE(tree->topLevelItem(0)->childCount() == 2);
}

TEST_CASE("S6: signals from multiple drivers create separate groups", "[chart][s6][selector]") {
    app();
    bm6::SignalBufferRegistry registry;
    registry.onSignalsRegistered(QStringLiteral("driver-a"),
                                 {makeMeta(QStringLiteral("driver-a/x"), dm5::SignalType::Double)});
    registry.onSignalsRegistered(QStringLiteral("driver-b"),
                                 {makeMeta(QStringLiteral("driver-b/y"), dm5::SignalType::Int64)});

    ch8::ChartManager manager(registry);
    ch8::SignalSelector selector(registry, manager);
    auto* tree = findTree(selector);
    REQUIRE(tree->topLevelItemCount() == 2);
    const auto labels = topLevelLabels(tree);
    REQUIRE(std::find(labels.begin(), labels.end(), QStringLiteral("Driver: driver-a")) != labels.end());
    REQUIRE(std::find(labels.begin(), labels.end(), QStringLiteral("Driver: driver-b")) != labels.end());
}

TEST_CASE("S6: derived signals (no slash in id) land in the Derived group", "[chart][s6][selector][derived]") {
    app();
    bm6::SignalBufferRegistry registry;
    registry.onSignalsRegistered(QStringLiteral("expression-engine"),
                                 {makeMeta(QStringLiteral("power"), dm5::SignalType::Double),
                                  makeMeta(QStringLiteral("over_temperature"), dm5::SignalType::Bool)});
    registry.onSignalsRegistered(QStringLiteral("driver-x"),
                                 {makeMeta(QStringLiteral("driver-x/raw"), dm5::SignalType::Double)});

    ch8::ChartManager manager(registry);
    ch8::SignalSelector selector(registry, manager);
    auto* tree = findTree(selector);
    REQUIRE(tree->topLevelItemCount() == 2);
    const auto labels = topLevelLabels(tree);
    REQUIRE(std::find(labels.begin(), labels.end(), QStringLiteral("Derived")) != labels.end());
    REQUIRE(std::find(labels.begin(), labels.end(), QStringLiteral("Driver: driver-x")) != labels.end());

    // The Derived group has both "power" and "over_temperature".
    QTreeWidgetItem* derived = nullptr;
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        if (tree->topLevelItem(i)->text(0) == QStringLiteral("Derived")) {
            derived = tree->topLevelItem(i);
            break;
        }
    }
    REQUIRE(derived != nullptr);
    REQUIRE(derived->childCount() == 2);
}

TEST_CASE("S6: filter() narrows visible leaves by signal-id substring", "[chart][s6][selector][filter]") {
    app();
    bm6::SignalBufferRegistry registry;
    registry.onSignalsRegistered(QStringLiteral("driver-a"),
                                 {makeMeta(QStringLiteral("driver-a/voltage"), dm5::SignalType::Double),
                                  makeMeta(QStringLiteral("driver-a/current"), dm5::SignalType::Double)});

    ch8::ChartManager manager(registry);
    ch8::SignalSelector selector(registry, manager);
    auto* tree = findTree(selector);

    selector.setFilter(QStringLiteral("volt"));
    REQUIRE(selector.filter() == QStringLiteral("volt"));

    auto* group = tree->topLevelItem(0);
    int visible = 0;
    for (int i = 0; i < group->childCount(); ++i) {
        if (!group->child(i)->isHidden()) {
            ++visible;
        }
    }
    REQUIRE(visible == 1);

    // Empty filter clears the narrowing.
    selector.setFilter(QStringLiteral(""));
    visible = 0;
    for (int i = 0; i < group->childCount(); ++i) {
        if (!group->child(i)->isHidden()) {
            ++visible;
        }
    }
    REQUIRE(visible == 2);
}

TEST_CASE("S6: refresh picks up registry changes", "[chart][s6][selector][refresh]") {
    app();
    bm6::SignalBufferRegistry registry;
    registry.onSignalsRegistered(QStringLiteral("driver-1"),
                                 {makeMeta(QStringLiteral("driver-1/a"), dm5::SignalType::Double)});

    ch8::ChartManager manager(registry);
    ch8::SignalSelector selector(registry, manager);
    auto* tree = findTree(selector);
    REQUIRE(tree->topLevelItem(0)->childCount() == 1);

    // Add a second driver. SignalSelector observes via refresh().
    registry.onSignalsRegistered(QStringLiteral("driver-2"),
                                 {makeMeta(QStringLiteral("driver-2/b"), dm5::SignalType::Int64)});
    selector.refresh();
    REQUIRE(tree->topLevelItemCount() == 2);

    // Unregister the first driver; tree drops it after refresh().
    registry.onSignalsUnregistered(QStringLiteral("driver-1"));
    selector.refresh();
    REQUIRE(tree->topLevelItemCount() == 1);
    REQUIRE(tree->topLevelItem(0)->text(0) == QStringLiteral("Driver: driver-2"));
}
