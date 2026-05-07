add_executable(session_smoke_test
    session_smoke_test.cpp
)
target_link_libraries(session_smoke_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        signalforge_session
        signalforge_buffer
        signalforge_decoder
)
catch_discover_tests(session_smoke_test DISCOVERY_MODE PRE_TEST)

add_executable(session_writer_lifecycle_test
    session_writer_lifecycle_test.cpp
)
target_link_libraries(session_writer_lifecycle_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        Qt6::Test
        signalforge_session
        signalforge_buffer
        signalforge_decoder
)
catch_discover_tests(session_writer_lifecycle_test DISCOVERY_MODE PRE_TEST)

add_executable(session_writer_encoding_test
    session_writer_encoding_test.cpp
)
target_link_libraries(session_writer_encoding_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        signalforge_session
        signalforge_buffer
        signalforge_decoder
)
catch_discover_tests(session_writer_encoding_test DISCOVERY_MODE PRE_TEST)

add_executable(session_writer_backpressure_test
    session_writer_backpressure_test.cpp
)
target_link_libraries(session_writer_backpressure_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        signalforge_session
        signalforge_decoder
)
catch_discover_tests(session_writer_backpressure_test DISCOVERY_MODE PRE_TEST)
