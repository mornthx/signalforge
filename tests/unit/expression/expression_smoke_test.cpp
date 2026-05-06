// tests/unit/expression/expression_smoke_test.cpp
//
// S1 placeholder. Verifies that the M7 freeze-surface headers compile,
// default configs are sensible, and minimal Expression / Engine /
// Validator construct + destruct cleanly. The exprtk preflight (HALT
// trigger #2) is enforced at signalforge_expression's compile time.
//
// Per-validator-fixture, evaluation, and tick tests land in S2-S9 with
// dedicated test files.

#include "buffer/signal_buffer_registry.hpp"
#include "expression/expression.hpp"
#include "expression/expression_engine.hpp"
#include "expression/expression_validator.hpp"

#include <QString>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <variant>

namespace ex7 = signalforge::expression;

TEST_CASE("S1: ExpressionEngineConfig has sane defaults", "[expression][s1][smoke]") {
    ex7::ExpressionEngineConfig cfg;
    REQUIRE(cfg.tickInterval == std::chrono::milliseconds(33));
    REQUIRE(cfg.useTimePrecision == true);
    REQUIRE(cfg.virtualDriverId == QStringLiteral("expression-engine"));
}

TEST_CASE("S1: Expression constructs and reports ids/types", "[expression][s1][smoke]") {
    ex7::Expression expr(QStringLiteral("derived/power"), QStringLiteral("Power"), QStringLiteral("W"), std::nullopt,
                         QStringLiteral("v * i"), ex7::ExpressionOutputType::Double,
                         {QStringLiteral("base/v"), QStringLiteral("base/i")});

    REQUIRE(expr.id() == QStringLiteral("derived/power"));
    REQUIRE(expr.name() == QStringLiteral("Power"));
    REQUIRE(expr.unit() == QStringLiteral("W"));
    REQUIRE(expr.formula() == QStringLiteral("v * i"));
    REQUIRE(expr.outputType() == ex7::ExpressionOutputType::Double);
    REQUIRE(expr.sourceSignalIds().size() == 2U);

    auto meta = expr.derivedSignalMetadata();
    REQUIRE(meta.id == QStringLiteral("derived/power"));
    REQUIRE(meta.type == signalforge::decoder::SignalType::Double);
}

TEST_CASE("S1: ExpressionEngine constructs and reports zero state", "[expression][s1][smoke]") {
    signalforge::buffer::SignalBufferRegistry registry;
    ex7::ExpressionEngine engine(registry);

    REQUIRE_FALSE(engine.isRunning());
    REQUIRE(engine.expressionCount() == 0U);
    REQUIRE(engine.lastTickDurationUs().count() == 0);

    auto stats = engine.stats();
    REQUIRE(stats.ticksTotal == 0U);
    REQUIRE(stats.evaluationsTotal == 0U);
    REQUIRE(stats.evaluationErrors == 0U);
}

TEST_CASE("S1: ExpressionValidator stub returns unexpected", "[expression][s1][smoke]") {
    auto result =
        ex7::ExpressionValidator::validateString(QStringLiteral(""), QStringLiteral("smoke://stub"), /*available=*/{});
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().size() >= 1U);
}
