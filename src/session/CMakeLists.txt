add_library(signalforge_session STATIC
    session_writer.cpp
    session_file_writer.cpp
    session_reader.cpp
)

set_target_properties(signalforge_session PROPERTIES AUTOMOC ON)

target_include_directories(signalforge_session PUBLIC ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(signalforge_session
    PUBLIC
        Qt6::Core
        signalforge_buffer
        signalforge_decoder
    PRIVATE
        signalforge_observability
)
