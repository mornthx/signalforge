add_library(signalforge_connection STATIC
    connection.cpp
    connection_manager.cpp
    connection_dialog.cpp
    connection_list_widget.cpp
    connection_status_widget.cpp
)

set_target_properties(signalforge_connection PROPERTIES AUTOMOC ON)

target_include_directories(signalforge_connection PUBLIC ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(signalforge_connection
    PUBLIC
        Qt6::Core
        Qt6::Widgets
        Qt6::SerialPort
        signalforge_drivers
        signalforge_decoder
        signalforge_frame
        signalforge_pipeline
    PRIVATE
        signalforge_observability
        yaml-cpp::yaml-cpp
)
