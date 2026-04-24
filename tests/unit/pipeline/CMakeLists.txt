add_executable(pipeline_test
    pipeline_test.cpp
)
target_link_libraries(pipeline_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        signalforge_pipeline
        signalforge_drivers
        signalforge_frame
)
catch_discover_tests(pipeline_test DISCOVERY_MODE PRE_TEST)
