// tests/unit/decode/decoder_test.cpp
#include "decode/decoder_interface.hpp"
#include "decode/decoder_registrar.hpp"
#include "decode/logging_signal_value_sink.hpp"
#include "decode/schema_decoder.hpp"
#include "decode/schema_validator.hpp"
#include "drivers/driver_interface.hpp"
#include "frame/raw_frame.hpp"
#include "pipeline/frame_pipeline.hpp"
#include "pipeline/pipeline_manager.hpp"

#include <QCoreApplication>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <memory>

namespace {

class CoreAppHolder {
public:
    CoreAppHolder() {
        signalforge::frame::registerMetatypes();
        signalforge::drivers::registerMetatypes();
        if (QCoreApplication::instance() == nullptr) {
            static int argc = 1;
            static char argv0[] = "decoder_test";
            static char* argv[] = {argv0, nullptr};
            app_ = std::make_unique<QCoreApplication>(argc, argv);
        }
    }

private:
    std::unique_ptr<QCoreApplication> app_;
};
[[maybe_unused]] CoreAppHolder g_app;

}  // namespace

using signalforge::decoder::DecoderRegistrar;
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

namespace {

RawFrame frameOf(QByteArray payload, const QString& sourceId = QStringLiteral("d")) {
    RawFrame f;
    f.sourceId = sourceId;
    f.payload = std::move(payload);
    f.recvAt = std::chrono::steady_clock::now();
    return f;
}

}  // namespace

TEST_CASE("SchemaDecoder: short-payload frame is malformed, emits nothing", "[decoder][schema_decoder]") {
    auto sink = std::make_shared<LoggingSignalValueSink>();
    SchemaDecoder decoder(buildSimpleSchema(), QStringLiteral("driver_short"));
    decoder.setSignalSink(sink);

    // Magic matches but only 2 bytes total, well below min_payload_bytes=6.
    QByteArray payload(2, '\0');
    payload[0] = static_cast<char>(0xAA);
    decoder.onFrame(frameOf(std::move(payload)));
    REQUIRE(sink->signalsReceived() == 0);
}

TEST_CASE("SchemaDecoder: multi-layout schema dispatches to the right layout by magic", "[decoder][schema_decoder]") {
    using namespace signalforge::decoder;
    Schema s;
    s.schemaVersion = 1;

    Layout a;
    a.name = QStringLiteral("la");
    a.endianness = Endianness::Little;
    a.match.offset = 0;
    a.match.bytes = {0xAA};
    a.minPayloadBytes = 2;
    FieldDef fa{QStringLiteral("a_byte"),
                1,
                FieldEncoding::Uint8,
                1,
                std::nullopt,
                std::nullopt,
                std::nullopt,
                {},
                std::nullopt,
                {}};
    a.fields.push_back(fa);
    s.layouts.push_back(a);

    Layout b;
    b.name = QStringLiteral("lb");
    b.endianness = Endianness::Little;
    b.match.offset = 0;
    b.match.bytes = {0xBB};
    b.minPayloadBytes = 2;
    FieldDef fb{QStringLiteral("b_byte"),
                1,
                FieldEncoding::Uint8,
                1,
                std::nullopt,
                std::nullopt,
                std::nullopt,
                {},
                std::nullopt,
                {}};
    b.fields.push_back(fb);
    s.layouts.push_back(b);

    auto sink = std::make_shared<LoggingSignalValueSink>();
    SchemaDecoder decoder(s, QStringLiteral("d_multi"));
    decoder.setSignalSink(sink);

    decoder.onFrame(frameOf(QByteArray::fromHex("AA77")));
    REQUIRE(sink->signalsReceived() == 1);

    decoder.onFrame(frameOf(QByteArray::fromHex("BB99")));
    REQUIRE(sink->signalsReceived() == 2);
}

TEST_CASE("SchemaDecoder: per-field endianness overrides layout default", "[decoder][schema_decoder]") {
    using namespace signalforge::decoder;
    Schema s;
    s.schemaVersion = 1;

    Layout l;
    l.name = QStringLiteral("only");
    l.endianness = Endianness::Little;
    l.match.offset = 0;
    l.match.bytes = {0xAA};
    l.minPayloadBytes = 5;

    FieldDef magic{QStringLiteral("magic"),
                   0,
                   FieldEncoding::Uint8,
                   1,
                   std::nullopt,
                   std::nullopt,
                   std::nullopt,
                   {},
                   std::nullopt,
                   {}};
    l.fields.push_back(magic);

    // Layout default is little; override one field to big.
    FieldDef be16{QStringLiteral("be"), 1,  FieldEncoding::Uint16, 2, Endianness::Big, std::nullopt,
                  std::nullopt,         {}, std::nullopt,          {}};
    l.fields.push_back(be16);

    FieldDef le16{QStringLiteral("le"), 3,  FieldEncoding::Uint16, 2, std::nullopt, std::nullopt,
                  std::nullopt,         {}, std::nullopt,          {}};
    l.fields.push_back(le16);

    s.layouts.push_back(l);

    class CaptureSink : public signalforge::decoder::SignalValueSink {
    public:
        void onSignal(std::chrono::steady_clock::time_point, const QString& id, const SignalValue& v) override {
            captured[id.toStdString()] = v;
        }
        std::unordered_map<std::string, SignalValue> captured;
    };
    auto sink = std::make_shared<CaptureSink>();
    SchemaDecoder decoder(s, QStringLiteral("d_endian"));
    decoder.setSignalSink(sink);

    // Bytes: AA 01 02 03 04
    // be16 from bytes [01 02] big-endian → 0x0102 = 258
    // le16 from bytes [03 04] little-endian → 0x0403 = 1027
    decoder.onFrame(frameOf(QByteArray::fromHex("AA01020304")));

    REQUIRE(std::get<std::int64_t>(sink->captured["d_endian/be"]) == 258);
    REQUIRE(std::get<std::int64_t>(sink->captured["d_endian/le"]) == 1027);
}

TEST_CASE("SchemaDecoder: scale + offset_transform produce double signal", "[decoder][schema_decoder]") {
    using namespace signalforge::decoder;
    Schema s;
    s.schemaVersion = 1;

    Layout l;
    l.name = QStringLiteral("scaled");
    l.endianness = Endianness::Little;
    l.match.offset = 0;
    l.match.bytes = {0xAA};
    l.minPayloadBytes = 3;
    FieldDef magic{QStringLiteral("m"), 0,  FieldEncoding::Uint8, 1, std::nullopt, std::nullopt,
                   std::nullopt,        {}, std::nullopt,         {}};
    l.fields.push_back(magic);
    FieldDef temp{QStringLiteral("temp"), 1, FieldEncoding::Int16, 2, std::nullopt, 0.01, -10.0, QStringLiteral("C"),
                  std::nullopt,           {}};
    l.fields.push_back(temp);
    s.layouts.push_back(l);

    class CaptureSink : public signalforge::decoder::SignalValueSink {
    public:
        void onSignal(std::chrono::steady_clock::time_point, const QString& id, const SignalValue& v) override {
            last = std::make_pair(id, v);
        }
        std::optional<std::pair<QString, SignalValue>> last;
    };
    auto sink = std::make_shared<CaptureSink>();
    SchemaDecoder decoder(s, QStringLiteral("d_scaled"));
    decoder.setSignalSink(sink);

    // raw int16 = 0x07D0 = 2000 → 2000 * 0.01 - 10.0 = 10.0
    decoder.onFrame(frameOf(QByteArray::fromHex("AAD007")));
    REQUIRE(sink->last.has_value());
    REQUIRE(sink->last->first == QStringLiteral("d_scaled/temp"));
    const auto* d = std::get_if<double>(&sink->last->second);
    REQUIRE(d != nullptr);
    REQUIRE(*d == Catch::Approx(10.0).margin(1e-9));
}

TEST_CASE("SchemaDecoder: fixed_string honors null terminator", "[decoder][schema_decoder]") {
    using namespace signalforge::decoder;
    Schema s;
    s.schemaVersion = 1;

    Layout l;
    l.name = QStringLiteral("named");
    l.endianness = Endianness::Little;
    l.match.offset = 0;
    l.match.bytes = {0xAA};
    l.minPayloadBytes = 9;
    FieldDef magic{QStringLiteral("m"), 0,  FieldEncoding::Uint8, 1, std::nullopt, std::nullopt,
                   std::nullopt,        {}, std::nullopt,         {}};
    l.fields.push_back(magic);
    FieldDef tag{QStringLiteral("tag"), 1, FieldEncoding::FixedString, 8, std::nullopt, std::nullopt, std::nullopt, {},
                 std::nullopt,          {}};
    l.fields.push_back(tag);
    s.layouts.push_back(l);

    class CaptureSink : public signalforge::decoder::SignalValueSink {
    public:
        void onSignal(std::chrono::steady_clock::time_point, const QString& id, const SignalValue& v) override {
            last = std::make_pair(id, v);
        }
        std::optional<std::pair<QString, SignalValue>> last;
    };
    auto sink = std::make_shared<CaptureSink>();
    SchemaDecoder decoder(s, QStringLiteral("d_str"));
    decoder.setSignalSink(sink);

    // "fw-42" + NUL + 2 trailing garbage bytes
    QByteArray payload;
    payload.append(static_cast<char>(0xAA));
    payload.append("fw-42", 5);
    payload.append('\0');
    payload.append(static_cast<char>(0x99));
    payload.append(static_cast<char>(0x77));
    decoder.onFrame(frameOf(payload));

    REQUIRE(sink->last.has_value());
    const auto* qs = std::get_if<QString>(&sink->last->second);
    REQUIRE(qs != nullptr);
    REQUIRE(*qs == QStringLiteral("fw-42"));
}

TEST_CASE("SchemaDecoder: cross-byte bit field assembles bits correctly", "[decoder][schema_decoder]") {
    using namespace signalforge::decoder;
    Schema s;
    s.schemaVersion = 1;

    Layout l;
    l.name = QStringLiteral("crossbit");
    l.endianness = Endianness::Little;
    l.match.offset = 0;
    l.match.bytes = {0xAA};
    l.minPayloadBytes = 3;

    FieldDef magic{QStringLiteral("m"), 0,  FieldEncoding::Uint8, 1, std::nullopt, std::nullopt,
                   std::nullopt,        {}, std::nullopt,         {}};
    l.fields.push_back(magic);

    FieldDef bf{QStringLiteral("flags"),
                1,
                FieldEncoding::BitField,
                2,
                std::nullopt,
                std::nullopt,
                std::nullopt,
                {},
                std::nullopt,
                {}};
    BitFieldDef low{QStringLiteral("low8"), 0, 8, std::nullopt};
    BitFieldDef cross{QStringLiteral("cross"), 4, 8, std::nullopt};
    BitFieldDef top{QStringLiteral("top4"), 12, 4, std::nullopt};
    bf.bitFields = {low, cross, top};
    l.fields.push_back(bf);

    s.layouts.push_back(l);

    class CaptureSink : public signalforge::decoder::SignalValueSink {
    public:
        void onSignal(std::chrono::steady_clock::time_point, const QString& id, const SignalValue& v) override {
            captured[id.toStdString()] = v;
        }
        std::unordered_map<std::string, SignalValue> captured;
    };
    auto sink = std::make_shared<CaptureSink>();
    SchemaDecoder decoder(s, QStringLiteral("d_bf"));
    decoder.setSignalSink(sink);

    // bytes: AA  D2 4B
    // little-endian uint16 over [D2 4B] = 0x4BD2 = 0b0100_1011_1101_0010
    // low8  bits 0..7  = 0xD2 = 210
    // cross bits 4..11 = 0xBD = 189   (bits 4..11 of 0x4BD2)
    // top4  bits 12..15 = 0x4 = 4
    decoder.onFrame(frameOf(QByteArray::fromHex("AAD24B")));

    REQUIRE(std::get<std::int64_t>(sink->captured["d_bf/flags/low8"]) == 0xD2);
    REQUIRE(std::get<std::int64_t>(sink->captured["d_bf/flags/cross"]) == 0xBD);
    REQUIRE(std::get<std::int64_t>(sink->captured["d_bf/flags/top4"]) == 0x4);
}

TEST_CASE("SchemaDecoder: float32 is decoded with endianness", "[decoder][schema_decoder]") {
    using namespace signalforge::decoder;
    Schema s;
    s.schemaVersion = 1;

    Layout l;
    l.name = QStringLiteral("flt");
    l.endianness = Endianness::Little;
    l.match.offset = 0;
    l.match.bytes = {0xAA};
    l.minPayloadBytes = 5;
    FieldDef magic{QStringLiteral("m"), 0,  FieldEncoding::Uint8, 1, std::nullopt, std::nullopt,
                   std::nullopt,        {}, std::nullopt,         {}};
    l.fields.push_back(magic);
    FieldDef f{QStringLiteral("v"),    1,
               FieldEncoding::Float32, 4,
               std::nullopt,           std::nullopt,
               std::nullopt,           QStringLiteral("V"),
               std::nullopt,           {}};
    l.fields.push_back(f);
    s.layouts.push_back(l);

    class CaptureSink : public signalforge::decoder::SignalValueSink {
    public:
        void onSignal(std::chrono::steady_clock::time_point, const QString&, const SignalValue& v) override {
            last = v;
        }
        std::optional<SignalValue> last;
    };
    auto sink = std::make_shared<CaptureSink>();
    SchemaDecoder decoder(s, QStringLiteral("d_flt"));
    decoder.setSignalSink(sink);

    // float32 little-endian for 1.5 = 0x3FC00000.  In little-endian bytes:
    // 00 00 C0 3F.
    decoder.onFrame(frameOf(QByteArray::fromHex("AA0000C03F")));
    REQUIRE(sink->last.has_value());
    const auto* d = std::get_if<double>(&sink->last.value());
    REQUIRE(d != nullptr);
    REQUIRE(*d == Catch::Approx(1.5).margin(1e-7));
}

TEST_CASE("DecoderRegistrar: driverTypeOf splits on first colon", "[decoder][registrar]") {
    REQUIRE(DecoderRegistrar::driverTypeOf(QStringLiteral("serial:0")) == QStringLiteral("serial"));
    REQUIRE(DecoderRegistrar::driverTypeOf(QStringLiteral("tcp:127.0.0.1:9000")) == QStringLiteral("tcp"));
    REQUIRE(DecoderRegistrar::driverTypeOf(QStringLiteral("udp:foo")) == QStringLiteral("udp"));
    REQUIRE(DecoderRegistrar::driverTypeOf(QStringLiteral("nocolon")).isEmpty());
}

TEST_CASE("DecoderRegistrar: empty schema map does not attach decoders", "[decoder][registrar]") {
    signalforge::pipeline::PipelineManager manager;
    DecoderRegistrar registrar(&manager, /*map=*/{}, /*sink=*/nullptr);
    REQUIRE(registrar.decoderCount() == 0);
}

TEST_CASE("DecoderRegistrar: unknown driver type is skipped, no decoder attached", "[decoder][registrar]") {
    signalforge::pipeline::PipelineManager manager;
    DecoderRegistrar registrar(&manager, {{QStringLiteral("serial"), QStringLiteral("/tmp/never-read.yaml")}}, nullptr);
    // Directly invoke the slot via signal name (private slot but we can test
    // by emitting from the manager).  The pipeline pointer is irrelevant
    // here because the registrar bails on the unknown type before touching it.
    QMetaObject::invokeMethod(&registrar, "onPipelineAttached", Qt::DirectConnection,
                              Q_ARG(QString, QStringLiteral("usb:0")),
                              Q_ARG(signalforge::pipeline::FramePipeline*, nullptr));
    REQUIRE(registrar.decoderCount() == 0);
}

TEST_CASE("SchemaValidator: schema_version != 1 is rejected with version-list message", "[decoder][validator]") {
    const QString yaml = QStringLiteral(R"YAML(
schema_version: 2
layouts:
  - name: only
    endianness: little
    match:
      offset: 0
      bytes: [0xAA]
    min_payload_bytes: 1
    fields:
      - name: x
        offset: 0
        encoding: uint8
        size_bytes: 1
)YAML");
    const auto result = SchemaValidator::validateString(yaml, QStringLiteral("v.yaml"));
    REQUIRE_FALSE(result.has_value());
    bool sawVersionMessage = false;
    for (const auto& e : result.error()) {
        if (e.fieldPath == QStringLiteral("schema_version") && e.message.contains(QStringLiteral("[1]"))) {
            sawVersionMessage = true;
            break;
        }
    }
    REQUIRE(sawVersionMessage);
}

TEST_CASE("SchemaValidator: invalid encoding enum is rejected with allowed-list", "[decoder][validator]") {
    const QString yaml = QStringLiteral(R"YAML(
schema_version: 1
layouts:
  - name: only
    endianness: little
    match:
      offset: 0
      bytes: [0xAA]
    min_payload_bytes: 1
    fields:
      - name: x
        offset: 0
        encoding: not_real
        size_bytes: 1
)YAML");
    const auto result = SchemaValidator::validateString(yaml, QStringLiteral("e.yaml"));
    REQUIRE_FALSE(result.has_value());
    bool sawEncodingError = false;
    for (const auto& e : result.error()) {
        if (e.fieldPath.endsWith(QStringLiteral(".encoding")) && e.message.contains(QStringLiteral("not_real")) &&
            e.message.contains(QStringLiteral("uint16"))) {
            sawEncodingError = true;
            break;
        }
    }
    REQUIRE(sawEncodingError);
}

TEST_CASE("SchemaValidator: size_bytes inconsistent with encoding is rejected", "[decoder][validator]") {
    const QString yaml = QStringLiteral(R"YAML(
schema_version: 1
layouts:
  - name: only
    endianness: little
    match:
      offset: 0
      bytes: [0xAA]
    min_payload_bytes: 4
    fields:
      - name: x
        offset: 0
        encoding: uint16
        size_bytes: 4
)YAML");
    const auto result = SchemaValidator::validateString(yaml, QStringLiteral("s.yaml"));
    REQUIRE_FALSE(result.has_value());
    bool sawSizeError = false;
    for (const auto& e : result.error()) {
        if (e.fieldPath.endsWith(QStringLiteral(".size_bytes")) && e.message.contains(QStringLiteral("uint16")) &&
            e.message.contains(QStringLiteral("expected 2"))) {
            sawSizeError = true;
            break;
        }
    }
    REQUIRE(sawSizeError);
}

TEST_CASE("SchemaValidator: duplicate field names within a layout are rejected", "[decoder][validator]") {
    const QString yaml = QStringLiteral(R"YAML(
schema_version: 1
layouts:
  - name: dup
    endianness: little
    match:
      offset: 0
      bytes: [0xAA]
    min_payload_bytes: 8
    fields:
      - name: counter
        offset: 0
        encoding: uint32
        size_bytes: 4
      - name: counter
        offset: 4
        encoding: uint32
        size_bytes: 4
)YAML");
    const auto result = SchemaValidator::validateString(yaml, QStringLiteral("d.yaml"));
    REQUIRE_FALSE(result.has_value());
    bool sawDuplicate = false;
    for (const auto& e : result.error()) {
        if (e.message.contains(QStringLiteral("duplicate field name"))) {
            sawDuplicate = true;
            break;
        }
    }
    REQUIRE(sawDuplicate);
}

TEST_CASE("SchemaValidator: bit_field range overlap is rejected", "[decoder][validator]") {
    const QString yaml = QStringLiteral(R"YAML(
schema_version: 1
layouts:
  - name: ov
    endianness: little
    match:
      offset: 0
      bytes: [0xAA]
    min_payload_bytes: 1
    fields:
      - name: status
        offset: 0
        encoding: bitfield
        size_bytes: 1
        bit_fields:
          - { name: a, bit_start: 0, bit_count: 3 }
          - { name: b, bit_start: 2, bit_count: 2 }
)YAML");
    const auto result = SchemaValidator::validateString(yaml, QStringLiteral("ov.yaml"));
    REQUIRE_FALSE(result.has_value());
    bool sawOverlap = false;
    for (const auto& e : result.error()) {
        if (e.message.contains(QStringLiteral("overlap"))) {
            sawOverlap = true;
            break;
        }
    }
    REQUIRE(sawOverlap);
}

TEST_CASE("SchemaValidator: bit_field exceeding container size is rejected", "[decoder][validator]") {
    const QString yaml = QStringLiteral(R"YAML(
schema_version: 1
layouts:
  - name: of
    endianness: little
    match:
      offset: 0
      bytes: [0xAA]
    min_payload_bytes: 1
    fields:
      - name: status
        offset: 0
        encoding: bitfield
        size_bytes: 1
        bit_fields:
          - { name: too_wide, bit_start: 5, bit_count: 5 }
)YAML");
    const auto result = SchemaValidator::validateString(yaml, QStringLiteral("of.yaml"));
    REQUIRE_FALSE(result.has_value());
    bool sawOverflow = false;
    for (const auto& e : result.error()) {
        if (e.message.contains(QStringLiteral("does not fit"))) {
            sawOverflow = true;
            break;
        }
    }
    REQUIRE(sawOverflow);
}

TEST_CASE("SchemaValidator: bit_count zero is rejected", "[decoder][validator]") {
    const QString yaml = QStringLiteral(R"YAML(
schema_version: 1
layouts:
  - name: zc
    endianness: little
    match:
      offset: 0
      bytes: [0xAA]
    min_payload_bytes: 1
    fields:
      - name: status
        offset: 0
        encoding: bitfield
        size_bytes: 1
        bit_fields:
          - { name: zero, bit_start: 0, bit_count: 0 }
)YAML");
    const auto result = SchemaValidator::validateString(yaml, QStringLiteral("zc.yaml"));
    REQUIRE_FALSE(result.has_value());
    bool sawError = false;
    for (const auto& e : result.error()) {
        if (e.fieldPath.contains(QStringLiteral("bit_count"))) {
            sawError = true;
            break;
        }
    }
    REQUIRE(sawError);
}

TEST_CASE("SchemaValidator: rejects 'bool' as top-level field encoding", "[decoder][validator]") {
    const QString yaml = QStringLiteral(R"YAML(
schema_version: 1
layouts:
  - name: bo
    endianness: little
    match:
      offset: 0
      bytes: [0xAA]
    min_payload_bytes: 1
    fields:
      - name: flag
        offset: 0
        encoding: bool
        size_bytes: 1
)YAML");
    const auto result = SchemaValidator::validateString(yaml, QStringLiteral("bo.yaml"));
    REQUIRE_FALSE(result.has_value());
    bool sawBoolReject = false;
    for (const auto& e : result.error()) {
        if (e.message.contains(QStringLiteral("bit_count: 1"))) {
            sawBoolReject = true;
            break;
        }
    }
    REQUIRE(sawBoolReject);
}

TEST_CASE("SchemaValidator: invalid_encoding fixture reports valid 1-based line", "[decoder][validator][fixtures]") {
    const QString path =
        QStringLiteral(SIGNALFORGE_REPO_ROOT "/tests/integration/fixtures/invalid_schemas/invalid_encoding.yaml");
    const auto result = SchemaValidator::validateFile(path);
    REQUIRE_FALSE(result.has_value());
    bool sawWithLine = false;
    for (const auto& e : result.error()) {
        if (e.fieldPath.endsWith(QStringLiteral(".encoding")) && e.lineNumber > 0) {
            sawWithLine = true;
            break;
        }
    }
    REQUIRE(sawWithLine);
}

TEST_CASE("SchemaValidator: every invalid fixture validates to a non-empty error list",
          "[decoder][validator][fixtures]") {
    const QStringList fixtures = {
        QStringLiteral("missing_version.yaml"),  QStringLiteral("missing_endianness.yaml"),
        QStringLiteral("invalid_encoding.yaml"), QStringLiteral("bit_overlap.yaml"),
        QStringLiteral("bit_overflow.yaml"),     QStringLiteral("duplicate_field.yaml"),
    };
    for (const auto& f : fixtures) {
        const QString path = QStringLiteral(SIGNALFORGE_REPO_ROOT "/tests/integration/fixtures/invalid_schemas/") + f;
        const auto result = SchemaValidator::validateFile(path);
        UNSCOPED_INFO("fixture: " << f.toStdString());
        REQUIRE_FALSE(result.has_value());
        REQUIRE_FALSE(result.error().empty());
    }
}

TEST_CASE("SchemaValidator: example temperature_sensor.yaml validates and exposes 5 fields",
          "[decoder][validator][fixtures]") {
    const QString path = QStringLiteral(SIGNALFORGE_REPO_ROOT "/examples/schemas/temperature_sensor.yaml");
    const auto result = SchemaValidator::validateFile(path);
    if (!result.has_value()) {
        for (const auto& e : result.error()) {
            UNSCOPED_INFO("err " << e.fieldPath.toStdString() << ":" << e.lineNumber << " " << e.message.toStdString());
        }
    }
    REQUIRE(result.has_value());
    REQUIRE(result->layouts.size() == 1);
    // 6 fields per the spec example: timestamp_ms, temperature, pressure,
    // status (bit-field parent), crc, padding.
    REQUIRE(result->layouts[0].fields.size() == 6);
}

TEST_CASE("SchemaValidator: example modbus_style.yaml validates with 2 layouts", "[decoder][validator][fixtures]") {
    const QString path = QStringLiteral(SIGNALFORGE_REPO_ROOT "/examples/schemas/modbus_style.yaml");
    const auto result = SchemaValidator::validateFile(path);
    if (!result.has_value()) {
        for (const auto& e : result.error()) {
            UNSCOPED_INFO("err " << e.fieldPath.toStdString() << ":" << e.lineNumber << " " << e.message.toStdString());
        }
    }
    REQUIRE(result.has_value());
    REQUIRE(result->layouts.size() == 2);
    REQUIRE(result->layouts[0].name == QStringLiteral("read_response"));
    REQUIRE(result->layouts[1].name == QStringLiteral("write_ack"));
}

TEST_CASE("DecoderRegistrar: valid schema attaches decoder on pipelineAttached", "[decoder][registrar]") {
    using signalforge::pipeline::FramePipeline;
    using signalforge::pipeline::PipelineConfig;
    using signalforge::pipeline::PipelineManager;

    const QString schemaPath = QStringLiteral(SIGNALFORGE_REPO_ROOT "/schemas/decoder_schema_v1.yaml");
    PipelineConfig cfg;
    cfg.driverId = QStringLiteral("replay:fixture");
    FramePipeline pipeline(cfg);

    PipelineManager manager;
    DecoderRegistrar registrar(&manager, {{QStringLiteral("replay"), schemaPath}},
                               std::make_shared<LoggingSignalValueSink>());

    REQUIRE(pipeline.sinkCount() == 0);
    REQUIRE(registrar.decoderCount() == 0);

    QMetaObject::invokeMethod(&registrar, "onPipelineAttached", Qt::DirectConnection, Q_ARG(QString, cfg.driverId),
                              Q_ARG(FramePipeline*, &pipeline));

    REQUIRE(registrar.decoderCount() == 1);
    REQUIRE(pipeline.sinkCount() == 1);

    QMetaObject::invokeMethod(&registrar, "onPipelineDetached", Qt::DirectConnection, Q_ARG(QString, cfg.driverId));
    REQUIRE(registrar.decoderCount() == 0);
}
