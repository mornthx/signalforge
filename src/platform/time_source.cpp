// src/platform/time_source.cpp
#include "platform/time_source.hpp"

namespace signalforge::platform {

std::chrono::steady_clock::time_point monotonicNow() noexcept {
    return std::chrono::steady_clock::now();
}

std::chrono::system_clock::time_point wallClockNow() noexcept {
    return std::chrono::system_clock::now();
}

ClockOrigin captureOrigin() noexcept {
    ClockOrigin origin;
    origin.monotonic = std::chrono::steady_clock::now();
    origin.wall = std::chrono::system_clock::now();
    return origin;
}

}  // namespace signalforge::platform
