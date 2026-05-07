add_library(signalforge_chart STATIC
    chart.cpp
    chart.hpp
    chart_manager.cpp
    chart_manager.hpp
    signal_selector.cpp
    signal_selector.hpp
    time_axis_manager.cpp
    time_axis_manager.hpp
)
set_target_properties(signalforge_chart PROPERTIES AUTOMOC ON)
target_include_directories(signalforge_chart PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(signalforge_chart
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::Widgets
        signalforge_buffer
        signalforge_decoder
    PRIVATE
        signalforge_observability
)
