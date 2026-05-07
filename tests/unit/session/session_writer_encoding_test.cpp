// tests/unit/session/session_writer_encoding_test.cpp
//
// S4 encoder tests. Verify that the on-disk bytes match
// docs/format/sfreplay-v1.md exactly:
//
// - Fixed prefix (magic + formatVersion + headerLen)
// - Variable section (recordedAt, descLen/desc, schemaIdLen/schemaId,
//   signalCount, signalCatalog)
// - Per-record: type + payloadLen + payload
// - Per-type Signal Value payload: signalIdx + timestampNs + value
// - Footer: REPLAYEO magic + totalRecords + reserved
//
// Includes binary payload tests (NUL / CRLF / 0xFF in QString and in
// raw bytes) to catch transcription bugs at the value-encoding
// boundary.
#include "buffer/signal_buffer_registry.hpp"
#include "decode/decoder_interface.hpp"
#include "session/session_writer.hpp"

#include <QByteArray>
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>

namespace s = signalforge::session;
namespace b = signalforge::buffer;
namespace d = signalforge::decoder;

namespace {

struct EncodingFixture {
    EncodingFixture() {
        if (!QCoreApplication::instance()) {
            static int argc = 0;
            static char* argv[] = {nullptr};
            app_ = std::make_unique<QCoreApplication>(argc, argv);
        }
    }
    std::unique_ptr<QCoreApplication> app_;
    QTemporaryDir tmp_;
};

d::SignalMetadata makeMeta(const QString& id, d::SignalType type) {
    d::SignalMetadata m;
    m.id = id;
    m.name = id;
    m.unit = QStringLiteral("u");
    m.type = type;
    return m;
}

QByteArray readFile(const QString& path) {
    QFile f(path);
    REQUIRE(f.open(QIODevice::ReadOnly));
    return f.readAll();
}

template <typename T> T readLe(const QByteArray& data, qsizetype offset) {
    T value{};
    std::memcpy(&value, data.constData() + offset, sizeof(T));
    return value;
}

}  // namespace

TEST_CASE_METHOD(EncodingFixture, "S4: header has SFREPLAY magic + formatVersion=1", "[session][s4][encoding]") {
    b::SignalBufferRegistry registry;
    s::SessionWriter writer(registry);
    REQUIRE(writer.start(tmp_.filePath(QStringLiteral("magic.sfreplay"))));
    (void)writer.stop();

    const QByteArray bytes = readFile(tmp_.filePath(QStringLiteral("magic.sfreplay")));
    REQUIRE(bytes.size() >= 16);
    REQUIRE(std::memcmp(bytes.constData(), "SFREPLAY", 8) == 0);
    REQUIRE(readLe<std::uint32_t>(bytes, 8) == 1u);
    const std::uint32_t headerLen = readLe<std::uint32_t>(bytes, 12);
    // Empty desc + empty schemaId + 0 signals → 36 bytes header.
    REQUIRE(headerLen == 36u);
}

TEST_CASE_METHOD(EncodingFixture, "S4: header carries description + schemaId + recordedAt", "[session][s4][encoding]") {
    b::SignalBufferRegistry registry;
    s::SessionWriter writer(registry);
    const QString path = tmp_.filePath(QStringLiteral("hdr.sfreplay"));
    REQUIRE(writer.start(path, QStringLiteral("Run #42"), QStringLiteral("dev-board-frame-v1")));
    (void)writer.stop();

    const QByteArray bytes = readFile(path);
    REQUIRE(bytes.size() >= 16);

    // recordedAt at offset 16 (8 bytes); should be > 0 (we just started).
    const std::int64_t recordedAt = readLe<std::int64_t>(bytes, 16);
    REQUIRE(recordedAt > 0);

    // descLen at offset 24
    const std::uint32_t descLen = readLe<std::uint32_t>(bytes, 24);
    REQUIRE(descLen == 7u);
    REQUIRE(QByteArray(bytes.constData() + 28, 7) == QByteArray("Run #42"));

    // schemaIdLen + schemaId ("dev-board-frame-v1" is 18 chars)
    qsizetype off = 28 + 7;
    const std::uint32_t schemaIdLen = readLe<std::uint32_t>(bytes, off);
    REQUIRE(schemaIdLen == 18u);
    REQUIRE(QByteArray(bytes.constData() + off + 4, 18) == QByteArray("dev-board-frame-v1"));
    off += 4 + 18;

    // signalCount = 0
    REQUIRE(readLe<std::uint32_t>(bytes, off) == 0u);
}

TEST_CASE_METHOD(EncodingFixture, "S4: signal catalog encodes type tag + scale/offset gating",
                 "[session][s4][encoding]") {
    b::SignalBufferRegistry registry;
    s::SessionWriter writer(registry);

    auto m = makeMeta(QStringLiteral("voltage"), d::SignalType::Double);
    m.unit = QStringLiteral("V");
    m.description = QStringLiteral("primary bus");
    m.scale = 0.5;
    // offset deliberately absent
    std::vector<d::SignalMetadata> sigs{m};
    writer.onSignalsRegistered(QStringLiteral("dev"), sigs);

    const QString path = tmp_.filePath(QStringLiteral("cat.sfreplay"));
    REQUIRE(writer.start(path));
    (void)writer.stop();

    const QByteArray bytes = readFile(path);
    REQUIRE(bytes.size() >= 16);
    const std::uint32_t headerLen = readLe<std::uint32_t>(bytes, 12);
    // Walk to signalCount @ 36 (empty desc + empty schemaId in this test):
    REQUIRE(readLe<std::uint32_t>(bytes, 24) == 0u);  // descLen
    REQUIRE(readLe<std::uint32_t>(bytes, 28) == 0u);  // schemaIdLen
    REQUIRE(readLe<std::uint32_t>(bytes, 32) == 1u);  // signalCount
    qsizetype off = 36;

    // signal entry: 4+7 ("voltage") + 4+7 ("voltage") + 4+1 ("V") + 4+11 ("primary bus") + 1 (type=2) + 1
    // (hasScale=1) + 8 (scale) + 1 (hasOffset=0)
    REQUIRE(readLe<std::uint32_t>(bytes, off) == 7u);  // signalIdLen
    off += 4;
    REQUIRE(QByteArray(bytes.constData() + off, 7) == QByteArray("voltage"));
    off += 7;
    REQUIRE(readLe<std::uint32_t>(bytes, off) == 7u);  // nameLen
    off += 4 + 7;
    REQUIRE(readLe<std::uint32_t>(bytes, off) == 1u);  // unitLen
    off += 4 + 1;
    REQUIRE(readLe<std::uint32_t>(bytes, off) == 11u);  // descLen
    off += 4 + 11;
    REQUIRE(static_cast<std::uint8_t>(bytes[off]) == 2u);  // type=Double
    off += 1;
    REQUIRE(static_cast<std::uint8_t>(bytes[off]) == 1u);  // hasScale=1
    off += 1;
    double scale{};
    std::memcpy(&scale, bytes.constData() + off, 8);
    REQUIRE(scale == 0.5);
    off += 8;
    REQUIRE(static_cast<std::uint8_t>(bytes[off]) == 0u);  // hasOffset=0
    off += 1;
    REQUIRE(off == headerLen);
}

TEST_CASE_METHOD(EncodingFixture, "S4: footer is REPLAYEO + totalRecords + reserved", "[session][s4][encoding]") {
    b::SignalBufferRegistry registry;
    s::SessionWriter writer(registry);
    REQUIRE(writer.start(tmp_.filePath(QStringLiteral("foot.sfreplay"))));
    (void)writer.stop();

    const QByteArray bytes = readFile(tmp_.filePath(QStringLiteral("foot.sfreplay")));
    REQUIRE(bytes.size() >= 16);
    const qsizetype footerOff = bytes.size() - 16;
    REQUIRE(std::memcmp(bytes.constData() + footerOff, "REPLAYEO", 8) == 0);
    // 0 records (no signals registered, no signal events sent).
    REQUIRE(readLe<std::uint32_t>(bytes, footerOff + 8) == 0u);
    REQUIRE(readLe<std::uint32_t>(bytes, footerOff + 12) == 0u);  // reserved
}

TEST_CASE_METHOD(EncodingFixture, "S4: signal value record encodes signalIdx + timestamp + double",
                 "[session][s4][encoding]") {
    b::SignalBufferRegistry registry;
    s::SessionWriter writer(registry);

    std::vector<d::SignalMetadata> sigs{makeMeta(QStringLiteral("v"), d::SignalType::Double)};
    writer.onSignalsRegistered(QStringLiteral("dev"), sigs);

    const QString path = tmp_.filePath(QStringLiteral("rec.sfreplay"));
    REQUIRE(writer.start(path));

    // Send one event with a known double value.
    writer.onSignal(writer.metadata().recordingStart + std::chrono::nanoseconds{1000000}, QStringLiteral("v"),
                    d::SignalValue{12.34});

    // Give the worker time to drain. stop() waits for it, so this
    // is sufficient.
    const std::size_t bytes = writer.stop();
    REQUIRE(bytes > 0);
    REQUIRE(writer.eventsRecorded() == 1);

    const QByteArray data = readFile(path);
    const std::uint32_t headerLen = readLe<std::uint32_t>(data, 12);
    qsizetype off = headerLen;

    // First record header
    const std::uint32_t recordType = readLe<std::uint32_t>(data, off);
    const std::uint32_t payloadLen = readLe<std::uint32_t>(data, off + 4);
    REQUIRE(recordType == 1u);  // Signal Value
    REQUIRE(payloadLen == 4u + 8u + 8u);

    off += 8;
    REQUIRE(readLe<std::uint32_t>(data, off) == 0u);          // signalIdx=0
    REQUIRE(readLe<std::int64_t>(data, off + 4) == 1000000);  // timestampNs
    double value{};
    std::memcpy(&value, data.constData() + off + 12, 8);
    REQUIRE(value == 12.34);
}

TEST_CASE_METHOD(EncodingFixture, "S4: bool / int64 / string per-type encoding", "[session][s4][encoding]") {
    b::SignalBufferRegistry registry;
    s::SessionWriter writer(registry);

    std::vector<d::SignalMetadata> sigs;
    sigs.push_back(makeMeta(QStringLiteral("flag"), d::SignalType::Bool));
    sigs.push_back(makeMeta(QStringLiteral("count"), d::SignalType::Int64));
    sigs.push_back(makeMeta(QStringLiteral("note"), d::SignalType::String));
    writer.onSignalsRegistered(QStringLiteral("dev"), sigs);

    const QString path = tmp_.filePath(QStringLiteral("types.sfreplay"));
    REQUIRE(writer.start(path));

    const auto t0 = writer.metadata().recordingStart;
    writer.onSignal(t0, QStringLiteral("flag"), d::SignalValue{true});
    writer.onSignal(t0, QStringLiteral("count"), d::SignalValue{std::int64_t{-12345}});
    // String with NUL + CRLF + 0xFF — the binary-payload-survives test.
    // Build via QChar to avoid C-string truncation at the embedded NUL.
    QString tricky;
    tricky.append(QChar{u'a'});
    tricky.append(QChar{u'\0'});
    tricky.append(QChar{u'b'});
    tricky.append(QChar{u'\r'});
    tricky.append(QChar{u'\n'});
    tricky.append(QChar{0xFF});
    writer.onSignal(t0, QStringLiteral("note"), d::SignalValue{tricky});
    (void)writer.stop();

    const QByteArray data = readFile(path);
    const std::uint32_t headerLen = readLe<std::uint32_t>(data, 12);
    qsizetype off = headerLen;

    // Record 1: bool. payload = 4 (signalIdx) + 8 (ts) + 1 (value)
    REQUIRE(readLe<std::uint32_t>(data, off) == 1u);
    REQUIRE(readLe<std::uint32_t>(data, off + 4) == 13u);
    REQUIRE(readLe<std::uint32_t>(data, off + 8) == 0u);              // signalIdx=0 (flag)
    REQUIRE(static_cast<std::uint8_t>(data[off + 8 + 4 + 8]) == 1u);  // bool=true
    off += 8 + 13;

    // Record 2: int64. payload = 4 + 8 + 8 = 20
    REQUIRE(readLe<std::uint32_t>(data, off) == 1u);
    REQUIRE(readLe<std::uint32_t>(data, off + 4) == 20u);
    REQUIRE(readLe<std::uint32_t>(data, off + 8) == 1u);  // signalIdx=1 (count)
    REQUIRE(readLe<std::int64_t>(data, off + 8 + 4 + 8) == -12345);
    off += 8 + 20;

    // Record 3: string. payload = 4 + 8 + 4 (strLen) + N (UTF-8 bytes)
    REQUIRE(readLe<std::uint32_t>(data, off) == 1u);
    const std::uint32_t plen = readLe<std::uint32_t>(data, off + 4);
    REQUIRE(readLe<std::uint32_t>(data, off + 8) == 2u);  // signalIdx=2 (note)
    const std::uint32_t strLen = readLe<std::uint32_t>(data, off + 8 + 4 + 8);
    // UTF-8 of "a\0b\r\n" + 0xC3 0xBF (U+00FF) = 5 + 2 = 7 bytes
    REQUIRE(strLen == 7u);
    const QByteArray utf8(data.constData() + off + 8 + 4 + 8 + 4, 7);
    REQUIRE(utf8.size() == 7);
    REQUIRE(static_cast<std::uint8_t>(utf8[0]) == 'a');
    REQUIRE(static_cast<std::uint8_t>(utf8[1]) == 0x00);
    REQUIRE(static_cast<std::uint8_t>(utf8[2]) == 'b');
    REQUIRE(static_cast<std::uint8_t>(utf8[3]) == '\r');
    REQUIRE(static_cast<std::uint8_t>(utf8[4]) == '\n');
    REQUIRE(static_cast<std::uint8_t>(utf8[5]) == 0xC3);
    REQUIRE(static_cast<std::uint8_t>(utf8[6]) == 0xBF);
    REQUIRE(plen == 4u + 8u + 4u + strLen);
}

TEST_CASE_METHOD(EncodingFixture, "S4: catalog extension record emitted on mid-stream registration",
                 "[session][s4][encoding]") {
    b::SignalBufferRegistry registry;
    s::SessionWriter writer(registry);

    // Register one signal pre-start.
    std::vector<d::SignalMetadata> initial{makeMeta(QStringLiteral("a"), d::SignalType::Double)};
    writer.onSignalsRegistered(QStringLiteral("d1"), initial);

    const QString path = tmp_.filePath(QStringLiteral("ext.sfreplay"));
    REQUIRE(writer.start(path));

    // Mid-stream extension — should produce a record type 2.
    std::vector<d::SignalMetadata> added{makeMeta(QStringLiteral("b"), d::SignalType::Double)};
    writer.onSignalsRegistered(QStringLiteral("d2"), added);

    // Now write a value for the new signal.
    const auto t0 = writer.metadata().recordingStart;
    writer.onSignal(t0 + std::chrono::microseconds{500}, QStringLiteral("b"), d::SignalValue{1.0});
    (void)writer.stop();

    const QByteArray data = readFile(path);
    const std::uint32_t headerLen = readLe<std::uint32_t>(data, 12);
    qsizetype off = headerLen;

    // First record: type 2 (Catalog Extension)
    REQUIRE(readLe<std::uint32_t>(data, off) == 2u);
    const std::uint32_t catLen = readLe<std::uint32_t>(data, off + 4);
    REQUIRE(catLen >= 4u);                                // at minimum addedSignalCount
    REQUIRE(readLe<std::uint32_t>(data, off + 8) == 1u);  // 1 added signal
    off += 8 + catLen;

    // Second record: type 1 (Signal Value), signalIdx must be 1 (the new signal).
    REQUIRE(readLe<std::uint32_t>(data, off) == 1u);
    REQUIRE(readLe<std::uint32_t>(data, off + 8) == 1u);  // signalIdx=1
}

TEST_CASE_METHOD(EncodingFixture, "S4: footer totalRecords matches written records", "[session][s4][encoding]") {
    b::SignalBufferRegistry registry;
    s::SessionWriter writer(registry);

    std::vector<d::SignalMetadata> sigs{makeMeta(QStringLiteral("x"), d::SignalType::Double)};
    writer.onSignalsRegistered(QStringLiteral("d"), sigs);

    const QString path = tmp_.filePath(QStringLiteral("count.sfreplay"));
    REQUIRE(writer.start(path));

    const auto t0 = writer.metadata().recordingStart;
    for (int i = 0; i < 17; ++i) {
        writer.onSignal(t0 + std::chrono::microseconds(i), QStringLiteral("x"), d::SignalValue{static_cast<double>(i)});
    }
    (void)writer.stop();

    const QByteArray data = readFile(path);
    const qsizetype footerOff = data.size() - 16;
    const std::uint32_t totalRecords = readLe<std::uint32_t>(data, footerOff + 8);
    // 17 signal value records (no extensions; no heartbeats since stop arrives < 10 s).
    REQUIRE(totalRecords == 17u);
}
