add_library(signalforge_session STATIC
    placeholder.cpp
)
target_include_directories(signalforge_session PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(signalforge_session PUBLIC Qt6::Core)
