# tools/sfreplay_inspect — read-only inspector for SFREPLAY v1 files (M10).
#
# Mirrors the M5 schema_lint and M7 expr_lint pattern: a standalone
# CLI that reuses Qt6::Core + nlohmann_json, no in-tree library
# dependency (so it doubles as a third-party reference parser of the
# format spec at docs/format/sfreplay-v1.md).

add_executable(sfreplay_inspect main.cpp)
target_link_libraries(sfreplay_inspect
    PRIVATE
        Qt6::Core
        nlohmann_json::nlohmann_json
)
