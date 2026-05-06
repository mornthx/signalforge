// src/buffer/signal_buffer.hpp
#pragma once

#include "decode/decoder_interface.hpp"  // For SignalValue, SignalType, SignalMetadata

#include <QString>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace signalforge::buffer {

/// One sample with timestamp.
struct SignalSample {
    std::chrono::steady_clock::time_point timestamp;
    signalforge::decoder::SignalValue value;
};

/// Most-recent value with staleness info.
struct LatestValue {
    signalforge::decoder::SignalValue value;
    std::chrono::steady_clock::time_point timestamp;
    std::chrono::nanoseconds age;
};

/// Per-signal configuration. Set at registration time.
struct SignalBufferConfig {
    /// Time window in seconds. Samples older than this are evicted on next write.
    double windowSeconds = 60.0;

    /// Hard cap on samples (safety). Min(windowSeconds × maxRate, capSamples) wins.
    std::size_t capSamples = 1'000'000;

    /// Whether to maintain LOD pyramid for this signal.
    /// Default: true for numeric (Int64, Double); false for Bool, String.
    /// If unset, the buffer applies the type-specific default at construction.
    std::optional<bool> lodEnabled;

    /// Estimated sample rate for budget calculation. If unset, registry uses
    /// 1000 Hz default.
    std::optional<double> estimatedRateHz;
};

/// Storage for one signal's time-series with optional LOD pyramid.
///
/// Threading: single writer (typically the decoder thread that produced the
/// signal), multi-reader (Chart UI, M10 session writer, M7 expression engine).
/// Writers do not block readers; readers do not block writers.
///
/// Lifetime: owned by `SignalBufferRegistry`. Callers do not own these
/// directly.
///
/// Freeze scope: this header is frozen at M6 close. See spec §6.1.
class SignalBuffer {
public:
    /// Construct with metadata + config. Allocates initial storage.
    SignalBuffer(const signalforge::decoder::SignalMetadata& metadata, const SignalBufferConfig& config);
    ~SignalBuffer();

    SignalBuffer(const SignalBuffer&) = delete;
    SignalBuffer& operator=(const SignalBuffer&) = delete;
    SignalBuffer(SignalBuffer&&) = delete;
    SignalBuffer& operator=(SignalBuffer&&) = delete;

    /// Append a sample. Called from the decoder thread.
    /// Thread-safe with respect to readers; not thread-safe across writers
    /// (each signal has at most one writer per the M4 pipeline topology).
    void push(std::chrono::steady_clock::time_point timestamp, const signalforge::decoder::SignalValue& value);

    /// Query a time range. Returns up to `target_sample_count` samples
    /// (decimated via LOD if needed), or all samples in range if 0.
    /// Thread-safe; called from any thread.
    [[nodiscard]] std::vector<SignalSample> queryRange(std::chrono::steady_clock::time_point t_start,
                                                       std::chrono::steady_clock::time_point t_end,
                                                       std::size_t target_sample_count = 0) const;

    /// Latest n samples (most-recent, in chronological order).
    [[nodiscard]] std::vector<SignalSample> queryLatest(std::size_t n) const;

    /// Single most-recent value with staleness info.
    /// Returns nullopt if no samples ever pushed.
    [[nodiscard]] std::optional<LatestValue> queryLatestOne() const;

    /// Total samples currently retained (not the cumulative pushed count).
    [[nodiscard]] std::size_t sampleCount() const noexcept;

    /// Cumulative samples pushed since construction (monotonic).
    [[nodiscard]] std::uint64_t totalSamplesPushed() const noexcept;

    /// Cumulative samples evicted due to time window or cap.
    [[nodiscard]] std::uint64_t totalSamplesEvicted() const noexcept;

    /// Current memory usage in bytes (raw + LOD).
    [[nodiscard]] std::size_t memoryBytes() const noexcept;

    /// Number of LOD bins currently retained at the given level.
    /// Levels are 1 (10:1), 2 (100:1), 3 (1000:1). Returns 0 for invalid
    /// levels and for signals where LOD is disabled (Bool, String).
    [[nodiscard]] std::size_t lodBinCount(int level) const noexcept;

    /// Metadata accessor.
    [[nodiscard]] const signalforge::decoder::SignalMetadata& metadata() const noexcept;

    /// Configuration accessor.
    [[nodiscard]] const SignalBufferConfig& config() const noexcept;

public:
    /// Internal: per-variant typed buffer (polymorphic; not in public API).
    /// The full definition lives in `signal_buffer.cpp`; only the
    /// forward-declared name is visible here so per-type implementations
    /// in the .cpp's anonymous namespace can inherit from it.
    /// Spec §6.2: TypedBuffer polymorphism is explicitly outside the M6
    /// freeze surface and may evolve.
    struct TypedBuffer;

private:
    std::unique_ptr<TypedBuffer> impl_;

    signalforge::decoder::SignalMetadata metadata_;
    SignalBufferConfig config_;

    // Atomic counters for cheap status queries.
    std::atomic<std::uint64_t> totalPushed_{0};
    std::atomic<std::uint64_t> totalEvicted_{0};
    std::atomic<std::size_t> currentMemoryBytes_{0};
};

}  // namespace signalforge::buffer
