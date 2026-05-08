// tests/unit/replay/replay_smoke_test.cpp
//
// S1 smoke tests: confirms the M11 freeze-surface enums and types
// are linkable + default-constructible. Does not exercise any
// behaviour (S3-S8 land that).
#include "replay/playback_controller.hpp"
#include "replay/replay_mode_manager.hpp"
#include "replay/session_player.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("M11 S1: PlaybackState enum has the seven spec values", "[replay][smoke]") {
    using signalforge::replay::PlaybackState;
    REQUIRE(static_cast<int>(PlaybackState::Idle) != static_cast<int>(PlaybackState::Loaded));
    REQUIRE(static_cast<int>(PlaybackState::Loaded) != static_cast<int>(PlaybackState::Playing));
    REQUIRE(static_cast<int>(PlaybackState::Playing) != static_cast<int>(PlaybackState::Paused));
    REQUIRE(static_cast<int>(PlaybackState::Paused) != static_cast<int>(PlaybackState::Seeking));
    REQUIRE(static_cast<int>(PlaybackState::Seeking) != static_cast<int>(PlaybackState::Ended));
    REQUIRE(static_cast<int>(PlaybackState::Ended) != static_cast<int>(PlaybackState::Error));
}

TEST_CASE("M11 S1: AppMode enum has the two spec values", "[replay][smoke]") {
    using signalforge::replay::AppMode;
    REQUIRE(static_cast<int>(AppMode::Live) != static_cast<int>(AppMode::Replay));
}
