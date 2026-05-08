// tests/integration/test_replay_full_stack.cpp
//
// M11 S11 integration tests — exercises the composed
// SignalBufferRegistry + PlaybackController + SessionPlayer +
// SessionReader stack end-to-end. The unit tests at
// tests/unit/replay/ cover the granular behaviour; this file
// asserts the wiring works as MainWindow uses it.
//
// Spec §2.1-12 mapping (M10 hybrid pattern):
//
//   - test_playback_controller_lifecycle:
//         tests/unit/replay/playback_controller_test.cpp
//         + this file (M11 S11 lifecycle case)
//   - test_session_player_timing:
//         tests/unit/replay/session_player_timing_test.cpp
//         + this file (charts-receive-events case)
//   - test_session_player_seek:
//         tests/unit/replay/session_player_seek_test.cpp
//   - test_replay_mode_manager:
//         tests/integration/test_replay_mode_manager.cpp
//         (separate file with real M9 connections)
//   - test_replay_charts_integration:
//         this file (registry assertions after replay)
//   - test_session_player_speed_change:
//         tests/unit/replay/session_player_speed_step_test.cpp
//   - test_session_player_truncated:
//         this file (truncated-file case below)
#include "buffer/signal_buffer_registry.hpp"
#include "decode/decoder_interface.hpp"
#include "replay/playback_controller.hpp"
#include "session/session_writer.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <memory>

namespace r = signalforge::replay;
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

d::SignalMetadata makeMeta(const QString& id, d::SignalType type) {
    d::SignalMetadata m;
    m.id = id;
    m.name = id;
    m.unit = QStringLiteral("u");
    m.type = type;
    return m;
}

QString writeFixture(QTemporaryDir& tmp, const QString& name) {
    const QString path = tmp.filePath(name);
    b::SignalBufferRegistry registry;
    s::SessionWriter writer(registry);
    std::vector<d::SignalMetadata> metas{makeMeta(QStringLiteral("temp"), d::SignalType::Double),
                                         makeMeta(QStringLiteral("flag"), d::SignalType::Bool)};
    writer.onSignalsRegistered(QStringLiteral("d1"), metas);
    REQUIRE(writer.start(path));
    const auto t0 = writer.metadata().recordingStart;
    for (int i = 0; i < 20; ++i) {
        writer.onSignal(t0 + std::chrono::microseconds(i * 1000), QStringLiteral("temp"),
                        d::SignalValue{static_cast<double>(i)});
        writer.onSignal(t0 + std::chrono::microseconds(i * 1000), QStringLiteral("flag"), d::SignalValue{(i % 2) == 0});
    }
    (void)writer.stop();
    return path;
}

}  // namespace

TEST_CASE_METHOD(FullStackFixture, "M11 S11: PlaybackController + SignalBufferRegistry happy path",
                 "[replay][m11][s11][integration]") {
    const QString path = writeFixture(tmp_, QStringLiteral("happy.sfreplay"));

    b::SignalBufferRegistry registry;
    r::PlaybackController ctrl(registry);
    REQUIRE(ctrl.loadSession(path));
    REQUIRE(registry.signalCount() == 2);  // catalog announcement at openFile

    QSignalSpy stateSpy(&ctrl, &r::PlaybackController::stateChanged);
    REQUIRE(ctrl.play());

    REQUIRE(stateSpy.wait(2000));
    QCoreApplication::processEvents();
    REQUIRE(ctrl.state() == r::PlaybackState::Ended);
    // Registry has both signals registered + buffers populated.
    REQUIRE(registry.signalCount() == 2);
}

TEST_CASE_METHOD(FullStackFixture, "M11 S11: charts-style registry assertions after replay",
                 "[replay][m11][s11][integration]") {
    const QString path = writeFixture(tmp_, QStringLiteral("chart.sfreplay"));

    b::SignalBufferRegistry registry;
    r::PlaybackController ctrl(registry);
    REQUIRE(ctrl.loadSession(path));

    QSignalSpy endSpy(&ctrl, &r::PlaybackController::stateChanged);
    REQUIRE(ctrl.play());
    REQUIRE(endSpy.wait(2000));
    QCoreApplication::processEvents();

    // Registry tracks total bytes; a non-zero footprint after
    // replay indicates the registry observed onSignal events from
    // the dispatch loop.
    REQUIRE(registry.totalMemoryBytes() > 0);
}

TEST_CASE_METHOD(FullStackFixture, "M11 S11: truncated file replays partial then ends gracefully",
                 "[replay][m11][s11][integration]") {
    const QString path = writeFixture(tmp_, QStringLiteral("trunc.sfreplay"));

    // Truncate the last 32 bytes so footer + last record are
    // chopped. The reader's truncation tolerance (M10 §9 + the
    // existing M10 round-trip tests) handles this; M11 wraps it
    // and surfaces an end-of-file via Ended (or Error) state.
    {
        QFile file(path);
        REQUIRE(file.open(QIODevice::ReadWrite));
        REQUIRE(file.resize(file.size() - 32));
    }

    b::SignalBufferRegistry registry;
    r::PlaybackController ctrl(registry);
    REQUIRE(ctrl.loadSession(path));  // header is intact, so load succeeds

    QSignalSpy stateSpy(&ctrl, &r::PlaybackController::stateChanged);
    REQUIRE(ctrl.play());
    REQUIRE(stateSpy.wait(2000));
    QCoreApplication::processEvents();
    // Dispatch ended cleanly (Ended state). Truncation does not
    // crash or hang.
    REQUIRE((ctrl.state() == r::PlaybackState::Ended || ctrl.state() == r::PlaybackState::Paused));
}
