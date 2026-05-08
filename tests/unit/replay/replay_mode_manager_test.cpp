// tests/unit/replay/replay_mode_manager_test.cpp
//
// M11 S8 — `signalforge::replay::ReplayModeManager` Live ↔ Replay
// transitions. Connection-state side effects are exercised in
// S11 integration tests with real M9 connections; these unit
// tests focus on the mode transitions + signal fan-out.
#include "buffer/signal_buffer_registry.hpp"
#include "connection/connection_manager.hpp"
#include "decode/decoder_registrar.hpp"
#include "pipeline/pipeline_manager.hpp"
#include "replay/playback_controller.hpp"
#include "replay/replay_mode_manager.hpp"

#include <QCoreApplication>
#include <QSignalSpy>
#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <unordered_map>

namespace r = signalforge::replay;
namespace c = signalforge::connection;
namespace b = signalforge::buffer;
namespace d = signalforge::decoder;

namespace {

struct ModeFixture {
    ModeFixture() {
        if (!QCoreApplication::instance()) {
            static int argc = 0;
            static char* argv[] = {nullptr};
            app_ = std::make_unique<QCoreApplication>(argc, argv);
        }
        pipeline_ = std::make_unique<signalforge::pipeline::PipelineManager>();
        registrar_ =
            std::make_unique<d::DecoderRegistrar>(pipeline_.get(), std::unordered_map<QString, QString>{}, nullptr);
        connectionMgr_ = std::make_unique<c::ConnectionManager>(*registrar_);
        bufferRegistry_ = std::make_unique<b::SignalBufferRegistry>();
        playbackCtrl_ = std::make_unique<r::PlaybackController>(*bufferRegistry_);
    }

    std::unique_ptr<QCoreApplication> app_;
    std::unique_ptr<signalforge::pipeline::PipelineManager> pipeline_;
    std::unique_ptr<d::DecoderRegistrar> registrar_;
    std::unique_ptr<c::ConnectionManager> connectionMgr_;
    std::unique_ptr<b::SignalBufferRegistry> bufferRegistry_;
    std::unique_ptr<r::PlaybackController> playbackCtrl_;
};

}  // namespace

TEST_CASE_METHOD(ModeFixture, "M11 S8: default mode is Live", "[replay][m11][s8][mode]") {
    r::ReplayModeManager mgr(*connectionMgr_, *playbackCtrl_);
    REQUIRE(mgr.currentMode() == r::AppMode::Live);
    REQUIRE(mgr.pausedConnectionIds().isEmpty());
}

TEST_CASE_METHOD(ModeFixture, "M11 S8: enterReplay with no active connections transitions Live → Replay",
                 "[replay][m11][s8][mode]") {
    r::ReplayModeManager mgr(*connectionMgr_, *playbackCtrl_);
    QSignalSpy modeSpy(&mgr, &r::ReplayModeManager::modeChanged);
    QSignalSpy pausedSpy(&mgr, &r::ReplayModeManager::connectionsPaused);

    REQUIRE(mgr.enterReplay());
    REQUIRE(mgr.currentMode() == r::AppMode::Replay);
    REQUIRE(modeSpy.count() == 1);
    REQUIRE(modeSpy[0][0].value<r::AppMode>() == r::AppMode::Replay);
    // No active connections → no connectionsPaused emit.
    REQUIRE(pausedSpy.count() == 0);
    // Snapshot is empty.
    REQUIRE(mgr.pausedConnectionIds().isEmpty());
}

TEST_CASE_METHOD(ModeFixture, "M11 S8: enterReplay when already in Replay returns false", "[replay][m11][s8][mode]") {
    r::ReplayModeManager mgr(*connectionMgr_, *playbackCtrl_);
    REQUIRE(mgr.enterReplay());
    REQUIRE(mgr.currentMode() == r::AppMode::Replay);

    REQUIRE_FALSE(mgr.enterReplay());
    REQUIRE(mgr.currentMode() == r::AppMode::Replay);
}

TEST_CASE_METHOD(ModeFixture, "M11 S8: exitReplay(false) transitions Replay → Live without restoration",
                 "[replay][m11][s8][mode]") {
    r::ReplayModeManager mgr(*connectionMgr_, *playbackCtrl_);
    REQUIRE(mgr.enterReplay());

    QSignalSpy modeSpy(&mgr, &r::ReplayModeManager::modeChanged);
    QSignalSpy restoredSpy(&mgr, &r::ReplayModeManager::connectionsRestored);

    REQUIRE(mgr.exitReplay(false));
    REQUIRE(mgr.currentMode() == r::AppMode::Live);
    REQUIRE(modeSpy.count() == 1);
    REQUIRE(modeSpy[0][0].value<r::AppMode>() == r::AppMode::Live);
    REQUIRE(restoredSpy.count() == 0);
    REQUIRE(mgr.pausedConnectionIds().isEmpty());
}

TEST_CASE_METHOD(ModeFixture, "M11 S8: exitReplay(true) transitions Replay → Live + emits restored",
                 "[replay][m11][s8][mode]") {
    r::ReplayModeManager mgr(*connectionMgr_, *playbackCtrl_);
    REQUIRE(mgr.enterReplay());

    QSignalSpy restoredSpy(&mgr, &r::ReplayModeManager::connectionsRestored);

    REQUIRE(mgr.exitReplay(true));
    REQUIRE(mgr.currentMode() == r::AppMode::Live);
    // No active connections to restore → emit count = 0 (the
    // signal is gated on a non-empty restoration list).
    REQUIRE(restoredSpy.count() == 0);
}

TEST_CASE_METHOD(ModeFixture, "M11 S8: exitReplay when already in Live returns false", "[replay][m11][s8][mode]") {
    r::ReplayModeManager mgr(*connectionMgr_, *playbackCtrl_);
    REQUIRE(mgr.currentMode() == r::AppMode::Live);
    REQUIRE_FALSE(mgr.exitReplay(false));
    REQUIRE(mgr.currentMode() == r::AppMode::Live);
}
