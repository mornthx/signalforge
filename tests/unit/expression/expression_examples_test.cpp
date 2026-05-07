// tests/unit/expression/expression_examples_test.cpp
//
// S6 — verifies the canonical schema yaml + the two example yaml files
// validate cleanly when the registry has the expected base signals.

#include "decode/decoder_interface.hpp"
#include "expression/expression_validator.hpp"

#include <QString>
#include <catch2/catch_test_macros.hpp>
#include <vector>

namespace ex7 = signalforge::expression;
namespace dm5 = signalforge::decoder;

namespace {

#ifndef SIGNALFORGE_REPO_ROOT
#define SIGNALFORGE_REPO_ROOT ""
#endif

QString repoPath(const QString& relative) {
    return QStringLiteral("%1/%2").arg(QStringLiteral(SIGNALFORGE_REPO_ROOT), relative);
}

dm5::SignalMetadata makeMeta(QString id, dm5::SignalType type) {
    dm5::SignalMetadata m;
    m.id = std::move(id);
    m.name = QStringLiteral("test");
    m.unit = QStringLiteral("");
    m.type = type;
    return m;
}

}  // namespace

TEST_CASE("S6: canonical schema yaml validates", "[examples][s6][canonical]") {
    // The canonical schema references a wide variety of base signals
    // since it covers all key combinations.
    std::vector<dm5::SignalMetadata> catalog{
        makeMeta(QStringLiteral("voltage"), dm5::SignalType::Double),
        makeMeta(QStringLiteral("voltage_offset"), dm5::SignalType::Double),
        makeMeta(QStringLiteral("current"), dm5::SignalType::Double),
        makeMeta(QStringLiteral("temperature"), dm5::SignalType::Double),
        makeMeta(QStringLiteral("counter"), dm5::SignalType::Int64),
        makeMeta(QStringLiteral("power_input"), dm5::SignalType::Double),
        makeMeta(QStringLiteral("noise_floor"), dm5::SignalType::Double),
    };
    auto result =
        ex7::ExpressionValidator::validateFile(repoPath(QStringLiteral("schemas/expression_schema_v1.yaml")), catalog);
    if (!result.has_value()) {
        for (const auto& e : result.error()) {
            UNSCOPED_INFO("error: " << e.message.toStdString());
        }
    }
    REQUIRE(result.has_value());
    REQUIRE(result->expressions.size() == 6U);  // 6 documented examples in canonical schema
}

TEST_CASE("S6: power_calculations.yaml validates", "[examples][s6][power]") {
    std::vector<dm5::SignalMetadata> catalog{
        makeMeta(QStringLiteral("voltage"), dm5::SignalType::Double),
        makeMeta(QStringLiteral("current"), dm5::SignalType::Double),
        makeMeta(QStringLiteral("power_input"), dm5::SignalType::Double),
    };
    auto result = ex7::ExpressionValidator::validateFile(
        repoPath(QStringLiteral("examples/expressions/power_calculations.yaml")), catalog);
    if (!result.has_value()) {
        for (const auto& e : result.error()) {
            UNSCOPED_INFO("error: " << e.message.toStdString());
        }
    }
    REQUIRE(result.has_value());
    REQUIRE(result->expressions.size() == 3U);
    // Topo order: power_total -> power_efficiency -> high_efficiency
    REQUIRE(result->expressions[0].id() == QStringLiteral("power_total"));
    REQUIRE(result->expressions.back().id() == QStringLiteral("high_efficiency"));
}

TEST_CASE("S6: alarms.yaml validates", "[examples][s6][alarms]") {
    std::vector<dm5::SignalMetadata> catalog{
        makeMeta(QStringLiteral("temperature"), dm5::SignalType::Double),
        makeMeta(QStringLiteral("voltage"), dm5::SignalType::Double),
        makeMeta(QStringLiteral("current"), dm5::SignalType::Double),
    };
    auto result =
        ex7::ExpressionValidator::validateFile(repoPath(QStringLiteral("examples/expressions/alarms.yaml")), catalog);
    if (!result.has_value()) {
        for (const auto& e : result.error()) {
            UNSCOPED_INFO("error: " << e.message.toStdString());
        }
    }
    REQUIRE(result.has_value());
    REQUIRE(result->expressions.size() == 4U);
    // any_alarm depends on over_temperature, under_voltage, over_current.
    // It must come AFTER all three in the topo order.
    int anyAlarmIdx = -1;
    for (std::size_t i = 0; i < result->expressions.size(); ++i) {
        if (result->expressions[i].id() == QStringLiteral("any_alarm")) {
            anyAlarmIdx = static_cast<int>(i);
        }
    }
    REQUIRE(anyAlarmIdx == 3);  // last position
}
