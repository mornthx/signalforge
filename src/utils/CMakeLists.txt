add_library(signalforge_utils INTERFACE)
target_include_directories(signalforge_utils INTERFACE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(signalforge_utils INTERFACE Qt6::Core)
