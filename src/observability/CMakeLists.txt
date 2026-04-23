add_library(signalforge_observability STATIC
    logging.cpp
    metrics.cpp
)
target_include_directories(signalforge_observability PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(signalforge_observability
    PUBLIC Qt6::Core spdlog::spdlog
)
target_compile_definitions(signalforge_observability
    PUBLIC SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE
)
