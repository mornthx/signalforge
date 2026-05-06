// src/buffer/signal_buffer.cpp
#include "buffer/signal_buffer.hpp"

#include "observability/metrics.hpp"

#include <QString>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <deque>
#include <memory>
#include <type_traits>
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
constexpr auto kPrefixPublishes = QLatin1String("signal_buffer_publishes_");

/// Default writer publish cadence (samples per publish). Spec §3.5 / §4.4
/// default. Time-based fallback (every 1 ms) is deferred to a future
/// tuning pass per plan §S4 note.
constexpr int kDefaultPublishCadence = 100;

/// LOD bin sizes per spec §3.4. Three decimation levels above raw.
constexpr std::uint64_t kLodBinSize1 = 10;
constexpr std::uint64_t kLodBinSize2 = 100;
constexpr std::uint64_t kLodBinSize3 = 1000;

[[nodiscard]] std::chrono::steady_clock::duration toSteadyDuration(double seconds) {
    using namespace std::chrono;
    if (seconds <= 0.0) {
        return steady_clock::duration::zero();
    }
    return duration_cast<steady_clock::duration>(duration<double>(seconds));
}

}  // namespace

/// LOD aggregate: min/max envelope plus the bin's time bounds.
template <typename T> struct LodBin {
    T min_val;
    T max_val;
    std::chrono::steady_clock::time_point t_start;
    std::chrono::steady_clock::time_point t_end;
};

namespace {

/// Per-LOD-level bookkeeping. `binSize` is fixed (10, 100, 1000); `bins`
/// holds completed aggregates; `firstBinIndex` is the cumulative index
/// (in bin-units) of `bins.front()`; `nextBinToEmit` is the cumulative
/// bin index of the bin that the next emit will produce.
template <typename T> struct LodLevel {
    std::uint64_t binSize = 0;
    std::deque<LodBin<T>> bins;
    std::uint64_t firstBinIndex = 0;
    std::uint64_t nextBinToEmit = 0;
};

/// NaN-safe min/max accumulator. For integer T, NaN check compiles out.
/// `hasValue` becomes true on the first non-NaN sample; subsequent samples
/// update min/max. If every sample in a bin is NaN, mn/mx are left as
/// the first sample's value (which is NaN) — the consumer will see a
/// sentinel-NaN bin.
template <typename T> void accumulateMinMax(T value, T& mn, T& mx, bool& hasValue) {
    if constexpr (std::is_floating_point_v<T>) {
        if (std::isnan(value)) {
            return;
        }
    }
    if (!hasValue) {
        mn = value;
        mx = value;
        hasValue = true;
    } else {
        if (value < mn) {
            mn = value;
        }
        if (value > mx) {
            mx = value;
        }
    }
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
        publishesMetric_ = reg.getOrCreate(kPrefixPublishes + meta.id, MetricKind::Counter);
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

        // 4. Bump cumulative push count.
        ++pushCount_;

        // 5. Update metrics.
        if (samplesStoredMetric_ != nullptr) {
            samplesStoredMetric_->add(1);
        }
        if (samplesEvictedMetric_ != nullptr) {
            samplesEvictedMetric_->set(static_cast<std::int64_t>(totalEvicted_));
        }
        if (memoryBytesMetric_ != nullptr) {
            memoryBytesMetric_->set(static_cast<std::int64_t>(memoryBytes()));
        }

        // 6. LOD pyramid maintenance (numeric derivations override; bool
        // and string are no-ops because LOD is disabled).
        onPushCompleted();

        // 7. Snapshot publish on cadence: build an immutable Segment of the
        // current state and atomic-store it for readers.
        if (++pushesSincePublish_ >= publishCadence_) {
            publishSegment();
            pushesSincePublish_ = 0;
            ++publishCount_;
            if (publishesMetric_ != nullptr) {
                publishesMetric_->add(1);
            }
        }
        return true;
    }

    [[nodiscard]] std::size_t samplesRetained() const noexcept {
        return timestamps_.size();
    }

    [[nodiscard]] std::uint64_t totalEvicted() const noexcept {
        return totalEvicted_;
    }

    [[nodiscard]] std::uint64_t pushCount() const noexcept {
        return pushCount_;
    }

    [[nodiscard]] std::uint64_t publishCount() const noexcept {
        return publishCount_;
    }

    /// Number of LOD bins currently retained at the given level (1..3).
    /// Returns 0 for invalid levels and for derivations where LOD is
    /// disabled (Bool, String).
    [[nodiscard]] virtual std::size_t lodBinCount(int /*level*/) const noexcept {
        return 0;
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

    /// Build an immutable Segment of the current writer-private state and
    /// atomic-store it on the per-derivation `publishedSegment_`. Called
    /// from `push()` once every `publishCadence_` successful pushes.
    virtual void publishSegment() = 0;

    /// Hook for derivations to update LOD aggregators after a successful
    /// push (and any associated eviction). Default: no-op (Bool, String).
    virtual void onPushCompleted() {}

    std::deque<std::chrono::steady_clock::time_point> timestamps_;

    Metric* samplesStoredMetric_ = nullptr;
    Metric* samplesEvictedMetric_ = nullptr;
    Metric* memoryBytesMetric_ = nullptr;
    Metric* publishesMetric_ = nullptr;

    std::chrono::steady_clock::duration windowDuration_;
    std::size_t capSamples_;
    std::uint64_t totalEvicted_ = 0;
    std::uint64_t pushCount_ = 0;

    int publishCadence_ = kDefaultPublishCadence;
    int pushesSincePublish_ = 0;
    std::uint64_t publishCount_ = 0;
};

namespace {

class BoolTypedBuffer final : public SignalBuffer::TypedBuffer {
public:
    using TypedBuffer::TypedBuffer;

    /// Immutable snapshot of bool storage. Bits live in `packedBits` with
    /// `firstBitOffset` leading bits skipped (in the first word) so the
    /// reader can decode bit i (0 ≤ i < totalBits) as
    /// `(packedBits[(firstBitOffset + i) / 64] >> ((firstBitOffset + i) % 64)) & 1`.
    struct Segment {
        std::shared_ptr<const std::vector<std::uint64_t>> packedBits;
        std::size_t firstBitOffset = 0;
        std::size_t totalBits = 0;
        std::shared_ptr<const std::vector<std::chrono::steady_clock::time_point>> timestamps;
    };

    [[nodiscard]] std::shared_ptr<const Segment> currentSegment() const noexcept {
        return publishedSegment_.load(std::memory_order_acquire);
    }

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

    void publishSegment() override {
        auto seg = std::make_shared<Segment>();
        seg->packedBits = std::make_shared<std::vector<std::uint64_t>>(packed_.begin(), packed_.end());
        seg->firstBitOffset = headBitOffset_;
        seg->totalBits = totalBits_;
        seg->timestamps = std::make_shared<std::vector<std::chrono::steady_clock::time_point>>(timestamps_.begin(),
                                                                                               timestamps_.end());
        publishedSegment_.store(std::move(seg), std::memory_order_release);
    }

private:
    std::deque<std::uint64_t> packed_;
    std::size_t headBitOffset_ = 0;
    std::size_t totalBits_ = 0;
    std::atomic<std::shared_ptr<const Segment>> publishedSegment_{};
};

/// Helper CRTP-style base for the three "linear" types (Int64, Double,
/// QString) whose Segment is just `vector<T>` + timestamps. Bool is
/// separate because of bit-pack representation.
template <typename T> class LinearTypedBuffer : public SignalBuffer::TypedBuffer {
public:
    /// Whether this typed buffer maintains the LOD pyramid by default.
    /// Numeric: true. QString: false.
    static constexpr bool kTypeSupportsLod = std::is_arithmetic_v<T>;

    LinearTypedBuffer(const SignalMetadata& m, const SignalBufferConfig& c)
        : SignalBuffer::TypedBuffer(m, c), lodEnabled_(kTypeSupportsLod && c.lodEnabled.value_or(true)) {
        lodLevels_[0].binSize = kLodBinSize1;
        lodLevels_[1].binSize = kLodBinSize2;
        lodLevels_[2].binSize = kLodBinSize3;
    }

    struct Segment {
        std::shared_ptr<const std::vector<T>> values;
        std::shared_ptr<const std::vector<std::chrono::steady_clock::time_point>> timestamps;
        std::shared_ptr<const std::vector<LodBin<T>>> lod1;
        std::shared_ptr<const std::vector<LodBin<T>>> lod2;
        std::shared_ptr<const std::vector<LodBin<T>>> lod3;
    };

    [[nodiscard]] std::shared_ptr<const Segment> currentSegment() const noexcept {
        return publishedSegment_.load(std::memory_order_acquire);
    }

    [[nodiscard]] std::size_t lodBinCount(int level) const noexcept override {
        if (!lodEnabled_) {
            return 0;
        }
        if (level < 1 || level > 3) {
            return 0;
        }
        return lodLevels_[static_cast<std::size_t>(level - 1)].bins.size();
    }

protected:
    void evictFrontValue() override {
        values_.pop_front();
    }

    [[nodiscard]] std::size_t valueMemoryBytes() const noexcept override {
        std::size_t bytes = values_.size() * sizeof(T);
        if (lodEnabled_) {
            for (const auto& lvl : lodLevels_) {
                bytes += lvl.bins.size() * sizeof(LodBin<T>);
            }
        }
        return bytes;
    }

    void clearValues() override {
        values_.clear();
        for (auto& lvl : lodLevels_) {
            lvl.bins.clear();
            lvl.firstBinIndex = 0;
            lvl.nextBinToEmit = 0;
        }
    }

    void publishSegment() override {
        auto seg = std::make_shared<Segment>();
        seg->values = std::make_shared<std::vector<T>>(values_.begin(), values_.end());
        seg->timestamps = std::make_shared<std::vector<std::chrono::steady_clock::time_point>>(timestamps_.begin(),
                                                                                               timestamps_.end());
        if (lodEnabled_) {
            seg->lod1 = std::make_shared<std::vector<LodBin<T>>>(lodLevels_[0].bins.begin(), lodLevels_[0].bins.end());
            seg->lod2 = std::make_shared<std::vector<LodBin<T>>>(lodLevels_[1].bins.begin(), lodLevels_[1].bins.end());
            seg->lod3 = std::make_shared<std::vector<LodBin<T>>>(lodLevels_[2].bins.begin(), lodLevels_[2].bins.end());
        }
        publishedSegment_.store(std::move(seg), std::memory_order_release);
    }

    void onPushCompleted() override {
        if (!lodEnabled_) {
            return;
        }
        const std::uint64_t N = pushCount_;
        const std::uint64_t E = totalEvicted_;
        for (auto& lvl : lodLevels_) {
            // Drop any front bins that span the eviction point.
            const std::uint64_t firstIntactBin = (E + lvl.binSize - 1) / lvl.binSize;
            while (!lvl.bins.empty() && lvl.firstBinIndex < firstIntactBin) {
                lvl.bins.pop_front();
                ++lvl.firstBinIndex;
            }
            // Emit a new bin if N just crossed `(nextBinToEmit + 1) * binSize`.
            if (N >= (lvl.nextBinToEmit + 1) * lvl.binSize) {
                const std::uint64_t binIdx = lvl.nextBinToEmit;
                if (binIdx >= firstIntactBin) {
                    // Bin covers cumulative samples [binIdx*binSize, (binIdx+1)*binSize).
                    // values_ holds samples [E, N); position of cumulative i is (i - E).
                    const std::size_t start = static_cast<std::size_t>(binIdx * lvl.binSize - E);
                    const std::size_t end = static_cast<std::size_t>((binIdx + 1) * lvl.binSize - E);
                    LodBin<T> entry{};
                    bool hasValue = false;
                    for (std::size_t i = start; i < end; ++i) {
                        accumulateMinMax<T>(values_[i], entry.min_val, entry.max_val, hasValue);
                    }
                    if (!hasValue) {
                        // All samples in the bin were NaN; emit the first sample's value.
                        entry.min_val = values_[start];
                        entry.max_val = values_[start];
                    }
                    entry.t_start = timestamps_[start];
                    entry.t_end = timestamps_[end - 1];
                    if (lvl.bins.empty()) {
                        lvl.firstBinIndex = binIdx;
                    }
                    lvl.bins.push_back(entry);
                }
                ++lvl.nextBinToEmit;
            }
        }
    }

    std::deque<T> values_;
    std::atomic<std::shared_ptr<const Segment>> publishedSegment_{};
    LodLevel<T> lodLevels_[3];
    bool lodEnabled_;
};

class Int64TypedBuffer final : public LinearTypedBuffer<std::int64_t> {
public:
    using LinearTypedBuffer::LinearTypedBuffer;

protected:
    bool pushValue(const SignalValue& value) override {
        const auto* p = std::get_if<std::int64_t>(&value);
        if (p == nullptr) {
            return false;
        }
        values_.push_back(*p);
        return true;
    }
};

class DoubleTypedBuffer final : public LinearTypedBuffer<double> {
public:
    using LinearTypedBuffer::LinearTypedBuffer;

protected:
    bool pushValue(const SignalValue& value) override {
        const auto* p = std::get_if<double>(&value);
        if (p == nullptr) {
            return false;
        }
        values_.push_back(*p);
        return true;
    }
};

class StringTypedBuffer final : public LinearTypedBuffer<QString> {
public:
    using LinearTypedBuffer::LinearTypedBuffer;

protected:
    bool pushValue(const SignalValue& value) override {
        const auto* p = std::get_if<QString>(&value);
        if (p == nullptr) {
            return false;
        }
        values_.push_back(*p);
        return true;
    }
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

std::size_t SignalBuffer::lodBinCount(int level) const noexcept {
    return impl_ != nullptr ? impl_->lodBinCount(level) : 0;
}

const SignalMetadata& SignalBuffer::metadata() const noexcept {
    return metadata_;
}

const SignalBufferConfig& SignalBuffer::config() const noexcept {
    return config_;
}

}  // namespace signalforge::buffer
