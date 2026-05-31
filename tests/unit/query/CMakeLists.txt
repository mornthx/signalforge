add_executable(filter_expr_test
    filter_expr_test.cpp
)
target_link_libraries(filter_expr_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        signalforge_query
)
catch_discover_tests(filter_expr_test DISCOVERY_MODE PRE_TEST)
