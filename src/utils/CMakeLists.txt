add_library(signalforge_utils STATIC
    mpsc_queue.cpp
)
target_include_directories(signalforge_utils PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(signalforge_utils
    PUBLIC Qt6::Core
    PRIVATE concurrentqueue
)
