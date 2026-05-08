add_executable(replay_smoke_test
    replay_smoke_test.cpp
)
target_link_libraries(replay_smoke_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        signalforge_replay
        signalforge_session
        signalforge_buffer
        signalforge_decoder
        signalforge_connection
)
catch_discover_tests(replay_smoke_test DISCOVERY_MODE PRE_TEST)

add_executable(session_player_lifecycle_test
    session_player_lifecycle_test.cpp
)
target_link_libraries(session_player_lifecycle_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        signalforge_replay
        signalforge_session
        signalforge_buffer
        signalforge_decoder
        signalforge_connection
)
catch_discover_tests(session_player_lifecycle_test DISCOVERY_MODE PRE_TEST)

add_executable(session_player_timing_test
    session_player_timing_test.cpp
)
target_link_libraries(session_player_timing_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        Qt6::Test
        signalforge_replay
        signalforge_session
        signalforge_buffer
        signalforge_decoder
        signalforge_connection
)
catch_discover_tests(session_player_timing_test DISCOVERY_MODE PRE_TEST)

add_executable(session_player_speed_step_test
    session_player_speed_step_test.cpp
)
target_link_libraries(session_player_speed_step_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        Qt6::Test
        signalforge_replay
        signalforge_session
        signalforge_buffer
        signalforge_decoder
        signalforge_connection
)
catch_discover_tests(session_player_speed_step_test DISCOVERY_MODE PRE_TEST)

add_executable(session_player_seek_test
    session_player_seek_test.cpp
)
target_link_libraries(session_player_seek_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        Qt6::Test
        signalforge_replay
        signalforge_session
        signalforge_buffer
        signalforge_decoder
        signalforge_connection
)
catch_discover_tests(session_player_seek_test DISCOVERY_MODE PRE_TEST)

add_executable(playback_controller_test
    playback_controller_test.cpp
)
target_link_libraries(playback_controller_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        Qt6::Test
        signalforge_replay
        signalforge_session
        signalforge_buffer
        signalforge_decoder
        signalforge_connection
)
catch_discover_tests(playback_controller_test DISCOVERY_MODE PRE_TEST)

add_executable(replay_mode_manager_test
    replay_mode_manager_test.cpp
)
target_link_libraries(replay_mode_manager_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        Qt6::Test
        signalforge_replay
        signalforge_session
        signalforge_buffer
        signalforge_decoder
        signalforge_connection
        signalforge_pipeline
)
catch_discover_tests(replay_mode_manager_test DISCOVERY_MODE PRE_TEST)
