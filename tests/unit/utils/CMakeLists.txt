add_executable(utils_test
    spsc_ring_test.cpp
    mpsc_queue_test.cpp
    snapshot_test.cpp
)
target_link_libraries(utils_test
    PRIVATE
        Catch2::Catch2WithMain
        signalforge_utils
)
catch_discover_tests(utils_test DISCOVERY_MODE PRE_TEST)
