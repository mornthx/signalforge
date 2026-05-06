add_executable(buffer_smoke_test
    buffer_smoke_test.cpp
)
target_link_libraries(buffer_smoke_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        signalforge_buffer
        signalforge_decoder
)
catch_discover_tests(buffer_smoke_test DISCOVERY_MODE PRE_TEST)
