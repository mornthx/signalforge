add_library(signalforge_action STATIC
    placeholder.cpp
)
target_include_directories(signalforge_action PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(signalforge_action PUBLIC Qt6::Core)
