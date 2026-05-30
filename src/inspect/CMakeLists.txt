add_library(signalforge_inspect STATIC
    parsed_signals_view.cpp
    parsed_signals_view.hpp
)
target_include_directories(signalforge_inspect PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(signalforge_inspect
    PUBLIC
        Qt6::Core
        Qt6::Widgets
        signalforge_buffer
        signalforge_query
    PRIVATE
        signalforge_observability
)
