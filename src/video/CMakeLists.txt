add_library(signalforge_video STATIC
    video_receiver.cpp
)
target_include_directories(signalforge_video PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(signalforge_video
    PUBLIC Qt6::Core Qt6::Gui Qt6::Network
    PRIVATE signalforge_platform signalforge_observability
)
