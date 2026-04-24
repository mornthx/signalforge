add_library(signalforge_drivers STATIC
    driver_interface.cpp
    io_worker_base.cpp
    replay_driver.cpp
)
target_include_directories(signalforge_drivers PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(signalforge_drivers
    PUBLIC Qt6::Core signalforge_frame
    PRIVATE signalforge_platform signalforge_observability
)
