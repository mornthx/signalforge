add_executable(platform_test
    time_source_test.cpp
    thread_utils_test.cpp
    app_paths_test.cpp
)
target_link_libraries(platform_test
    PRIVATE
        Catch2::Catch2WithMain
        signalforge_platform
        signalforge_observability
)
catch_discover_tests(platform_test DISCOVERY_MODE PRE_TEST)
