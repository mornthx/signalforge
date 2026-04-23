include(FetchContent)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)  # option() honors normal variables

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.14.1
)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.3
)

FetchContent_Declare(
    concurrentqueue
    GIT_REPOSITORY https://github.com/cameron314/concurrentqueue.git
    GIT_TAG v1.0.4
)

FetchContent_Declare(
    exprtk
    GIT_REPOSITORY https://github.com/ArashPartow/exprtk.git
    GIT_TAG 0.0.3
)

FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG 0.8.0
)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)

# sentry-native — crash reporting backend (ADR-002). Pinned to 0.7.17.
#
# SENTRY_BACKEND=crashpad is the default on Linux. sentry-native vendors
# Chromium Crashpad + mini_chromium and supplies its own CMake wrapping,
# which is the whole reason M2 switched from direct Crashpad integration
# to sentry-native. The crashpad backend gives out-of-process crash capture
# via a bundled crashpad_handler binary; minidumps land in
# SENTRY_DATABASE_PATH per architecture §14.3.
#
# SENTRY_TRANSPORT=none because architecture §14.3 forbids an upload backend
# in V1. Crashpad itself still requires libcurl headers at configure time
# (via the vendored chromium util/net code) regardless of transport setting;
# libcurl4-openssl-dev is required on the build host.
FetchContent_Declare(
    sentry_native
    GIT_REPOSITORY https://github.com/getsentry/sentry-native.git
    GIT_TAG 0.7.17
)

set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)

set(SENTRY_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SENTRY_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SENTRY_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SENTRY_BACKEND "crashpad" CACHE STRING "" FORCE)
set(SENTRY_TRANSPORT "none" CACHE STRING "" FORCE)

FetchContent_MakeAvailable(spdlog Catch2 concurrentqueue exprtk yaml-cpp nlohmann_json sentry_native)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
