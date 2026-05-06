// src/buffer/signal_buffer.cpp
#include "buffer/signal_buffer.hpp"

namespace signalforge::buffer {

/// Internal polymorphic per-variant typed buffer.
///
/// S1 establishes only the empty interface; the per-type implementations
/// (`BoolTypedBuffer`, `Int64TypedBuffer`, `DoubleTypedBuffer`,
/// `StringTypedBuffer`) land in S2.
struct SignalBuffer::TypedBuffer {
    virtual ~TypedBuffer() = default;
};

SignalBuffer::SignalBuffer(const signalforge::decoder::SignalMetadata& metadata, const SignalBufferConfig& config)
    : metadata_(metadata), config_(config) {
    // S1 constructs the buffer with no operational backing yet — the typed
    // implementation is plugged in during S2.
}

SignalBuffer::~SignalBuffer() = default;

void SignalBuffer::push(std::chrono::steady_clock::time_point /*timestamp*/,
                        const signalforge::decoder::SignalValue& /*value*/) {
    // S2 wires the variant dispatch + typed-buffer push.
}

std::vector<SignalSample> SignalBuffer::queryRange(std::chrono::steady_clock::time_point /*t_start*/,
                                                   std::chrono::steady_clock::time_point /*t_end*/,
                                                   std::size_t /*target_sample_count*/) const {
    // S6 wires the query path.
    return {};
}

std::vector<SignalSample> SignalBuffer::queryLatest(std::size_t /*n*/) const {
    // S6 wires the query path.
    return {};
}

std::optional<LatestValue> SignalBuffer::queryLatestOne() const {
    // S6 wires the query path.
    return std::nullopt;
}

std::size_t SignalBuffer::sampleCount() const noexcept {
    return 0;
}

std::uint64_t SignalBuffer::totalSamplesPushed() const noexcept {
    return totalPushed_.load(std::memory_order_relaxed);
}

std::uint64_t SignalBuffer::totalSamplesEvicted() const noexcept {
    return totalEvicted_.load(std::memory_order_relaxed);
}

std::size_t SignalBuffer::memoryBytes() const noexcept {
    return currentMemoryBytes_.load(std::memory_order_relaxed);
}

const signalforge::decoder::SignalMetadata& SignalBuffer::metadata() const noexcept {
    return metadata_;
}

const SignalBufferConfig& SignalBuffer::config() const noexcept {
    return config_;
}

}  // namespace signalforge::buffer
