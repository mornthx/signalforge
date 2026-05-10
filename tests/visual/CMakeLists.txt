# M15 S1 — V0.2 visual test suite (per M15-concerns C4).
#
# pytest-driven Python harness; no Catch2 / no C++. Tests are
# discovered + run via ctest with the `visual` label.
#
# Each test launches the production signalforge binary headlessly
# under xvfb-run with `--capture-screenshot-after-ms` /
# `--capture-screenshot-path` flags (M15 S1), captures a PNG, and
# compares against the committed baseline under
# `tests/visual/baselines/`.
#
# CI gate: pixel-level diff only (no vision LLM per M15-concerns
# C7 public-repo security). Vision-LLM verdicts are local-only via
# CC's Read tool or the operator's optional MiMo benchmark.

# Discover all test_*.py files under tests/ and register one
# ctest target per file.
file(GLOB M15_VISUAL_TESTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_*.py")

foreach(test_file ${M15_VISUAL_TESTS})
    get_filename_component(test_name ${test_file} NAME_WE)
    # Invoke each test file as a plain Python script. The file's
    # `if __name__ == "__main__":` block defers to
    # `tests/visual/lib/runner.py`, which discovers `test_*`
    # callables in module globals + runs them. Stdlib-only — no
    # pytest dep (M15 S2 dropped pytest after CI's toolcache
    # python lacked pygments and pytest's brittle dep chain).
    add_test(
        NAME "M15-visual-${test_name}"
        COMMAND ${CMAKE_COMMAND} -E env
            "PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}"
            "SIGNALFORGE_BINARY=$<TARGET_FILE:signalforge>"
            python3 "${test_file}"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties("M15-visual-${test_name}" PROPERTIES LABELS "visual")
endforeach()
