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
