add_executable(selection_model_test
    selection_model_test.cpp
)
target_link_libraries(selection_model_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        Qt6::Test
        signalforge_workbench
)
catch_discover_tests(selection_model_test DISCOVERY_MODE PRE_TEST)
