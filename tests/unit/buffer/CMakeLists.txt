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

add_executable(signal_buffer_push_test
    signal_buffer_push_test.cpp
)
target_link_libraries(signal_buffer_push_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        signalforge_buffer
        signalforge_decoder
)
catch_discover_tests(signal_buffer_push_test DISCOVERY_MODE PRE_TEST)

add_executable(signal_buffer_eviction_test
    signal_buffer_eviction_test.cpp
)
target_link_libraries(signal_buffer_eviction_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        signalforge_buffer
        signalforge_decoder
)
catch_discover_tests(signal_buffer_eviction_test DISCOVERY_MODE PRE_TEST)

add_executable(signal_buffer_publish_test
    signal_buffer_publish_test.cpp
)
target_link_libraries(signal_buffer_publish_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        signalforge_buffer
        signalforge_decoder
        signalforge_observability
)
catch_discover_tests(signal_buffer_publish_test DISCOVERY_MODE PRE_TEST)
