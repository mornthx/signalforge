add_library(signalforge_pipeline STATIC
    frame_pipeline.cpp
    frame_sink.hpp
)
target_include_directories(signalforge_pipeline PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(signalforge_pipeline
    PUBLIC
        Qt6::Core
        signalforge_drivers
        signalforge_frame
    PRIVATE
        signalforge_observability
        signalforge_platform
        signalforge_utils
)
