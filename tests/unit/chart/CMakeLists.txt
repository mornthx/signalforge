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
