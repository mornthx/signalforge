// tests/unit/video/video_protocol_test.cpp
#include "video/video_protocol.hpp"
#include "zvid_test_util.hpp"

#include <QByteArray>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>

using signalforge::video::kZvidHeaderSize;
using signalforge::video::kZvidMagic;
using signalforge::video::parseZvidHeader;

namespace {
const std::uint8_t* bytes(const QByteArray& b) {
    return reinterpret_cast<const std::uint8_t*>(b.constData());
}
}  // namespace

TEST_CASE("parseZvidHeader: decodes all fields little-endian", "[video][protocol]") {
    const QByteArray dg = sfvideotest::makeHeader(kZvidMagic, 0x11223344, 0x0000A0C0, 2'764'800, 1280, 720, 3, 1440);
    const auto h = parseZvidHeader(bytes(dg), static_cast<std::size_t>(dg.size()));
    REQUIRE(h.has_value());
    CHECK(h->magic == kZvidMagic);
    CHECK(h->frame == 0x11223344U);
    CHECK(h->offset == 0x0000A0C0U);
    CHECK(h->frameBytes == 2'764'800U);
    CHECK(h->width == 1280);
    CHECK(h->height == 720);
    CHECK(h->bpp == 3);
    CHECK(h->chunkLen == 1440);
}

TEST_CASE("parseZvidHeader: rejects a too-short datagram", "[video][protocol]") {
    QByteArray dg = sfvideotest::makeHeader(kZvidMagic, 1, 0, 96, 8, 4, 3, 24);
    dg.truncate(static_cast<int>(kZvidHeaderSize) - 1);
    CHECK_FALSE(parseZvidHeader(bytes(dg), static_cast<std::size_t>(dg.size())).has_value());
}

TEST_CASE("parseZvidHeader: rejects a foreign magic", "[video][protocol]") {
    const QByteArray dg = sfvideotest::makeHeader(0xDEADBEEF, 1, 0, 96, 8, 4, 3, 24);
    CHECK_FALSE(parseZvidHeader(bytes(dg), static_cast<std::size_t>(dg.size())).has_value());
}

TEST_CASE("parseZvidHeader: rejects a null pointer", "[video][protocol]") {
    CHECK_FALSE(parseZvidHeader(nullptr, 64).has_value());
}
