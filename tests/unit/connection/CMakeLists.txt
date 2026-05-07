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
