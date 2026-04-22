add_library(signalforge_signal STATIC
    placeholder.cpp
)
target_include_directories(signalforge_signal PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(signalforge_signal PUBLIC Qt6::Core)
