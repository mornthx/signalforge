add_executable(frame_test
    raw_frame_test.cpp
    stats_test.cpp
    backpressure_test.cpp
)
target_link_libraries(frame_test
    PRIVATE
        Catch2::Catch2WithMain
        signalforge_frame
)
catch_discover_tests(frame_test DISCOVERY_MODE PRE_TEST)
