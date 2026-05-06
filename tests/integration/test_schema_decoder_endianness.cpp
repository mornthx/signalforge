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

RawFrame makeFrame(const QByteArray& payload, const QString& sourceId) {
    RawFrame f;
    f.sourceId = sourceId;
    f.payload = payload;
    f.recvAt = std::chrono::steady_clock::now();
    return f;
}

Schema makeSchema(Endianness layoutEndianness, std::optional<Endianness> overrideOnSecond) {
    Schema s;
    s.schemaVersion = 1;
    Layout l;
    l.name = QStringLiteral("ed");
    l.endianness = layoutEndianness;
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
    FieldDef a{QStringLiteral("a"), 1,  FieldEncoding::Uint16, 2, std::nullopt, std::nullopt,
               std::nullopt,        {}, std::nullopt,          {}};
    l.fields.push_back(a);
    FieldDef b{QStringLiteral("b"), 3,  FieldEncoding::Uint16, 2, overrideOnSecond, std::nullopt,
               std::nullopt,        {}, std::nullopt,          {}};
    l.fields.push_back(b);
    s.layouts.push_back(l);
    return s;
}

}  // namespace

TEST_CASE("integration decoder: little-endian layout default", "[integration][decoder][endianness]") {
    SchemaDecoder decoder(makeSchema(Endianness::Little, std::nullopt), QStringLiteral("e:little"));
    auto sink = std::make_shared<CaptureSink>();
    decoder.setSignalSink(sink);

    // bytes: AA  01 02   03 04
    // little: a = 0x0201 = 513; b = 0x0403 = 1027
    decoder.onFrame(makeFrame(QByteArray::fromHex("AA01020304"), QStringLiteral("e:little")));
    REQUIRE(std::get<std::int64_t>(sink->captured["e:little/a"]) == 513);
    REQUIRE(std::get<std::int64_t>(sink->captured["e:little/b"]) == 1027);
}

TEST_CASE("integration decoder: big-endian layout default", "[integration][decoder][endianness]") {
    SchemaDecoder decoder(makeSchema(Endianness::Big, std::nullopt), QStringLiteral("e:big"));
    auto sink = std::make_shared<CaptureSink>();
    decoder.setSignalSink(sink);

    // bytes: AA  01 02   03 04
    // big: a = 0x0102 = 258; b = 0x0304 = 772
    decoder.onFrame(makeFrame(QByteArray::fromHex("AA01020304"), QStringLiteral("e:big")));
    REQUIRE(std::get<std::int64_t>(sink->captured["e:big/a"]) == 258);
    REQUIRE(std::get<std::int64_t>(sink->captured["e:big/b"]) == 772);
}

TEST_CASE("integration decoder: per-field override beats layout default", "[integration][decoder][endianness]") {
    SchemaDecoder decoder(makeSchema(Endianness::Little, Endianness::Big), QStringLiteral("e:mix"));
    auto sink = std::make_shared<CaptureSink>();
    decoder.setSignalSink(sink);

    // bytes: AA  01 02   03 04
    // a (little): 0x0201 = 513
    // b (big override): 0x0304 = 772
    decoder.onFrame(makeFrame(QByteArray::fromHex("AA01020304"), QStringLiteral("e:mix")));
    REQUIRE(std::get<std::int64_t>(sink->captured["e:mix/a"]) == 513);
    REQUIRE(std::get<std::int64_t>(sink->captured["e:mix/b"]) == 772);
}
