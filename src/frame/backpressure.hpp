// src/frame/backpressure.hpp
#pragma once

#include <QString>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <optional>

namespace signalforge::frame {

/// Reasons a backpressure event may fire.
///
/// Values are explicit so enum round-trips and logging remain stable across
/// future additions.
enum class BackpressureReason : int {
    QueueFilling = 0,    ///< Queue crossed above the "high" watermark (default 80%)
    QueueFull = 1,       ///< Queue at capacity; drop decisions are being made
    QueueRecovered = 2,  ///< Queue fell back below the "recover" watermark (default 60%)
};

/// Observed backpressure event. Produced by `WatermarkTracker::observe()`
/// when a queue crosses a watermark threshold or is full.
///
/// Consumers per spec §3.3: the producer that receives this signal MUST
/// (1) log it via `SF_LOG_WARN` and
/// (2) update the metric `queue_watermark_<queueName>` in MetricsRegistry.
/// The producer MUST NOT forward this signal to a global broker — V1 does
/// not include a cross-queue broadcast mechanism.
struct BackpressureSignal {
    QString queueName;  ///< Stable identifier for the queue
    BackpressureReason reason = BackpressureReason::QueueFilling;
    std::uint32_t watermarkPct = 0;              ///< 0..100, rounded to integer percent
    std::uint64_t currentDepth = 0;              ///< Current item count at observation
    std::uint64_t capacity = 0;                  ///< Queue capacity
    std::chrono::steady_clock::time_point at{};  ///< When observed
};

/// Thread-safe watermark tracker. One instance per bounded queue.
///
/// The producing thread calls `observe(currentDepth, queueName)` before each
/// push. If a state-crossing is detected, the call returns a
/// `BackpressureSignal` describing the event. The caller emits / logs it
/// per the consumer contract on `BackpressureSignal`.
///
/// Thread safety: `observe()` is safe to call concurrently from multiple
/// producer threads. Across concurrent `observe()` calls that see a
/// threshold crossing, at most one returns a `QueueFilling` signal (the
/// thread whose compare-exchange wins); the others return `std::nullopt`.
/// The same holds for `QueueRecovered`.
class WatermarkTracker {
public:
    /// Construct a tracker. `capacity` is the bound of the queue being
    /// tracked; `highPct` and `recoverPct` are the upper and lower thresholds
    /// in whole-percent units. Hysteresis requires `recoverPct < highPct`.
    /// `capacity == 0` is tolerated but `observe()` will never return a
    /// signal (the queue is effectively unbounded).
    explicit WatermarkTracker(std::uint64_t capacity, std::uint32_t highPct = 80, std::uint32_t recoverPct = 60);

    /// Update with current queue depth. Returns optional signal to emit.
    ///
    /// When observe() returns a BackpressureSignal, the caller is responsible
    /// for exactly two actions:
    ///
    ///   1. Log the signal via SF_LOG_WARN (always, no filtering)
    ///   2. Update the metric "queue_watermark_<queueName>" in
    ///      MetricsRegistry via set() with the current watermarkPct
    ///
    /// The caller MUST NOT forward the signal to any global broker, pub/sub
    /// bus, or signal aggregator. Per-queue observation is the only mechanism
    /// in V1 — see spec §3.3 for rationale.
    ///
    /// Thread-safe: callable from producer thread without external locking.
    [[nodiscard]] std::optional<BackpressureSignal> observe(std::uint64_t currentDepth, const QString& queueName);

    /// Current peak watermark observed since construction or last `reset()`.
    /// Safe from any thread; value is monotonic between resets.
    [[nodiscard]] std::uint32_t peakPct() const noexcept;

    /// Reset the peak watermark and the "above high" state to initial values.
    /// Not thread-safe with concurrent `observe()` — caller quiesces producers.
    void reset() noexcept;

private:
    const std::uint64_t capacity_;
    const std::uint32_t highPct_;
    const std::uint32_t recoverPct_;
    std::atomic<std::uint32_t> peakPct_{0};
    std::atomic<bool> aboveHigh_{false};
};

}  // namespace signalforge::frame
