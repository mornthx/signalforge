// tests/unit/expression/expression_evaluation_test.cpp
//
// S2 — verifies Expression evaluation for valid formulas, type
// promotion (bool / int64 / double sources, double / bool / int64
// outputs), restricted-syntax rejection (control flow), and NaN
// propagation when a source is missing.

#include "expression/expression.hpp"

#include <QString>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <variant>

namespace ex7 = signalforge::expression;
namespace dm5 = signalforge::decoder;

namespace {

ex7::Expression makeDoubleExpr(QString id, QString formula, std::vector<QString> srcIds) {
    return ex7::Expression(std::move(id), id, QStringLiteral(""), std::nullopt, std::move(formula),
                           ex7::ExpressionOutputType::Double, std::move(srcIds));
}

}  // namespace

TEST_CASE("S2: arithmetic formula evaluates correctly", "[expression][s2][evaluate]") {
    auto expr = makeDoubleExpr(QStringLiteral("power"), QStringLiteral("voltage * current"),
                               {QStringLiteral("voltage"), QStringLiteral("current")});

    auto result = expr.evaluate(
        {{QStringLiteral("voltage"), dm5::SignalValue{12.0}}, {QStringLiteral("current"), dm5::SignalValue{2.0}}});
    REQUIRE(std::get<double>(result) == 24.0);
}

TEST_CASE("S2: bool source promotes to 0.0 / 1.0", "[expression][s2][type_promote][bool]") {
    auto expr = makeDoubleExpr(QStringLiteral("alarm_score"), QStringLiteral("flag * 5"), {QStringLiteral("flag")});

    auto resultTrue = expr.evaluate({{QStringLiteral("flag"), dm5::SignalValue{true}}});
    REQUIRE(std::get<double>(resultTrue) == 5.0);
    auto resultFalse = expr.evaluate({{QStringLiteral("flag"), dm5::SignalValue{false}}});
    REQUIRE(std::get<double>(resultFalse) == 0.0);
}

TEST_CASE("S2: int64 source promotes to double", "[expression][s2][type_promote][int64]") {
    auto expr = makeDoubleExpr(QStringLiteral("scaled"), QStringLiteral("count * 0.5"), {QStringLiteral("count")});

    auto result = expr.evaluate({{QStringLiteral("count"), dm5::SignalValue{static_cast<std::int64_t>(40)}}});
    REQUIRE(std::get<double>(result) == 20.0);
}

TEST_CASE("S2: bool output type maps non-zero to true", "[expression][s2][output_map][bool]") {
    ex7::Expression expr(QStringLiteral("over_temp"), QStringLiteral("Over temp"), QStringLiteral(""), std::nullopt,
                         QStringLiteral("temp > 80"), ex7::ExpressionOutputType::Bool, {QStringLiteral("temp")});

    auto resultHot = expr.evaluate({{QStringLiteral("temp"), dm5::SignalValue{95.0}}});
    REQUIRE(std::get<bool>(resultHot) == true);
    auto resultCold = expr.evaluate({{QStringLiteral("temp"), dm5::SignalValue{50.0}}});
    REQUIRE(std::get<bool>(resultCold) == false);
}

TEST_CASE("S2: int64 output type rounds via round()", "[expression][s2][output_map][int64]") {
    ex7::Expression expr(QStringLiteral("rounded"), QStringLiteral("Rounded"), QStringLiteral(""), std::nullopt,
                         QStringLiteral("x"), ex7::ExpressionOutputType::Int64, {QStringLiteral("x")});

    auto resultUp = expr.evaluate({{QStringLiteral("x"), dm5::SignalValue{3.7}}});
    REQUIRE(std::get<std::int64_t>(resultUp) == 4);
    auto resultDown = expr.evaluate({{QStringLiteral("x"), dm5::SignalValue{2.3}}});
    REQUIRE(std::get<std::int64_t>(resultDown) == 2);
}

TEST_CASE("S2: missing source produces NaN result", "[expression][s2][nan]") {
    auto expr =
        makeDoubleExpr(QStringLiteral("derived"), QStringLiteral("a + b"), {QStringLiteral("a"), QStringLiteral("b")});
    // Only provide `a`; `b` slot is initialized to NaN.
    auto result = expr.evaluate({{QStringLiteral("a"), dm5::SignalValue{1.0}}});
    REQUIRE(std::isnan(std::get<double>(result)));
}

TEST_CASE("S2: control-flow formula rejected (if/while/?:)", "[expression][s2][whitelist]") {
    REQUIRE_THROWS_AS(
        makeDoubleExpr(QStringLiteral("bad"), QStringLiteral("if (x > 0) { 1; } else { 0; }"), {QStringLiteral("x")}),
        std::runtime_error);
    REQUIRE_THROWS_AS(
        makeDoubleExpr(QStringLiteral("bad"), QStringLiteral("while(x < 10) x := x + 1"), {QStringLiteral("x")}),
        std::runtime_error);
}

TEST_CASE("S2: assignment ops rejected", "[expression][s2][whitelist]") {
    REQUIRE_THROWS_AS(makeDoubleExpr(QStringLiteral("bad"), QStringLiteral("x := 5"), {QStringLiteral("x")}),
                      std::runtime_error);
}

TEST_CASE("S2: whitelisted math built-ins evaluate correctly", "[expression][s2][builtins]") {
    auto expr = makeDoubleExpr(QStringLiteral("derived"), QStringLiteral("max(a, b) + sqrt(c)"),
                               {QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")});

    auto result = expr.evaluate({{QStringLiteral("a"), dm5::SignalValue{3.0}},
                                 {QStringLiteral("b"), dm5::SignalValue{7.0}},
                                 {QStringLiteral("c"), dm5::SignalValue{16.0}}});
    REQUIRE(std::get<double>(result) == 11.0);  // max(3,7) + sqrt(16) = 7 + 4
}
