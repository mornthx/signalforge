// tests/unit/decode/decoder_buffer_cache_test.cpp
//
// S11.5 — verify SchemaDecoder's buffer-pointer cache:
//   1. When the attached sink is a SignalBufferRegistry, the decoder's
//      cache populates with non-null SignalBuffer pointers and the hot
//      path stores into those buffers directly.
//   2. When the sink is a non-registry sink (LoggingSignalValueSink),
//      the cache stays empty and the standard onSignal fallback is
//      exercised.
//   3. Calling setSignalSink with a different sink rebuilds the cache.

#include "buffer/signal_buffer.hpp"
#include "buffer/signal_buffer_registry.hpp"
#include "decode/decoder_interface.hpp"
#include "decode/schema.hpp"
#include "decode/schema_decoder.hpp"
#include "frame/raw_frame.hpp"
#include "tests/test_only/logging_signal_value_sink.hpp"

#include <QByteArray>
#include <QString>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <memory>

namespace {

using namespace signalforge::decoder;
using signalforge::buffer::SignalBuffer;
using signalforge::buffer::SignalBufferRegistry;
using signalforge::frame::RawFrame;

Schema buildSchema() {
    Schema s;
    s.schemaVersion = 1;
    s.id = QStringLiteral("cache-test");
    Layout l;
    l.name = QStringLiteral("layout");
    l.endianness = Endianness::Little;
    l.match.offset = 0;
    l.match.bytes = {0xAA};
    l.minPayloadBytes = 5;
    auto add = [&](QString name, int offset, FieldEncoding enc, int sz) {
        FieldDef f;
        f.name = std::move(name);
        f.offset = offset;
        f.encoding = enc;
        f.sizeBytes = sz;
        l.fields.push_back(std::move(f));
    };
    add(QStringLiteral("magic"), 0, FieldEncoding::Uint8, 1);
    add(QStringLiteral("counter"), 1, FieldEncoding::Uint32, 4);
    s.layouts.push_back(std::move(l));
    return s;
}

RawFrame buildFrame() {
    RawFrame frame;
    frame.sourceId = QStringLiteral("cache-test/driver");
    frame.recvAt = std::chrono::steady_clock::now();
    QByteArray payload(5, '\0');
    payload[0] = static_cast<char>(0xAA);
    payload[1] = 0x2A;  // counter = 42
    payload[2] = 0;
    payload[3] = 0;
    payload[4] = 0;
    frame.payload = std::move(payload);
    return frame;
}

}  // namespace

TEST_CASE("S11.5: SchemaDecoder uses buffer cache for SignalBufferRegistry sink", "[decoder][s11_5][cache][registry]") {
    SchemaDecoder decoder(buildSchema(), QStringLiteral("cache-test/driver"));

    SignalBufferRegistry registry;
    auto sink = std::shared_ptr<SignalValueSink>(&registry, [](SignalValueSink*) {});
    decoder.setSignalSink(sink);

    // After setSignalSink, the registry has registered the catalog and the
    // decoder's cache contains pointers to the allocated buffers.
    REQUIRE(registry.signalCount() == 2U);  // magic + counter

    auto frame = buildFrame();
    decoder.onFrame(frame);

    // Each buffer received exactly one push.
    auto* magicBuf = registry.bufferFor(QStringLiteral("cache-test/driver/magic"));
    auto* counterBuf = registry.bufferFor(QStringLiteral("cache-test/driver/counter"));
    REQUIRE(magicBuf != nullptr);
    REQUIRE(counterBuf != nullptr);
    REQUIRE(magicBuf->totalSamplesPushed() == 1U);
    REQUIRE(counterBuf->totalSamplesPushed() == 1U);
}

TEST_CASE("S11.5: SchemaDecoder falls back to onSignal for non-registry sink", "[decoder][s11_5][cache][fallback]") {
    SchemaDecoder decoder(buildSchema(), QStringLiteral("cache-test/driver-fb"));

    auto sink = std::make_shared<LoggingSignalValueSink>();
    decoder.setSignalSink(sink);

    auto frame = buildFrame();
    decoder.onFrame(frame);

    // The logging sink received the signals via the standard onSignal path.
    REQUIRE(sink->signalsReceived() == 2U);
}

TEST_CASE("S11.5: setSignalSink rebuilds cache on second call", "[decoder][s11_5][cache][rebuild]") {
    SchemaDecoder decoder(buildSchema(), QStringLiteral("cache-test/driver-rb"));

    auto loggingSink = std::make_shared<LoggingSignalValueSink>();
    decoder.setSignalSink(loggingSink);

    auto frame = buildFrame();
    decoder.onFrame(frame);
    REQUIRE(loggingSink->signalsReceived() == 2U);

    // Switch to a registry sink; cache must be rebuilt.
    SignalBufferRegistry registry;
    auto registrySink = std::shared_ptr<SignalValueSink>(&registry, [](SignalValueSink*) {});
    decoder.setSignalSink(registrySink);

    decoder.onFrame(frame);

    // Logging sink must NOT have received the second frame (it was unregistered).
    REQUIRE(loggingSink->signalsReceived() == 2U);

    // Registry buffers received the second frame.
    REQUIRE(registry.bufferFor(QStringLiteral("cache-test/driver-rb/magic"))->totalSamplesPushed() == 1U);
    REQUIRE(registry.bufferFor(QStringLiteral("cache-test/driver-rb/counter"))->totalSamplesPushed() == 1U);
}
