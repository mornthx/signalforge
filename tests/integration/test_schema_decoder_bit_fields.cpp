#include "decode/decoder_interface.hpp"
#include "decode/schema.hpp"
#include "decode/schema_decoder.hpp"
#include "frame/raw_frame.hpp"

#include <QByteArray>
#include <QString>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

using namespace signalforge::decoder;
using signalforge::frame::RawFrame;

namespace {

class CaptureSink : public SignalValueSink {
public:
    void onSignal(std::chrono::steady_clock::time_point, const QString& id, const SignalValue& v) override {
        captured[id.toStdString()] = v;
    }
    std::unordered_map<std::string, SignalValue> captured;
};

RawFrame makeFrame(const QByteArray& payload) {
    RawFrame f;
    f.sourceId = QStringLiteral("integration:bits");
    f.payload = payload;
    f.recvAt = std::chrono::steady_clock::now();
    return f;
}

Schema makeBitSchema() {
    Schema s;
    s.schemaVersion = 1;
    s.id = QStringLiteral("inline-bits");

    Layout l;
    l.name = QStringLiteral("bits");
    l.endianness = Endianness::Little;
    l.match.offset = 0;
    l.match.bytes = {0xAA};
    l.minPayloadBytes = 3;

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

    FieldDef bf;
    bf.name = QStringLiteral("flags");
    bf.offset = 1;
    bf.encoding = FieldEncoding::BitField;
    bf.sizeBytes = 2;  // 16-bit container exercised cross-byte
    bf.bitFields.push_back({QStringLiteral("single"), 0, 1, std::nullopt});
    bf.bitFields.push_back({QStringLiteral("two_bit"), 1, 2, std::nullopt});
    bf.bitFields.push_back({QStringLiteral("byte"), 3, 8, std::nullopt});
    bf.bitFields.push_back({QStringLiteral("crossbyte"), 11, 4, std::nullopt});
    l.fields.push_back(bf);
    s.layouts.push_back(l);
    return s;
}

}  // namespace

TEST_CASE("integration decoder: bit fields of varying widths and cross-byte placement",
          "[integration][decoder][bit_fields]") {
    SchemaDecoder decoder(makeBitSchema(), QStringLiteral("integration:bits"));
    auto sink = std::make_shared<CaptureSink>();
    decoder.setSignalSink(sink);

    // Container bytes: 0xC1 0xA5 (little-endian uint16 = 0xA5C1).
    // 0xA5C1 = 0b1010_0101_1100_0001
    //   single (bit 0)        = 1     → bool true
    //   two_bit (bits 1..2)   = 0b00  → 0
    //   byte (bits 3..10)     = 0xB8  = 184  (8 bits crossing the byte boundary)
    //   crossbyte (bits 11..14) = 0b0100 = 4
    QByteArray payload(3, '\0');
    payload[0] = static_cast<char>(0xAA);
    payload[1] = static_cast<char>(0xC1);
    payload[2] = static_cast<char>(0xA5);
    decoder.onFrame(makeFrame(payload));

    REQUIRE(std::get<bool>(sink->captured["integration:bits/flags/single"]) == true);
    REQUIRE(std::get<std::int64_t>(sink->captured["integration:bits/flags/two_bit"]) == 0);
    REQUIRE(std::get<std::int64_t>(sink->captured["integration:bits/flags/byte"]) == 0xB8);
    REQUIRE(std::get<std::int64_t>(sink->captured["integration:bits/flags/crossbyte"]) == 4);
}
