add_library(signalforge_workbench STATIC
    selection_model.cpp
    selection_model.hpp
    signal_identity.cpp
    signal_identity.hpp
)
target_include_directories(signalforge_workbench PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(signalforge_workbench
    PUBLIC
        Qt6::Core
)
