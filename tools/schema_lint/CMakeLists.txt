# tools/schema_lint — standalone CLI for validating decoder schema yaml files.
#
# This subdirectory builds a single executable, `schema_lint`, that reuses
# the in-tree `signalforge_decoder` static library for `SchemaValidator`.
# It is wired into the main build via `add_subdirectory(tools/schema_lint)`
# at the project root, so it builds as part of the normal Debug / Release
# / debug-asan presets.
#
# (Spec §4.7 references the `tools/crash_test/` pattern; that pattern's
# rationale was crash-handler-specific independent dependency pinning,
# which does not apply here.  See `.claude/M5-concerns.md` entry #5 for
# the deviation rationale.)

add_executable(schema_lint main.cpp)
target_link_libraries(schema_lint
    PRIVATE
        Qt6::Core
        signalforge_decoder
        signalforge_frame
        nlohmann_json::nlohmann_json
)
