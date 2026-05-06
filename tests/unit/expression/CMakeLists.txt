add_executable(expression_smoke_test
    expression_smoke_test.cpp
)
target_link_libraries(expression_smoke_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        signalforge_expression
        signalforge_buffer
        signalforge_decoder
)
catch_discover_tests(expression_smoke_test DISCOVERY_MODE PRE_TEST)

add_executable(expression_evaluation_test
    expression_evaluation_test.cpp
)
target_link_libraries(expression_evaluation_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        signalforge_expression
        signalforge_decoder
)
catch_discover_tests(expression_evaluation_test DISCOVERY_MODE PRE_TEST)

add_executable(expression_validator_test
    expression_validator_test.cpp
)
target_link_libraries(expression_validator_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        signalforge_expression
        signalforge_decoder
)
target_compile_definitions(expression_validator_test
    PRIVATE
        SIGNALFORGE_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/tests/integration/fixtures"
)
catch_discover_tests(expression_validator_test DISCOVERY_MODE PRE_TEST)
