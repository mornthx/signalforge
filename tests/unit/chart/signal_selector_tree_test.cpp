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
#include <QFrame>
#include <QLabel>
#include <QString>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <algorithm>
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

// ── M17 S3 — panel header + filter count + deterministic order ───────────

TEST_CASE("M17 S3: panel header is present with objectName=panelHeader and 'Signals' label", "[chart][m17][selector]") {
    app();
    bm6::SignalBufferRegistry registry;
    ch8::ChartManager manager(registry);
    ch8::SignalSelector selector(registry, manager);

    auto* header = selector.findChild<QFrame*>(QStringLiteral("panelHeader"));
    REQUIRE(header != nullptr);
    auto* label = header->findChild<QLabel*>();
    REQUIRE(label != nullptr);
    REQUIRE(label->text() == QStringLiteral("Signals"));
    REQUIRE(label->property("class").toString() == QStringLiteral("heading"));
}

TEST_CASE("M17 S3: count label uses class=caption and reads 'No signals available' when empty",
          "[chart][m17][selector]") {
    app();
    bm6::SignalBufferRegistry registry;
    ch8::ChartManager manager(registry);
    ch8::SignalSelector selector(registry, manager);

    auto* countLabel = selector.findChild<QLabel*>(QStringLiteral("signalSelectorCount"));
    REQUIRE(countLabel != nullptr);
    REQUIRE(countLabel->property("class").toString() == QStringLiteral("caption"));
    REQUIRE(countLabel->text() == QStringLiteral("No signals available"));
}

TEST_CASE("M17 S3: count label reads 'M signals' when unfiltered, 'N / M signals' when filtered",
          "[chart][m17][selector]") {
    app();
    bm6::SignalBufferRegistry registry;
    registry.onSignalsRegistered(QStringLiteral("driver-a"),
                                 {makeMeta(QStringLiteral("driver-a/voltage"), dm5::SignalType::Double),
                                  makeMeta(QStringLiteral("driver-a/current"), dm5::SignalType::Double),
                                  makeMeta(QStringLiteral("driver-a/temperature"), dm5::SignalType::Double)});

    ch8::ChartManager manager(registry);
    ch8::SignalSelector selector(registry, manager);

    auto* countLabel = selector.findChild<QLabel*>(QStringLiteral("signalSelectorCount"));
    REQUIRE(countLabel != nullptr);
    REQUIRE(countLabel->text() == QStringLiteral("3 signals"));

    selector.setFilter(QStringLiteral("volt"));
    REQUIRE(countLabel->text() == QStringLiteral("1 / 3 signals"));

    selector.setFilter(QStringLiteral(""));
    REQUIRE(countLabel->text() == QStringLiteral("3 signals"));
}

TEST_CASE("M17 S3: top-level groups appear in alphabetical order regardless of insertion order",
          "[chart][m17][selector][adr-014]") {
    app();
    bm6::SignalBufferRegistry registry;
    // Insert drivers in reverse-alphabetical order to surface any
    // hashtable-iteration anti-pattern (widget-styling-guide §9 row 6).
    registry.onSignalsRegistered(QStringLiteral("z-zulu"),
                                 {makeMeta(QStringLiteral("z-zulu/a"), dm5::SignalType::Double)});
    registry.onSignalsRegistered(QStringLiteral("m-mike"),
                                 {makeMeta(QStringLiteral("m-mike/a"), dm5::SignalType::Double)});
    registry.onSignalsRegistered(QStringLiteral("a-alpha"),
                                 {makeMeta(QStringLiteral("a-alpha/a"), dm5::SignalType::Double)});

    ch8::ChartManager manager(registry);
    ch8::SignalSelector selector(registry, manager);
    auto* tree = findTree(selector);
    REQUIRE(tree->topLevelItemCount() == 3);

    REQUIRE(tree->topLevelItem(0)->text(0) == QStringLiteral("Driver: a-alpha"));
    REQUIRE(tree->topLevelItem(1)->text(0) == QStringLiteral("Driver: m-mike"));
    REQUIRE(tree->topLevelItem(2)->text(0) == QStringLiteral("Driver: z-zulu"));
}

TEST_CASE("M17 S3: leaves within a group appear in alphabetical order", "[chart][m17][selector][adr-014]") {
    app();
    bm6::SignalBufferRegistry registry;
    // Register 3 signals in non-alphabetical order; expect tree
    // to show them sorted.
    registry.onSignalsRegistered(QStringLiteral("driver-x"),
                                 {makeMeta(QStringLiteral("driver-x/voltage"), dm5::SignalType::Double),
                                  makeMeta(QStringLiteral("driver-x/amperage"), dm5::SignalType::Double),
                                  makeMeta(QStringLiteral("driver-x/temperature"), dm5::SignalType::Double)});

    ch8::ChartManager manager(registry);
    ch8::SignalSelector selector(registry, manager);
    auto* tree = findTree(selector);
    REQUIRE(tree->topLevelItemCount() == 1);
    auto* group = tree->topLevelItem(0);
    REQUIRE(group->childCount() == 3);

    // Leaves carry the signal id in UserRole; the visible text
    // is the field-name portion (post-slash).
    REQUIRE(group->child(0)->data(0, Qt::UserRole).toString() == QStringLiteral("driver-x/amperage"));
    REQUIRE(group->child(1)->data(0, Qt::UserRole).toString() == QStringLiteral("driver-x/temperature"));
    REQUIRE(group->child(2)->data(0, Qt::UserRole).toString() == QStringLiteral("driver-x/voltage"));
}

TEST_CASE("M17 S3: tree header is hidden (panel header takes title role)", "[chart][m17][selector]") {
    app();
    bm6::SignalBufferRegistry registry;
    ch8::ChartManager manager(registry);
    ch8::SignalSelector selector(registry, manager);
    auto* tree = findTree(selector);
    REQUIRE(tree->isHeaderHidden());
}

TEST_CASE("M17 S4: SignalSelector outer objectName is signalSelectorPanel", "[chart][m17][selector]") {
    app();
    bm6::SignalBufferRegistry registry;
    ch8::ChartManager manager(registry);
    ch8::SignalSelector selector(registry, manager);
    REQUIRE(selector.objectName() == QStringLiteral("signalSelectorPanel"));
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
