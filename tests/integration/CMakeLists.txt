add_executable(driver_lifecycle_with_mock
    driver_lifecycle_with_mock.cpp
)
target_link_libraries(driver_lifecycle_with_mock
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        Qt6::Test
        signalforge_drivers
        signalforge_frame
        signalforge_mocks
)
catch_discover_tests(driver_lifecycle_with_mock DISCOVERY_MODE PRE_TEST)
