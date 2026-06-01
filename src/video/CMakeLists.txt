add_library(signalforge_video STATIC
    video_receiver.cpp
    video_view.cpp
    video_page.cpp
    video_recorder.cpp
    color_correction.cpp
    color_panel.cpp
)
target_include_directories(signalforge_video PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(signalforge_video
    PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network
    PRIVATE signalforge_platform signalforge_observability
)
