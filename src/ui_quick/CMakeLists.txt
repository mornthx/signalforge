add_library(signalforge_ui_quick STATIC
    placeholder.cpp
)
target_include_directories(signalforge_ui_quick PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(signalforge_ui_quick PUBLIC Qt6::Core Qt6::Quick Qt6::QuickWidgets)
