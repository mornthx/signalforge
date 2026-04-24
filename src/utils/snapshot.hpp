// src/utils/snapshot.hpp
#pragma once

#include <atomic>
#include <memory>
#include <utility>

namespace signalforge::utils {

/// Double-buffered snapshot for single-writer / multi-reader fan-out.
///
/// Producer calls `publish(T)` to atomically swap in a new value. Consumers
/// call `read()` to obtain the most-recently-published `T`.
///
/// Reads are lock-free and never block the writer. Writes are lock-free for
/// the producer. The previous value's lifetime extends until the last reader
/// releases its shared_ptr to it.
///
/// Thread safety: any number of readers may call `read()` concurrently; at
/// most one thread is expected to call `publish()`. Concurrent publishers
/// are not guaranteed to be safe — in practice `store()` on the atomic is
/// lock-free, but the "writer" contract is single-producer by design.
///
/// Implementation (per spec §4.4.3 and clarification 8):
/// `std::atomic<std::shared_ptr<const T>>` from C++20. Verified available
/// on GCC 13.3 (Ubuntu 24.04 default). No custom fallback — if this
/// feature breaks at build time, the symptom is a compile error here.
template <typename T> class Snapshot {
public:
    Snapshot() = default;
    ~Snapshot() = default;

    Snapshot(const Snapshot&) = delete;
    Snapshot& operator=(const Snapshot&) = delete;
    Snapshot(Snapshot&&) = delete;
    Snapshot& operator=(Snapshot&&) = delete;

    /// Writer-only. Installs a new value. Previous readers retain access
    /// to the value they last observed until their shared_ptrs drop.
    void publish(T value) {
        auto sp = std::make_shared<const T>(std::move(value));
        current_.store(std::move(sp), std::memory_order_release);
    }

    /// Safe from any thread. Returns `std::shared_ptr<const T>` — the
    /// caller holds ownership for the duration of its use; the value is
    /// guaranteed not to be destroyed while the shared_ptr is alive.
    /// Returns `nullptr` if no value has been published yet.
    [[nodiscard]] std::shared_ptr<const T> read() const {
        return current_.load(std::memory_order_acquire);
    }

private:
    std::atomic<std::shared_ptr<const T>> current_{};
};

}  // namespace signalforge::utils
