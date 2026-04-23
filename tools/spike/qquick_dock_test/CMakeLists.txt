cmake_minimum_required(VERSION 3.22)
project(qquick_dock_test CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.10 REQUIRED COMPONENTS
    Core Widgets Quick QuickWidgets Test
)
qt_standard_project_setup()

add_executable(qquick_dock_test
    main.cpp
    main_window.cpp
    main_window.hpp
    resources.qrc
)
target_link_libraries(qquick_dock_test PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Quick
    Qt6::QuickWidgets
    Qt6::Test
)
