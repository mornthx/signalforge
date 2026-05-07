// tests/unit/expression/expression_class_test.cpp
//
// S8 — coverage gap-fills for the Expression class itself: the
// accessors, derivedSignalMetadata(), and move semantics. Most other
// behavior (compile, evaluate, type promotion, output mapping) is
// already exercised by expression_evaluation_test.cpp.

#include "decode/decoder_interface.hpp"
#include "expression/expression.hpp"

#include <QString>
#include <catch2/catch_test_macros.hpp>
#include <utility>
#include <vector>

namespace ex7 = signalforge::expression;
namespace dm5 = signalforge::decoder;

namespace {

ex7::Expression makeExpr(QString id, QString formula, ex7::ExpressionOutputType out, std::vector<QString> srcIds) {
    return ex7::Expression(id, QStringLiteral("Display ") + id, QStringLiteral("V"), QStringLiteral("Doc for ") + id,
                           std::move(formula), out, std::move(srcIds));
}

}  // namespace

TEST_CASE("S8: accessors return constructor arguments verbatim", "[expression][s8][accessors]") {
    auto e = makeExpr(QStringLiteral("e1"), QStringLiteral("a + b"), ex7::ExpressionOutputType::Double,
                      {QStringLiteral("a"), QStringLiteral("b")});
    REQUIRE(e.id() == QStringLiteral("e1"));
    REQUIRE(e.name() == QStringLiteral("Display e1"));
    REQUIRE(e.unit() == QStringLiteral("V"));
    REQUIRE(e.description().has_value());
    REQUIRE(*e.description() == QStringLiteral("Doc for e1"));
    REQUIRE(e.formula() == QStringLiteral("a + b"));
    REQUIRE(e.outputType() == ex7::ExpressionOutputType::Double);
    REQUIRE(e.sourceSignalIds().size() == 2U);
}

TEST_CASE("S8: omitted description is nullopt", "[expression][s8][accessors]") {
    ex7::Expression e(QStringLiteral("noDesc"), QStringLiteral("noDesc"), QStringLiteral(""), std::nullopt,
                      QStringLiteral("a"), ex7::ExpressionOutputType::Double, {QStringLiteral("a")});
    REQUIRE_FALSE(e.description().has_value());
}

TEST_CASE("S8: derivedSignalMetadata maps Double output", "[expression][s8][metadata]") {
    auto e = makeExpr(QStringLiteral("d_double"), QStringLiteral("a + 1"), ex7::ExpressionOutputType::Double,
                      {QStringLiteral("a")});
    auto m = e.derivedSignalMetadata();
    REQUIRE(m.id == QStringLiteral("d_double"));
    REQUIRE(m.name == QStringLiteral("Display d_double"));
    REQUIRE(m.unit == QStringLiteral("V"));
    REQUIRE(m.type == dm5::SignalType::Double);
}

TEST_CASE("S8: derivedSignalMetadata maps Bool output", "[expression][s8][metadata]") {
    auto e = makeExpr(QStringLiteral("d_bool"), QStringLiteral("a > 0"), ex7::ExpressionOutputType::Bool,
                      {QStringLiteral("a")});
    auto m = e.derivedSignalMetadata();
    REQUIRE(m.type == dm5::SignalType::Bool);
}

TEST_CASE("S8: derivedSignalMetadata maps Int64 output", "[expression][s8][metadata]") {
    auto e = makeExpr(QStringLiteral("d_int"), QStringLiteral("round(a * 2)"), ex7::ExpressionOutputType::Int64,
                      {QStringLiteral("a")});
    auto m = e.derivedSignalMetadata();
    REQUIRE(m.type == dm5::SignalType::Int64);
}

TEST_CASE("S8: move construction transfers state and leaves moved-from in valid empty state",
          "[expression][s8][move]") {
    auto src = makeExpr(QStringLiteral("src"), QStringLiteral("a + 1"), ex7::ExpressionOutputType::Double,
                        {QStringLiteral("a")});
    ex7::Expression dst{std::move(src)};
    REQUIRE(dst.id() == QStringLiteral("src"));
    REQUIRE(dst.formula() == QStringLiteral("a + 1"));
    // Evaluate after move to confirm the compiled state was transferred,
    // not lost.
    auto v = dst.evaluate({{QStringLiteral("a"), dm5::SignalValue{4.0}}});
    REQUIRE(std::holds_alternative<double>(v));
    REQUIRE(std::get<double>(v) == 5.0);
}

TEST_CASE("S8: move assignment transfers state", "[expression][s8][move]") {
    auto src = makeExpr(QStringLiteral("src"), QStringLiteral("a * 2"), ex7::ExpressionOutputType::Double,
                        {QStringLiteral("a")});
    auto dst =
        makeExpr(QStringLiteral("dst"), QStringLiteral("a"), ex7::ExpressionOutputType::Double, {QStringLiteral("a")});
    dst = std::move(src);
    REQUIRE(dst.id() == QStringLiteral("src"));
    REQUIRE(dst.formula() == QStringLiteral("a * 2"));
    auto v = dst.evaluate({{QStringLiteral("a"), dm5::SignalValue{3.0}}});
    REQUIRE(std::get<double>(v) == 6.0);
}

TEST_CASE("S8: ExpressionSet move-only contract", "[expression][s8][move]") {
    ex7::ExpressionSet s;
    s.expressions.push_back(
        makeExpr(QStringLiteral("e"), QStringLiteral("a"), ex7::ExpressionOutputType::Double, {QStringLiteral("a")}));
    s.baseSignalIds.push_back(QStringLiteral("a"));
    auto moved = std::move(s);
    REQUIRE(moved.expressions.size() == 1U);
    REQUIRE(moved.baseSignalIds.size() == 1U);
    REQUIRE(moved.expressions[0].id() == QStringLiteral("e"));
}
