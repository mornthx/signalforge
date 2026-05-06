add_library(signalforge_app_ui STATIC
    connection_manager.cpp
    connection_manager.hpp
    main_window.cpp
    main_window.hpp
)
target_include_directories(signalforge_app_ui PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(signalforge_app_ui
    PUBLIC
        Qt6::Core
        Qt6::Widgets
        signalforge_frame
        signalforge_drivers
        signalforge_pipeline
        signalforge_decoder
        signalforge_buffer
    PRIVATE
        signalforge_observability
        signalforge_platform
)
set_target_properties(signalforge_app_ui PROPERTIES AUTOMOC ON)

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
