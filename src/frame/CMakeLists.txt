add_library(signalforge_frame STATIC
    raw_frame.cpp
)
target_include_directories(signalforge_frame PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(signalforge_frame PUBLIC Qt6::Core)
