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

add_executable(signal_identity_test
    signal_identity_test.cpp
)
target_link_libraries(signal_identity_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        signalforge_workbench
)
catch_discover_tests(signal_identity_test DISCOVERY_MODE PRE_TEST)

add_executable(components_test
    components_test.cpp
)
target_link_libraries(components_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Test
        signalforge_workbench
)
catch_discover_tests(components_test DISCOVERY_MODE PRE_TEST)

add_executable(workbench_frame_test
    workbench_frame_test.cpp
)
target_link_libraries(workbench_frame_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Test
        signalforge_workbench
)
catch_discover_tests(workbench_frame_test DISCOVERY_MODE PRE_TEST)
