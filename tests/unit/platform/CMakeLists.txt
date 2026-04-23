add_executable(platform_test
    time_source_test.cpp
    thread_utils_test.cpp
    app_paths_test.cpp
    crash_reporting_test.cpp
)
target_link_libraries(platform_test
    PRIVATE
        Catch2::Catch2WithMain
        signalforge_platform
        signalforge_observability
)

# Expose the built crashpad_handler binary path to crash_reporting_test.cpp.
# sentry-native's crashpad backend needs a valid handler binary at
# sentry_init() time; tests pass this path via CrashReporterConfig.
target_compile_definitions(platform_test
    PRIVATE
        SIGNALFORGE_TEST_CRASHPAD_HANDLER_PATH="$<TARGET_FILE:crashpad_handler>"
)

catch_discover_tests(platform_test DISCOVERY_MODE PRE_TEST)
