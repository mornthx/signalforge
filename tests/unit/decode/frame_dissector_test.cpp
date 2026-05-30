// tests/unit/decode/frame_dissector_test.cpp
//
// M34 P3-S1 — FrameDissector: resolve a raw payload into a dissection tree
// (byte ranges + decoded values) using the same primitives as SchemaDecoder.

#include "decode/frame_dissector.hpp"
#include "decode/schema.hpp"

#include <QByteArray>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <optional>

namespace dec = signalforge::decoder;
using Catch::Approx;

namespace {

dec::FieldDef num(QString name, int offset, dec::FieldEncoding enc, int size, QString unit = QString(),
                  std::optional<double> scale = std::nullopt) {
    dec::FieldDef f;
    f.name = std::move(name);
    f.offset = offset;
    f.encoding = enc;
    f.sizeBytes = size;
    f.unit = std::move(unit);
    f.scale = scale;
    return f;
}

// A temperature-sensor-style 16-byte little-endian layout (mirrors
// examples/schemas/temperature_sensor.yaml), built in code so the test is
// self-contained.
dec::Schema tempSchema() {
    dec::Layout layout;
    layout.name = QStringLiteral("telemetry");
    layout.endianness = dec::Endianness::Little;
    layout.match.offset = 0;
    layout.match.bytes = {0xAA};
    layout.minPayloadBytes = 16;
    layout.fields.push_back(
        num(QStringLiteral("timestamp_ms"), 1, dec::FieldEncoding::Uint32, 4, QStringLiteral("ms")));
    layout.fields.push_back(
        num(QStringLiteral("temperature"), 5, dec::FieldEncoding::Int16, 2, QStringLiteral("C"), 0.01));
    layout.fields.push_back(
        num(QStringLiteral("pressure"), 7, dec::FieldEncoding::Uint16, 2, QStringLiteral("kPa"), 0.1));

    dec::FieldDef status = num(QStringLiteral("status"), 9, dec::FieldEncoding::BitField, 1);
    status.bitFields = {
        dec::BitFieldDef{QStringLiteral("alarm"), 0, 1, std::nullopt},
        dec::BitFieldDef{QStringLiteral("calibration_active"), 1, 1, std::nullopt},
        dec::BitFieldDef{QStringLiteral("sensor_mode"), 2, 2, std::nullopt},
        dec::BitFieldDef{QStringLiteral("reserved"), 4, 4, std::nullopt},
    };
    layout.fields.push_back(status);
    layout.fields.push_back(num(QStringLiteral("crc"), 10, dec::FieldEncoding::Uint16, 2));
    layout.fields.push_back(num(QStringLiteral("padding"), 12, dec::FieldEncoding::Uint32, 4));

    dec::Schema schema;
    schema.id = QStringLiteral("temperature_sensor");
    schema.layouts = {layout};
    return schema;
}

// 16-byte payload: magic AA, ts=1000, temp raw 2345 (23.45C), pressure raw
// 1013 (101.3kPa), status 0x05 (alarm=1, mode=1), crc, padding.
QByteArray tempFrame() {
    return QByteArray::fromHex("AA"
                               "E8030000"  // timestamp_ms = 1000 (LE)
                               "2909"      // temperature  = 2345 -> 23.45
                               "F503"      // pressure     = 1013 -> 101.3
                               "05"        // status: alarm=1, mode=1
                               "ABCD"      // crc
                               "00000000"  // padding
    );
}

const dec::DissectedField* find(const dec::Dissection& d, const QString& name) {
    for (const auto& f : d.fields) {
        if (f.name == name) {
            return &f;
        }
    }
    return nullptr;
}

}  // namespace

TEST_CASE("P3-S1: dissects each field with byte ranges and decoded values", "[decode][dissect][m34]") {
    dec::FrameDissector dissector(tempSchema());
    const dec::Dissection d = dissector.dissect(tempFrame());

    REQUIRE(d.matched);
    CHECK(d.layoutName == QStringLiteral("telemetry"));
    CHECK(d.diagnostic.isEmpty());
    CHECK(d.payloadSize == 16);

    // Synthetic magic node accounts for byte 0.
    const auto* magic = find(d, QStringLiteral("(magic)"));
    REQUIRE(magic != nullptr);
    CHECK(magic->byteOffset == 0);
    CHECK(magic->byteLength == 1);
    CHECK(magic->rawHex == QStringLiteral("aa"));

    const auto* ts = find(d, QStringLiteral("timestamp_ms"));
    REQUIRE(ts != nullptr);
    CHECK(ts->byteOffset == 1);
    CHECK(ts->byteLength == 4);
    CHECK(ts->value == QStringLiteral("1000"));
    CHECK(ts->unit == QStringLiteral("ms"));
    CHECK(ts->typeLabel == QStringLiteral("uint32"));

    // int16 with scale -> double, and the type label carries the scale.
    const auto* temp = find(d, QStringLiteral("temperature"));
    REQUIRE(temp != nullptr);
    CHECK(temp->byteOffset == 5);
    CHECK(temp->byteLength == 2);
    CHECK(temp->value.toDouble() == Approx(23.45));
    CHECK(temp->rawHex == QStringLiteral("29 09"));
    CHECK(temp->typeLabel.contains(QStringLiteral("int16")));
    CHECK(temp->typeLabel.contains(QStringLiteral("0.01")));

    const auto* press = find(d, QStringLiteral("pressure"));
    REQUIRE(press != nullptr);
    CHECK(press->value.toDouble() == Approx(101.3));
}

TEST_CASE("P3-S1: bitfield expands into bit-slice children", "[decode][dissect][m34]") {
    dec::FrameDissector dissector(tempSchema());
    const dec::Dissection d = dissector.dissect(tempFrame());

    const auto* status = find(d, QStringLiteral("status"));
    REQUIRE(status != nullptr);
    REQUIRE(status->children.size() == 4);
    // Children highlight the container byte (offset 9, 1 byte).
    CHECK(status->byteOffset == 9);
    CHECK(status->children[0].byteOffset == 9);
    CHECK(status->children[0].byteLength == 1);

    CHECK(status->children[0].name == QStringLiteral("alarm"));
    CHECK(status->children[0].bitStart == 0);
    CHECK(status->children[0].bitCount == 1);
    CHECK(status->children[0].value == QStringLiteral("true"));  // bit0 of 0x05

    CHECK(status->children[1].name == QStringLiteral("calibration_active"));
    CHECK(status->children[1].value == QStringLiteral("false"));  // bit1 of 0x05

    CHECK(status->children[2].name == QStringLiteral("sensor_mode"));
    CHECK(status->children[2].bitCount == 2);
    CHECK(status->children[2].value == QStringLiteral("1"));  // bits2-3 of 0x05 -> 1
}

TEST_CASE("P3-S1: unmatched payload reports no match, no fields", "[decode][dissect][m34]") {
    dec::FrameDissector dissector(tempSchema());
    const dec::Dissection d = dissector.dissect(QByteArray::fromHex("BB1122"));

    CHECK_FALSE(d.matched);
    CHECK(d.fields.empty());
    CHECK_FALSE(d.diagnostic.isEmpty());
}

TEST_CASE("P3-S1: malformed (short) frame still dissects in-bounds fields", "[decode][dissect][m34]") {
    dec::FrameDissector dissector(tempSchema());
    // Magic + timestamp + temperature only (7 bytes); below min_payload_bytes.
    const dec::Dissection d = dissector.dissect(QByteArray::fromHex("AA"
                                                                    "E8030000"
                                                                    "2909"));

    REQUIRE(d.matched);  // magic matched
    CHECK(d.diagnostic.contains(QStringLiteral("malformed")));

    const auto* ts = find(d, QStringLiteral("timestamp_ms"));
    REQUIRE(ts != nullptr);
    CHECK_FALSE(ts->truncated);
    CHECK(ts->value == QStringLiteral("1000"));

    // Fields beyond the truncation are marked, not crashed.
    const auto* press = find(d, QStringLiteral("pressure"));
    REQUIRE(press != nullptr);
    CHECK(press->truncated);
    CHECK(press->value == QStringLiteral("—"));

    const auto* status = find(d, QStringLiteral("status"));
    REQUIRE(status != nullptr);
    CHECK(status->truncated);
}

TEST_CASE("P3-S1: big-endian and signed sign-extension match the decoder", "[decode][dissect][m34]") {
    dec::Layout layout;
    layout.name = QStringLiteral("be");
    layout.endianness = dec::Endianness::Big;
    layout.match.offset = 0;
    layout.match.bytes = {0x7E};
    layout.minPayloadBytes = 5;
    layout.fields.push_back(num(QStringLiteral("temp"), 1, dec::FieldEncoding::Int16, 2));  // big-endian signed
    layout.fields.push_back(num(QStringLiteral("flow"), 3, dec::FieldEncoding::Uint16, 2));
    dec::Schema schema;
    schema.layouts = {layout};
    dec::FrameDissector dissector(schema);

    // temp = 0xFFFE big-endian = -2 (signed); flow = 0x0102 = 258.
    const dec::Dissection d = dissector.dissect(QByteArray::fromHex("7E"
                                                                    "FFFE"
                                                                    "0102"));
    REQUIRE(d.matched);
    const auto* temp = find(d, QStringLiteral("temp"));
    REQUIRE(temp != nullptr);
    CHECK(temp->value == QStringLiteral("-2"));
    const auto* flow = find(d, QStringLiteral("flow"));
    REQUIRE(flow != nullptr);
    CHECK(flow->value == QStringLiteral("258"));
}

TEST_CASE("P3-S1: float32 and fixed string decode", "[decode][dissect][m34]") {
    dec::Layout layout;
    layout.name = QStringLiteral("mixed");
    layout.endianness = dec::Endianness::Little;
    layout.match.offset = 0;
    layout.match.bytes = {0x5A};
    layout.minPayloadBytes = 9;
    layout.fields.push_back(num(QStringLiteral("voltage"), 1, dec::FieldEncoding::Float32, 4, QStringLiteral("V")));
    layout.fields.push_back(num(QStringLiteral("tag"), 5, dec::FieldEncoding::FixedString, 4));
    dec::Schema schema;
    schema.layouts = {layout};
    dec::FrameDissector dissector(schema);

    // voltage 1.5f LE = 00 00 C0 3F ; tag = "AB\0" + pad.
    const dec::Dissection d = dissector.dissect(QByteArray::fromHex("5A"
                                                                    "0000C03F") +
                                                QByteArray("AB\0X", 4));
    REQUIRE(d.matched);
    const auto* v = find(d, QStringLiteral("voltage"));
    REQUIRE(v != nullptr);
    CHECK(v->value.toDouble() == Approx(1.5));
    CHECK(v->typeLabel == QStringLiteral("float32"));
    const auto* tag = find(d, QStringLiteral("tag"));
    REQUIRE(tag != nullptr);
    CHECK(tag->value == QStringLiteral("AB"));  // null-terminated
    CHECK(tag->typeLabel == QStringLiteral("char[4]"));
}

TEST_CASE("P3-S1: first matching layout wins among several", "[decode][dissect][m34]") {
    dec::Layout a;
    a.name = QStringLiteral("alpha");
    a.match.bytes = {0x01};
    a.minPayloadBytes = 2;
    a.fields.push_back(num(QStringLiteral("a_field"), 1, dec::FieldEncoding::Uint8, 1));
    dec::Layout b;
    b.name = QStringLiteral("beta");
    b.match.bytes = {0x02};
    b.minPayloadBytes = 2;
    b.fields.push_back(num(QStringLiteral("b_field"), 1, dec::FieldEncoding::Uint8, 1));
    dec::Schema schema;
    schema.layouts = {a, b};
    dec::FrameDissector dissector(schema);

    const dec::Dissection d = dissector.dissect(QByteArray::fromHex("0207"));
    REQUIRE(d.matched);
    CHECK(d.layoutName == QStringLiteral("beta"));
    REQUIRE(find(d, QStringLiteral("b_field")) != nullptr);
    CHECK(find(d, QStringLiteral("b_field"))->value == QStringLiteral("7"));
    CHECK(find(d, QStringLiteral("a_field")) == nullptr);
}

TEST_CASE("P3-S1: empty schema yields a diagnostic, not a crash", "[decode][dissect][m34]") {
    dec::FrameDissector dissector(dec::Schema{});
    const dec::Dissection d = dissector.dissect(QByteArray::fromHex("DEADBEEF"));
    CHECK_FALSE(d.matched);
    CHECK(d.diagnostic.contains(QStringLiteral("no layouts")));
}
