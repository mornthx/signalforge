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
