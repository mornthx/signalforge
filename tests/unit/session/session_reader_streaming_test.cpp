// tests/unit/session/session_reader_streaming_test.cpp
//
// M11 S2 — exercises the streaming + seek API additions to
// `signalforge::session::SessionReader`. The S6/S7 round-trip
// suite still owns the M10 bulk-replay regression coverage; this
// file covers the new surfaces.
//
// Coverage goals (per `.claude/M11-plan.md` §S2):
//   - Streaming returns the same events as `replayAll`.
//   - Forward seek to a mid-file timestamp.
//   - Backward seek resets catalog state to header-only and
//     re-walks.
//   - Seek past EOF clamps to the last record.
//   - `currentRecordIndex()` and `atEnd()` track correctly.
//   - Catalog-extension fan-out fires when the streaming caller
//     binds a catalog sink and crosses an extension during seek.
#include "decode/decoder_interface.hpp"
#include "session/session_reader.hpp"
#include "session/session_writer.hpp"

#include <QCoreApplication>
#include <QTemporaryDir>
#include <buffer/signal_buffer_registry.hpp>
#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <string>

namespace s = signalforge::session;
namespace b = signalforge::buffer;
namespace d = signalforge::decoder;

namespace {

struct StreamingFixture {
    StreamingFixture() {
        if (!QCoreApplication::instance()) {
            static int argc = 0;
            static char* argv[] = {nullptr};
            app_ = std::make_unique<QCoreApplication>(argc, argv);
        }
    }
    std::unique_ptr<QCoreApplication> app_;
    QTemporaryDir tmp_;
};

struct CountingCatalogSink : public d::SignalValueSink {
    int signalsCalls = 0;
    int registrationsCalls = 0;
    std::vector<std::pair<QString, std::vector<d::SignalMetadata>>> registrations;

    void onSignal(std::chrono::steady_clock::time_point /*t*/, const QString& /*id*/,
                  const d::SignalValue& /*v*/) override {
        ++signalsCalls;
    }
    void onSignalsRegistered(const QString& driverId, const std::vector<d::SignalMetadata>& sigs) override {
        ++registrationsCalls;
        registrations.emplace_back(driverId, sigs);
    }
};

d::SignalMetadata makeMeta(const QString& id, d::SignalType type) {
    d::SignalMetadata m;
    m.id = id;
    m.name = id;
    m.unit = QStringLiteral("u");
    m.type = type;
    return m;
}

QString writeMultiRecordFile(QTemporaryDir& tmp, const QString& name) {
    const QString path = tmp.filePath(name);
    b::SignalBufferRegistry registry;
    s::SessionWriter writer(registry);
    std::vector<d::SignalMetadata> initial{makeMeta(QStringLiteral("a"), d::SignalType::Double)};
    writer.onSignalsRegistered(QStringLiteral("d1"), initial);
    REQUIRE(writer.start(path));
    const auto t0 = writer.metadata().recordingStart;
    writer.onSignal(t0 + std::chrono::microseconds(10), QStringLiteral("a"), d::SignalValue{1.0});
    writer.onSignal(t0 + std::chrono::microseconds(20), QStringLiteral("a"), d::SignalValue{2.0});
    writer.onSignal(t0 + std::chrono::microseconds(30), QStringLiteral("a"), d::SignalValue{3.0});
    writer.onSignal(t0 + std::chrono::microseconds(40), QStringLiteral("a"), d::SignalValue{4.0});
    writer.onSignal(t0 + std::chrono::microseconds(50), QStringLiteral("a"), d::SignalValue{5.0});
    (void)writer.stop();
    return path;
}

}  // namespace

TEST_CASE_METHOD(StreamingFixture, "M11 S2: streaming returns the same events as replayAll", "[session][m11][s2]") {
    const QString path = writeMultiRecordFile(tmp_, QStringLiteral("stream.sfreplay"));

    s::SessionReader reader;
    REQUIRE(reader.open(path));

    s::ReplayRecord rec;
    std::vector<std::int64_t> timestamps;
    std::vector<double> values;
    while (reader.readNextRecord(rec)) {
        timestamps.push_back(rec.timestampNs);
        REQUIRE(std::holds_alternative<double>(rec.value));
        values.push_back(std::get<double>(rec.value));
    }
    REQUIRE(timestamps.size() == 5);
    REQUIRE(values.size() == 5);
    REQUIRE(timestamps[0] == 10000);
    REQUIRE(timestamps[4] == 50000);
    REQUIRE(values[0] == 1.0);
    REQUIRE(values[4] == 5.0);
    REQUIRE(reader.atEnd());
    REQUIRE(reader.currentRecordIndex() == 5);
    REQUIRE(reader.currentTimestampNs() == 50000);
}

TEST_CASE_METHOD(StreamingFixture, "M11 S2: seek forward to mid-file timestamp", "[session][m11][s2]") {
    const QString path = writeMultiRecordFile(tmp_, QStringLiteral("seek_fwd.sfreplay"));

    s::SessionReader reader;
    REQUIRE(reader.open(path));

    REQUIRE(reader.seekToTimestamp(30000));

    s::ReplayRecord rec;
    REQUIRE(reader.readNextRecord(rec));
    REQUIRE(rec.timestampNs == 30000);
    REQUIRE(std::get<double>(rec.value) == 3.0);

    // Continue streaming.
    REQUIRE(reader.readNextRecord(rec));
    REQUIRE(rec.timestampNs == 40000);
    REQUIRE(reader.readNextRecord(rec));
    REQUIRE(rec.timestampNs == 50000);
    REQUIRE_FALSE(reader.readNextRecord(rec));
    REQUIRE(reader.atEnd());
}

TEST_CASE_METHOD(StreamingFixture, "M11 S2: backward seek resets catalog + re-walks", "[session][m11][s2]") {
    const QString path = writeMultiRecordFile(tmp_, QStringLiteral("seek_back.sfreplay"));

    s::SessionReader reader;
    REQUIRE(reader.open(path));

    // Pull 3 records.
    s::ReplayRecord rec;
    REQUIRE(reader.readNextRecord(rec));
    REQUIRE(reader.readNextRecord(rec));
    REQUIRE(reader.readNextRecord(rec));
    REQUIRE(reader.currentRecordIndex() == 3);
    REQUIRE(reader.currentTimestampNs() == 30000);

    // Seek back to before the first record.
    REQUIRE(reader.seekToTimestamp(0));
    REQUIRE(reader.currentRecordIndex() == 0);
    REQUIRE(reader.currentTimestampNs() == 0);
    REQUIRE_FALSE(reader.atEnd());

    // Replay from start.
    REQUIRE(reader.readNextRecord(rec));
    REQUIRE(rec.timestampNs == 10000);
    REQUIRE(std::get<double>(rec.value) == 1.0);
}

TEST_CASE_METHOD(StreamingFixture, "M11 S2: seek past EOF clamps to last record", "[session][m11][s2]") {
    const QString path = writeMultiRecordFile(tmp_, QStringLiteral("seek_eof.sfreplay"));

    s::SessionReader reader;
    REQUIRE(reader.open(path));

    // Last record's ts is 50_000 ns; seek to a much larger value.
    REQUIRE(reader.seekToTimestamp(1'000'000));

    s::ReplayRecord rec;
    REQUIRE(reader.readNextRecord(rec));
    REQUIRE(rec.timestampNs == 50000);
    REQUIRE(std::get<double>(rec.value) == 5.0);
    REQUIRE_FALSE(reader.readNextRecord(rec));
    REQUIRE(reader.atEnd());
}

TEST_CASE_METHOD(StreamingFixture, "M11 S2: catalog-extension dispatched to bound sink during seek",
                 "[session][m11][s2]") {
    const QString path = tmp_.filePath(QStringLiteral("seek_ext.sfreplay"));

    // Construct a file with an extension between records.
    {
        b::SignalBufferRegistry registry;
        s::SessionWriter writer(registry);
        std::vector<d::SignalMetadata> initial{makeMeta(QStringLiteral("a"), d::SignalType::Double)};
        writer.onSignalsRegistered(QStringLiteral("d1"), initial);
        REQUIRE(writer.start(path));
        const auto t0 = writer.metadata().recordingStart;
        writer.onSignal(t0 + std::chrono::microseconds(10), QStringLiteral("a"), d::SignalValue{1.0});
        // Mid-stream: register a second signal.
        std::vector<d::SignalMetadata> added{makeMeta(QStringLiteral("b"), d::SignalType::Double)};
        writer.onSignalsRegistered(QStringLiteral("d2"), added);
        writer.onSignal(t0 + std::chrono::microseconds(20), QStringLiteral("b"), d::SignalValue{99.0});
        (void)writer.stop();
    }

    s::SessionReader reader;
    REQUIRE(reader.open(path));

    CountingCatalogSink sink;
    reader.bindCatalogSink(&sink);

    // Seek past the catalog extension to the second signal-value record.
    REQUIRE(reader.seekToTimestamp(20000));

    // The bound sink should have observed exactly one extension
    // (driverId == "session-replay", 1 added signal "b").
    REQUIRE(sink.registrationsCalls == 1);
    REQUIRE(sink.registrations.size() == 1);
    REQUIRE(sink.registrations[0].second.size() == 1);
    REQUIRE(sink.registrations[0].second[0].id == QStringLiteral("b"));

    // Reading next now returns the b=99.0 record.
    s::ReplayRecord rec;
    REQUIRE(reader.readNextRecord(rec));
    REQUIRE(rec.timestampNs == 20000);
    REQUIRE(rec.signalId == QStringLiteral("b"));
    REQUIRE(std::get<double>(rec.value) == 99.0);
}

TEST_CASE_METHOD(StreamingFixture, "M11 S2: currentRecordIndex tracks signal-value-only ordinals",
                 "[session][m11][s2]") {
    // Mixed file: 1 signal-value, 1 catalog-extension, 1 signal-value.
    const QString path = tmp_.filePath(QStringLiteral("mixed.sfreplay"));
    {
        b::SignalBufferRegistry registry;
        s::SessionWriter writer(registry);
        std::vector<d::SignalMetadata> initial{makeMeta(QStringLiteral("a"), d::SignalType::Double)};
        writer.onSignalsRegistered(QStringLiteral("d1"), initial);
        REQUIRE(writer.start(path));
        const auto t0 = writer.metadata().recordingStart;
        writer.onSignal(t0 + std::chrono::microseconds(1), QStringLiteral("a"), d::SignalValue{10.0});
        std::vector<d::SignalMetadata> added{makeMeta(QStringLiteral("b"), d::SignalType::Double)};
        writer.onSignalsRegistered(QStringLiteral("d2"), added);
        writer.onSignal(t0 + std::chrono::microseconds(2), QStringLiteral("b"), d::SignalValue{20.0});
        (void)writer.stop();
    }

    s::SessionReader reader;
    REQUIRE(reader.open(path));

    s::ReplayRecord rec;
    REQUIRE(reader.readNextRecord(rec));
    REQUIRE(reader.currentRecordIndex() == 1);  // 1 signal-value seen
    REQUIRE(reader.recordsRead() == 1);

    REQUIRE(reader.readNextRecord(rec));
    REQUIRE(reader.currentRecordIndex() == 2);  // 2 signal-values seen
    // recordsRead_ now counts: 1 SignalValue + 1 CatalogExt + 1 SignalValue = 3.
    REQUIRE(reader.recordsRead() == 3);

    REQUIRE_FALSE(reader.readNextRecord(rec));
}

TEST_CASE_METHOD(StreamingFixture, "M11 S2: replayAll still works after streaming + seek (M10 round-trip preserved)",
                 "[session][m11][s2][regression]") {
    const QString path = writeMultiRecordFile(tmp_, QStringLiteral("regression.sfreplay"));

    s::SessionReader reader;
    REQUIRE(reader.open(path));

    // Stream a few records first, then seek + bulk replay.
    s::ReplayRecord rec;
    REQUIRE(reader.readNextRecord(rec));
    REQUIRE(reader.readNextRecord(rec));
    REQUIRE(reader.seekToTimestamp(0));

    CountingCatalogSink sink;
    REQUIRE(reader.replayAll(sink));
    REQUIRE(reader.fileComplete());
    REQUIRE(sink.signalsCalls == 5);
}
