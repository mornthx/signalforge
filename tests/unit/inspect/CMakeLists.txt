add_executable(parsed_signals_view_test
    parsed_signals_view_test.cpp
)
target_link_libraries(parsed_signals_view_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        signalforge_inspect
        signalforge_buffer
        signalforge_decoder
)
catch_discover_tests(parsed_signals_view_test DISCOVERY_MODE PRE_TEST)

add_executable(raw_frame_tap_test
    raw_frame_tap_test.cpp
)
target_link_libraries(raw_frame_tap_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        signalforge_inspect
)
catch_discover_tests(raw_frame_tap_test DISCOVERY_MODE PRE_TEST)
