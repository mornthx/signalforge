add_executable(connection_smoke_test
    connection_smoke_test.cpp
)
target_link_libraries(connection_smoke_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        Qt6::Widgets
        signalforge_connection
        signalforge_drivers
        signalforge_decoder
)
catch_discover_tests(connection_smoke_test DISCOVERY_MODE PRE_TEST)

add_executable(connection_test
    connection_test.cpp
)
target_link_libraries(connection_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        Qt6::Test
        signalforge_connection
        signalforge_drivers
)
target_compile_definitions(connection_test
    PRIVATE
        SIGNALFORGE_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/tests/integration/fixtures"
)
catch_discover_tests(connection_test DISCOVERY_MODE PRE_TEST)
