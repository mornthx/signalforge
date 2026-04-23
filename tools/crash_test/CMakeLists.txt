cmake_minimum_required(VERSION 3.22)
project(signalforge_crash_test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# Pin sentry-native independently of the main project so this tool can be
# built and run out of tree. Keep SENTRY_BACKEND=crashpad (matches ADR-002)
# and SENTRY_TRANSPORT=none (no upload).
FetchContent_Declare(
    sentry_native
    GIT_REPOSITORY https://github.com/getsentry/sentry-native.git
    GIT_TAG 0.7.17
)
set(SENTRY_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SENTRY_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SENTRY_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SENTRY_BACKEND "crashpad" CACHE STRING "" FORCE)
set(SENTRY_TRANSPORT "none" CACHE STRING "" FORCE)

FetchContent_MakeAvailable(sentry_native)

add_executable(crash_test main.cpp)
target_link_libraries(crash_test PRIVATE sentry)

# After build, copy the crashpad_handler binary next to crash_test so it
# can be spawned without a pre-set handler path. See README.md.
add_custom_command(TARGET crash_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:crashpad_handler>
        $<TARGET_FILE_DIR:crash_test>/crashpad_handler
    COMMENT "Staging crashpad_handler next to crash_test"
)
