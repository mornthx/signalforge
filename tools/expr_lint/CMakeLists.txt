# tools/expr_lint — standalone CLI for validating expression yaml files.
#
# Wires into the main build via `add_subdirectory(tools/expr_lint)` at the
# project root, so it builds as part of the normal Debug / Release /
# debug-asan presets. Mirrors `tools/schema_lint/` (M5).

add_executable(expr_lint main.cpp)
target_link_libraries(expr_lint
    PRIVATE
        Qt6::Core
        signalforge_decoder
        signalforge_expression
        nlohmann_json::nlohmann_json
)
