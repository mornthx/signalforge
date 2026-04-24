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

add_library(signalforge_socat_fixture STATIC
    socat_fixture.cpp
    socat_fixture.hpp
)
target_link_libraries(signalforge_socat_fixture
    PUBLIC Qt6::Core
)
target_include_directories(signalforge_socat_fixture PUBLIC ${CMAKE_SOURCE_DIR})

add_executable(test_serial_driver_loopback
    test_serial_driver_loopback.cpp
)
target_link_libraries(test_serial_driver_loopback
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        Qt6::Test
        signalforge_drivers
        signalforge_frame
        signalforge_socat_fixture
)
catch_discover_tests(test_serial_driver_loopback DISCOVERY_MODE PRE_TEST PROPERTIES LABELS "socat")

add_executable(test_replay_driver_skeleton
    test_replay_driver_skeleton.cpp
)
target_link_libraries(test_replay_driver_skeleton
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        Qt6::Test
        signalforge_drivers
        signalforge_frame
)
target_compile_definitions(test_replay_driver_skeleton
    PRIVATE
        SIGNALFORGE_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)
catch_discover_tests(test_replay_driver_skeleton DISCOVERY_MODE PRE_TEST)
