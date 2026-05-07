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
constexpr auto kPrefixQueries = QLatin1String("signal_buffer_queries_");
constexpr auto kPrefixQueryUs = QLatin1String("signal_buffer_query_us_");

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

// LodLevel is defined later (after ChunkedStore, since it needs
// ChunkedStore<LodBin<T>> for cheap publish snapshots — ADR-005).

/// Per-buffer chunk size in samples. Sealed chunks are exactly this
/// size; the writer's mutable tail holds 0..kChunkSize - 1 samples.
/// Tunable in the bench; see ADR-005 for the rationale (per-publish
/// work bounded at O(N / kChunkSize + kChunkSize) instead of O(N)).
constexpr std::size_t kChunkSize = 4096;

/// Append-only chunked storage with O(1) push/pop_front and cheap
/// snapshot publishing (ADR-005). Used by TypedBuffer for timestamps
/// and by LinearTypedBuffer<T> for typed values; BoolTypedBuffer keeps
/// its bit-packed deque for now (deferred per ADR-005 — bool's pack
/// density already gives 64x compression vs the linear case).
///
/// Invariant: every sealed chunk has size == kChunkSize. The mutable
/// tail holds the most recent 0..kChunkSize - 1 samples; when it
/// fills, it is moved into a `shared_ptr<const std::vector<T>>` and
/// pushed into `sealedChunks_`. firstChunkOffset_ tracks how many
/// front samples of `sealedChunks_.front()` have been logically
/// evicted; when it reaches the chunk's size, the chunk is dropped
/// and the offset resets to 0.
///
/// Thread model: single-writer / multi-reader, like the deques it
/// replaces. All non-snapshot methods are writer-only; readers
/// consume the immutable `Snapshot` produced by `snapshot()` and
/// captured into the published Segment.
template <typename T> class ChunkedStore {
public:
    void push_back(const T& v) {
        tail_.push_back(v);
        ++totalSize_;
        if (tail_.size() >= kChunkSize) {
            sealedChunks_.push_back(std::make_shared<const std::vector<T>>(std::move(tail_)));
            tail_.clear();
            tail_.reserve(kChunkSize);
        }
    }

    void pop_front() noexcept {
        if (totalSize_ == 0) {
            return;
        }
        --totalSize_;
        if (!sealedChunks_.empty()) {
            ++firstChunkOffset_;
            if (firstChunkOffset_ == sealedChunks_.front()->size()) {
                sealedChunks_.pop_front();
                firstChunkOffset_ = 0;
            }
        } else {
            // Sealed empty: front lives in the tail. O(tail_.size())
            // shift on this rare path; only fires for caps < kChunkSize
            // before the first seal, or briefly after a chunk drop.
            tail_.erase(tail_.begin());
        }
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return totalSize_;
    }
    [[nodiscard]] bool empty() const noexcept {
        return totalSize_ == 0;
    }

    [[nodiscard]] const T& front() const noexcept {
        if (!sealedChunks_.empty()) {
            return (*sealedChunks_.front())[firstChunkOffset_];
        }
        return tail_.front();
    }
    [[nodiscard]] const T& back() const noexcept {
        if (!tail_.empty()) {
            return tail_.back();
        }
        return sealedChunks_.back()->back();
    }

    /// Random access by logical retained index [0, size()).
    [[nodiscard]] const T& at(std::size_t i) const noexcept {
        if (!sealedChunks_.empty()) {
            const std::size_t firstChunkRetained = sealedChunks_.front()->size() - firstChunkOffset_;
            if (i < firstChunkRetained) {
                return (*sealedChunks_.front())[firstChunkOffset_ + i];
            }
            i -= firstChunkRetained;
            // Subsequent chunks are guaranteed kChunkSize (invariant).
            const std::size_t numFollowing = sealedChunks_.size() - 1;
            const std::size_t chunkRel = i / kChunkSize;
            const std::size_t chunkOff = i % kChunkSize;
            if (chunkRel < numFollowing) {
                return (*sealedChunks_[1 + chunkRel])[chunkOff];
            }
            i -= numFollowing * kChunkSize;
        }
        return tail_[i];
    }

    void clear() noexcept {
        sealedChunks_.clear();
        tail_.clear();
        firstChunkOffset_ = 0;
        totalSize_ = 0;
    }

    /// Approximate retained-storage cost. The shared_ptr-controlled
    /// chunks are still counted as "ours" since at least the writer
    /// references them; readers may also hold them via the published
    /// Segment, but that's a transient amplification and not budgeted.
    [[nodiscard]] std::size_t memoryBytes() const noexcept {
        std::size_t bytes = tail_.capacity() * sizeof(T);
        for (const auto& chunk : sealedChunks_) {
            bytes += chunk->size() * sizeof(T);
        }
        return bytes;
    }

    /// Immutable view of the writer's state at publish time. Captured
    /// into the published `Segment` and read by reader-side queries.
    struct Snapshot {
        std::vector<std::shared_ptr<const std::vector<T>>> chunks;
        std::shared_ptr<const std::vector<T>> tail;
        std::size_t firstChunkOffset = 0;
        std::size_t totalSize = 0;

        [[nodiscard]] bool empty() const noexcept {
            return totalSize == 0;
        }
        [[nodiscard]] std::size_t size() const noexcept {
            return totalSize;
        }

        [[nodiscard]] const T& at(std::size_t i) const noexcept {
            if (!chunks.empty()) {
                const std::size_t firstChunkRetained = chunks[0]->size() - firstChunkOffset;
                if (i < firstChunkRetained) {
                    return (*chunks[0])[firstChunkOffset + i];
                }
                i -= firstChunkRetained;
                const std::size_t numFollowing = chunks.size() - 1;
                const std::size_t chunkRel = i / kChunkSize;
                const std::size_t chunkOff = i % kChunkSize;
                if (chunkRel < numFollowing) {
                    return (*chunks[1 + chunkRel])[chunkOff];
                }
                i -= numFollowing * kChunkSize;
            }
            return (*tail)[i];
        }
    };

    [[nodiscard]] Snapshot snapshot() const {
        Snapshot snap;
        snap.chunks.reserve(sealedChunks_.size());
        for (const auto& c : sealedChunks_) {
            snap.chunks.push_back(c);
        }
        snap.tail = std::make_shared<const std::vector<T>>(tail_);
        snap.firstChunkOffset = firstChunkOffset_;
        snap.totalSize = totalSize_;
        return snap;
    }

private:
    std::deque<std::shared_ptr<const std::vector<T>>> sealedChunks_;
    std::vector<T> tail_;
    std::size_t firstChunkOffset_ = 0;
    std::size_t totalSize_ = 0;
};

/// Binary-search a `ChunkedStore<T>::Snapshot` for the smallest logical
/// index `i` such that `snap.at(i) >= target`. Returns `snap.size()` if
/// every retained value is less than `target`. Used by queryRange to
/// implement the t_start / t_end window over chunked timestamps.
/// O(chunks.size() + log(kChunkSize)). For 1 M samples / 4 k chunk
/// size that's ~244 + 12 ops.
template <typename T>
[[nodiscard]] std::size_t snapshotLowerBound(const typename ChunkedStore<T>::Snapshot& snap, const T& target) {
    if (snap.empty()) {
        return 0;
    }
    std::size_t logicalStart = 0;
    if (!snap.chunks.empty()) {
        const auto& c0 = *snap.chunks[0];
        const auto startIt = c0.begin() + static_cast<std::ptrdiff_t>(snap.firstChunkOffset);
        const auto endIt = c0.end();
        if (startIt != endIt && *(endIt - 1) >= target) {
            const auto it = std::lower_bound(startIt, endIt, target);
            return logicalStart + static_cast<std::size_t>(it - startIt);
        }
        logicalStart += static_cast<std::size_t>(endIt - startIt);
    }
    for (std::size_t c = 1; c < snap.chunks.size(); ++c) {
        const auto& cv = *snap.chunks[c];
        if (!cv.empty() && cv.back() >= target) {
            const auto it = std::lower_bound(cv.begin(), cv.end(), target);
            return logicalStart + static_cast<std::size_t>(it - cv.begin());
        }
        logicalStart += cv.size();
    }
    if (snap.tail && !snap.tail->empty()) {
        const auto& tv = *snap.tail;
        const auto it = std::lower_bound(tv.begin(), tv.end(), target);
        return logicalStart + static_cast<std::size_t>(it - tv.begin());
    }
    return snap.size();
}

/// Same as `snapshotLowerBound` but for `upper_bound` semantics.
template <typename T>
[[nodiscard]] std::size_t snapshotUpperBound(const typename ChunkedStore<T>::Snapshot& snap, const T& target) {
    if (snap.empty()) {
        return 0;
    }
    std::size_t logicalStart = 0;
    if (!snap.chunks.empty()) {
        const auto& c0 = *snap.chunks[0];
        const auto startIt = c0.begin() + static_cast<std::ptrdiff_t>(snap.firstChunkOffset);
        const auto endIt = c0.end();
        if (startIt != endIt && *(endIt - 1) > target) {
            const auto it = std::upper_bound(startIt, endIt, target);
            return logicalStart + static_cast<std::size_t>(it - startIt);
        }
        logicalStart += static_cast<std::size_t>(endIt - startIt);
    }
    for (std::size_t c = 1; c < snap.chunks.size(); ++c) {
        const auto& cv = *snap.chunks[c];
        if (!cv.empty() && cv.back() > target) {
            const auto it = std::upper_bound(cv.begin(), cv.end(), target);
            return logicalStart + static_cast<std::size_t>(it - cv.begin());
        }
        logicalStart += cv.size();
    }
    if (snap.tail && !snap.tail->empty()) {
        const auto& tv = *snap.tail;
        const auto it = std::upper_bound(tv.begin(), tv.end(), target);
        return logicalStart + static_cast<std::size_t>(it - tv.begin());
    }
    return snap.size();
}

/// Per-LOD-level bookkeeping. `binSize` is fixed (10 / 100 / 1000);
/// `bins` holds completed aggregates as a `ChunkedStore<LodBin<T>>`
/// so that `publishSegment` can snapshot it via shared_ptr-vector
/// + tail copy (ADR-005). `firstBinIndex` is the cumulative bin
/// index of the oldest retained bin; `nextBinToEmit` is the
/// cumulative bin index of the bin that the next emit will produce.
template <typename T> struct LodLevel {
    std::uint64_t binSize = 0;
    ChunkedStore<LodBin<T>> bins;
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
        : windowDuration_(toSteadyDuration(cfg.windowSeconds)), capSamples_(cfg.capSamples),
          estimatedRateHz_(cfg.estimatedRateHz.value_or(1000.0)) {
        auto& reg = MetricsRegistry::instance();
        samplesStoredMetric_ = reg.getOrCreate(kPrefixSamplesStored + meta.id, MetricKind::Counter);
        samplesEvictedMetric_ = reg.getOrCreate(kPrefixSamplesEvicted + meta.id, MetricKind::Counter);
        memoryBytesMetric_ = reg.getOrCreate(kPrefixMemoryBytes + meta.id, MetricKind::Gauge);
        publishesMetric_ = reg.getOrCreate(kPrefixPublishes + meta.id, MetricKind::Counter);
        queriesMetric_ = reg.getOrCreate(kPrefixQueries + meta.id, MetricKind::Counter);
        queryUsMetric_ = reg.getOrCreate(kPrefixQueryUs + meta.id, MetricKind::Gauge);
    }
    virtual ~TypedBuffer() = default;

    TypedBuffer(const TypedBuffer&) = delete;
    TypedBuffer& operator=(const TypedBuffer&) = delete;

    /// Append one sample after applying time-window and cap eviction.
    /// Returns true if stored; false on type mismatch (no eviction is
    /// performed in that case — the caller's push is a no-op).
    bool push(std::chrono::steady_clock::time_point t, const SignalValue& value) {
        const std::uint64_t evictedBefore = totalEvicted_;

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
        // (timestamps_ is now a ChunkedStore<TimePoint>; same API
        // shape — front/empty/pop_front/push_back/size — so the
        // eviction loop is unchanged from the M6 deque version.)

        // 4. Bump cumulative push count.
        ++pushCount_;

        // 5. Update metrics. Hot-path principle: only the always-needed
        //   counter (samples_stored) is updated per push. Eviction and
        //   memory metrics update only when their value actually changes
        //   (samples_evicted) or on the publish cadence (memory_bytes,
        //   updated alongside the publish event).
        if (samplesStoredMetric_ != nullptr) {
            samplesStoredMetric_->add(1);
        }
        if (totalEvicted_ != evictedBefore && samplesEvictedMetric_ != nullptr) {
            samplesEvictedMetric_->set(static_cast<std::int64_t>(totalEvicted_));
        }

        // 6. LOD pyramid maintenance (numeric derivations override; bool
        // and string are no-ops because LOD is disabled).
        onPushCompleted();

        // 7. Snapshot publish on cadence: build an immutable Segment of the
        // current state and atomic-store it for readers. Memory-bytes gauge
        // is also updated here (rather than per push) since publish is the
        // natural amortization point for accounting work.
        if (++pushesSincePublish_ >= publishCadence_) {
            publishSegment();
            pushesSincePublish_ = 0;
            ++publishCount_;
            if (publishesMetric_ != nullptr) {
                publishesMetric_->add(1);
            }
            if (memoryBytesMetric_ != nullptr) {
                memoryBytesMetric_->set(static_cast<std::int64_t>(memoryBytes()));
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

    /// Reader-side queries. Each derivation loads its own published Segment
    /// (via `currentSegment()`) and constructs `SignalSample` results from
    /// the immutable snapshot. Query metrics are updated by these methods.
    [[nodiscard]] virtual std::vector<SignalSample> queryRange(std::chrono::steady_clock::time_point t_start,
                                                               std::chrono::steady_clock::time_point t_end,
                                                               std::size_t target_sample_count) const = 0;
    [[nodiscard]] virtual std::vector<SignalSample> queryLatest(std::size_t n) const = 0;
    [[nodiscard]] virtual std::optional<LatestValue> queryLatestOne() const = 0;

protected:
    /// RAII-style helper: derived queryX() methods construct a `QueryTimer`
    /// at the top to update the queries / query_us metrics on scope exit.
    class QueryTimer {
    public:
        explicit QueryTimer(const TypedBuffer& tb) noexcept : tb_(tb), start_(std::chrono::steady_clock::now()) {}
        ~QueryTimer() {
            if (tb_.queriesMetric_ != nullptr) {
                tb_.queriesMetric_->add(1);
            }
            if (tb_.queryUsMetric_ != nullptr) {
                const auto us =
                    std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start_)
                        .count();
                tb_.queryUsMetric_->set(static_cast<std::int64_t>(us));
            }
        }
        QueryTimer(const QueryTimer&) = delete;
        QueryTimer& operator=(const QueryTimer&) = delete;

    private:
        const TypedBuffer& tb_;
        std::chrono::steady_clock::time_point start_;
    };

    /// LOD level selection per spec §4.5.
    /// Returns 0 (raw) / 1 / 2 / 3 based on density thresholds.
    [[nodiscard]] int selectLodLevel(std::chrono::steady_clock::time_point t_start,
                                     std::chrono::steady_clock::time_point t_end,
                                     std::size_t target_sample_count) const noexcept {
        if (target_sample_count == 0) {
            return 0;
        }
        const auto delta_ns = (t_end - t_start).count();
        if (delta_ns <= 0) {
            return 0;
        }
        const double samples_per_pixel = static_cast<double>(delta_ns) / static_cast<double>(target_sample_count);
        const double signal_period_ns = 1e9 / estimatedRateHz_;
        const double effective_density = signal_period_ns / samples_per_pixel;
        if (effective_density < 0.5) {
            return 3;
        }
        if (effective_density < 5.0) {
            return 2;
        }
        if (effective_density < 50.0) {
            return 1;
        }
        return 0;
    }

public:
    /// Total memory: the chunked timestamp store + the per-type value
    /// bytes. Bound by retained samples regardless of how readers
    /// hold transient snapshots.
    [[nodiscard]] std::size_t memoryBytes() const noexcept {
        return timestamps_.memoryBytes() + valueMemoryBytes();
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

    // ChunkedStore replaces M6's `std::deque<TimePoint>` (ADR-005).
    // Same logical API — push_back / pop_front / front / size / empty
    // / at — but with sealed-chunk + mutable-tail storage so that
    // publishSegment captures it as O(N/kChunkSize) shared_ptr copies
    // rather than an O(N) element copy.
    ChunkedStore<std::chrono::steady_clock::time_point> timestamps_;

    Metric* samplesStoredMetric_ = nullptr;
    Metric* samplesEvictedMetric_ = nullptr;
    Metric* memoryBytesMetric_ = nullptr;
    Metric* publishesMetric_ = nullptr;
    Metric* queriesMetric_ = nullptr;
    Metric* queryUsMetric_ = nullptr;

    std::chrono::steady_clock::duration windowDuration_;
    std::size_t capSamples_;
    std::uint64_t totalEvicted_ = 0;
    std::uint64_t pushCount_ = 0;
    double estimatedRateHz_;

    int publishCadence_ = kDefaultPublishCadence;
    int pushesSincePublish_ = 0;
    std::uint64_t publishCount_ = 0;
};

namespace {

class BoolTypedBuffer final : public SignalBuffer::TypedBuffer {
public:
    using TypedBuffer::TypedBuffer;

    /// Immutable snapshot of bool storage. Bits live in `packedBits`
    /// with `firstBitOffset` leading bits skipped (in the first word)
    /// so the reader can decode bit i (0 ≤ i < totalBits) as
    /// `(packedBits[(firstBitOffset + i) / 64] >> ((firstBitOffset + i) % 64)) & 1`.
    /// Timestamps are a chunked snapshot per ADR-005; bool's bit-pack
    /// is left in deque form (deferred per ADR-005's BoolTypedBuffer
    /// note: bool already gets 64x compression via packing).
    struct Segment {
        std::shared_ptr<const std::vector<std::uint64_t>> packedBits;
        std::size_t firstBitOffset = 0;
        std::size_t totalBits = 0;
        ChunkedStore<std::chrono::steady_clock::time_point>::Snapshot timestamps;
    };

    [[nodiscard]] std::shared_ptr<const Segment> currentSegment() const noexcept {
        return publishedSegment_.load(std::memory_order_acquire);
    }

    [[nodiscard]] std::vector<SignalSample> queryRange(std::chrono::steady_clock::time_point t_start,
                                                       std::chrono::steady_clock::time_point t_end,
                                                       std::size_t /*target_sample_count*/) const override {
        QueryTimer timer(*this);
        std::vector<SignalSample> out;
        const auto seg = currentSegment();
        if (!seg || seg->timestamps.empty()) {
            return out;
        }
        const auto& packed = *seg->packedBits;
        const std::size_t firstBit = seg->firstBitOffset;

        const std::size_t lo = snapshotLowerBound<std::chrono::steady_clock::time_point>(seg->timestamps, t_start);
        const std::size_t hi = snapshotUpperBound<std::chrono::steady_clock::time_point>(seg->timestamps, t_end);
        if (hi <= lo) {
            return out;
        }
        out.reserve(hi - lo);
        for (std::size_t idx = lo; idx < hi; ++idx) {
            const std::size_t bitIdx = firstBit + idx;
            const bool b = ((packed[bitIdx / 64] >> (bitIdx % 64)) & std::uint64_t{1}) != 0;
            out.push_back({seg->timestamps.at(idx), SignalValue{b}});
        }
        return out;
    }

    [[nodiscard]] std::vector<SignalSample> queryLatest(std::size_t n) const override {
        QueryTimer timer(*this);
        std::vector<SignalSample> out;
        const auto seg = currentSegment();
        if (!seg || seg->timestamps.empty() || n == 0) {
            return out;
        }
        const auto& packed = *seg->packedBits;
        const std::size_t firstBit = seg->firstBitOffset;
        const std::size_t total = seg->timestamps.size();
        const std::size_t take = std::min(n, total);
        const std::size_t startIdx = total - take;
        out.reserve(take);
        for (std::size_t idx = startIdx; idx < total; ++idx) {
            const std::size_t bitIdx = firstBit + idx;
            const bool b = ((packed[bitIdx / 64] >> (bitIdx % 64)) & std::uint64_t{1}) != 0;
            out.push_back({seg->timestamps.at(idx), SignalValue{b}});
        }
        return out;
    }

    [[nodiscard]] std::optional<LatestValue> queryLatestOne() const override {
        QueryTimer timer(*this);
        const auto seg = currentSegment();
        if (!seg || seg->timestamps.empty()) {
            return std::nullopt;
        }
        const auto& packed = *seg->packedBits;
        const std::size_t firstBit = seg->firstBitOffset;
        const std::size_t idx = seg->timestamps.size() - 1;
        const std::size_t bitIdx = firstBit + idx;
        const bool b = ((packed[bitIdx / 64] >> (bitIdx % 64)) & std::uint64_t{1}) != 0;
        LatestValue lv;
        lv.value = SignalValue{b};
        lv.timestamp = seg->timestamps.at(idx);
        lv.age = std::chrono::steady_clock::now() - lv.timestamp;
        return lv;
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
        seg->timestamps = timestamps_.snapshot();
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
        // Chunked snapshots per ADR-005: chunk pointer-vector + tail
        // copy. Reader consumes via `at(i)` and the snapshotLowerBound
        // / snapshotUpperBound helpers in this file. LOD bins are also
        // chunked to avoid an O(N/binSize) copy per publish — at N = 1 M
        // and binSize = 10, the M6-style vector copy was 100 k bins per
        // publish.
        typename ChunkedStore<T>::Snapshot values;
        typename ChunkedStore<std::chrono::steady_clock::time_point>::Snapshot timestamps;
        typename ChunkedStore<LodBin<T>>::Snapshot lod1;
        typename ChunkedStore<LodBin<T>>::Snapshot lod2;
        typename ChunkedStore<LodBin<T>>::Snapshot lod3;
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
        return lodLevels_[static_cast<std::size_t>(level - 1)].bins.size();  // ChunkedStore::size()
    }

    [[nodiscard]] std::vector<SignalSample> queryRange(std::chrono::steady_clock::time_point t_start,
                                                       std::chrono::steady_clock::time_point t_end,
                                                       std::size_t target_sample_count) const override {
        QueryTimer timer(*this);
        std::vector<SignalSample> out;
        const auto seg = currentSegment();
        if (!seg || seg->timestamps.empty()) {
            return out;
        }

        // Decide whether to use LOD or raw.
        int level = 0;
        if (lodEnabled_ && target_sample_count > 0) {
            level = selectLodLevel(t_start, t_end, target_sample_count);
        }

        if (level == 0) {
            // Raw range: binary search timestamps, copy values across.
            const std::size_t lo =
                snapshotLowerBound<std::chrono::steady_clock::time_point>(seg->timestamps, t_start);
            const std::size_t hi =
                snapshotUpperBound<std::chrono::steady_clock::time_point>(seg->timestamps, t_end);
            if (hi <= lo) {
                return out;
            }
            out.reserve(hi - lo);
            for (std::size_t idx = lo; idx < hi; ++idx) {
                out.push_back({seg->timestamps.at(idx), SignalValue{seg->values.at(idx)}});
            }
            return out;
        }

        // LOD output: 2 SignalSamples per bin (min @ t_start, max @ t_end).
        const auto& binsSnap = (level == 1) ? seg->lod1 : (level == 2) ? seg->lod2 : seg->lod3;
        if (binsSnap.empty()) {
            return out;
        }
        out.reserve(binsSnap.size() * 2);
        for (std::size_t i = 0; i < binsSnap.size(); ++i) {
            const auto& bin = binsSnap.at(i);
            // Include a bin if any portion of its range overlaps the query window.
            if (bin.t_end < t_start || bin.t_start > t_end) {
                continue;
            }
            out.push_back({bin.t_start, SignalValue{bin.min_val}});
            out.push_back({bin.t_end, SignalValue{bin.max_val}});
        }
        return out;
    }

    [[nodiscard]] std::vector<SignalSample> queryLatest(std::size_t n) const override {
        QueryTimer timer(*this);
        std::vector<SignalSample> out;
        const auto seg = currentSegment();
        if (!seg || seg->timestamps.empty() || n == 0) {
            return out;
        }
        const std::size_t total = seg->timestamps.size();
        const std::size_t take = std::min(n, total);
        const std::size_t startIdx = total - take;
        out.reserve(take);
        for (std::size_t idx = startIdx; idx < total; ++idx) {
            out.push_back({seg->timestamps.at(idx), SignalValue{seg->values.at(idx)}});
        }
        return out;
    }

    [[nodiscard]] std::optional<LatestValue> queryLatestOne() const override {
        QueryTimer timer(*this);
        const auto seg = currentSegment();
        if (!seg || seg->timestamps.empty()) {
            return std::nullopt;
        }
        const std::size_t idx = seg->timestamps.size() - 1;
        LatestValue lv;
        lv.value = SignalValue{seg->values.at(idx)};
        lv.timestamp = seg->timestamps.at(idx);
        lv.age = std::chrono::steady_clock::now() - lv.timestamp;
        return lv;
    }

protected:
    void evictFrontValue() override {
        values_.pop_front();
    }

    [[nodiscard]] std::size_t valueMemoryBytes() const noexcept override {
        std::size_t bytes = values_.memoryBytes();
        if (lodEnabled_) {
            for (const auto& lvl : lodLevels_) {
                bytes += lvl.bins.memoryBytes();
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
        // O(N / kChunkSize) shared_ptr copies + O(kChunkSize) tail
        // copies — see ADR-005 for the cost analysis. LOD bins are
        // also chunked, so the per-publish copy cost stays bounded
        // even at N = 1 M (where lod1 would otherwise be a 100 k-entry
        // vector copy on every publish).
        seg->values = values_.snapshot();
        seg->timestamps = timestamps_.snapshot();
        if (lodEnabled_) {
            seg->lod1 = lodLevels_[0].bins.snapshot();
            seg->lod2 = lodLevels_[1].bins.snapshot();
            seg->lod3 = lodLevels_[2].bins.snapshot();
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
                    // Chunked-store random access (`values_.at(i)`) is O(1)
                    // — same big-O as the M6 deque's operator[].
                    const std::size_t start = static_cast<std::size_t>(binIdx * lvl.binSize - E);
                    const std::size_t end = static_cast<std::size_t>((binIdx + 1) * lvl.binSize - E);
                    LodBin<T> entry{};
                    bool hasValue = false;
                    for (std::size_t i = start; i < end; ++i) {
                        accumulateMinMax<T>(values_.at(i), entry.min_val, entry.max_val, hasValue);
                    }
                    if (!hasValue) {
                        // All samples in the bin were NaN; emit the first sample's value.
                        entry.min_val = values_.at(start);
                        entry.max_val = values_.at(start);
                    }
                    entry.t_start = timestamps_.at(start);
                    entry.t_end = timestamps_.at(end - 1);
                    if (lvl.bins.empty()) {
                        lvl.firstBinIndex = binIdx;
                    }
                    lvl.bins.push_back(entry);
                }
                ++lvl.nextBinToEmit;
            }
        }
    }

    ChunkedStore<T> values_;
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

std::vector<SignalSample> SignalBuffer::queryRange(std::chrono::steady_clock::time_point t_start,
                                                   std::chrono::steady_clock::time_point t_end,
                                                   std::size_t target_sample_count) const {
    return impl_ != nullptr ? impl_->queryRange(t_start, t_end, target_sample_count) : std::vector<SignalSample>{};
}

std::vector<SignalSample> SignalBuffer::queryLatest(std::size_t n) const {
    return impl_ != nullptr ? impl_->queryLatest(n) : std::vector<SignalSample>{};
}

std::optional<LatestValue> SignalBuffer::queryLatestOne() const {
    return impl_ != nullptr ? impl_->queryLatestOne() : std::nullopt;
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
