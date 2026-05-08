add_executable(driver_lifecycle_with_mock
    driver_lifecycle_with_mock.cpp
)
target_link_libraries(driver_lifecycle_with_mock
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        Qt6::Test
        signalforge_drivers
        signalforge_frame
        signalforge_mocks
)
catch_discover_tests(driver_lifecycle_with_mock DISCOVERY_MODE PRE_TEST)

add_executable(test_app_smoke
    test_app_smoke.cpp
)
target_link_libraries(test_app_smoke
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
)
target_compile_definitions(test_app_smoke
    PRIVATE
        SIGNALFORGE_APP_BINARY="$<TARGET_FILE:signalforge>"
)
# The smoke test launches the app binary; make sure it is up to date.
add_dependencies(test_app_smoke signalforge)
catch_discover_tests(test_app_smoke DISCOVERY_MODE PRE_TEST)

add_library(signalforge_socat_fixture STATIC
    socat_fixture.cpp
    socat_fixture.hpp
)
target_link_libraries(signalforge_socat_fixture
    PUBLIC Qt6::Core
)
target_include_directories(signalforge_socat_fixture PUBLIC ${CMAKE_SOURCE_DIR})

add_executable(test_serial_driver_loopback
    test_serial_driver_loopback.cpp
)
target_link_libraries(test_serial_driver_loopback
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        Qt6::Test
        signalforge_drivers
        signalforge_frame
        signalforge_socat_fixture
)
catch_discover_tests(test_serial_driver_loopback DISCOVERY_MODE PRE_TEST PROPERTIES LABELS "socat")

add_library(signalforge_echo_server_fixture STATIC
    echo_server_fixture.cpp
    echo_server_fixture.hpp
)
target_link_libraries(signalforge_echo_server_fixture
    PUBLIC Qt6::Core Qt6::Network
)
target_include_directories(signalforge_echo_server_fixture PUBLIC ${CMAKE_SOURCE_DIR})
set_target_properties(signalforge_echo_server_fixture PROPERTIES AUTOMOC ON)

add_executable(test_tcp_driver_echo
    test_tcp_driver_echo.cpp
)
target_link_libraries(test_tcp_driver_echo
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        Qt6::Network
        Qt6::Test
        signalforge_drivers
        signalforge_frame
        signalforge_echo_server_fixture
)
catch_discover_tests(test_tcp_driver_echo DISCOVERY_MODE PRE_TEST)

add_executable(test_udp_driver_loopback
    test_udp_driver_loopback.cpp
)
target_link_libraries(test_udp_driver_loopback
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        Qt6::Network
        Qt6::Test
        signalforge_drivers
        signalforge_frame
)
catch_discover_tests(test_udp_driver_loopback DISCOVERY_MODE PRE_TEST)

add_executable(test_pipeline_driver_integration
    test_pipeline_driver_integration.cpp
)
target_link_libraries(test_pipeline_driver_integration
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        Qt6::Network
        Qt6::Test
        signalforge_pipeline
        signalforge_drivers
        signalforge_frame
)
target_compile_definitions(test_pipeline_driver_integration
    PRIVATE
        SIGNALFORGE_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)
catch_discover_tests(test_pipeline_driver_integration DISCOVERY_MODE PRE_TEST)

add_executable(test_pipeline_fanout
    test_pipeline_fanout.cpp
)
target_link_libraries(test_pipeline_fanout
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        Qt6::Network
        Qt6::Test
        signalforge_pipeline
        signalforge_drivers
        signalforge_frame
)
catch_discover_tests(test_pipeline_fanout DISCOVERY_MODE PRE_TEST)

add_executable(test_pipeline_backpressure
    test_pipeline_backpressure.cpp
)
target_link_libraries(test_pipeline_backpressure
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        Qt6::Network
        Qt6::Test
        signalforge_pipeline
        signalforge_drivers
        signalforge_frame
        signalforge_observability
)
catch_discover_tests(test_pipeline_backpressure DISCOVERY_MODE PRE_TEST)

add_executable(test_driver_error_paths
    test_driver_error_paths.cpp
)
target_link_libraries(test_driver_error_paths
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        Qt6::Network
        Qt6::Test
        signalforge_drivers
        signalforge_frame
)
target_compile_definitions(test_driver_error_paths
    PRIVATE
        SIGNALFORGE_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)
catch_discover_tests(test_driver_error_paths DISCOVERY_MODE PRE_TEST)

add_executable(test_replay_driver_skeleton
    test_replay_driver_skeleton.cpp
)
target_link_libraries(test_replay_driver_skeleton
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        Qt6::Test
        signalforge_drivers
        signalforge_frame
)
target_compile_definitions(test_replay_driver_skeleton
    PRIVATE
        SIGNALFORGE_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)
catch_discover_tests(test_replay_driver_skeleton DISCOVERY_MODE PRE_TEST)

# M5 decoder integration tests.  All consume schemas under
# ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/{valid,invalid}_schemas/.
foreach(decoder_test
    test_schema_decoder_basic
    test_schema_decoder_bit_fields
    test_schema_decoder_endianness
    test_schema_decoder_unmatched
    test_schema_validator_errors
)
    add_executable(${decoder_test} ${decoder_test}.cpp)
    target_link_libraries(${decoder_test}
        PRIVATE
            Catch2::Catch2WithMain
            Qt6::Core
            signalforge_decoder
            signalforge_frame
            signalforge_observability
            signalforge_test_only
    )
    target_compile_definitions(${decoder_test}
        PRIVATE
            SIGNALFORGE_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    )
    catch_discover_tests(${decoder_test} DISCOVERY_MODE PRE_TEST)
endforeach()

# M6 signal-buffer integration tests (spec §5.3).
foreach(buffer_test
    test_signal_buffer_round_trip
    test_signal_buffer_concurrent
    test_signal_buffer_lod
    test_signal_buffer_window_eviction
    test_signal_buffer_budget
)
    add_executable(${buffer_test} ${buffer_test}.cpp)
    target_link_libraries(${buffer_test}
        PRIVATE
            Catch2::Catch2WithMain
            Qt6::Core
            signalforge_buffer
            signalforge_decoder
            signalforge_observability
    )
    catch_discover_tests(${buffer_test} DISCOVERY_MODE PRE_TEST)
endforeach()

# M7 expression-engine integration tests (spec §5.3).
foreach(expression_test
    test_expression_engine_basic
    test_expression_engine_dependency_chain
    test_expression_engine_cycle_rejection
    test_expression_engine_type_promotion
    test_expression_engine_qstring_rejection
    test_expression_engine_restricted_syntax
)
    add_executable(${expression_test} ${expression_test}.cpp)
    target_link_libraries(${expression_test}
        PRIVATE
            Catch2::Catch2WithMain
            Qt6::Core
            signalforge_buffer
            signalforge_decoder
            signalforge_expression
            signalforge_observability
    )
    catch_discover_tests(${expression_test} DISCOVERY_MODE PRE_TEST)
endforeach()

# M8 chart-subsystem integration tests (spec §2.1-12).
# Most tests use QGuiApplication; the SignalSelector test uses
# QApplication (QtWidgets) since SignalSelector is a QWidget.
foreach(chart_test
    test_chart_basic_rendering
    test_chart_30hz_sustained
    test_chart_global_time_axis
    test_chart_lod_selection
    test_chart_signal_type_display
    test_chart_window_activation
)
    add_executable(${chart_test} ${chart_test}.cpp)
    target_link_libraries(${chart_test}
        PRIVATE
            Catch2::Catch2WithMain
            Qt6::Core
            Qt6::Gui
            Qt6::Quick
            Qt6::Test
            signalforge_buffer
            signalforge_chart
            signalforge_decoder
    )
    catch_discover_tests(${chart_test} DISCOVERY_MODE PRE_TEST)
endforeach()

add_executable(test_signal_selector_tree_population
    test_signal_selector_tree_population.cpp
)
target_link_libraries(test_signal_selector_tree_population
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        Qt6::Widgets
        signalforge_buffer
        signalforge_chart
        signalforge_decoder
)
catch_discover_tests(test_signal_selector_tree_population DISCOVERY_MODE PRE_TEST)

add_executable(test_replay_driver_full_cycle
    test_replay_driver_full_cycle.cpp
)
target_link_libraries(test_replay_driver_full_cycle
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        Qt6::Test
        signalforge_connection
        signalforge_drivers
        signalforge_frame
)
catch_discover_tests(test_replay_driver_full_cycle DISCOVERY_MODE PRE_TEST)

add_executable(test_connection_lifecycle_full_stack
    test_connection_lifecycle_full_stack.cpp
)
target_link_libraries(test_connection_lifecycle_full_stack
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        Qt6::Widgets
        Qt6::Test
        signalforge_connection
        signalforge_drivers
        signalforge_decoder
        signalforge_pipeline
        signalforge_frame
)
catch_discover_tests(test_connection_lifecycle_full_stack DISCOVERY_MODE PRE_TEST)

add_executable(test_session_full_stack_round_trip
    test_session_full_stack_round_trip.cpp
)
target_link_libraries(test_session_full_stack_round_trip
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        signalforge_session
        signalforge_buffer
        signalforge_decoder
)
catch_discover_tests(test_session_full_stack_round_trip DISCOVERY_MODE PRE_TEST)

add_executable(test_sfreplay_inspect
    test_sfreplay_inspect.cpp
)
target_link_libraries(test_sfreplay_inspect
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        signalforge_session
        signalforge_buffer
        signalforge_decoder
        nlohmann_json::nlohmann_json
)
target_compile_definitions(test_sfreplay_inspect
    PRIVATE
        SIGNALFORGE_SFREPLAY_INSPECT_BINARY="$<TARGET_FILE:sfreplay_inspect>"
)
add_dependencies(test_sfreplay_inspect sfreplay_inspect)
catch_discover_tests(test_sfreplay_inspect DISCOVERY_MODE PRE_TEST)

add_executable(test_replay_full_stack
    test_replay_full_stack.cpp
)
target_link_libraries(test_replay_full_stack
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        Qt6::Test
        signalforge_replay
        signalforge_session
        signalforge_buffer
        signalforge_decoder
)
catch_discover_tests(test_replay_full_stack DISCOVERY_MODE PRE_TEST)

add_executable(test_m12_optimization_correctness
    test_m12_optimization_correctness.cpp
)
target_link_libraries(test_m12_optimization_correctness
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        Qt6::Test
        signalforge_replay
        signalforge_session
        signalforge_buffer
        signalforge_decoder
)
catch_discover_tests(test_m12_optimization_correctness DISCOVERY_MODE PRE_TEST)
