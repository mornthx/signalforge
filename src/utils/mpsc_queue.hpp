// src/utils/mpsc_queue.hpp
#pragma once

#include <cstddef>
#include <memory>
#include <optional>

namespace signalforge::utils {

/// Multi-producer / single-consumer queue wrapping
/// `moodycamel::ConcurrentQueue`. Thin adapter so callers do not depend
/// directly on moodycamel headers.
///
/// Thread safety: multiple producers may call `push()` concurrently;
/// exactly one thread may call `pop()`. `sizeApprox()` is safe from any
/// thread but value is approximate.
///
/// Bounded-vs-unbounded: moodycamel's queue grows dynamically. `push()`
/// returns `false` only on allocation failure (swallowed `std::bad_alloc`);
/// any other exception from `T`'s move constructor propagates. This matches
/// CLAUDE.md Forbidden-7 "no swallowed exceptions" — only `bad_alloc` is
/// swallowed and converted to `false`, which matches the declared contract.
///
/// Instantiation: explicit template instantiations for supported `T` live
/// in `mpsc_queue.cpp`. The template must be instantiated for any new `T`
/// before first use at link time; see the end of `mpsc_queue.cpp`.
template <typename T> class MpscQueue {
public:
    explicit MpscQueue(std::size_t initialCapacity = 4096);
    ~MpscQueue();

    MpscQueue(const MpscQueue&) = delete;
    MpscQueue& operator=(const MpscQueue&) = delete;
    MpscQueue(MpscQueue&&) = delete;
    MpscQueue& operator=(MpscQueue&&) = delete;

    /// Multi-producer push. Returns `false` only on allocation failure.
    /// Any other exception from `T`'s move constructor propagates.
    [[nodiscard]] bool push(T item);

    /// Single-consumer pop. Returns `std::nullopt` when queue is empty.
    [[nodiscard]] std::optional<T> pop();

    /// Approximate size. Value may lag; safe from any thread.
    [[nodiscard]] std::size_t sizeApprox() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace signalforge::utils
