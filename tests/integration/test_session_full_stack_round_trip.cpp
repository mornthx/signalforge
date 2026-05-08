// tests/integration/test_session_full_stack_round_trip.cpp
//
// M10 S7: full-stack integration test for the session
// recording / replay path. Composes:
//
//   SignalBufferRegistry (M6) — fans out signals
//      |
//      ├─→ chart-style consumer (M6 buffers — exercised
//      |    indirectly via the registry's onSignal path)
//      └─→ SessionWriter (M10) — writes a .sfreplay file
//
//   ↓ (file)
//
//   SessionReader (M10) — reads the file back
//      └─→ CapturingSink — records the replayed events
//
// Asserts the captured events match what the writer received,
// including binary string payloads + multi-driver registrations.
// This is the milestone-level HALT trigger #2 gate; the unit
// test session_reader_test.cpp covers the same path more
// granularly. The integration test's purpose is to compose the
// frozen surfaces (M5 SignalValueSink + M6 registry + M10
// writer + M10 reader) the way MainWindow will in S9.
//
// Spec §2.1-12 mapping (M9 pattern: unit + integration overlap):
//
// - test_session_writer_basic_lifecycle:
//     tests/unit/session/session_writer_lifecycle_test.cpp
// - test_session_writer_replay_round_trip:
//     tests/unit/session/session_reader_test.cpp + this file
// - test_session_writer_metadata:
//     tests/unit/session/session_writer_lifecycle_test.cpp +
//     this file
// - test_session_writer_disk_full: deferred (SessionFileWriter
//     surfaces disk errors via the worker `error` signal which
//     transitions to RecordingState::Error; synthetic
//     fault-injection harness is V1.5+ work).
// - test_session_writer_concurrent_access:
//     this file (registry + writer fanout during recording).
// - test_session_writer_threading:
//     tests/unit/session/session_writer_lifecycle_test.cpp
//     (worker thread joined on stop + multi-cycle).
// - test_session_writer_long_session:
//     tests/benchmark/bench_session_writer.cpp (S10; opt-in via
//     -DSIGNALFORGE_BENCHMARKS=ON).
#include "buffer/signal_buffer_registry.hpp"
#include "decode/decoder_interface.hpp"
#include "session/session_reader.hpp"
#include "session/session_writer.hpp"

#include <QCoreApplication>
#include <QTemporaryDir>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <memory>

namespace s = signalforge::session;
namespace b = signalforge::buffer;
namespace d = signalforge::decoder;

namespace {

struct FullStackFixture {
    FullStackFixture() {
        if (!QCoreApplication::instance()) {
            static int argc = 0;
            static char* argv[] = {nullptr};
            app_ = std::make_unique<QCoreApplication>(argc, argv);
        }
    }
    std::unique_ptr<QCoreApplication> app_;
    QTemporaryDir tmp_;
};

struct CapturingSink : public d::SignalValueSink {
    struct Event {
        QString signalId;
        d::SignalValue value;
    };
    std::vector<Event> events;
    std::vector<std::vector<d::SignalMetadata>> registrations;
    void onSignal(std::chrono::steady_clock::time_point /*t*/, const QString& id, const d::SignalValue& v) override {
        events.push_back({id, v});
    }
    void onSignalsRegistered(const QString& /*driverId*/, const std::vector<d::SignalMetadata>& sigs) override {
        registrations.push_back(sigs);
    }
    void onSignalsUnregistered(const QString& /*driverId*/) override {}
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

TEST_CASE_METHOD(FullStackFixture, "S7: full-stack round-trip via registry + writer + reader",
                 "[session][s7][integration][round-trip]") {
    const QString recordPath = tmp_.filePath(QStringLiteral("full-stack.sfreplay"));

    // Phase 1: register a SignalBufferRegistry, attach a SessionWriter
    // alongside it (the writer doubles as a SignalValueSink), drive
    // events through both.
    {
        b::SignalBufferRegistry registry;
        s::SessionWriter writer(registry);

        // Two drivers with different signals.
        std::vector<d::SignalMetadata> driverA{
            makeMeta(QStringLiteral("a/voltage"), d::SignalType::Double),
            makeMeta(QStringLiteral("a/temperature"), d::SignalType::Double),
        };
        std::vector<d::SignalMetadata> driverB{
            makeMeta(QStringLiteral("b/state"), d::SignalType::Bool),
            makeMeta(QStringLiteral("b/count"), d::SignalType::Int64),
        };

        // Pre-recording: register driver A on both registry and writer.
        // (In production a TeeSink fans out — for the integration test we
        // call both directly to simulate the fanout.)
        registry.onSignalsRegistered(QStringLiteral("a"), driverA);
        writer.onSignalsRegistered(QStringLiteral("a"), driverA);

        REQUIRE(
            writer.start(recordPath, QStringLiteral("Full-stack round-trip"), QStringLiteral("dev-board-frame-v1")));

        // Mid-recording: register driver B (Catalog Extension path).
        registry.onSignalsRegistered(QStringLiteral("b"), driverB);
        writer.onSignalsRegistered(QStringLiteral("b"), driverB);

        // Drive a small batch of signals via both sinks. Concurrent
        // access — the writer enqueues to its worker queue while the
        // registry handles its own buffer storage. M10 spec §3.2 +
        // §5.1 say the writer must not block the main thread on
        // disk; this test exercises the no-block path implicitly.
        const auto t0 = writer.metadata().recordingStart;
        for (int i = 0; i < 10; ++i) {
            const auto t = t0 + std::chrono::microseconds(i * 100);
            registry.onSignal(t, QStringLiteral("a/voltage"), d::SignalValue{12.0 + i * 0.1});
            writer.onSignal(t, QStringLiteral("a/voltage"), d::SignalValue{12.0 + i * 0.1});
            registry.onSignal(t, QStringLiteral("b/state"), d::SignalValue{(i % 2) == 0});
            writer.onSignal(t, QStringLiteral("b/state"), d::SignalValue{(i % 2) == 0});
        }

        // Tricky string at the end with binary payload.
        QString tricky;
        tricky.append(QChar{u'a'});
        tricky.append(QChar{u'\0'});
        tricky.append(QChar{u'b'});
        tricky.append(QChar{u'\r'});
        tricky.append(QChar{u'\n'});
        tricky.append(QChar{0xFF});

        // We didn't register a string signal — quick add via mid-stream
        // catalog extension to test the catalog path during heavy
        // signal traffic.
        std::vector<d::SignalMetadata> noteSig{makeMeta(QStringLiteral("a/note"), d::SignalType::String)};
        registry.onSignalsRegistered(QStringLiteral("a-note"), noteSig);
        writer.onSignalsRegistered(QStringLiteral("a-note"), noteSig);

        writer.onSignal(t0 + std::chrono::microseconds(2000), QStringLiteral("a/note"), d::SignalValue{tricky});

        const std::size_t bytes = writer.stop();
        REQUIRE(bytes > 0);
        REQUIRE(writer.eventsRecorded() == 21);  // 20 from the loop + 1 string
        REQUIRE(writer.droppedEvents() == 0);
    }

    // Phase 2: read the file back through SessionReader, verify the
    // captured events match.
    s::SessionReader reader;
    REQUIRE(reader.open(recordPath));
    const auto md = reader.metadata();
    REQUIRE(md.description == QStringLiteral("Full-stack round-trip"));
    REQUIRE(md.decoderSchemaId == QStringLiteral("dev-board-frame-v1"));
    REQUIRE(md.signalCatalog.size() == 2);  // initial: only driver A's 2 signals

    CapturingSink sink;
    REQUIRE(reader.replayAll(sink));
    REQUIRE(reader.fileComplete());

    // 3 registrations: initial driver-A catalog, then 2 mid-stream
    // catalog extensions (driver B + a-note).
    REQUIRE(sink.registrations.size() == 3);
    REQUIRE(sink.registrations[0].size() == 2);
    REQUIRE(sink.registrations[1].size() == 2);
    REQUIRE(sink.registrations[2].size() == 1);

    // 21 signal events match the writer's record.
    REQUIRE(sink.events.size() == 21);

    // Spot-check a few:
    REQUIRE(sink.events[0].signalId == QStringLiteral("a/voltage"));
    REQUIRE(std::get<double>(sink.events[0].value) == 12.0);
    REQUIRE(sink.events[1].signalId == QStringLiteral("b/state"));
    REQUIRE(std::get<bool>(sink.events[1].value) == true);
    REQUIRE(sink.events[20].signalId == QStringLiteral("a/note"));
    const QString rt = std::get<QString>(sink.events[20].value);
    REQUIRE(rt.size() == 6);
    REQUIRE(rt[1] == QChar{u'\0'});
    REQUIRE(rt[5] == QChar{0xFF});
}
