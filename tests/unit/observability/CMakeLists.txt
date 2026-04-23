add_executable(observability_test
    metrics_test.cpp
)
target_link_libraries(observability_test
    PRIVATE
        Catch2::Catch2WithMain
        signalforge_observability
)
catch_discover_tests(observability_test DISCOVERY_MODE PRE_TEST)
