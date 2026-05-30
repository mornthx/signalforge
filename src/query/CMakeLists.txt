add_library(signalforge_query STATIC
    filter_expr.cpp
    filter_expr.hpp
)
target_include_directories(signalforge_query PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(signalforge_query
    PUBLIC
        Qt6::Core
)
