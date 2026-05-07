// tests/unit/session/session_writer_lifecycle_test.cpp
//
// S3 lifecycle tests for SessionWriter. Cover the basic
// start / stop state machine, the worker-thread join contract,
// multiple-recording cycles, and the historical-catalog tracking
// via onSignalsRegistered (so when start() fires the file's
// initial catalog reflects every signal the writer has observed
// so far).
//
// Full encoder / round-trip / disk-full / threading tests land
// in S4 / S5 / S7.
#include "buffer/signal_buffer_registry.hpp"
#include "decode/decoder_interface.hpp"
#include "session/session_writer.hpp"

#include <QCoreApplication>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <memory>

namespace s = signalforge::session;
namespace b = signalforge::buffer;
namespace d = signalforge::decoder;

namespace {

/// Catch2 fixture: gives every test a QCoreApplication
/// (required for QThread/QTimer to work) and a fresh temp dir.
struct LifecycleFixture {
    LifecycleFixture() {
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

}  // namespace

TEST_CASE_METHOD(LifecycleFixture, "S3: writer is Idle at construction", "[session][s3][lifecycle]") {
    b::SignalBufferRegistry registry;
    s::SessionWriter writer(registry);
    REQUIRE(writer.state() == s::RecordingState::Idle);
    REQUIRE_FALSE(writer.isRecording());
    REQUIRE(writer.eventsRecorded() == 0);
    REQUIRE(writer.bytesWritten() == 0);
    REQUIRE(writer.droppedEvents() == 0);
    REQUIRE(writer.currentFilePath().isEmpty());
}

TEST_CASE_METHOD(LifecycleFixture, "S3: start() opens file and transitions to Recording", "[session][s3][lifecycle]") {
    b::SignalBufferRegistry registry;
    s::SessionWriter writer(registry);
    QSignalSpy startedSpy(&writer, &s::SessionWriter::recordingStarted);
    QSignalSpy stoppedSpy(&writer, &s::SessionWriter::recordingStopped);

    const QString path = tmp_.filePath(QStringLiteral("rec1.sfreplay"));
    REQUIRE(writer.start(path, QStringLiteral("hello"), QStringLiteral("schema-x")));
    REQUIRE(writer.state() == s::RecordingState::Recording);
    REQUIRE(writer.isRecording());
    REQUIRE(writer.currentFilePath() == path);

    // recordingStarted is emitted synchronously from start(); no
    // event-loop spin needed.
    REQUIRE(startedSpy.count() == 1);
    REQUIRE(stoppedSpy.count() == 0);

    // Metadata reflects the start() call's args.
    const auto md = writer.metadata();
    REQUIRE(md.description == QStringLiteral("hello"));
    REQUIRE(md.decoderSchemaId == QStringLiteral("schema-x"));
    REQUIRE_FALSE(md.recordingEnd.has_value());

    // File exists on disk.
    REQUIRE(QFileInfo::exists(path));

    const std::size_t bytes = writer.stop();
    REQUIRE(writer.state() == s::RecordingState::Idle);
    REQUIRE_FALSE(writer.isRecording());

    // recordingStopped fires synchronously from stop().
    REQUIRE(stoppedSpy.count() == 1);
    REQUIRE(stoppedSpy[0][1].toULongLong() == bytes);

    // metadata.recordingEnd is now populated.
    REQUIRE(writer.metadata().recordingEnd.has_value());
}

TEST_CASE_METHOD(LifecycleFixture, "S3: start() while recording returns false", "[session][s3][lifecycle]") {
    b::SignalBufferRegistry registry;
    s::SessionWriter writer(registry);

    REQUIRE(writer.start(tmp_.filePath(QStringLiteral("a.sfreplay"))));
    // Second start() must reject without disrupting the in-flight
    // recording.
    REQUIRE_FALSE(writer.start(tmp_.filePath(QStringLiteral("b.sfreplay"))));
    REQUIRE(writer.state() == s::RecordingState::Recording);
    // The first path is preserved.
    REQUIRE(writer.currentFilePath().endsWith(QStringLiteral("a.sfreplay")));

    (void)writer.stop();
}

TEST_CASE_METHOD(LifecycleFixture, "S3: stop() in Idle is a no-op returning 0", "[session][s3][lifecycle]") {
    b::SignalBufferRegistry registry;
    s::SessionWriter writer(registry);
    REQUIRE(writer.stop() == 0);
    REQUIRE(writer.state() == s::RecordingState::Idle);
}

TEST_CASE_METHOD(LifecycleFixture, "S3: multiple start/stop cycles", "[session][s3][lifecycle]") {
    b::SignalBufferRegistry registry;
    s::SessionWriter writer(registry);

    for (int i = 0; i < 3; ++i) {
        const QString path = tmp_.filePath(QStringLiteral("cycle-%1.sfreplay").arg(i));
        REQUIRE(writer.start(path));
        REQUIRE(writer.state() == s::RecordingState::Recording);
        (void)writer.stop();
        REQUIRE(writer.state() == s::RecordingState::Idle);
        REQUIRE(QFileInfo::exists(path));
    }
}

TEST_CASE_METHOD(LifecycleFixture, "S3: destructor joins worker if recording is in flight",
                 "[session][s3][lifecycle]") {
    b::SignalBufferRegistry registry;
    {
        s::SessionWriter writer(registry);
        REQUIRE(writer.start(tmp_.filePath(QStringLiteral("dtor.sfreplay"))));
        REQUIRE(writer.state() == s::RecordingState::Recording);
        // Let writer go out of scope without explicit stop() —
        // destructor must drain + join the worker without
        // crashing.
    }
    SUCCEED("writer destructor returned cleanly");
}

TEST_CASE_METHOD(LifecycleFixture, "S3: signal catalog tracking via onSignalsRegistered", "[session][s3][lifecycle]") {
    b::SignalBufferRegistry registry;
    s::SessionWriter writer(registry);

    // Pre-register two signals BEFORE start() — the writer's
    // metadata.signalCatalog must reflect them at recording start.
    std::vector<d::SignalMetadata> first;
    first.push_back(makeMeta(QStringLiteral("a/s1"), d::SignalType::Double));
    first.push_back(makeMeta(QStringLiteral("a/s2"), d::SignalType::Double));
    writer.onSignalsRegistered(QStringLiteral("a"), first);

    REQUIRE(writer.metadata().signalCatalog.size() == 2);

    // Open a recording. Catalog snapshot becomes the file's
    // initial catalog (S4 will write it; S3 just tracks it).
    REQUIRE(writer.start(tmp_.filePath(QStringLiteral("cat.sfreplay"))));

    // Mid-recording, register a third signal. The writer's live
    // catalog grows; the worker enqueues a CatalogExtensionEvent
    // (S4 will encode it).
    std::vector<d::SignalMetadata> second;
    second.push_back(makeMeta(QStringLiteral("b/s3"), d::SignalType::Double));
    writer.onSignalsRegistered(QStringLiteral("b"), second);

    REQUIRE(writer.metadata().signalCatalog.size() == 3);

    (void)writer.stop();
    // Catalog persists across stop() (live state, not file
    // state).
    REQUIRE(writer.metadata().signalCatalog.size() == 3);
}

TEST_CASE_METHOD(LifecycleFixture, "S3: signal events while recording increment counter", "[session][s3][lifecycle]") {
    b::SignalBufferRegistry registry;
    s::SessionWriter writer(registry);

    std::vector<d::SignalMetadata> sigs;
    sigs.push_back(makeMeta(QStringLiteral("d/s1"), d::SignalType::Double));
    writer.onSignalsRegistered(QStringLiteral("d"), sigs);

    REQUIRE(writer.start(tmp_.filePath(QStringLiteral("evt.sfreplay"))));

    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 10; ++i) {
        writer.onSignal(t0 + std::chrono::microseconds(i * 1000), QStringLiteral("d/s1"),
                        d::SignalValue{static_cast<double>(i)});
    }

    // S3 increments eventsRecorded for every successful enqueue.
    // The worker's encoding is a no-op until S4, so bytesWritten
    // stays 0 — but the counter reflects ingestion.
    REQUIRE(writer.eventsRecorded() == 10);
    REQUIRE(writer.droppedEvents() == 0);

    (void)writer.stop();
    REQUIRE(writer.eventsRecorded() == 10);
}

TEST_CASE_METHOD(LifecycleFixture, "S3: signals before start are ignored by counter (catalog-only)",
                 "[session][s3][lifecycle]") {
    b::SignalBufferRegistry registry;
    s::SessionWriter writer(registry);
    const auto t0 = std::chrono::steady_clock::now();
    writer.onSignal(t0, QStringLiteral("nope"), d::SignalValue{1.0});
    REQUIRE(writer.eventsRecorded() == 0);
    REQUIRE(writer.droppedEvents() == 0);
    REQUIRE(writer.state() == s::RecordingState::Idle);
}
