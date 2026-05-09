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
# WILL_FAIL = TRUE for S1 only. The smoke test correctly catches the
# run-4 0×0 chart-sizing bug (Tier A asserts non_white_pixels > 0;
# QQuickWidget framebuffer is currently 0×0 because Chart QQuickItem
# is sized 0×0 — see M14-progress §"Audit findings"). S2 fixes run-4
# and clears WILL_FAIL in the same commit, at which point the smoke
# becomes a normal regression test.
#
# Explicit add_test + set_tests_properties (vs catch_discover_tests):
# the catch2 helper's PROPERTIES forwarding does not reliably attach
# WILL_FAIL to the discovered test in PRE_TEST mode. The smoke has
# exactly one TEST_CASE, so we add it by hand with the same name.
set(M14_SMOKE_TEST_NAME "M14 S1: release binary smoke (Tier A + Tier B)")
add_test(NAME "${M14_SMOKE_TEST_NAME}"
    COMMAND test_release_binary_smoke "${M14_SMOKE_TEST_NAME}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
set_tests_properties("${M14_SMOKE_TEST_NAME}" PROPERTIES WILL_FAIL TRUE)
