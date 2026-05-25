// tests/unit/replay/playback_controller_test.cpp
//
// M11 S7 — `signalforge::replay::PlaybackController` state
// machine + signal fan-out from the owned SessionPlayer.
#include "buffer/signal_buffer_registry.hpp"
#include "decode/decoder_interface.hpp"
#include "replay/playback_controller.hpp"
#include "session/session_writer.hpp"

#include <QCoreApplication>
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

struct ControllerFixture {
    ControllerFixture() {
        if (!QCoreApplication::instance()) {
            static int argc = 0;
            static char* argv[] = {nullptr};
            app_ = std::make_unique<QCoreApplication>(argc, argv);
        }
    }
    std::unique_ptr<QCoreApplication> app_;
    QTemporaryDir tmp_;
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

TEST_CASE_METHOD(ControllerFixture, "M11 S7: full happy-path lifecycle Idle → Loaded → Playing → Ended → Idle",
                 "[replay][m11][s7]") {
    const QString path = writeFile(tmp_, QStringLiteral("happy.sfreplay"), {0, 100, 200});
    b::SignalBufferRegistry registry;
    r::PlaybackController ctrl(registry);

    REQUIRE(ctrl.state() == r::PlaybackState::Idle);

    QSignalSpy stateSpy(&ctrl, &r::PlaybackController::stateChanged);
    QSignalSpy loadedSpy(&ctrl, &r::PlaybackController::sessionLoaded);

    REQUIRE(ctrl.loadSession(path));
    REQUIRE(ctrl.state() == r::PlaybackState::Loaded);
    REQUIRE(ctrl.currentFilePath() == path);
    REQUIRE(loadedSpy.count() == 1);
    REQUIRE(ctrl.totalRecords() == 3);

    REQUIRE(ctrl.play());
    REQUIRE(ctrl.state() == r::PlaybackState::Playing);

    QSignalSpy endedSpy(&ctrl, &r::PlaybackController::stateChanged);
    REQUIRE(endedSpy.wait(2000));
    QCoreApplication::processEvents();
    REQUIRE(ctrl.state() == r::PlaybackState::Ended);

    QSignalSpy closedSpy(&ctrl, &r::PlaybackController::sessionClosed);
    ctrl.closeSession();
    REQUIRE(ctrl.state() == r::PlaybackState::Idle);
    REQUIRE(closedSpy.count() == 1);
}

TEST_CASE_METHOD(ControllerFixture, "M11 S7: play in Idle is rejected", "[replay][m11][s7]") {
    b::SignalBufferRegistry registry;
    r::PlaybackController ctrl(registry);
    REQUIRE_FALSE(ctrl.play());
    REQUIRE(ctrl.state() == r::PlaybackState::Idle);
}

TEST_CASE_METHOD(ControllerFixture, "M11 S7: pause in Loaded is rejected", "[replay][m11][s7]") {
    const QString path = writeFile(tmp_, QStringLiteral("pause.sfreplay"), {0, 100});
    b::SignalBufferRegistry registry;
    r::PlaybackController ctrl(registry);

    REQUIRE(ctrl.loadSession(path));
    REQUIRE(ctrl.state() == r::PlaybackState::Loaded);
    REQUIRE_FALSE(ctrl.pause());
    REQUIRE(ctrl.state() == r::PlaybackState::Loaded);
}

TEST_CASE_METHOD(ControllerFixture, "M11 S7: loadSession while loaded auto-closes prior session", "[replay][m11][s7]") {
    const QString path1 = writeFile(tmp_, QStringLiteral("a.sfreplay"), {0, 100});
    const QString path2 = writeFile(tmp_, QStringLiteral("b.sfreplay"), {0, 100, 200});

    b::SignalBufferRegistry registry;
    r::PlaybackController ctrl(registry);

    REQUIRE(ctrl.loadSession(path1));
    REQUIRE(ctrl.totalRecords() == 2);

    REQUIRE(ctrl.loadSession(path2));
    REQUIRE(ctrl.currentFilePath() == path2);
    REQUIRE(ctrl.totalRecords() == 3);
}

TEST_CASE_METHOD(ControllerFixture, "M11 S7: stepForward from Loaded transitions to Paused", "[replay][m11][s7]") {
    const QString path = writeFile(tmp_, QStringLiteral("step.sfreplay"), {0, 100, 200});
    b::SignalBufferRegistry registry;
    r::PlaybackController ctrl(registry);

    REQUIRE(ctrl.loadSession(path));
    REQUIRE(ctrl.state() == r::PlaybackState::Loaded);

    REQUIRE(ctrl.stepForward());
    REQUIRE(ctrl.state() == r::PlaybackState::Paused);
    REQUIRE(ctrl.currentRecordIndex() == 1);
}

TEST_CASE_METHOD(ControllerFixture, "M11 S7: closeSession from any state returns to Idle", "[replay][m11][s7]") {
    const QString path = writeFile(tmp_, QStringLiteral("close.sfreplay"), {0, 100});
    b::SignalBufferRegistry registry;
    r::PlaybackController ctrl(registry);

    REQUIRE(ctrl.loadSession(path));
    REQUIRE(ctrl.play());
    REQUIRE(ctrl.state() == r::PlaybackState::Playing);

    ctrl.closeSession();
    REQUIRE(ctrl.state() == r::PlaybackState::Idle);
    REQUIRE(ctrl.currentFilePath().isEmpty());
}

TEST_CASE_METHOD(ControllerFixture, "M18 UX: seek emits immediate positionChanged for slider/status feedback",
                 "[replay][m18][ux]") {
    const QString path = writeFile(tmp_, QStringLiteral("seek-feedback.sfreplay"), {0, 100, 200});
    b::SignalBufferRegistry registry;
    r::PlaybackController ctrl(registry);
    REQUIRE(ctrl.loadSession(path));

    QSignalSpy positionSpy(&ctrl, &r::PlaybackController::positionChanged);
    REQUIRE(ctrl.seek(150000));

    REQUIRE(positionSpy.count() == 1);
    REQUIRE(ctrl.currentPositionNs() >= 100000);
    REQUIRE(ctrl.currentRecordIndex() >= 1);
}

TEST_CASE_METHOD(ControllerFixture, "M18 UX: speed change emits positionChanged without waiting for next record",
                 "[replay][m18][ux]") {
    const QString path = writeFile(tmp_, QStringLiteral("speed-feedback.sfreplay"), {0, 100, 200});
    b::SignalBufferRegistry registry;
    r::PlaybackController ctrl(registry);
    REQUIRE(ctrl.loadSession(path));

    QSignalSpy speedSpy(&ctrl, &r::PlaybackController::speedChanged);
    QSignalSpy positionSpy(&ctrl, &r::PlaybackController::positionChanged);
    REQUIRE(ctrl.setSpeed(2.0));

    REQUIRE(speedSpy.count() == 1);
    REQUIRE(positionSpy.count() == 1);
    REQUIRE(ctrl.currentSpeed() == 2.0);
}
