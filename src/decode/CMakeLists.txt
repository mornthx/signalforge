add_library(signalforge_decode STATIC
    placeholder.cpp
)
target_include_directories(signalforge_decode PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(signalforge_decode PUBLIC Qt6::Core)
