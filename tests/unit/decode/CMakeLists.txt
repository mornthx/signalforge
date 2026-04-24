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
)
catch_discover_tests(decoder_test DISCOVERY_MODE PRE_TEST)
