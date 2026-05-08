// tests/unit/replay/session_player_timing_test.cpp
//
// M11 S4 — `signalforge::replay::SessionPlayer` 1× timing
// dispatch. Verifies the worker thread reads records in order
// from `SessionReader::readNextRecord` and dispatches them to
// the configured `SignalValueSink` on the main thread via
// queued connection. The S4 dispatch loop honours
// `speedFactor_=1.0`; S5 covers non-1× scaling.
#include "decode/decoder_interface.hpp"
#include "replay/session_player.hpp"
#include "session/session_writer.hpp"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <buffer/signal_buffer_registry.hpp>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <memory>
#include <mutex>
#include <vector>

namespace r = signalforge::replay;
namespace s = signalforge::session;
namespace b = signalforge::buffer;
namespace d = signalforge::decoder;

namespace {

struct TimingFixture {
    TimingFixture() {
        if (!QCoreApplication::instance()) {
            static int argc = 0;
            static char* argv[] = {nullptr};
            app_ = std::make_unique<QCoreApplication>(argc, argv);
        }
    }
    std::unique_ptr<QCoreApplication> app_;
    QTemporaryDir tmp_;
};

/// Thread-safe capture: the worker dispatches via QueuedConnection
/// to the main thread, where this sink's onSignal runs. A mutex
/// is still helpful so test threads can read the vector during
/// processEvents loops.
struct ThreadSafeSink : public d::SignalValueSink {
    mutable std::mutex mu;
    std::vector<std::pair<QString, double>> events;
    int registrationCount = 0;

    void onSignal(std::chrono::steady_clock::time_point /*t*/, const QString& id, const d::SignalValue& v) override {
        std::lock_guard lk(mu);
        events.emplace_back(id, std::get<double>(v));
    }
    void onSignalsRegistered(const QString& /*driverId*/, const std::vector<d::SignalMetadata>& /*sigs*/) override {
        std::lock_guard lk(mu);
        ++registrationCount;
    }

    std::size_t size() const {
        std::lock_guard lk(mu);
        return events.size();
    }
};

d::SignalMetadata makeMeta(const QString& id) {
    d::SignalMetadata m;
    m.id = id;
    m.name = id;
    m.unit = QStringLiteral("u");
    m.type = d::SignalType::Double;
    return m;
}

QString writeFile(QTemporaryDir& tmp, const QString& name, const std::vector<std::int64_t>& deltasUs,
                  std::vector<double>& outValues) {
    const QString path = tmp.filePath(name);
    b::SignalBufferRegistry registry;
    s::SessionWriter writer(registry);
    writer.onSignalsRegistered(QStringLiteral("d1"), {makeMeta(QStringLiteral("a"))});
    REQUIRE(writer.start(path));
    const auto t0 = writer.metadata().recordingStart;
    outValues.clear();
    for (std::size_t i = 0; i < deltasUs.size(); ++i) {
        const double v = static_cast<double>(i + 1);
        writer.onSignal(t0 + std::chrono::microseconds(deltasUs[i]), QStringLiteral("a"), d::SignalValue{v});
        outValues.push_back(v);
    }
    (void)writer.stop();
    return path;
}

}  // namespace

TEST_CASE_METHOD(TimingFixture, "M11 S4: 1× delivery preserves order across all records", "[replay][m11][s4][timing]") {
    std::vector<double> expected;
    const QString path = writeFile(tmp_, QStringLiteral("ord.sfreplay"), {0, 100, 200, 300, 400}, expected);

    ThreadSafeSink sink;
    r::SessionPlayer player(sink);
    REQUIRE(player.openFile(path));

    QSignalSpy endSpy(&player, &r::SessionPlayer::endReached);

    player.play();

    // 5 records, max delta 400 µs total → playback under 1 ms; the
    // 5 s timeout below is generous to absorb scheduler jitter.
    REQUIRE(endSpy.wait(5000));

    // Drain pending queued sink dispatches to the main thread.
    QCoreApplication::processEvents();

    REQUIRE(sink.size() == expected.size());
    REQUIRE(sink.events.size() == 5);
    for (std::size_t i = 0; i < expected.size(); ++i) {
        REQUIRE(sink.events[i].second == expected[i]);
    }
    REQUIRE(player.atEnd());
    REQUIRE_FALSE(player.isPlaying());
}

TEST_CASE_METHOD(TimingFixture, "M11 S4: pause stops delivery mid-replay", "[replay][m11][s4][timing]") {
    // Big inter-record gap so the worker enters a long sleep
    // between records 1 and 2 (which the test interrupts via
    // pause()).
    std::vector<double> expected;
    const QString path = writeFile(tmp_, QStringLiteral("pause.sfreplay"), {0, 200000}, expected);  // 200 ms gap

    ThreadSafeSink sink;
    r::SessionPlayer player(sink);
    REQUIRE(player.openFile(path));

    player.play();

    // Wait long enough for record 1 to dispatch + the worker to
    // start sleeping toward record 2 (~5 ms is plenty).
    QSignalSpy posSpy(&player, &r::SessionPlayer::positionUpdated);
    REQUIRE(posSpy.wait(500));
    QCoreApplication::processEvents();

    REQUIRE(sink.size() == 1);
    REQUIRE(player.isPlaying());

    // Pause within the long inter-record sleep.
    player.pause();
    REQUIRE_FALSE(player.isPlaying());
    QCoreApplication::processEvents();

    // Still only 1 record delivered — record 2 was paused before
    // its sleep ended.
    REQUIRE(sink.size() == 1);
    REQUIRE_FALSE(player.atEnd());
}

TEST_CASE_METHOD(TimingFixture, "M11 S4: resume from pause continues from same position", "[replay][m11][s4][timing]") {
    std::vector<double> expected;
    const QString path =
        writeFile(tmp_, QStringLiteral("resume.sfreplay"), {0, 100000, 200000}, expected);  // 100 ms gaps

    ThreadSafeSink sink;
    r::SessionPlayer player(sink);
    REQUIRE(player.openFile(path));

    player.play();

    QSignalSpy endSpy(&player, &r::SessionPlayer::endReached);
    QSignalSpy posSpy(&player, &r::SessionPlayer::positionUpdated);

    // Wait for at least record 1 to dispatch, then pause.
    REQUIRE(posSpy.wait(500));
    QCoreApplication::processEvents();
    const std::size_t paused = sink.size();
    REQUIRE(paused >= 1);
    REQUIRE(paused < 3);  // not yet at end

    player.pause();
    REQUIRE_FALSE(player.isPlaying());
    QCoreApplication::processEvents();

    // Resume.
    player.play();
    REQUIRE(endSpy.wait(5000));
    QCoreApplication::processEvents();

    REQUIRE(sink.size() == 3);
    REQUIRE(player.atEnd());
    REQUIRE_FALSE(player.isPlaying());
}
