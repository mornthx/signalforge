#include "decode/logging_signal_value_sink.hpp"
#include "decode/schema_decoder.hpp"
#include "decode/schema_validator.hpp"
#include "frame/raw_frame.hpp"

#include <QString>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <memory>

using signalforge::decoder::LoggingSignalValueSink;
using signalforge::decoder::SchemaDecoder;
using signalforge::decoder::SchemaValidator;
using signalforge::decoder::SignalType;
using signalforge::frame::RawFrame;

namespace {

RawFrame makeFrame(const QByteArray& payload, const QString& sourceId) {
    RawFrame f;
    f.sourceId = sourceId;
    f.payload = payload;
    f.recvAt = std::chrono::steady_clock::now();
    return f;
}

}  // namespace

TEST_CASE("integration decoder: temperature_sensor.yaml end-to-end emits all expected signals",
          "[integration][decoder][basic]") {
    const QString schemaPath = QStringLiteral(SIGNALFORGE_FIXTURES_DIR "/valid_schemas/temperature_sensor.yaml");

    auto validation = SchemaValidator::validateFile(schemaPath);
    REQUIRE(validation.has_value());

    SchemaDecoder decoder(std::move(*validation), QStringLiteral("integration:basic"));
    auto sink = std::make_shared<LoggingSignalValueSink>();
    decoder.setSignalSink(sink);
    REQUIRE(sink->registrationsReceived() == 1);

    // Build a 16-byte frame matching the temperature_sensor schema:
    //  [0]      0xAA                            (magic)
    //  [1..4]   0x00010203 little-endian uint32 (timestamp_ms = 0x03020100)
    //  [5..6]   0x00C8 little-endian int16      (temperature raw = 200; 200 * 0.01 = 2.0 °C)
    //  [7..8]   0x0064 little-endian uint16     (pressure raw = 100; 100 * 0.1 = 10.0 kPa)
    //  [9]      0b01010001                      (status bits: alarm=1, calib=0, mode=0b00, reserved=0b0101)
    //  [10..11] 0xBEEF little-endian            (crc, not verified by V1)
    //  [12..15] 0xDEADBEEF little-endian        (padding)
    QByteArray payload(16, '\0');
    payload[0] = static_cast<char>(0xAA);
    payload[1] = static_cast<char>(0x00);
    payload[2] = static_cast<char>(0x01);
    payload[3] = static_cast<char>(0x02);
    payload[4] = static_cast<char>(0x03);
    payload[5] = static_cast<char>(0xC8);
    payload[6] = static_cast<char>(0x00);
    payload[7] = static_cast<char>(0x64);
    payload[8] = static_cast<char>(0x00);
    payload[9] = static_cast<char>(0b01010001);
    payload[10] = static_cast<char>(0xEF);
    payload[11] = static_cast<char>(0xBE);
    payload[12] = static_cast<char>(0xEF);
    payload[13] = static_cast<char>(0xBE);
    payload[14] = static_cast<char>(0xAD);
    payload[15] = static_cast<char>(0xDE);

    decoder.onFrame(makeFrame(payload, QStringLiteral("integration:basic")));

    // 5 top-level fields + 4 bit_fields = 9 signals total.
    REQUIRE(sink->signalsReceived() == 9);
    // status.alarm + status.calibration_active are bools (alarm=true, calib=false)
    REQUIRE(sink->signalsByType(SignalType::Bool) == 2);
    // status.sensor_mode + status.reserved are int64 (multi-bit slices),
    // plus timestamp_ms, crc, padding = 5 int64 signals.
    REQUIRE(sink->signalsByType(SignalType::Int64) == 5);
    // temperature + pressure both have scale → Double signals.
    REQUIRE(sink->signalsByType(SignalType::Double) == 2);
}

TEST_CASE("integration decoder: validation failure prevents construction", "[integration][decoder][basic]") {
    const QString badPath = QStringLiteral(SIGNALFORGE_FIXTURES_DIR "/invalid_schemas/missing_endianness.yaml");
    const auto result = SchemaValidator::validateFile(badPath);
    REQUIRE_FALSE(result.has_value());
    REQUIRE_FALSE(result.error().empty());
}
