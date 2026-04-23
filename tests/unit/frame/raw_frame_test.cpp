// tests/unit/frame/raw_frame_test.cpp
#include "frame/raw_frame.hpp"

#include <QByteArray>
#include <QVariant>
#include <catch2/catch_test_macros.hpp>
#include <chrono>

using namespace signalforge::frame;

TEST_CASE("RawFrame: default-constructed state is well-defined", "[frame][raw_frame]") {
    RawFrame f;
    REQUIRE(f.sourceId.isEmpty());
    REQUIRE(f.payload.isEmpty());
    REQUIRE(f.protocolHint.isEmpty());
    REQUIRE(f.deviceAt.has_value() == false);
    REQUIRE(f.sequenceNumber == 0);
    REQUIRE(f.flags == 0);
    REQUIRE(f.recvAt == SteadyTimestamp{});
}

TEST_CASE("RawFrame: fields populate as expected", "[frame][raw_frame]") {
    RawFrame f;
    f.sourceId = "dev0";
    f.payload = QByteArray::fromRawData("\x01\x02\x03", 3);
    f.protocolHint = "serial";
    f.sequenceNumber = 42;
    f.flags = 0x2;
    const auto now = std::chrono::steady_clock::now();
    f.recvAt = now;
    f.deviceAt = now + std::chrono::microseconds(100);

    REQUIRE(f.sourceId == "dev0");
    REQUIRE(f.payload.size() == 3);
    REQUIRE(static_cast<unsigned char>(f.payload[0]) == 0x01);
    REQUIRE(static_cast<unsigned char>(f.payload[1]) == 0x02);
    REQUIRE(static_cast<unsigned char>(f.payload[2]) == 0x03);
    REQUIRE(f.sequenceNumber == 42);
    REQUIRE(f.flags == 0x2);
    REQUIRE(f.recvAt == now);
    REQUIRE(f.deviceAt.has_value());
    REQUIRE(*f.deviceAt > f.recvAt);
}

TEST_CASE("RawFrame: copy is cheap via QByteArray implicit sharing", "[frame][raw_frame]") {
    RawFrame src;
    src.sourceId = "dev0";
    src.payload = QByteArray(1024 * 1024, '\xAB');  // 1 MB
    const char* srcData = src.payload.constData();

    RawFrame dst = src;  // copy
    // constData() returns the same buffer pointer under implicit sharing.
    REQUIRE(dst.payload.constData() == srcData);
    REQUIRE(dst.payload.size() == src.payload.size());
}

TEST_CASE("RawFrame: payload bytes are interpreted as 0-255 unsigned", "[frame][raw_frame]") {
    RawFrame f;
    // Populate with values covering the full unsigned-byte range.
    const unsigned char bytes[] = {0x00, 0x7F, 0x80, 0xFE, 0xFF};
    f.payload = QByteArray(reinterpret_cast<const char*>(bytes), sizeof(bytes));

    REQUIRE(f.payload.size() == 5);
    REQUIRE(static_cast<unsigned char>(f.payload[0]) == 0x00);
    REQUIRE(static_cast<unsigned char>(f.payload[1]) == 0x7F);
    REQUIRE(static_cast<unsigned char>(f.payload[2]) == 0x80);
    REQUIRE(static_cast<unsigned char>(f.payload[3]) == 0xFE);
    REQUIRE(static_cast<unsigned char>(f.payload[4]) == 0xFF);
}

TEST_CASE("RawFrame: QVariant round-trip after registerMetatypes", "[frame][raw_frame]") {
    registerMetatypes();
    registerMetatypes();  // idempotent

    RawFrame f;
    f.sourceId = "dev0";
    f.payload = QByteArray("hello", 5);
    f.sequenceNumber = 7;

    QVariant v = QVariant::fromValue(f);
    REQUIRE(v.canConvert<RawFrame>());

    const auto back = v.value<RawFrame>();
    REQUIRE(back.sourceId == f.sourceId);
    REQUIRE(back.payload == f.payload);
    REQUIRE(back.sequenceNumber == f.sequenceNumber);
}

TEST_CASE("RawFrame: deviceAt default is nullopt", "[frame][raw_frame]") {
    RawFrame f;
    REQUIRE(!f.deviceAt.has_value());
    f.deviceAt = std::chrono::steady_clock::now();
    REQUIRE(f.deviceAt.has_value());
    f.deviceAt.reset();
    REQUIRE(!f.deviceAt.has_value());
}
