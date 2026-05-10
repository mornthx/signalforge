add_subdirectory(mocks)
add_subdirectory(test_only)
add_subdirectory(unit)
add_subdirectory(integration)
# M15 S1 — visual test suite (Python pytest under ctest "visual" label).
add_subdirectory(visual)
if(SIGNALFORGE_BENCHMARKS)
    add_subdirectory(benchmark)
endif()
