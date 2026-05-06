#include "decode/decoder_interface.hpp"
#include "decode/logging_signal_value_sink.hpp"
#include "decode/schema.hpp"
#include "decode/schema_decoder.hpp"
#include "frame/raw_frame.hpp"
#include "observability/metrics.hpp"

#include <QByteArray>
#include <QString>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <memory>
#include <optional>

using namespace signalforge::decoder;
using signalforge::frame::RawFrame;
using signalforge::observability::MetricsRegistry;

namespace {

RawFrame makeFrame(const QByteArray& payload, const QString& sourceId) {
    RawFrame f;
    f.sourceId = sourceId;
    f.payload = payload;
    f.recvAt = std::chrono::steady_clock::now();
    return f;
}

Schema singleLayoutSchema() {
    Schema s;
    s.schemaVersion = 1;
    Layout l;
    l.name = QStringLiteral("only");
    l.endianness = Endianness::Little;
    l.match.offset = 0;
    l.match.bytes = {0xAA};
    l.minPayloadBytes = 2;
    FieldDef magic{QStringLiteral("m"), 0,  FieldEncoding::Uint8, 1, std::nullopt, std::nullopt,
                   std::nullopt,        {}, std::nullopt,         {}};
    l.fields.push_back(magic);
    FieldDef one{QStringLiteral("byte"), 1,  FieldEncoding::Uint8, 1, std::nullopt, std::nullopt,
                 std::nullopt,           {}, std::nullopt,         {}};
    l.fields.push_back(one);
    s.layouts.push_back(l);
    return s;
}

}  // namespace

TEST_CASE("integration decoder: frames with foreign magic increment unmatched and emit no signals",
          "[integration][decoder][unmatched]") {
    const QString driverId = QStringLiteral("integration:unmatched");
    SchemaDecoder decoder(singleLayoutSchema(), driverId);
    auto sink = std::make_shared<LoggingSignalValueSink>();
    decoder.setSignalSink(sink);

    auto* mUnmatched = MetricsRegistry::instance().getOrCreate(QStringLiteral("decoder_frames_unmatched_") + driverId,
                                                               signalforge::observability::MetricKind::Counter);
    REQUIRE(mUnmatched != nullptr);
    const auto baseline = mUnmatched->value();

    decoder.onFrame(makeFrame(QByteArray::fromHex("BB42"), driverId));
    decoder.onFrame(makeFrame(QByteArray::fromHex("CC13"), driverId));
    decoder.onFrame(makeFrame(QByteArray::fromHex("FF99"), driverId));

    REQUIRE(sink->signalsReceived() == 0);
    REQUIRE(mUnmatched->value() - baseline == 3);

    // A matched frame still works.
    decoder.onFrame(makeFrame(QByteArray::fromHex("AA01"), driverId));
    REQUIRE(sink->signalsReceived() == 2);  // magic + byte
}
