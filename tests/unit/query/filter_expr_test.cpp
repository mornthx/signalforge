// tests/unit/query/filter_expr_test.cpp
//
// M30 S1 — display-filter expression engine: parsing, operators,
// precedence, coercion, and parse-error reporting.

#include "query/filter_expr.hpp"

#include <catch2/catch_test_macros.hpp>
#include <map>

namespace sfq = signalforge::query;

namespace {

/// Build a FieldLookup from a fixed map of field -> value.
sfq::FieldLookup row(std::map<QString, sfq::FieldValue> fields) {
    return [fields = std::move(fields)](const QString& f) -> std::optional<sfq::FieldValue> {
        const auto it = fields.find(f);
        if (it == fields.end()) {
            return std::nullopt;
        }
        return it->second;
    };
}

/// Parse and require success, returning the usable filter.
sfq::FilterExpr ok(const QString& src) {
    auto r = sfq::FilterExpr::parse(src);
    REQUIRE(r.ok());
    REQUIRE(r.expr.has_value());
    return *r.expr;
}

}  // namespace

TEST_CASE("empty / whitespace filter matches everything", "[query][s1]") {
    CHECK(ok(QString()).isEmpty());
    CHECK(ok(QStringLiteral("   ")).isEmpty());
    CHECK(ok(QString()).matches(row({{QStringLiteral("value"), 1.0}})));
    CHECK(ok(QString()).matches(row({})));
}

TEST_CASE("string equality is case-insensitive", "[query][s1]") {
    const auto f = ok(QStringLiteral("source == udp:rig"));
    CHECK(f.matches(row({{QStringLiteral("source"), QStringLiteral("udp:rig")}})));
    CHECK(f.matches(row({{QStringLiteral("source"), QStringLiteral("UDP:RIG")}})));
    CHECK_FALSE(f.matches(row({{QStringLiteral("source"), QStringLiteral("tcp:rig")}})));
}

TEST_CASE("numeric comparisons with coercion", "[query][s1]") {
    CHECK(ok(QStringLiteral("value > 20")).matches(row({{QStringLiteral("value"), 25.0}})));
    CHECK_FALSE(ok(QStringLiteral("value > 20")).matches(row({{QStringLiteral("value"), 5.0}})));
    CHECK(ok(QStringLiteral("value <= 10")).matches(row({{QStringLiteral("value"), 10.0}})));
    CHECK(ok(QStringLiteral("value == 25")).matches(row({{QStringLiteral("value"), 25.0}})));
    // A string field that parses as a number coerces.
    CHECK(ok(QStringLiteral("value >= 3")).matches(row({{QStringLiteral("value"), QStringLiteral("3.5")}})));
    // A non-numeric field cannot satisfy a numeric comparison.
    CHECK_FALSE(ok(QStringLiteral("value > 1")).matches(row({{QStringLiteral("value"), QStringLiteral("hi")}})));
}

TEST_CASE("boolean literals", "[query][s1]") {
    CHECK(ok(QStringLiteral("active == true")).matches(row({{QStringLiteral("active"), true}})));
    CHECK(ok(QStringLiteral("active == false")).matches(row({{QStringLiteral("active"), false}})));
    CHECK_FALSE(ok(QStringLiteral("active == true")).matches(row({{QStringLiteral("active"), false}})));
    // numeric field treated as truthy
    CHECK(ok(QStringLiteral("active == true")).matches(row({{QStringLiteral("active"), 1.0}})));
}

TEST_CASE("contains is a case-insensitive substring", "[query][s1]") {
    const auto f = ok(QStringLiteral("name contains temp"));
    CHECK(f.matches(row({{QStringLiteral("name"), QStringLiteral("Temperature")}})));
    CHECK_FALSE(f.matches(row({{QStringLiteral("name"), QStringLiteral("pressure")}})));
}

TEST_CASE("&& binds tighter than ||", "[query][s1]") {
    // a==1 && b==2 || c==3  ==  (a&&b) || c
    const auto f = ok(QStringLiteral("a == 1 && b == 2 || c == 3"));
    CHECK(f.matches(row({{QStringLiteral("a"), 1.0}, {QStringLiteral("b"), 2.0}, {QStringLiteral("c"), 0.0}})));
    CHECK(f.matches(row({{QStringLiteral("a"), 9.0}, {QStringLiteral("b"), 9.0}, {QStringLiteral("c"), 3.0}})));
    CHECK_FALSE(f.matches(row({{QStringLiteral("a"), 1.0}, {QStringLiteral("b"), 9.0}, {QStringLiteral("c"), 9.0}})));
}

TEST_CASE("negation and parentheses", "[query][s1]") {
    CHECK(ok(QStringLiteral("!(value > 20)")).matches(row({{QStringLiteral("value"), 5.0}})));
    CHECK_FALSE(ok(QStringLiteral("!(value > 20)")).matches(row({{QStringLiteral("value"), 25.0}})));
    const auto f = ok(QStringLiteral("(value > 1 || value < 0) && name contains x"));
    CHECK(f.matches(row({{QStringLiteral("value"), 5.0}, {QStringLiteral("name"), QStringLiteral("axb")}})));
    CHECK_FALSE(f.matches(row({{QStringLiteral("value"), 0.5}, {QStringLiteral("name"), QStringLiteral("axb")}})));
}

TEST_CASE("quoted string literals keep spaces", "[query][s1]") {
    const auto f = ok(QStringLiteral("name == \"hello world\""));
    CHECK(f.matches(row({{QStringLiteral("name"), QStringLiteral("hello world")}})));
    CHECK_FALSE(f.matches(row({{QStringLiteral("name"), QStringLiteral("hello")}})));
}

TEST_CASE("missing field never matches", "[query][s1]") {
    CHECK_FALSE(ok(QStringLiteral("ghost == 1")).matches(row({{QStringLiteral("value"), 1.0}})));
    CHECK_FALSE(ok(QStringLiteral("ghost contains x")).matches(row({})));
}

TEST_CASE("parse errors report a message and position", "[query][s1]") {
    auto missingValue = sfq::FilterExpr::parse(QStringLiteral("value =="));
    CHECK_FALSE(missingValue.ok());
    CHECK(missingValue.errorPos >= 0);

    auto missingOp = sfq::FilterExpr::parse(QStringLiteral("value 20"));
    CHECK_FALSE(missingOp.ok());

    auto unbalanced = sfq::FilterExpr::parse(QStringLiteral("(value > 1"));
    CHECK_FALSE(unbalanced.ok());

    auto danglingAnd = sfq::FilterExpr::parse(QStringLiteral("value == 1 &&"));
    CHECK_FALSE(danglingAnd.ok());

    auto badChar = sfq::FilterExpr::parse(QStringLiteral("value @ 1"));
    CHECK_FALSE(badChar.ok());
    CHECK(badChar.errorPos == 6);
}
