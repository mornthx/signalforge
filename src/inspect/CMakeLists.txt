add_library(signalforge_inspect STATIC
    parsed_signals_view.cpp
    parsed_signals_view.hpp
    raw_frame_tap.cpp
    raw_frame_tap.hpp
    raw_packet_view.cpp
    raw_packet_view.hpp
)
target_include_directories(signalforge_inspect PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(signalforge_inspect
    PUBLIC
        Qt6::Core
        Qt6::Widgets
        signalforge_buffer
        signalforge_query
        signalforge_pipeline
    PRIVATE
        signalforge_observability
)
