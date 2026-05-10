add_library(signalforge_app_ui STATIC
    main_window.cpp
    main_window.hpp
    ${CMAKE_SOURCE_DIR}/resources/qml.qrc
)
target_include_directories(signalforge_app_ui PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(signalforge_app_ui
    PUBLIC
        Qt6::Core
        Qt6::Widgets
        Qt6::Quick
        Qt6::QuickWidgets
        signalforge_frame
        signalforge_drivers
        signalforge_pipeline
        signalforge_decoder
        signalforge_buffer
        signalforge_chart
        signalforge_connection
        signalforge_session
        signalforge_replay
    PRIVATE
        signalforge_observability
        signalforge_platform
)
set_target_properties(signalforge_app_ui PROPERTIES AUTOMOC ON AUTORCC ON)

add_executable(signalforge
    main.cpp
)
target_include_directories(signalforge PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(signalforge
    PRIVATE
        Qt6::Widgets
        signalforge_app_ui
        signalforge_observability
        signalforge_platform
        signalforge_frame
        signalforge_drivers
)
