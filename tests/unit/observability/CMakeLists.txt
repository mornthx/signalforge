add_executable(observability_test
    metrics_test.cpp
)
target_link_libraries(observability_test
    PRIVATE
        Catch2::Catch2WithMain
        signalforge_observability
)
catch_discover_tests(observability_test DISCOVERY_MODE PRE_TEST)

# with_fields tests run in their own process so init_logging()'s per-process
# idempotency and the SIGNALFORGE_LOG_DIR override can both be honored
# cleanly without interference from other test cases.
add_executable(with_fields_test
    with_fields_test.cpp
)
target_link_libraries(with_fields_test
    PRIVATE
        Catch2::Catch2WithMain
        signalforge_observability
)
catch_discover_tests(with_fields_test DISCOVERY_MODE PRE_TEST)
