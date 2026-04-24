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

add_executable(test_app_smoke
    test_app_smoke.cpp
)
target_link_libraries(test_app_smoke
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
)
target_compile_definitions(test_app_smoke
    PRIVATE
        SIGNALFORGE_APP_BINARY="$<TARGET_FILE:signalforge>"
)
# The smoke test launches the app binary; make sure it is up to date.
add_dependencies(test_app_smoke signalforge)
catch_discover_tests(test_app_smoke DISCOVERY_MODE PRE_TEST)
