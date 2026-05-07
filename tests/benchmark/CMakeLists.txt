# Benchmarks are opt-in via -DSIGNALFORGE_BENCHMARKS=ON. They are not
# part of ctest and never run in CI (spec §5.7).

add_executable(bench_driver_throughput bench_driver_throughput.cpp)
target_link_libraries(bench_driver_throughput
    PRIVATE
        Qt6::Core
        Qt6::Network
        signalforge_drivers
        signalforge_frame
        signalforge_echo_server_fixture
        signalforge_socat_fixture
)

add_executable(bench_driver_latency bench_driver_latency.cpp)
target_link_libraries(bench_driver_latency
    PRIVATE
        Qt6::Core
        Qt6::Network
        signalforge_drivers
        signalforge_frame
        signalforge_echo_server_fixture
)

add_executable(bench_driver_footprint bench_driver_footprint.cpp)
target_link_libraries(bench_driver_footprint
    PRIVATE
        Qt6::Core
        Qt6::Network
        signalforge_drivers
        signalforge_frame
)

add_executable(bench_pipeline_throughput bench_pipeline_throughput.cpp)
target_link_libraries(bench_pipeline_throughput
    PRIVATE
        Qt6::Core
        Qt6::Network
        signalforge_drivers
        signalforge_frame
        signalforge_pipeline
)

add_executable(bench_decoder_throughput bench_decoder_throughput.cpp)
target_link_libraries(bench_decoder_throughput
    PRIVATE
        Qt6::Core
        signalforge_decoder
        signalforge_frame
)

add_executable(bench_signal_buffer bench_signal_buffer.cpp)
target_link_libraries(bench_signal_buffer
    PRIVATE
        Qt6::Core
        signalforge_buffer
        signalforge_decoder
        signalforge_frame
)

add_executable(bench_expression_engine bench_expression_engine.cpp)
target_link_libraries(bench_expression_engine
    PRIVATE
        Qt6::Core
        signalforge_buffer
        signalforge_decoder
        signalforge_expression
)
