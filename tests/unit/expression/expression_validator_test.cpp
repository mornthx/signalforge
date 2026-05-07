// tests/unit/expression/expression_validator_test.cpp
//
// S3 — verifies ExpressionValidator behavior for the spec §5.2 13
// invalid fixtures + the happy-path multi-expression valid fixture.
// Each invalid case asserts the validator returns at least one error
// containing the expected expression-id (or empty for top-level
// errors) plus actionable hint text.

#include "decode/decoder_interface.hpp"
#include "expression/expression_validator.hpp"

#include <QString>
#include <catch2/catch_test_macros.hpp>
#include <vector>

namespace ex7 = signalforge::expression;
namespace dm5 = signalforge::decoder;

namespace {

#ifndef SIGNALFORGE_FIXTURES_DIR
#define SIGNALFORGE_FIXTURES_DIR ""
#endif

QString fixturePath(const QString& subdir, const QString& name) {
    return QStringLiteral("%1/%2/%3.yaml").arg(QStringLiteral(SIGNALFORGE_FIXTURES_DIR), subdir, name);
}

dm5::SignalMetadata makeMeta(QString id, dm5::SignalType type) {
    dm5::SignalMetadata m;
    m.id = std::move(id);
    m.name = QStringLiteral("test");
    m.unit = QStringLiteral("");
    m.type = type;
    return m;
}

/// Standard available-signal catalog used across tests. Includes the
/// signals the fixtures reference plus a string-typed `label_signal`
/// for the qstring_source test.
std::vector<dm5::SignalMetadata> defaultCatalog() {
    return {
        makeMeta(QStringLiteral("voltage"), dm5::SignalType::Double),
        makeMeta(QStringLiteral("current"), dm5::SignalType::Double),
        makeMeta(QStringLiteral("voltage_raw"), dm5::SignalType::Double),
        makeMeta(QStringLiteral("temperature"), dm5::SignalType::Double),
        makeMeta(QStringLiteral("counter"), dm5::SignalType::Int64),
        makeMeta(QStringLiteral("power_input"), dm5::SignalType::Double),
        makeMeta(QStringLiteral("label_signal"), dm5::SignalType::String),
    };
}

bool errorsContainExpressionId(const std::vector<ex7::ExpressionValidationError>& errs, const QString& expectedId) {
    for (const auto& e : errs) {
        if (e.expressionId == expectedId) {
            return true;
        }
    }
    return false;
}

bool errorsContainText(const std::vector<ex7::ExpressionValidationError>& errs, const QString& needle) {
    for (const auto& e : errs) {
        if (e.message.contains(needle, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

}  // namespace

TEST_CASE("S3: missing schema_version is rejected", "[validator][s3][fixtures]") {
    auto result = ex7::ExpressionValidator::validateFile(
        fixturePath(QStringLiteral("invalid_expressions"), QStringLiteral("missing_version")), defaultCatalog());
    REQUIRE_FALSE(result.has_value());
    REQUIRE(errorsContainText(result.error(), QStringLiteral("schema_version")));
}

TEST_CASE("S3: missing required field 'id' is rejected", "[validator][s3][fixtures]") {
    auto result = ex7::ExpressionValidator::validateFile(
        fixturePath(QStringLiteral("invalid_expressions"), QStringLiteral("missing_id")), defaultCatalog());
    REQUIRE_FALSE(result.has_value());
    REQUIRE(errorsContainText(result.error(), QStringLiteral("'id'")));
}

TEST_CASE("S3: duplicate expression id is rejected", "[validator][s3][fixtures]") {
    auto result = ex7::ExpressionValidator::validateFile(
        fixturePath(QStringLiteral("invalid_expressions"), QStringLiteral("duplicate_id")), defaultCatalog());
    REQUIRE_FALSE(result.has_value());
    REQUIRE(errorsContainExpressionId(result.error(), QStringLiteral("power")));
    REQUIRE(errorsContainText(result.error(), QStringLiteral("duplicate")));
}

TEST_CASE("S3: id colliding with base signal is rejected", "[validator][s3][fixtures]") {
    auto result = ex7::ExpressionValidator::validateFile(
        fixturePath(QStringLiteral("invalid_expressions"), QStringLiteral("id_collides_with_base")), defaultCatalog());
    REQUIRE_FALSE(result.has_value());
    REQUIRE(errorsContainExpressionId(result.error(), QStringLiteral("voltage")));
    REQUIRE(errorsContainText(result.error(), QStringLiteral("collides")));
}

TEST_CASE("S3: exprtk operator-level syntax error is rejected", "[validator][s3][fixtures]") {
    auto result = ex7::ExpressionValidator::validateFile(
        fixturePath(QStringLiteral("invalid_expressions"), QStringLiteral("unknown_operator")), defaultCatalog());
    REQUIRE_FALSE(result.has_value());
    REQUIRE(errorsContainExpressionId(result.error(), QStringLiteral("bad_syntax")));
}

TEST_CASE("S3: 'if' keyword is rejected (whitelist violation)", "[validator][s3][fixtures]") {
    auto result = ex7::ExpressionValidator::validateFile(
        fixturePath(QStringLiteral("invalid_expressions"), QStringLiteral("restricted_syntax_if")), defaultCatalog());
    REQUIRE_FALSE(result.has_value());
    REQUIRE(errorsContainExpressionId(result.error(), QStringLiteral("alarm_state")));
    REQUIRE(errorsContainText(result.error(), QStringLiteral("forbidden")));
}

TEST_CASE("S3: 'while' keyword is rejected (whitelist violation)", "[validator][s3][fixtures]") {
    auto result = ex7::ExpressionValidator::validateFile(
        fixturePath(QStringLiteral("invalid_expressions"), QStringLiteral("restricted_syntax_while")),
        defaultCatalog());
    REQUIRE_FALSE(result.has_value());
    REQUIRE(errorsContainExpressionId(result.error(), QStringLiteral("looped")));
}

TEST_CASE("S3: unknown source signal is rejected", "[validator][s3][fixtures]") {
    auto result = ex7::ExpressionValidator::validateFile(
        fixturePath(QStringLiteral("invalid_expressions"), QStringLiteral("unknown_source")), defaultCatalog());
    REQUIRE_FALSE(result.has_value());
    REQUIRE(errorsContainExpressionId(result.error(), QStringLiteral("phantom")));
    REQUIRE(errorsContainText(result.error(), QStringLiteral("unknown source")));
}

TEST_CASE("S3: QString-typed source is rejected", "[validator][s3][fixtures]") {
    auto result = ex7::ExpressionValidator::validateFile(
        fixturePath(QStringLiteral("invalid_expressions"), QStringLiteral("qstring_source")), defaultCatalog());
    REQUIRE_FALSE(result.has_value());
    REQUIRE(errorsContainExpressionId(result.error(), QStringLiteral("derived_from_label")));
    REQUIRE(errorsContainText(result.error(), QStringLiteral("QString")));
}

TEST_CASE("S3: simple two-expression cycle is rejected", "[validator][s3][fixtures]") {
    auto result = ex7::ExpressionValidator::validateFile(
        fixturePath(QStringLiteral("invalid_expressions"), QStringLiteral("cycle_simple")), defaultCatalog());
    REQUIRE_FALSE(result.has_value());
    REQUIRE(errorsContainText(result.error(), QStringLiteral("cycle")));
}

TEST_CASE("S3: self-loop cycle is rejected", "[validator][s3][fixtures]") {
    auto result = ex7::ExpressionValidator::validateFile(
        fixturePath(QStringLiteral("invalid_expressions"), QStringLiteral("cycle_self")), defaultCatalog());
    REQUIRE_FALSE(result.has_value());
    REQUIRE(errorsContainText(result.error(), QStringLiteral("cycle")));
}

TEST_CASE("S3: three-node cycle is rejected", "[validator][s3][fixtures]") {
    auto result = ex7::ExpressionValidator::validateFile(
        fixturePath(QStringLiteral("invalid_expressions"), QStringLiteral("cycle_three")), defaultCatalog());
    REQUIRE_FALSE(result.has_value());
    REQUIRE(errorsContainText(result.error(), QStringLiteral("cycle")));
}

TEST_CASE("S3: bool-output with arithmetic is permissive (not blocked)", "[validator][s3][fixtures]") {
    // Per spec §4.3 step 6, bool-output with arithmetic is a hint, not
    // a hard rejection. The validator currently does not block this
    // case; the type coercion in S2 maps non-zero to true. Test that
    // validation succeeds.
    auto result = ex7::ExpressionValidator::validateFile(
        fixturePath(QStringLiteral("invalid_expressions"), QStringLiteral("bool_output_with_arithmetic")),
        defaultCatalog());
    REQUIRE(result.has_value());
    REQUIRE(result->expressions.size() == 1U);
}

TEST_CASE("S3: valid multi-expression set produces topologically-sorted output", "[validator][s3][happy]") {
    auto result = ex7::ExpressionValidator::validateFile(
        fixturePath(QStringLiteral("valid_expressions"), QStringLiteral("multi")), defaultCatalog());
    REQUIRE(result.has_value());
    REQUIRE(result->expressions.size() == 3U);

    // power_total has only base sources, evaluates first.
    // power_efficiency depends on power_total + power_input → after.
    // over_temperature has only base sources → either before or after
    // power_total (independent branch).
    // The order must place power_efficiency after power_total.
    int powerTotalIdx = -1;
    int efficiencyIdx = -1;
    for (std::size_t i = 0; i < result->expressions.size(); ++i) {
        const auto& e = result->expressions[i];
        if (e.id() == QStringLiteral("power_total")) {
            powerTotalIdx = static_cast<int>(i);
        } else if (e.id() == QStringLiteral("power_efficiency")) {
            efficiencyIdx = static_cast<int>(i);
        }
    }
    REQUIRE(powerTotalIdx >= 0);
    REQUIRE(efficiencyIdx >= 0);
    REQUIRE(efficiencyIdx > powerTotalIdx);

    // baseSignalIds union — should include voltage / current / power_input / temperature
    // (NOT power_total, which is a derived signal).
    std::sort(result->baseSignalIds.begin(), result->baseSignalIds.end());
    REQUIRE(std::find(result->baseSignalIds.begin(), result->baseSignalIds.end(), QStringLiteral("voltage")) !=
            result->baseSignalIds.end());
    REQUIRE(std::find(result->baseSignalIds.begin(), result->baseSignalIds.end(), QStringLiteral("power_total")) ==
            result->baseSignalIds.end());
}

TEST_CASE("S7: validateFileSyntaxOnly accepts unknown sources", "[validator][s7][lint-mode]") {
    // The valid fixture references base signals that are NOT in any
    // catalog at all here. The full validateFile would reject this, but
    // validateFileSyntaxOnly skips source-existence checks.
    auto result = ex7::ExpressionValidator::validateFileSyntaxOnly(
        fixturePath(QStringLiteral("valid_expressions"), QStringLiteral("multi")));
    if (!result.has_value()) {
        for (const auto& e : result.error()) {
            UNSCOPED_INFO("error: " << e.message.toStdString());
        }
    }
    REQUIRE(result.has_value());
    REQUIRE(result->expressions.size() == 3U);
}

TEST_CASE("S7: validateFileSyntaxOnly still rejects cycles", "[validator][s7][lint-mode]") {
    // Cycles must still be caught even in linter mode.
    auto result = ex7::ExpressionValidator::validateFileSyntaxOnly(
        fixturePath(QStringLiteral("invalid_expressions"), QStringLiteral("cycle_simple")));
    REQUIRE_FALSE(result.has_value());
    bool sawCycle = false;
    for (const auto& e : result.error()) {
        if (e.message.contains(QStringLiteral("cycle"))) {
            sawCycle = true;
        }
    }
    REQUIRE(sawCycle);
}

TEST_CASE("S7: validateStringSyntaxOnly accepts forward-referenced base signals", "[validator][s7][lint-mode]") {
    // Same content with no catalog: validateString rejects, syntax-only
    // accepts.
    const QString yaml = QStringLiteral("schema_version: 1\n"
                                        "expressions:\n"
                                        "  - id: power\n"
                                        "    formula: voltage * current\n");
    auto full = ex7::ExpressionValidator::validateString(yaml, QStringLiteral("<inline>"), {});
    REQUIRE_FALSE(full.has_value());
    auto syntax = ex7::ExpressionValidator::validateStringSyntaxOnly(yaml, QStringLiteral("<inline>"));
    REQUIRE(syntax.has_value());
    REQUIRE(syntax->expressions.size() == 1U);
}

// S8 — coverage gap-fills via inline yaml strings. Each case targets a
// specific top-level / per-entry shape error path that the existing
// fixture set did not exercise.
TEST_CASE("S8: schema_version != 1 is rejected", "[validator][s8][shape]") {
    const QString yaml = QStringLiteral("schema_version: 2\n"
                                        "expressions:\n"
                                        "  - id: e\n"
                                        "    formula: 1 + 1\n");
    auto r = ex7::ExpressionValidator::validateString(yaml, QStringLiteral("<inline>"), {});
    REQUIRE_FALSE(r.has_value());
    REQUIRE(errorsContainText(r.error(), QStringLiteral("schema_version")));
}

TEST_CASE("S8: missing top-level 'expressions' is rejected", "[validator][s8][shape]") {
    const QString yaml = QStringLiteral("schema_version: 1\n");
    auto r = ex7::ExpressionValidator::validateString(yaml, QStringLiteral("<inline>"), {});
    REQUIRE_FALSE(r.has_value());
    REQUIRE(errorsContainText(r.error(), QStringLiteral("expressions")));
}

TEST_CASE("S8: 'expressions' must be a sequence", "[validator][s8][shape]") {
    const QString yaml = QStringLiteral("schema_version: 1\n"
                                        "expressions: not_a_sequence\n");
    auto r = ex7::ExpressionValidator::validateString(yaml, QStringLiteral("<inline>"), {});
    REQUIRE_FALSE(r.has_value());
    REQUIRE(errorsContainText(r.error(), QStringLiteral("sequence")));
}

TEST_CASE("S8: expression entry must be a mapping", "[validator][s8][shape]") {
    const QString yaml = QStringLiteral("schema_version: 1\n"
                                        "expressions:\n"
                                        "  - just_a_string\n");
    auto r = ex7::ExpressionValidator::validateString(yaml, QStringLiteral("<inline>"), {});
    REQUIRE_FALSE(r.has_value());
    REQUIRE(errorsContainText(r.error(), QStringLiteral("mapping")));
}

TEST_CASE("S8: empty expression id is rejected", "[validator][s8][shape]") {
    const QString yaml = QStringLiteral("schema_version: 1\n"
                                        "expressions:\n"
                                        "  - id: \"\"\n"
                                        "    formula: 1 + 1\n");
    auto r = ex7::ExpressionValidator::validateString(yaml, QStringLiteral("<inline>"), {});
    REQUIRE_FALSE(r.has_value());
    REQUIRE(errorsContainText(r.error(), QStringLiteral("non-empty")));
}

TEST_CASE("S8: missing formula is rejected", "[validator][s8][shape]") {
    const QString yaml = QStringLiteral("schema_version: 1\n"
                                        "expressions:\n"
                                        "  - id: e\n");
    auto r = ex7::ExpressionValidator::validateString(yaml, QStringLiteral("<inline>"), {});
    REQUIRE_FALSE(r.has_value());
    REQUIRE(errorsContainText(r.error(), QStringLiteral("formula")));
}

TEST_CASE("S8: empty formula is rejected", "[validator][s8][shape]") {
    const QString yaml = QStringLiteral("schema_version: 1\n"
                                        "expressions:\n"
                                        "  - id: e\n"
                                        "    formula: \"   \"\n");
    auto r = ex7::ExpressionValidator::validateString(yaml, QStringLiteral("<inline>"), {});
    REQUIRE_FALSE(r.has_value());
    REQUIRE(errorsContainText(r.error(), QStringLiteral("empty formula")));
}

TEST_CASE("S8: unknown output type is rejected", "[validator][s8][shape]") {
    const QString yaml = QStringLiteral("schema_version: 1\n"
                                        "expressions:\n"
                                        "  - id: e\n"
                                        "    formula: 1 + 1\n"
                                        "    type: int32\n");
    auto r = ex7::ExpressionValidator::validateString(yaml, QStringLiteral("<inline>"), {});
    REQUIRE_FALSE(r.has_value());
    REQUIRE(errorsContainText(r.error(), QStringLiteral("output type")));
}

TEST_CASE("S8: validateFile reports cannot-open for missing file", "[validator][s8][io]") {
    auto r = ex7::ExpressionValidator::validateFile(QStringLiteral("/tmp/__signalforge_no_such_file__.yaml"), {});
    REQUIRE_FALSE(r.has_value());
    REQUIRE(errorsContainText(r.error(), QStringLiteral("cannot open")));
}
