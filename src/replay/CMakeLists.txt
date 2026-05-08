add_library(signalforge_replay STATIC
    playback_controller.cpp
    session_player.cpp
    replay_mode_manager.cpp
)

set_target_properties(signalforge_replay PROPERTIES AUTOMOC ON)

target_include_directories(signalforge_replay PUBLIC ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(signalforge_replay
    PUBLIC
        Qt6::Core
        Qt6::Widgets
        signalforge_session
        signalforge_buffer
        signalforge_decoder
        signalforge_connection
    PRIVATE
        signalforge_observability
)
