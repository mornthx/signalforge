add_library(signalforge_workbench STATIC
    selection_model.cpp
    selection_model.hpp
    signal_identity.cpp
    signal_identity.hpp
    workbench_frame.cpp
    workbench_frame.hpp
    components/activity_rail.cpp
    components/activity_rail.hpp
    components/segmented_control.cpp
    components/segmented_control.hpp
    components/status_chip.cpp
    components/status_chip.hpp
    components/section_header.cpp
    components/section_header.hpp
    components/empty_state.cpp
    components/empty_state.hpp
    components/inspector_panel.cpp
    components/inspector_panel.hpp
)
target_include_directories(signalforge_workbench PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(signalforge_workbench
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)
