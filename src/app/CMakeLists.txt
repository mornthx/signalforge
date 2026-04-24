add_executable(signalforge
    main.cpp
    main_window.cpp
    main_window.hpp
)
target_include_directories(signalforge PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(signalforge
    PRIVATE
        Qt6::Widgets
        signalforge_observability
        signalforge_platform
        signalforge_frame
        signalforge_drivers
)
