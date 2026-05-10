# M14 S1 — CI release-binary GUI smoke test target.
# Per M14-concerns C4: lives in tests/integration/gui/ for ctest
# discovery alongside the rest of the integration suite.

add_executable(test_release_binary_smoke
    test_release_binary_smoke.cpp
)
target_link_libraries(test_release_binary_smoke
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
)
target_compile_definitions(test_release_binary_smoke
    PRIVATE
        SIGNALFORGE_RELEASE_BINARY_PATH="$<TARGET_FILE:signalforge>"
        SIGNALFORGE_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
        SIGNALFORGE_M14_SMOKE_HARNESS="${CMAKE_SOURCE_DIR}/tests/integration/gui/release_binary_smoke.sh"
        SIGNALFORGE_M14_SMOKE_FIXTURE="${CMAKE_SOURCE_DIR}/tests/integration/gui/fixtures/m14_smoke.yaml"
)
# M14 S1 + Wave 1 closure: smoke now passes normally. The harness sets
# SF_F4_DIAG=1 so the chart's QSG-root contains a bright orange
# QSGSimpleRectNode that survives software-RHI rasterization under
# xvfb-run (production users never set this env var; behavior is
# unchanged). Tier A asserts non-white pixels exist — the rect
# guarantees ≥ 3600 pixels and ≥ 1 driver-attached signal node.
#
# Explicit add_test + set_tests_properties (vs catch_discover_tests):
# the catch2 helper's PROPERTIES forwarding does not reliably attach
# per-test properties to PRE_TEST-discovered tests. The smoke has
# exactly one TEST_CASE so we add it by hand with the same name.
set(M14_SMOKE_TEST_NAME "M14 S1: release binary smoke (Tier A + Tier B)")
add_test(NAME "${M14_SMOKE_TEST_NAME}"
    COMMAND test_release_binary_smoke "${M14_SMOKE_TEST_NAME}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
