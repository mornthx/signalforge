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
