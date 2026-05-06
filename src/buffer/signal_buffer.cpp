// src/buffer/signal_buffer.cpp
#include "buffer/signal_buffer.hpp"

#include "observability/metrics.hpp"

#include <QString>
#include <chrono>
#include <cstdint>
#include <deque>
#include <utility>
#include <variant>

namespace signalforge::buffer {

namespace {

using signalforge::decoder::SignalMetadata;
using signalforge::decoder::SignalType;
using signalforge::decoder::SignalValue;
using signalforge::observability::Metric;
using signalforge::observability::MetricKind;
using signalforge::observability::MetricsRegistry;

constexpr auto kPrefixSamplesStored = QLatin1String("signal_buffer_samples_stored_");
constexpr auto kPrefixSamplesEvicted = QLatin1String("signal_buffer_samples_evicted_");
constexpr auto kPrefixMemoryBytes = QLatin1String("signal_buffer_memory_bytes_");

[[nodiscard]] std::chrono::steady_clock::duration toSteadyDuration(double seconds) {
    using namespace std::chrono;
    if (seconds <= 0.0) {
        return steady_clock::duration::zero();
    }
    return duration_cast<steady_clock::duration>(duration<double>(seconds));
}

}  // namespace

/// Internal polymorphic per-variant typed buffer.
///
/// Holds the (sliding-window) timestamp deque + metric pointers shared by
/// every variant; derived classes manage the typed value storage.
struct SignalBuffer::TypedBuffer {
    TypedBuffer(const SignalMetadata& meta, const SignalBufferConfig& cfg)
        : windowDuration_(toSteadyDuration(cfg.windowSeconds)), capSamples_(cfg.capSamples) {
        auto& reg = MetricsRegistry::instance();
        samplesStoredMetric_ = reg.getOrCreate(kPrefixSamplesStored + meta.id, MetricKind::Counter);
        samplesEvictedMetric_ = reg.getOrCreate(kPrefixSamplesEvicted + meta.id, MetricKind::Counter);
        memoryBytesMetric_ = reg.getOrCreate(kPrefixMemoryBytes + meta.id, MetricKind::Gauge);
    }
    virtual ~TypedBuffer() = default;

    TypedBuffer(const TypedBuffer&) = delete;
    TypedBuffer& operator=(const TypedBuffer&) = delete;

    /// Append one sample after applying time-window and cap eviction.
    /// Returns true if stored; false on type mismatch (no eviction is
    /// performed in that case — the caller's push is a no-op).
    bool push(std::chrono::steady_clock::time_point t, const SignalValue& value) {
        // 1. Time-window eviction: drop everything older than `t - window`.
        if (windowDuration_ > std::chrono::steady_clock::duration::zero()) {
            const auto cutoff = t - windowDuration_;
            while (!timestamps_.empty() && timestamps_.front() < cutoff) {
                evictFrontValue();
                timestamps_.pop_front();
                ++totalEvicted_;
            }
        }

        // 2. Type check + append the new sample.
        if (!pushValue(value)) {
            return false;
        }
        timestamps_.push_back(t);

        // 3. Cap eviction (safety): drop oldest until at cap.
        while (timestamps_.size() > capSamples_) {
            evictFrontValue();
            timestamps_.pop_front();
            ++totalEvicted_;
        }

        // 4. Update metrics.
        if (samplesStoredMetric_ != nullptr) {
            samplesStoredMetric_->add(1);
        }
        if (samplesEvictedMetric_ != nullptr) {
            samplesEvictedMetric_->set(static_cast<std::int64_t>(totalEvicted_));
        }
        if (memoryBytesMetric_ != nullptr) {
            memoryBytesMetric_->set(static_cast<std::int64_t>(memoryBytes()));
        }
        return true;
    }

    [[nodiscard]] std::size_t samplesRetained() const noexcept {
        return timestamps_.size();
    }

    [[nodiscard]] std::uint64_t totalEvicted() const noexcept {
        return totalEvicted_;
    }

    /// Total memory: the timestamp deque (approximated as size × element
    /// size) + the per-type value bytes.
    [[nodiscard]] std::size_t memoryBytes() const noexcept {
        return timestamps_.size() * sizeof(std::chrono::steady_clock::time_point) + valueMemoryBytes();
    }

    void clear() {
        timestamps_.clear();
        clearValues();
    }

protected:
    /// Append the variant value to the type-specific store. Returns false
    /// on type mismatch (variant alternative does not match this type).
    virtual bool pushValue(const SignalValue& value) = 0;

    /// Drop the oldest entry from the type-specific store. Called in
    /// lock-step with `timestamps_.pop_front()`.
    virtual void evictFrontValue() = 0;

    /// Bytes consumed by the type-specific value vector(s).
    [[nodiscard]] virtual std::size_t valueMemoryBytes() const noexcept = 0;

    virtual void clearValues() = 0;

    std::deque<std::chrono::steady_clock::time_point> timestamps_;

    Metric* samplesStoredMetric_ = nullptr;
    Metric* samplesEvictedMetric_ = nullptr;
    Metric* memoryBytesMetric_ = nullptr;

    std::chrono::steady_clock::duration windowDuration_;
    std::size_t capSamples_;
    std::uint64_t totalEvicted_ = 0;
};

namespace {

class BoolTypedBuffer final : public SignalBuffer::TypedBuffer {
public:
    using TypedBuffer::TypedBuffer;

protected:
    bool pushValue(const SignalValue& value) override {
        const auto* p = std::get_if<bool>(&value);
        if (p == nullptr) {
            return false;
        }
        // Bit index counted from the start of packed_.front() (the first word
        // may have leading evicted bits accounted for via headBitOffset_).
        const std::size_t bitIdx = headBitOffset_ + totalBits_;
        const std::size_t word = bitIdx / 64;
        const std::size_t offset = bitIdx % 64;
        while (word >= packed_.size()) {
            packed_.push_back(0);
        }
        if (*p) {
            packed_[word] |= (std::uint64_t{1} << offset);
        }
        // (no else: word was zero-initialized, so unset bits remain 0)
        ++totalBits_;
        return true;
    }

    void evictFrontValue() override {
        ++headBitOffset_;
        if (headBitOffset_ >= 64) {
            packed_.pop_front();
            headBitOffset_ -= 64;
        }
        --totalBits_;
    }

    [[nodiscard]] std::size_t valueMemoryBytes() const noexcept override {
        return packed_.size() * sizeof(std::uint64_t);
    }

    void clearValues() override {
        packed_.clear();
        headBitOffset_ = 0;
        totalBits_ = 0;
    }

private:
    std::deque<std::uint64_t> packed_;
    std::size_t headBitOffset_ = 0;
    std::size_t totalBits_ = 0;
};

class Int64TypedBuffer final : public SignalBuffer::TypedBuffer {
public:
    using TypedBuffer::TypedBuffer;

protected:
    bool pushValue(const SignalValue& value) override {
        const auto* p = std::get_if<std::int64_t>(&value);
        if (p == nullptr) {
            return false;
        }
        values_.push_back(*p);
        return true;
    }

    void evictFrontValue() override {
        values_.pop_front();
    }

    [[nodiscard]] std::size_t valueMemoryBytes() const noexcept override {
        return values_.size() * sizeof(std::int64_t);
    }

    void clearValues() override {
        values_.clear();
    }

private:
    std::deque<std::int64_t> values_;
};

class DoubleTypedBuffer final : public SignalBuffer::TypedBuffer {
public:
    using TypedBuffer::TypedBuffer;

protected:
    bool pushValue(const SignalValue& value) override {
        const auto* p = std::get_if<double>(&value);
        if (p == nullptr) {
            return false;
        }
        values_.push_back(*p);
        return true;
    }

    void evictFrontValue() override {
        values_.pop_front();
    }

    [[nodiscard]] std::size_t valueMemoryBytes() const noexcept override {
        return values_.size() * sizeof(double);
    }

    void clearValues() override {
        values_.clear();
    }

private:
    std::deque<double> values_;
};

class StringTypedBuffer final : public SignalBuffer::TypedBuffer {
public:
    using TypedBuffer::TypedBuffer;

protected:
    bool pushValue(const SignalValue& value) override {
        const auto* p = std::get_if<QString>(&value);
        if (p == nullptr) {
            return false;
        }
        values_.push_back(*p);
        return true;
    }

    void evictFrontValue() override {
        values_.pop_front();
    }

    [[nodiscard]] std::size_t valueMemoryBytes() const noexcept override {
        return values_.size() * sizeof(QString);
    }

    void clearValues() override {
        values_.clear();
    }

private:
    std::deque<QString> values_;
};

[[nodiscard]] std::unique_ptr<SignalBuffer::TypedBuffer> makeTypedBuffer(const SignalMetadata& meta,
                                                                         const SignalBufferConfig& cfg) {
    switch (meta.type) {
    case SignalType::Bool:
        return std::make_unique<BoolTypedBuffer>(meta, cfg);
    case SignalType::Int64:
        return std::make_unique<Int64TypedBuffer>(meta, cfg);
    case SignalType::Double:
        return std::make_unique<DoubleTypedBuffer>(meta, cfg);
    case SignalType::String:
        return std::make_unique<StringTypedBuffer>(meta, cfg);
    }
    return nullptr;  // unreachable: enum is exhaustive
}

}  // namespace

SignalBuffer::SignalBuffer(const SignalMetadata& metadata, const SignalBufferConfig& config)
    : impl_(makeTypedBuffer(metadata, config)), metadata_(metadata), config_(config) {}

SignalBuffer::~SignalBuffer() = default;

void SignalBuffer::push(std::chrono::steady_clock::time_point timestamp, const SignalValue& value) {
    if (impl_ == nullptr) {
        return;
    }
    if (!impl_->push(timestamp, value)) {
        return;
    }
    totalPushed_.fetch_add(1, std::memory_order_relaxed);
    totalEvicted_.store(impl_->totalEvicted(), std::memory_order_relaxed);
    currentMemoryBytes_.store(impl_->memoryBytes(), std::memory_order_relaxed);
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
    return impl_ != nullptr ? impl_->samplesRetained() : 0;
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

const SignalMetadata& SignalBuffer::metadata() const noexcept {
    return metadata_;
}

const SignalBufferConfig& SignalBuffer::config() const noexcept {
    return config_;
}

}  // namespace signalforge::buffer
