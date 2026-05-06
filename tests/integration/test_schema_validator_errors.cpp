#include "decode/schema_validator.hpp"

#include <QString>
#include <QStringList>
#include <catch2/catch_test_macros.hpp>
#include <functional>
#include <vector>

using signalforge::decoder::SchemaValidator;
using signalforge::decoder::ValidationError;

namespace {

bool anyError(const std::vector<ValidationError>& errors, const std::function<bool(const ValidationError&)>& pred) {
    for (const auto& e : errors) {
        if (pred(e)) {
            return true;
        }
    }
    return false;
}

}  // namespace

TEST_CASE("integration validator: missing_version reports 'schema_version' field", "[integration][validator][errors]") {
    const auto r =
        SchemaValidator::validateFile(QStringLiteral(SIGNALFORGE_FIXTURES_DIR "/invalid_schemas/missing_version.yaml"));
    REQUIRE_FALSE(r.has_value());
    REQUIRE(
        anyError(r.error(), [](const ValidationError& e) { return e.fieldPath == QStringLiteral("schema_version"); }));
}

TEST_CASE("integration validator: missing_endianness reports both layout and field errors",
          "[integration][validator][errors]") {
    const auto r = SchemaValidator::validateFile(
        QStringLiteral(SIGNALFORGE_FIXTURES_DIR "/invalid_schemas/missing_endianness.yaml"));
    REQUIRE_FALSE(r.has_value());
    REQUIRE(anyError(r.error(),
                     [](const ValidationError& e) { return e.fieldPath == QStringLiteral("layouts[0].endianness"); }));
    REQUIRE(anyError(r.error(), [](const ValidationError& e) {
        return e.fieldPath.contains(QStringLiteral("fields[0].endianness")) &&
               e.message.contains(QStringLiteral("multi-byte"));
    }));
}

TEST_CASE("integration validator: invalid_encoding lists allowed enum values", "[integration][validator][errors]") {
    const auto r = SchemaValidator::validateFile(
        QStringLiteral(SIGNALFORGE_FIXTURES_DIR "/invalid_schemas/invalid_encoding.yaml"));
    REQUIRE_FALSE(r.has_value());
    REQUIRE(anyError(r.error(), [](const ValidationError& e) {
        return e.fieldPath.endsWith(QStringLiteral(".encoding")) &&
               e.message.contains(QStringLiteral("not_a_real_type")) && e.message.contains(QStringLiteral("uint16"));
    }));
}

TEST_CASE("integration validator: bit_overlap identifies overlapping ranges", "[integration][validator][errors]") {
    const auto r =
        SchemaValidator::validateFile(QStringLiteral(SIGNALFORGE_FIXTURES_DIR "/invalid_schemas/bit_overlap.yaml"));
    REQUIRE_FALSE(r.has_value());
    REQUIRE(
        anyError(r.error(), [](const ValidationError& e) { return e.message.contains(QStringLiteral("overlap")); }));
}

TEST_CASE("integration validator: bit_overflow identifies out-of-range slice", "[integration][validator][errors]") {
    const auto r =
        SchemaValidator::validateFile(QStringLiteral(SIGNALFORGE_FIXTURES_DIR "/invalid_schemas/bit_overflow.yaml"));
    REQUIRE_FALSE(r.has_value());
    REQUIRE(anyError(r.error(),
                     [](const ValidationError& e) { return e.message.contains(QStringLiteral("does not fit")); }));
}

TEST_CASE("integration validator: duplicate_field rejects duplicate within layout",
          "[integration][validator][errors]") {
    const auto r =
        SchemaValidator::validateFile(QStringLiteral(SIGNALFORGE_FIXTURES_DIR "/invalid_schemas/duplicate_field.yaml"));
    REQUIRE_FALSE(r.has_value());
    REQUIRE(anyError(r.error(), [](const ValidationError& e) {
        return e.message.contains(QStringLiteral("duplicate field name"));
    }));
}

TEST_CASE("integration validator: every invalid fixture has a 1-based line for at least one error",
          "[integration][validator][errors]") {
    const QStringList fixtures = {
        QStringLiteral("missing_version.yaml"),  QStringLiteral("missing_endianness.yaml"),
        QStringLiteral("invalid_encoding.yaml"), QStringLiteral("bit_overlap.yaml"),
        QStringLiteral("bit_overflow.yaml"),     QStringLiteral("duplicate_field.yaml"),
    };
    for (const auto& f : fixtures) {
        const QString path = QStringLiteral(SIGNALFORGE_FIXTURES_DIR "/invalid_schemas/") + f;
        const auto r = SchemaValidator::validateFile(path);
        UNSCOPED_INFO("fixture: " << f.toStdString());
        REQUIRE_FALSE(r.has_value());
        bool hasLine = false;
        for (const auto& e : r.error()) {
            if (e.lineNumber > 0) {
                hasLine = true;
                break;
            }
        }
        REQUIRE(hasLine);
    }
}
