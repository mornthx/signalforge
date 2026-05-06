add_library(signalforge_expression STATIC
    expression.cpp
    expression.hpp
    expression_engine.cpp
    expression_engine.hpp
    expression_validator.cpp
    expression_validator.hpp
)
set_target_properties(signalforge_expression PROPERTIES AUTOMOC ON)
target_include_directories(signalforge_expression PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(signalforge_expression
    PUBLIC
        Qt6::Core
        signalforge_buffer
        signalforge_decoder
    PRIVATE
        signalforge_observability
        yaml-cpp::yaml-cpp
)
# exprtk is a single-header library; expose its include directory only
# to this library's TUs (PIMPL keeps it out of the public headers).
target_include_directories(signalforge_expression PRIVATE ${exprtk_SOURCE_DIR})
