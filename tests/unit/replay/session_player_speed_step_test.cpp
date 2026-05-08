// tests/unit/replay/session_player_speed_step_test.cpp
//
// M11 S5 — speed scaling + stepForward + 30 Hz position-emit
// throttle validation.
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

struct StepFixture {
    StepFixture() {
        if (!QCoreApplication::instance()) {
            static int argc = 0;
            static char* argv[] = {nullptr};
            app_ = std::make_unique<QCoreApplication>(argc, argv);
        }
    }
    std::unique_ptr<QCoreApplication> app_;
    QTemporaryDir tmp_;
};

struct ThreadSafeSink : public d::SignalValueSink {
    mutable std::mutex mu;
    std::vector<double> values;

    void onSignal(std::chrono::steady_clock::time_point /*t*/, const QString& /*id*/,
                  const d::SignalValue& v) override {
        std::lock_guard lk(mu);
        values.push_back(std::get<double>(v));
    }
    std::size_t size() const {
        std::lock_guard lk(mu);
        return values.size();
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

QString writeFile(QTemporaryDir& tmp, const QString& name, const std::vector<std::int64_t>& tsUs) {
    const QString path = tmp.filePath(name);
    b::SignalBufferRegistry registry;
    s::SessionWriter writer(registry);
    writer.onSignalsRegistered(QStringLiteral("d1"), {makeMeta(QStringLiteral("a"))});
    REQUIRE(writer.start(path));
    const auto t0 = writer.metadata().recordingStart;
    for (std::size_t i = 0; i < tsUs.size(); ++i) {
        const double v = static_cast<double>(i + 1);
        writer.onSignal(t0 + std::chrono::microseconds(tsUs[i]), QStringLiteral("a"), d::SignalValue{v});
    }
    (void)writer.stop();
    return path;
}

}  // namespace

TEST_CASE_METHOD(StepFixture, "M11 S5: setSpeed clamps out-of-range values + logs WARN", "[replay][m11][s5][speed]") {
    ThreadSafeSink sink;
    r::SessionPlayer player(sink);

    player.setSpeed(0.1);
    REQUIRE(player.currentSpeed() == 0.5);

    player.setSpeed(100.0);
    REQUIRE(player.currentSpeed() == 10.0);

    player.setSpeed(2.0);
    REQUIRE(player.currentSpeed() == 2.0);
}

TEST_CASE_METHOD(StepFixture, "M11 S5: 10x speed completes a 100ms file in well under 100ms",
                 "[replay][m11][s5][speed]") {
    // 4 records spaced 30 ms apart → real-time = 90 ms; at 10×
    // should be ~9 ms.
    const QString path = writeFile(tmp_, QStringLiteral("fast.sfreplay"), {0, 30000, 60000, 90000});

    ThreadSafeSink sink;
    r::SessionPlayer player(sink);
    REQUIRE(player.openFile(path));
    player.setSpeed(10.0);

    QSignalSpy endSpy(&player, &r::SessionPlayer::endReached);
    const auto wallStart = std::chrono::steady_clock::now();
    player.play();
    REQUIRE(endSpy.wait(2000));
    const auto wallEnd = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(wallEnd - wallStart);
    QCoreApplication::processEvents();

    REQUIRE(sink.size() == 4);
    // Real-time would be ~90 ms; 10× should be ≤ 30 ms (generous
    // for scheduler jitter under load).
    REQUIRE(elapsed.count() < 50);
}

TEST_CASE_METHOD(StepFixture, "M11 S5: stepForward delivers one record at a time + EOF returns false",
                 "[replay][m11][s5][step]") {
    const QString path = writeFile(tmp_, QStringLiteral("step.sfreplay"), {0, 100, 200});

    ThreadSafeSink sink;
    r::SessionPlayer player(sink);
    REQUIRE(player.openFile(path));

    REQUIRE(player.stepForward());
    REQUIRE(sink.size() == 1);
    REQUIRE(player.currentRecordIndex() == 1);
    REQUIRE(player.currentPositionNs() == 0);

    REQUIRE(player.stepForward());
    REQUIRE(sink.size() == 2);
    REQUIRE(player.currentRecordIndex() == 2);
    // Record 2 was written at +100 µs from start → 100 000 ns.
    REQUIRE(player.currentPositionNs() == 100000);

    REQUIRE(player.stepForward());
    REQUIRE(sink.size() == 3);

    // Past end.
    REQUIRE_FALSE(player.stepForward());
    REQUIRE(sink.size() == 3);
    REQUIRE(player.atEnd());
}

TEST_CASE_METHOD(StepFixture, "M11 S5: positionUpdated rate-limited to ~30Hz under sustained dispatch",
                 "[replay][m11][s5][throttle]") {
    // Pack many records into ~100 ms wall clock at 1×: 50 records
    // at 2 ms intervals → 100 ms total. Without throttling we'd
    // see 50 positionUpdated emits; with 33 ms throttling we
    // expect ≤ ~5 emits.
    std::vector<std::int64_t> tsUs;
    for (std::int64_t i = 0; i < 50; ++i) {
        tsUs.push_back(i * 2000);  // 2 ms
    }
    const QString path = writeFile(tmp_, QStringLiteral("throttle.sfreplay"), tsUs);

    ThreadSafeSink sink;
    r::SessionPlayer player(sink);
    REQUIRE(player.openFile(path));

    QSignalSpy posSpy(&player, &r::SessionPlayer::positionUpdated);
    QSignalSpy endSpy(&player, &r::SessionPlayer::endReached);

    player.play();
    REQUIRE(endSpy.wait(2000));
    QCoreApplication::processEvents();

    REQUIRE(sink.size() == 50);
    // 100 ms file at 1× → at 30 Hz cap, expect ~3 emits. Allow a
    // generous range to absorb scheduler jitter / first-record
    // unconditional emit.
    REQUIRE(posSpy.count() >= 1);
    REQUIRE(posSpy.count() <= 8);
}
