// src/utils/mpsc_queue.cpp
#include "utils/mpsc_queue.hpp"

#include "frame/raw_frame.hpp"

#include <concurrentqueue.h>
#include <new>
#include <utility>

namespace signalforge::utils {

template <typename T> struct MpscQueue<T>::Impl {
    moodycamel::ConcurrentQueue<T> q;
    explicit Impl(std::size_t initialCapacity) : q(initialCapacity) {}
};

template <typename T>
MpscQueue<T>::MpscQueue(std::size_t initialCapacity) : impl_(std::make_unique<Impl>(initialCapacity)) {}

template <typename T> MpscQueue<T>::~MpscQueue() = default;

template <typename T> bool MpscQueue<T>::push(T item) {
    try {
        return impl_->q.enqueue(std::move(item));
    } catch (const std::bad_alloc&) {
        return false;
    }
    // Any other exception (e.g., from T's move constructor) propagates by
    // design; per CLAUDE.md Forbidden-7 we do not swallow unknown errors.
}

template <typename T> std::optional<T> MpscQueue<T>::pop() {
    T value;
    if (impl_->q.try_dequeue(value)) {
        return std::optional<T>{std::move(value)};
    }
    return std::nullopt;
}

template <typename T> std::size_t MpscQueue<T>::sizeApprox() const noexcept {
    return impl_->q.size_approx();
}

// Explicit instantiations. Add new T entries here when another TU needs
// MpscQueue<U>; templates are not visible outside this TU otherwise.
template class MpscQueue<int>;
template class MpscQueue<signalforge::frame::RawFrame>;

}  // namespace signalforge::utils
