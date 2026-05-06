cmake_minimum_required(VERSION 3.22)
project(SignalForge VERSION 0.0.1 LANGUAGES CXX)

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
add_subdirectory(src/signal)
add_subdirectory(src/action)
add_subdirectory(src/session)
add_subdirectory(src/models)
add_subdirectory(src/utils)
add_subdirectory(src/pipeline)
add_subdirectory(src/ui_widgets)
add_subdirectory(src/ui_quick)
add_subdirectory(src/app)
add_subdirectory(tools/schema_lint)
add_subdirectory(tests)
