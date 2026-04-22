add_library(signalforge_drivers STATIC
    placeholder.cpp
)
target_include_directories(signalforge_drivers PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(signalforge_drivers PUBLIC Qt6::Core)
