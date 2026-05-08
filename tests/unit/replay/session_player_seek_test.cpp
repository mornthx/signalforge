// tests/unit/replay/session_player_seek_test.cpp
//
// M11 S6 — `signalforge::replay::SessionPlayer` seek + stepBackward.
// Validates clamping, mid-file seek, backward step, and the
// "step backward from start is a no-op" edge case.
#include "decode/decoder_interface.hpp"
#include "replay/session_player.hpp"
#include "session/session_writer.hpp"

#include <QCoreApplication>
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

struct SeekFixture {
    SeekFixture() {
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
    void clear() {
        std::lock_guard lk(mu);
        values.clear();
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

TEST_CASE_METHOD(SeekFixture, "M11 S6: seek to mid-file timestamp", "[replay][m11][s6][seek]") {
    // 5 records at 0/100/200/300/400 µs.
    const QString path = writeFile(tmp_, QStringLiteral("mid.sfreplay"), {0, 100, 200, 300, 400});

    ThreadSafeSink sink;
    r::SessionPlayer player(sink);
    REQUIRE(player.openFile(path));

    REQUIRE(player.seek(200000));                   // 200 µs
    REQUIRE(player.currentPositionNs() == 100000);  // last passed record is record 2 at 100 µs

    REQUIRE(player.stepForward());
    // The first record after the seek should be record 3 at 200 µs.
    REQUIRE(player.currentPositionNs() == 200000);
    REQUIRE(sink.values.back() == 3.0);
}

TEST_CASE_METHOD(SeekFixture, "M11 S6: seek before start clamps to start", "[replay][m11][s6][seek]") {
    const QString path = writeFile(tmp_, QStringLiteral("before.sfreplay"), {0, 100, 200});

    ThreadSafeSink sink;
    r::SessionPlayer player(sink);
    REQUIRE(player.openFile(path));

    // Step a record forward, then seek back to before-start.
    REQUIRE(player.stepForward());
    REQUIRE(player.currentRecordIndex() == 1);

    REQUIRE(player.seek(-1000));  // negative → clamped to 0
    REQUIRE(player.currentRecordIndex() == 0);
    REQUIRE(player.currentPositionNs() == 0);

    // First stepForward after seek delivers record 1 again.
    REQUIRE(player.stepForward());
    REQUIRE(sink.values.back() == 1.0);
}

TEST_CASE_METHOD(SeekFixture, "M11 S6: seek past end clamps to last record", "[replay][m11][s6][seek]") {
    const QString path = writeFile(tmp_, QStringLiteral("after.sfreplay"), {0, 100, 200});

    ThreadSafeSink sink;
    r::SessionPlayer player(sink);
    REQUIRE(player.openFile(path));

    REQUIRE(player.seek(1'000'000));  // 1 s, way past last record at 200 µs
    // After seek-clamp, the reader is positioned at the last record.
    REQUIRE(player.stepForward());
    REQUIRE(player.currentPositionNs() == 200000);
    REQUIRE(sink.values.back() == 3.0);
    REQUIRE_FALSE(player.stepForward());
    REQUIRE(player.atEnd());
}

TEST_CASE_METHOD(SeekFixture, "M11 S6: stepBackward replays records before current", "[replay][m11][s6][step-back]") {
    const QString path = writeFile(tmp_, QStringLiteral("back.sfreplay"), {0, 100, 200, 300});

    ThreadSafeSink sink;
    r::SessionPlayer player(sink);
    REQUIRE(player.openFile(path));

    // Step forward to record 3.
    REQUIRE(player.stepForward());
    REQUIRE(player.stepForward());
    REQUIRE(player.stepForward());
    REQUIRE(player.currentRecordIndex() == 3);
    REQUIRE(sink.size() == 3);
    sink.clear();

    REQUIRE(player.stepBackward());
    REQUIRE(player.currentRecordIndex() == 2);
    // Re-dispatch records 1 and 2.
    REQUIRE(sink.size() == 2);
    REQUIRE(sink.values[0] == 1.0);
    REQUIRE(sink.values[1] == 2.0);
}

TEST_CASE_METHOD(SeekFixture, "M11 S6: stepBackward at start returns false (no-op)", "[replay][m11][s6][step-back]") {
    const QString path = writeFile(tmp_, QStringLiteral("start.sfreplay"), {0, 100});

    ThreadSafeSink sink;
    r::SessionPlayer player(sink);
    REQUIRE(player.openFile(path));

    // currentRecordIndex == 0; backing up is impossible.
    REQUIRE_FALSE(player.stepBackward());
    REQUIRE(player.currentRecordIndex() == 0);
}
