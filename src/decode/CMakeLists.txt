add_library(signalforge_decoder STATIC
    decoder_interface.hpp
    logging_signal_value_sink.cpp
    logging_signal_value_sink.hpp
    schema.hpp
    schema_validator.cpp
    schema_validator.hpp
)
target_include_directories(signalforge_decoder PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(signalforge_decoder
    PUBLIC
        Qt6::Core
        signalforge_frame
        signalforge_pipeline
    PRIVATE
        signalforge_observability
        signalforge_platform
        yaml-cpp::yaml-cpp
)
