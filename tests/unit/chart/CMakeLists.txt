add_executable(chart_smoke_test
    chart_smoke_test.cpp
)
target_link_libraries(chart_smoke_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        signalforge_chart
        signalforge_buffer
        signalforge_decoder
)
catch_discover_tests(chart_smoke_test DISCOVERY_MODE PRE_TEST)

add_executable(time_axis_manager_test
    time_axis_manager_test.cpp
)
target_link_libraries(time_axis_manager_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        Qt6::Test
        signalforge_chart
)
catch_discover_tests(time_axis_manager_test DISCOVERY_MODE PRE_TEST)

add_executable(chart_signal_lifecycle_test
    chart_signal_lifecycle_test.cpp
)
target_link_libraries(chart_signal_lifecycle_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        signalforge_chart
        signalforge_buffer
        signalforge_decoder
)
catch_discover_tests(chart_signal_lifecycle_test DISCOVERY_MODE PRE_TEST)

add_executable(chart_manager_test
    chart_manager_test.cpp
)
target_link_libraries(chart_manager_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        signalforge_chart
        signalforge_buffer
        signalforge_decoder
)
catch_discover_tests(chart_manager_test DISCOVERY_MODE PRE_TEST)

add_executable(signal_selector_tree_test
    signal_selector_tree_test.cpp
)
target_link_libraries(signal_selector_tree_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        Qt6::Widgets
        signalforge_chart
        signalforge_buffer
        signalforge_decoder
)
catch_discover_tests(signal_selector_tree_test DISCOVERY_MODE PRE_TEST)

add_executable(chart_config_persistence_test
    chart_config_persistence_test.cpp
)
target_link_libraries(chart_config_persistence_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        signalforge_chart
        signalforge_buffer
        signalforge_decoder
)
catch_discover_tests(chart_config_persistence_test DISCOVERY_MODE PRE_TEST)
