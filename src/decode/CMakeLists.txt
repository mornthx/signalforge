add_library(signalforge_decoder STATIC
    decoder_interface.hpp
    decoder_registrar.cpp
    decoder_registrar.hpp
    schema.hpp
    schema_decoder.cpp
    schema_decoder.hpp
    schema_validator.cpp
    schema_validator.hpp
)
set_target_properties(signalforge_decoder PROPERTIES AUTOMOC ON)
target_include_directories(signalforge_decoder PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(signalforge_decoder
    PUBLIC
        Qt6::Core
        signalforge_frame
        signalforge_pipeline
    PRIVATE
        signalforge_observability
        signalforge_platform
        signalforge_buffer
        yaml-cpp::yaml-cpp
)
