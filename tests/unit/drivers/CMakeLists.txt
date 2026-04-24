add_executable(drivers_test
    driver_interface_test.cpp
    driver_configs_test.cpp
    io_worker_base_test.cpp
    replay_driver_test.cpp
)
target_link_libraries(drivers_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Test
        signalforge_drivers
        signalforge_frame
        signalforge_mocks
)
catch_discover_tests(drivers_test DISCOVERY_MODE PRE_TEST)
