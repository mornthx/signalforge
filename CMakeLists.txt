cmake_minimum_required(VERSION 3.22)
# M13: Version bumped to 1.0.0 for V1 release per spec §3.3 P.
# Pre-M13 alpha tags (v0.0.0-alpha.1 .. v0.0.13-alpha.1) used 0.0.x;
# v1.0.0 is the canonical V1 final tag.
project(SignalForge VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.10 REQUIRED COMPONENTS
    Core Widgets Quick QuickWidgets Network SerialPort Test
)
qt_standard_project_setup()

include(cmake/dependencies.cmake)

option(SIGNALFORGE_BENCHMARKS "Build the M3 driver benchmark executables (throughput/latency/footprint)" OFF)

enable_testing()

add_subdirectory(src/platform)
add_subdirectory(src/observability)
add_subdirectory(src/drivers)
add_subdirectory(src/frame)
add_subdirectory(src/decode)
add_subdirectory(src/buffer)
add_subdirectory(src/query)
add_subdirectory(src/workbench)
add_subdirectory(src/expression)
add_subdirectory(src/chart)
add_subdirectory(src/signal)
add_subdirectory(src/action)
add_subdirectory(src/session)
add_subdirectory(src/models)
add_subdirectory(src/utils)
add_subdirectory(src/pipeline)
add_subdirectory(src/ui_widgets)
add_subdirectory(src/ui_quick)
add_subdirectory(src/connection)
add_subdirectory(src/replay)
add_subdirectory(src/inspect)
add_subdirectory(src/dashboard)
add_subdirectory(src/app)
add_subdirectory(tools/schema_lint)
add_subdirectory(tools/expr_lint)
add_subdirectory(tools/sfreplay_inspect)
add_subdirectory(tools/profile)
add_subdirectory(tests)

# M13 S1: install rules + CPack DEB packaging for V1.0 release.
# When CPack is run via `cmake --build <build> --target package`,
# CMAKE_INSTALL_PREFIX defaults to /opt/signalforge per the cpack
# config below. Absolute install paths (/usr/share/applications/,
# /usr/share/icons/...) are honoured by CPack DEB's staging dir
# logic.
include(cmake/install.cmake)
include(cmake/cpack-deb.cmake)
