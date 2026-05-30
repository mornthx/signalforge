add_executable(decoder_test
    decoder_test.cpp
)
target_link_libraries(decoder_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        signalforge_decoder
        signalforge_pipeline
        signalforge_frame
        signalforge_test_only
)
target_compile_definitions(decoder_test
    PRIVATE
        SIGNALFORGE_REPO_ROOT="${CMAKE_SOURCE_DIR}"
)
catch_discover_tests(decoder_test DISCOVERY_MODE PRE_TEST)

add_executable(frame_dissector_test
    frame_dissector_test.cpp
)
target_link_libraries(frame_dissector_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        signalforge_decoder
)
catch_discover_tests(frame_dissector_test DISCOVERY_MODE PRE_TEST)

add_executable(decoder_buffer_cache_test
    decoder_buffer_cache_test.cpp
)
target_link_libraries(decoder_buffer_cache_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        signalforge_decoder
        signalforge_buffer
        signalforge_frame
        signalforge_test_only
)
catch_discover_tests(decoder_buffer_cache_test DISCOVERY_MODE PRE_TEST)
