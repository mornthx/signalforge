add_executable(video_test
    video_protocol_test.cpp
    video_receiver_test.cpp
)
target_link_libraries(video_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Test
        Qt6::Gui
        Qt6::Network
        signalforge_video
)
catch_discover_tests(video_test DISCOVERY_MODE PRE_TEST)
