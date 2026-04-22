add_library(signalforge_models STATIC
    placeholder.cpp
)
target_include_directories(signalforge_models PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(signalforge_models PUBLIC Qt6::Core)
