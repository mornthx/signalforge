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
