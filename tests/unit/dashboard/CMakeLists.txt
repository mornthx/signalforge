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

add_executable(panels_test
    panels_test.cpp
)
target_link_libraries(panels_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        signalforge_dashboard
        signalforge_buffer
        signalforge_decoder
)
catch_discover_tests(panels_test DISCOVERY_MODE PRE_TEST)

# PlotPanel hosts qrc:/qml/ChartHost.qml, so this test binary embeds the
# shared qml.qrc itself (the app binary gets it via signalforge_app_ui).
add_executable(plot_panel_test
    plot_panel_test.cpp
    ${CMAKE_SOURCE_DIR}/resources/qml.qrc
)
set_target_properties(plot_panel_test PROPERTIES AUTORCC ON)
target_link_libraries(plot_panel_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Quick
        Qt6::QuickWidgets
        signalforge_dashboard
        signalforge_chart
        signalforge_buffer
        signalforge_decoder
)
catch_discover_tests(plot_panel_test DISCOVERY_MODE PRE_TEST)

add_executable(dashboard_test
    dashboard_test.cpp
    ${CMAKE_SOURCE_DIR}/resources/qml.qrc
)
set_target_properties(dashboard_test PROPERTIES AUTORCC ON)
target_link_libraries(dashboard_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Quick
        Qt6::QuickWidgets
        signalforge_dashboard
        signalforge_chart
        signalforge_buffer
        signalforge_decoder
)
catch_discover_tests(dashboard_test DISCOVERY_MODE PRE_TEST)

add_executable(signal_list_panel_test
    signal_list_panel_test.cpp
    ${CMAKE_SOURCE_DIR}/resources/qml.qrc
)
set_target_properties(signal_list_panel_test PROPERTIES AUTORCC ON)
target_link_libraries(signal_list_panel_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Quick
        Qt6::QuickWidgets
        signalforge_dashboard
        signalforge_chart
        signalforge_buffer
        signalforge_decoder
)
catch_discover_tests(signal_list_panel_test DISCOVERY_MODE PRE_TEST)
