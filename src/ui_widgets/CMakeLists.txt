add_library(signalforge_ui_widgets STATIC
    placeholder.cpp
)
target_include_directories(signalforge_ui_widgets PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(signalforge_ui_widgets PUBLIC Qt6::Core Qt6::Widgets)
