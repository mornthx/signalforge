// tests/unit/decode/decoder_test.cpp
#include "decode/decoder_interface.hpp"
#include "decode/logging_signal_value_sink.hpp"
#include "decode/schema_validator.hpp"

#include <catch2/catch_test_macros.hpp>
#include <chrono>

using signalforge::decoder::Endianness;
using signalforge::decoder::FieldEncoding;
using signalforge::decoder::LoggingSignalValueSink;
using signalforge::decoder::SchemaValidator;
using signalforge::decoder::SignalMetadata;
using signalforge::decoder::SignalType;
using signalforge::decoder::SignalValue;

TEST_CASE("LoggingSignalValueSink: construction starts with zero counters", "[decoder][logging_sink]") {
    LoggingSignalValueSink sink;
    REQUIRE(sink.signalsReceived() == 0);
    REQUIRE(sink.signalsByType(SignalType::Bool) == 0);
    REQUIRE(sink.signalsByType(SignalType::Int64) == 0);
    REQUIRE(sink.signalsByType(SignalType::Double) == 0);
    REQUIRE(sink.signalsByType(SignalType::String) == 0);
    REQUIRE(sink.registrationsReceived() == 0);
    REQUIRE(sink.unregistrationsReceived() == 0);
}

TEST_CASE("LoggingSignalValueSink: onSignal records each variant type", "[decoder][logging_sink]") {
    LoggingSignalValueSink sink;
    const auto ts = std::chrono::steady_clock::now();

    sink.onSignal(ts, QStringLiteral("id1"), SignalValue{true});
    sink.onSignal(ts, QStringLiteral("id2"), SignalValue{std::int64_t{42}});
    sink.onSignal(ts, QStringLiteral("id3"), SignalValue{3.14});
    sink.onSignal(ts, QStringLiteral("id4"), SignalValue{QStringLiteral("text")});
    sink.onSignal(ts, QStringLiteral("id5"), SignalValue{false});

    REQUIRE(sink.signalsReceived() == 5);
    REQUIRE(sink.signalsByType(SignalType::Bool) == 2);
    REQUIRE(sink.signalsByType(SignalType::Int64) == 1);
    REQUIRE(sink.signalsByType(SignalType::Double) == 1);
    REQUIRE(sink.signalsByType(SignalType::String) == 1);
}

TEST_CASE("LoggingSignalValueSink: registration and unregistration callbacks count", "[decoder][logging_sink]") {
    LoggingSignalValueSink sink;
    std::vector<SignalMetadata> meta;
    meta.push_back({QStringLiteral("d1/a"), QStringLiteral("a"), QStringLiteral(""), SignalType::Int64, std::nullopt,
                    std::nullopt, std::nullopt});

    sink.onSignalsRegistered(QStringLiteral("d1"), meta);
    REQUIRE(sink.registrationsReceived() == 1);

    sink.onSignalsUnregistered(QStringLiteral("d1"));
    REQUIRE(sink.unregistrationsReceived() == 1);
}

TEST_CASE("LoggingSignalValueSink: resetCounters zeroes all state", "[decoder][logging_sink]") {
    LoggingSignalValueSink sink;
    const auto ts = std::chrono::steady_clock::now();
    sink.onSignal(ts, QStringLiteral("x"), SignalValue{std::int64_t{1}});
    sink.onSignalsRegistered(QStringLiteral("d"), {});
    REQUIRE(sink.signalsReceived() == 1);
    REQUIRE(sink.registrationsReceived() == 1);

    sink.resetCounters();
    REQUIRE(sink.signalsReceived() == 0);
    REQUIRE(sink.signalsByType(SignalType::Int64) == 0);
    REQUIRE(sink.registrationsReceived() == 0);
    REQUIRE(sink.unregistrationsReceived() == 0);
}

TEST_CASE("SchemaValidator: rejects empty content with version + layouts errors", "[decoder][validator]") {
    const auto result = SchemaValidator::validateString(QStringLiteral("{}"), QStringLiteral("test.yaml"));
    REQUIRE_FALSE(result.has_value());
    const auto& errors = result.error();
    REQUIRE_FALSE(errors.empty());
    bool sawVersionError = false;
    bool sawLayoutsError = false;
    for (const auto& e : errors) {
        if (e.fieldPath == QStringLiteral("schema_version")) {
            sawVersionError = true;
        }
        if (e.fieldPath == QStringLiteral("layouts")) {
            sawLayoutsError = true;
        }
    }
    REQUIRE(sawVersionError);
    REQUIRE(sawLayoutsError);
}

TEST_CASE("SchemaValidator: minimal valid schema parses with one layout and one field", "[decoder][validator]") {
    const QString yaml = QStringLiteral(R"YAML(
schema_version: 1
description: minimal
layouts:
  - name: only
    endianness: little
    match:
      offset: 0
      bytes: [0xAA]
    min_payload_bytes: 4
    fields:
      - name: counter
        offset: 0
        encoding: uint32
        size_bytes: 4
)YAML");
    const auto result = SchemaValidator::validateString(yaml, QStringLiteral("inline.yaml"));
    REQUIRE(result.has_value());
    const auto& schema = *result;
    REQUIRE(schema.schemaVersion == 1);
    REQUIRE(schema.description == QStringLiteral("minimal"));
    REQUIRE(schema.id == QStringLiteral("inline.yaml"));
    REQUIRE(schema.layouts.size() == 1);
    const auto& layout = schema.layouts[0];
    REQUIRE(layout.name == QStringLiteral("only"));
    REQUIRE(layout.endianness == Endianness::Little);
    REQUIRE(layout.match.offset == 0);
    REQUIRE(layout.match.bytes.size() == 1);
    REQUIRE(layout.match.bytes[0] == 0xAAu);
    REQUIRE(layout.minPayloadBytes == 4);
    REQUIRE(layout.fields.size() == 1);
    REQUIRE(layout.fields[0].encoding == FieldEncoding::Uint32);
    REQUIRE(layout.fields[0].sizeBytes == 4);
}

TEST_CASE("SchemaValidator: missing layout endianness for multi-byte field is reported", "[decoder][validator]") {
    const QString yaml = QStringLiteral(R"YAML(
schema_version: 1
layouts:
  - name: bad
    match:
      offset: 0
      bytes: [0xAA]
    min_payload_bytes: 2
    fields:
      - name: x
        offset: 0
        encoding: uint16
        size_bytes: 2
)YAML");
    const auto result = SchemaValidator::validateString(yaml, QStringLiteral("inline.yaml"));
    REQUIRE_FALSE(result.has_value());
    bool sawEndiannessError = false;
    for (const auto& e : result.error()) {
        if (e.fieldPath.contains(QStringLiteral("endianness"))) {
            sawEndiannessError = true;
            break;
        }
    }
    REQUIRE(sawEndiannessError);
}

TEST_CASE("SchemaValidator: canonical schemas/decoder_schema_v1.yaml validates cleanly", "[decoder][validator]") {
    // The canonical example must always validate; if it ever does not,
    // either the validator regressed or schema v1 freeze was breached.
    const QString path = QStringLiteral(SIGNALFORGE_REPO_ROOT "/schemas/decoder_schema_v1.yaml");
    const auto result = SchemaValidator::validateFile(path);
    if (!result.has_value()) {
        for (const auto& e : result.error()) {
            UNSCOPED_INFO("validation error at " << e.filePath.toStdString() << ":" << e.lineNumber << " "
                                                 << e.fieldPath.toStdString() << " — " << e.message.toStdString());
        }
    }
    REQUIRE(result.has_value());
    const auto& schema = *result;
    REQUIRE(schema.schemaVersion == 1);
    REQUIRE(schema.layouts.size() == 2);
    REQUIRE(schema.layouts[0].name == QStringLiteral("telemetry"));
    REQUIRE(schema.layouts[1].name == QStringLiteral("heartbeat"));
}
