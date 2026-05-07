// tests/integration/test_signal_selector_tree_population.cpp
//
// S9 integration test (spec §2.1-12 #7): two mock drivers + 1
// derived signal (M7 expression-engine virtual driver) — the
// SignalSelector's tree shows 3 top-level groups with the
// expected leaves. Complement to the S6 unit-level test which
// exercised the same paths in isolation; this version verifies
// the full registry → ChartManager → SignalSelector wiring.

#include "buffer/signal_buffer_registry.hpp"
#include "chart/chart_manager.hpp"
#include "chart/signal_selector.hpp"
#include "decode/decoder_interface.hpp"

#include <QApplication>
#include <QString>
#include <QTreeWidget>
#include <catch2/catch_test_macros.hpp>
#include <vector>

namespace ch8 = signalforge::chart;
namespace bm6 = signalforge::buffer;
namespace dm5 = signalforge::decoder;

namespace {

QApplication& app() {
    static int argc = 1;
    static char arg0[] = "test_signal_selector_tree_population";
    static char* argv[] = {arg0, nullptr};
    static QApplication instance(argc, argv);
    return instance;
}

dm5::SignalMetadata makeMeta(QString id, dm5::SignalType type) {
    dm5::SignalMetadata m;
    m.id = std::move(id);
    m.type = type;
    return m;
}

}  // namespace

TEST_CASE("S9 integration: 2 mock drivers + Derived → 3 groups", "[integration][s9][selector]") {
    app();
    bm6::SignalBufferRegistry registry;
    registry.onSignalsRegistered(QStringLiteral("serial-1"),
                                 {makeMeta(QStringLiteral("serial-1/voltage"), dm5::SignalType::Double),
                                  makeMeta(QStringLiteral("serial-1/current"), dm5::SignalType::Double)});
    registry.onSignalsRegistered(QStringLiteral("tcp-1"),
                                 {makeMeta(QStringLiteral("tcp-1/rpm"), dm5::SignalType::Int64)});
    registry.onSignalsRegistered(QStringLiteral("expression-engine"),
                                 {makeMeta(QStringLiteral("power"), dm5::SignalType::Double)});

    ch8::ChartManager manager(registry);
    ch8::SignalSelector selector(registry, manager);
    auto* tree = selector.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    REQUIRE(tree->topLevelItemCount() == 3);

    int totalLeaves = 0;
    bool sawSerial = false;
    bool sawTcp = false;
    bool sawDerived = false;
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        auto* group = tree->topLevelItem(i);
        const QString label = group->text(0);
        if (label == QStringLiteral("Driver: serial-1")) {
            sawSerial = true;
            REQUIRE(group->childCount() == 2);
        } else if (label == QStringLiteral("Driver: tcp-1")) {
            sawTcp = true;
            REQUIRE(group->childCount() == 1);
        } else if (label == QStringLiteral("Derived")) {
            sawDerived = true;
            REQUIRE(group->childCount() == 1);
        }
        totalLeaves += group->childCount();
    }
    REQUIRE(sawSerial);
    REQUIRE(sawTcp);
    REQUIRE(sawDerived);
    REQUIRE(totalLeaves == 4);
}
