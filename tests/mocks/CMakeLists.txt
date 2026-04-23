add_library(signalforge_mocks STATIC
    mock_driver.cpp
)
target_include_directories(signalforge_mocks PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(signalforge_mocks
    PUBLIC
        Qt6::Core
        signalforge_drivers
        signalforge_frame
)
