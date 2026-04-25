// tests/unit/decode/decoder_test.cpp
#include "decode/decoder_interface.hpp"
#include "decode/logging_signal_value_sink.hpp"
#include "decode/schema_decoder.hpp"
#include "decode/schema_validator.hpp"
#include "frame/raw_frame.hpp"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <memory>

using signalforge::decoder::Endianness;
using signalforge::decoder::FieldEncoding;
using signalforge::decoder::LoggingSignalValueSink;
using signalforge::decoder::SchemaDecoder;
using signalforge::decoder::SchemaValidator;
using signalforge::decoder::SignalMetadata;
using signalforge::decoder::SignalType;
using signalforge::decoder::SignalValue;
using signalforge::frame::RawFrame;

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

namespace {

signalforge::decoder::Schema buildSimpleSchema() {
    using namespace signalforge::decoder;
    Schema s;
    s.schemaVersion = 1;
    s.id = QStringLiteral("inline");
    s.description = QStringLiteral("simple");

    Layout layout;
    layout.name = QStringLiteral("only");
    layout.endianness = Endianness::Little;
    layout.match.offset = 0;
    layout.match.bytes = {0xAA};
    layout.minPayloadBytes = 6;

    FieldDef magic;
    magic.name = QStringLiteral("magic");
    magic.offset = 0;
    magic.encoding = FieldEncoding::Uint8;
    magic.sizeBytes = 1;
    layout.fields.push_back(magic);

    FieldDef counter;
    counter.name = QStringLiteral("counter");
    counter.offset = 1;
    counter.encoding = FieldEncoding::Uint16;
    counter.sizeBytes = 2;
    layout.fields.push_back(counter);

    FieldDef temperature;
    temperature.name = QStringLiteral("temperature");
    temperature.offset = 3;
    temperature.encoding = FieldEncoding::Int16;
    temperature.sizeBytes = 2;
    temperature.scale = 0.01;
    layout.fields.push_back(temperature);

    FieldDef status;
    status.name = QStringLiteral("status");
    status.offset = 5;
    status.encoding = FieldEncoding::BitField;
    status.sizeBytes = 1;
    BitFieldDef alarm{QStringLiteral("alarm"), 0, 1, std::nullopt};
    BitFieldDef mode{QStringLiteral("mode"), 1, 2, std::nullopt};
    BitFieldDef code{QStringLiteral("code"), 3, 5, std::nullopt};
    status.bitFields.push_back(alarm);
    status.bitFields.push_back(mode);
    status.bitFields.push_back(code);
    layout.fields.push_back(status);

    s.layouts.push_back(std::move(layout));
    return s;
}

}  // namespace

TEST_CASE("SchemaDecoder: signalMetadata catalogs every field + bit field", "[decoder][schema_decoder]") {
    SchemaDecoder decoder(buildSimpleSchema(), QStringLiteral("driver1"));
    const auto meta = decoder.signalMetadata();
    // magic (1) + counter (1) + temperature (1) + 3 bit fields = 6 signals.
    REQUIRE(meta.size() == 6);
    REQUIRE(meta[0].id == QStringLiteral("driver1/magic"));
    REQUIRE(meta[0].type == SignalType::Int64);
    REQUIRE(meta[1].id == QStringLiteral("driver1/counter"));
    REQUIRE(meta[1].type == SignalType::Int64);
    REQUIRE(meta[2].id == QStringLiteral("driver1/temperature"));
    REQUIRE(meta[2].type == SignalType::Double);  // scale present → Double
    REQUIRE(meta[3].id == QStringLiteral("driver1/status/alarm"));
    REQUIRE(meta[3].type == SignalType::Bool);
    REQUIRE(meta[4].id == QStringLiteral("driver1/status/mode"));
    REQUIRE(meta[4].type == SignalType::Int64);
    REQUIRE(meta[5].id == QStringLiteral("driver1/status/code"));
    REQUIRE(meta[5].type == SignalType::Int64);
}

TEST_CASE("SchemaDecoder: matched frame produces all expected signals", "[decoder][schema_decoder]") {
    auto sink = std::make_shared<LoggingSignalValueSink>();
    SchemaDecoder decoder(buildSimpleSchema(), QStringLiteral("driver2"));
    decoder.setSignalSink(sink);
    REQUIRE(sink->registrationsReceived() == 1);

    // Build a frame: magic=0xAA, counter=0x0102 (little-endian → 0x0201 = 513),
    // temperature=raw 200 (little-endian) * 0.01 = 2.0,
    // status byte 0b01010001: alarm=1, mode=0b00=0, code=0b01010=10.
    QByteArray payload(6, '\0');
    payload[0] = static_cast<char>(0xAA);
    payload[1] = static_cast<char>(0x02);
    payload[2] = static_cast<char>(0x01);
    payload[3] = static_cast<char>(0xC8);
    payload[4] = static_cast<char>(0x00);
    payload[5] = static_cast<char>(0b01010001);

    RawFrame frame;
    frame.sourceId = QStringLiteral("driver2");
    frame.payload = payload;
    frame.recvAt = std::chrono::steady_clock::now();

    decoder.onFrame(frame);
    REQUIRE(sink->signalsReceived() == 6);
    REQUIRE(sink->signalsByType(SignalType::Bool) == 1);    // alarm
    REQUIRE(sink->signalsByType(SignalType::Int64) == 4);   // magic, counter, mode, code
    REQUIRE(sink->signalsByType(SignalType::Double) == 1);  // temperature
}

TEST_CASE("SchemaDecoder: unmatched frame emits nothing", "[decoder][schema_decoder]") {
    auto sink = std::make_shared<LoggingSignalValueSink>();
    SchemaDecoder decoder(buildSimpleSchema(), QStringLiteral("driver3"));
    decoder.setSignalSink(sink);

    QByteArray payload(6, '\0');
    payload[0] = static_cast<char>(0xBB);  // wrong magic
    RawFrame frame;
    frame.sourceId = QStringLiteral("driver3");
    frame.payload = payload;
    frame.recvAt = std::chrono::steady_clock::now();

    decoder.onFrame(frame);
    REQUIRE(sink->signalsReceived() == 0);
}
