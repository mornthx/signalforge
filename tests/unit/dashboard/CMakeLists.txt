add_executable(panel_factory_test
    panel_factory_test.cpp
)
target_link_libraries(panel_factory_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Test
        signalforge_dashboard
        signalforge_decoder
)
catch_discover_tests(panel_factory_test DISCOVERY_MODE PRE_TEST)
