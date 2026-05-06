add_library(signalforge_test_only STATIC
    logging_signal_value_sink.cpp
)
target_include_directories(signalforge_test_only PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(signalforge_test_only
    PUBLIC
        Qt6::Core
        signalforge_decoder
    PRIVATE
        signalforge_observability
)
