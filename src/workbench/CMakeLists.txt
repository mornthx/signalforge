add_library(signalforge_workbench STATIC
    selection_model.cpp
    selection_model.hpp
)
target_include_directories(signalforge_workbench PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(signalforge_workbench
    PUBLIC
        Qt6::Core
)
