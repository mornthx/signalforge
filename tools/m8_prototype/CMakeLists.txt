# tools/m8_prototype — exploratory benchmark for M8's chart subsystem.
#
# Opt-in via -DSIGNALFORGE_M8_PROTOTYPE=ON; default OFF so it does not
# touch normal builds. Lives on the prototype/m8-perf branch only and
# is not merged to main.
#
# Architecture: a single-window QQuickItem-driven scene with N
# LineChartItem instances, each rendering a private QSGGeometryNode.
# Pure C++ — no QML files. Scenarios are selected via --scenario N.

find_package(Qt6 6.10 REQUIRED COMPONENTS Core Gui Quick QuickWidgets)

add_executable(m8_prototype
    main.cpp
    line_chart_item.cpp
    line_chart_item.hpp
    scenario_runner.cpp
    scenario_runner.hpp
)

set_target_properties(m8_prototype PROPERTIES
    AUTOMOC ON
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)

target_link_libraries(m8_prototype
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::QuickWidgets
        signalforge_buffer
        signalforge_decoder
)

target_include_directories(m8_prototype
    PRIVATE
        ${CMAKE_SOURCE_DIR}
)
