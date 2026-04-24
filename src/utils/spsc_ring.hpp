// src/utils/spsc_ring.hpp
#pragma once

#include <atomic>
#include <cstddef>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace signalforge::utils {

namespace detail {

inline constexpr std::size_t kCacheLine = 64;

// Round x up to the next power of 2. Returns 1 for x == 0.
inline std::size_t nextPow2(std::size_t x) noexcept {
    if (x <= 1) {
        return 1;
    }
    std::size_t p = 1;
    while (p < x) {
        p <<= 1;
    }
    return p;
}

}  // namespace detail

/// Single-producer / single-consumer lock-free ring buffer.
///
/// Thread safety: exactly one thread may call `push()`; exactly one thread
/// (possibly different) may call `pop()`. Read-only observers (`size`,
/// `capacity`, `peakSize`) are safe from any thread but return approximate
/// values.
///
/// The buffer is bounded; `push()` returns false when full. Dropping or
/// overwriting is the caller's decision, not the ring's.
///
/// `push(T item)` takes `item` by value. On both success and failure paths
/// `item` has been moved from (the by-value parameter consumed the caller's
/// value); on success it is moved into internal storage, on failure it is
/// discarded at scope exit. Callers needing to retry with the same item
/// should copy before calling `push`.
template <typename T> class SpscRing {
public:
    /// `capacity` must be > 0. The internal buffer is rounded up to the
    /// next power of two plus one slot (to distinguish empty from full via
    /// index comparison), so the effective usable capacity is the rounded
    /// power of two. For `capacity == 1` the internal buffer is size 2
    /// (usable 1), for `capacity == 5` it becomes size 8 (usable 7), etc.
    explicit SpscRing(std::size_t capacity) {
        const std::size_t usable = detail::nextPow2(capacity);
        storageSize_ = usable == 0 ? 2 : (usable << 1);  // +1 sentinel slot; round to pow2
        mask_ = storageSize_ - 1;
        buffer_.resize(storageSize_);
    }
    ~SpscRing() = default;

    SpscRing(const SpscRing&) = delete;
    SpscRing& operator=(const SpscRing&) = delete;
    SpscRing(SpscRing&&) = delete;
    SpscRing& operator=(SpscRing&&) = delete;

    /// Producer-only. Returns `false` if the ring is full; in that case
    /// `item` is discarded. On both return paths the caller's original
    /// object is in a moved-from state per pass-by-value semantics.
    [[nodiscard]] bool push(T item) {
        const std::size_t t = tail_.load(std::memory_order_relaxed);
        const std::size_t next = (t + 1) & mask_;
        if (next == head_.load(std::memory_order_acquire)) {
            return false;  // full; item destructs at scope end
        }
        buffer_[t] = std::move(item);
        tail_.store(next, std::memory_order_release);
        // Update peak (producer-only writer to peak_).
        const std::size_t newSize = (next + storageSize_ - head_.load(std::memory_order_relaxed)) & mask_;
        std::size_t observedPeak = peak_.load(std::memory_order_relaxed);
        while (newSize > observedPeak &&
               !peak_.compare_exchange_weak(observedPeak, newSize, std::memory_order_relaxed)) {
            // observedPeak refreshed; retry.
        }
        return true;
    }

    /// Consumer-only. Returns `std::nullopt` if the ring is empty.
    [[nodiscard]] std::optional<T> pop() {
        const std::size_t h = head_.load(std::memory_order_relaxed);
        if (h == tail_.load(std::memory_order_acquire)) {
            return std::nullopt;
        }
        T value = std::move(buffer_[h]);
        buffer_[h] = T{};  // drop any residual payload so destructors run
        head_.store((h + 1) & mask_, std::memory_order_release);
        return value;
    }

    /// Approximate current size (tail - head mod storageSize). Safe from
    /// any thread; the value may be stale by the time it's read.
    [[nodiscard]] std::size_t size() const noexcept {
        const std::size_t h = head_.load(std::memory_order_acquire);
        const std::size_t t = tail_.load(std::memory_order_acquire);
        return (t + storageSize_ - h) & mask_;
    }

    /// Maximum number of items the ring can hold simultaneously.
    [[nodiscard]] std::size_t capacity() const noexcept {
        return storageSize_ - 1;
    }

    /// Peak size observed since construction. Monotonic until the object
    /// is destroyed.
    [[nodiscard]] std::size_t peakSize() const noexcept {
        return peak_.load(std::memory_order_relaxed);
    }

private:
    std::size_t storageSize_ = 0;  // Power of two; number of slots in buffer_
    std::size_t mask_ = 0;         // storageSize_ - 1
    std::vector<T> buffer_;
    alignas(detail::kCacheLine) std::atomic<std::size_t> head_{0};
    alignas(detail::kCacheLine) std::atomic<std::size_t> tail_{0};
    alignas(detail::kCacheLine) std::atomic<std::size_t> peak_{0};
};

}  // namespace signalforge::utils
