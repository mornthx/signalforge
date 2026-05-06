// src/buffer/signal_buffer.cpp
#include "buffer/signal_buffer.hpp"

#include "observability/metrics.hpp"

#include <QString>
#include <cstdint>
#include <utility>
#include <variant>
#include <vector>

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

}  // namespace

/// Internal polymorphic per-variant typed buffer.
///
/// Holds the timestamp vector + metric pointers shared by every variant;
/// derived classes manage the typed value storage.
struct SignalBuffer::TypedBuffer {
    explicit TypedBuffer(const SignalMetadata& meta) {
        auto& reg = MetricsRegistry::instance();
        samplesStoredMetric_ = reg.getOrCreate(kPrefixSamplesStored + meta.id, MetricKind::Counter);
        samplesEvictedMetric_ = reg.getOrCreate(kPrefixSamplesEvicted + meta.id, MetricKind::Counter);
        memoryBytesMetric_ = reg.getOrCreate(kPrefixMemoryBytes + meta.id, MetricKind::Gauge);
    }
    virtual ~TypedBuffer() = default;

    TypedBuffer(const TypedBuffer&) = delete;
    TypedBuffer& operator=(const TypedBuffer&) = delete;

    /// Append one sample. Returns true if stored; false on type mismatch
    /// (caller should not increment its push counter on a false return).
    bool push(std::chrono::steady_clock::time_point t, const SignalValue& value) {
        if (!pushValue(value)) {
            return false;
        }
        timestamps_.push_back(t);
        if (samplesStoredMetric_ != nullptr) {
            samplesStoredMetric_->add(1);
        }
        if (memoryBytesMetric_ != nullptr) {
            memoryBytesMetric_->set(static_cast<std::int64_t>(memoryBytes()));
        }
        return true;
    }

    [[nodiscard]] std::size_t samplesRetained() const noexcept {
        return timestamps_.size();
    }

    /// Total memory: the timestamp vector capacity + the per-type value bytes.
    [[nodiscard]] std::size_t memoryBytes() const noexcept {
        return timestamps_.capacity() * sizeof(std::chrono::steady_clock::time_point) + valueMemoryBytes();
    }

    void clear() {
        timestamps_.clear();
        clearValues();
    }

protected:
    /// Append the variant value to the type-specific store. Returns false
    /// on type mismatch (variant alternative does not match this type).
    virtual bool pushValue(const SignalValue& value) = 0;

    /// Bytes consumed by the type-specific value vector(s).
    [[nodiscard]] virtual std::size_t valueMemoryBytes() const noexcept = 0;

    virtual void clearValues() = 0;

    std::vector<std::chrono::steady_clock::time_point> timestamps_;

    Metric* samplesStoredMetric_ = nullptr;
    Metric* samplesEvictedMetric_ = nullptr;
    Metric* memoryBytesMetric_ = nullptr;
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
        const std::size_t bitIdx = timestamps_.size();
        const std::size_t word = bitIdx / 64;
        const std::size_t bit = bitIdx % 64;
        if (word >= packed_.size()) {
            packed_.push_back(0);
        }
        if (*p) {
            packed_[word] |= (std::uint64_t{1} << bit);
        }
        return true;
    }

    [[nodiscard]] std::size_t valueMemoryBytes() const noexcept override {
        return packed_.capacity() * sizeof(std::uint64_t);
    }

    void clearValues() override {
        packed_.clear();
    }

private:
    std::vector<std::uint64_t> packed_;
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

    [[nodiscard]] std::size_t valueMemoryBytes() const noexcept override {
        return values_.capacity() * sizeof(std::int64_t);
    }

    void clearValues() override {
        values_.clear();
    }

private:
    std::vector<std::int64_t> values_;
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

    [[nodiscard]] std::size_t valueMemoryBytes() const noexcept override {
        return values_.capacity() * sizeof(double);
    }

    void clearValues() override {
        values_.clear();
    }

private:
    std::vector<double> values_;
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

    [[nodiscard]] std::size_t valueMemoryBytes() const noexcept override {
        return values_.capacity() * sizeof(QString);
    }

    void clearValues() override {
        values_.clear();
    }

private:
    std::vector<QString> values_;
};

[[nodiscard]] std::unique_ptr<SignalBuffer::TypedBuffer> makeTypedBuffer(const SignalMetadata& meta) {
    switch (meta.type) {
    case SignalType::Bool:
        return std::make_unique<BoolTypedBuffer>(meta);
    case SignalType::Int64:
        return std::make_unique<Int64TypedBuffer>(meta);
    case SignalType::Double:
        return std::make_unique<DoubleTypedBuffer>(meta);
    case SignalType::String:
        return std::make_unique<StringTypedBuffer>(meta);
    }
    return nullptr;  // unreachable: enum is exhaustive
}

}  // namespace

SignalBuffer::SignalBuffer(const SignalMetadata& metadata, const SignalBufferConfig& config)
    : impl_(makeTypedBuffer(metadata)), metadata_(metadata), config_(config) {}

SignalBuffer::~SignalBuffer() = default;

void SignalBuffer::push(std::chrono::steady_clock::time_point timestamp, const SignalValue& value) {
    if (impl_ == nullptr) {
        return;
    }
    if (!impl_->push(timestamp, value)) {
        return;
    }
    totalPushed_.fetch_add(1, std::memory_order_relaxed);
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
