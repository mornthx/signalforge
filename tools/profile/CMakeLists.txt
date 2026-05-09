add_executable(profile_main profile_main.cpp)
target_link_libraries(profile_main
    PRIVATE
        Qt6::Core
        signalforge_replay
        signalforge_session
        signalforge_buffer
        signalforge_decoder
)
target_include_directories(profile_main PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(profile_main PROPERTIES AUTOMOC ON)
