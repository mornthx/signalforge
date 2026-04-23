// src/frame/backpressure.cpp
#include "frame/backpressure.hpp"

#include <algorithm>

namespace signalforge::frame {

namespace {

std::uint32_t percentOf(std::uint64_t depth, std::uint64_t capacity) noexcept {
    if (capacity == 0) {
        return 0;
    }
    const std::uint64_t pct = (depth * 100) / capacity;
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(pct, 100));
}

}  // namespace

WatermarkTracker::WatermarkTracker(std::uint64_t capacity, std::uint32_t highPct, std::uint32_t recoverPct)
    : capacity_(capacity), highPct_(highPct), recoverPct_(recoverPct) {}

std::optional<BackpressureSignal> WatermarkTracker::observe(std::uint64_t currentDepth, const QString& queueName) {
    // Effectively-unbounded queue: no signal ever fires.
    if (capacity_ == 0) {
        return std::nullopt;
    }

    const std::uint32_t pct = percentOf(currentDepth, capacity_);

    // Peak is monotonic: update via CAS loop if current pct exceeds recorded peak.
    std::uint32_t observedPeak = peakPct_.load(std::memory_order_relaxed);
    while (pct > observedPeak && !peakPct_.compare_exchange_weak(observedPeak, pct, std::memory_order_relaxed)) {
        // observedPeak refreshed by failed CAS; retry.
    }

    BackpressureSignal sig;
    sig.queueName = queueName;
    sig.watermarkPct = pct;
    sig.currentDepth = currentDepth;
    sig.capacity = capacity_;
    sig.at = std::chrono::steady_clock::now();

    // QueueFull: depth has reached capacity. Fired on every observe at
    // capacity because this signals an active drop decision, not a state
    // transition. The aboveHigh_ flag is set as a side effect so a later
    // fall below recoverPct_ can produce a QueueRecovered.
    if (currentDepth >= capacity_) {
        aboveHigh_.store(true, std::memory_order_release);
        sig.reason = BackpressureReason::QueueFull;
        return sig;
    }

    // QueueFilling: first observation crossing above highPct_. Multiple
    // concurrent producers race, and only the thread that wins the CAS
    // returns the signal; the others return nullopt.
    if (pct >= highPct_) {
        bool expected = false;
        if (aboveHigh_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            sig.reason = BackpressureReason::QueueFilling;
            return sig;
        }
        return std::nullopt;
    }

    // QueueRecovered: crossing below recoverPct_ after being above high.
    if (pct < recoverPct_) {
        bool expected = true;
        if (aboveHigh_.compare_exchange_strong(expected, false, std::memory_order_acq_rel)) {
            sig.reason = BackpressureReason::QueueRecovered;
            return sig;
        }
        return std::nullopt;
    }

    // In the hysteresis band [recoverPct_, highPct_): no signal.
    return std::nullopt;
}

std::uint32_t WatermarkTracker::peakPct() const noexcept {
    return peakPct_.load(std::memory_order_relaxed);
}

void WatermarkTracker::reset() noexcept {
    peakPct_.store(0, std::memory_order_relaxed);
    aboveHigh_.store(false, std::memory_order_relaxed);
}

}  // namespace signalforge::frame
