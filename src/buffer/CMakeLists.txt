add_library(signalforge_buffer STATIC
    signal_buffer.cpp
    signal_buffer.hpp
    signal_buffer_registry.cpp
    signal_buffer_registry.hpp
)
target_include_directories(signalforge_buffer PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(signalforge_buffer
    PUBLIC
        Qt6::Core
        signalforge_decoder
    PRIVATE
        signalforge_observability
)
