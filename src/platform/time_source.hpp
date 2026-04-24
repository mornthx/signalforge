// src/platform/time_source.hpp
#pragma once

#include <chrono>

namespace signalforge::platform {

/// Monotonic time. Use for all internal timestamps, ordering, and elapsed-time
/// computation.
///
/// Thread safety: safe from any thread. Noexcept.
[[nodiscard]] std::chrono::steady_clock::time_point monotonicNow() noexcept;

/// Wall-clock time. Use ONLY for display, file naming, and user-facing
/// formatting. Never for comparisons or ordering (wall clock can jump
/// backwards on NTP corrections or manual edits).
///
/// Thread safety: safe from any thread. Noexcept.
[[nodiscard]] std::chrono::system_clock::time_point wallClockNow() noexcept;

/// Paired snapshot of the monotonic and wall clocks captured close together.
/// Used to embed in session file headers so offline consumers can translate
/// `steady_clock` points into human-readable wall-clock time.
///
/// Value type; trivially copyable.
struct ClockOrigin {
    std::chrono::steady_clock::time_point monotonic;
    std::chrono::system_clock::time_point wall;
};

/// Capture both clocks as close together as possible. Call once at session
/// start. The two reads are not atomic at the hardware level; on a modern
/// x86 host the skew is sub-microsecond, which is well within the
/// nanosecond-resolution use case.
///
/// Thread safety: safe from any thread. Noexcept.
[[nodiscard]] ClockOrigin captureOrigin() noexcept;

}  // namespace signalforge::platform
