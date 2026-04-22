add_library(signalforge_platform STATIC
    placeholder.cpp
)
target_include_directories(signalforge_platform PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(signalforge_platform PUBLIC Qt6::Core)
