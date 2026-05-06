# M6 — Signal Buffer

| Field | Value |
|---|---|
| Milestone ID | M6 |
| Sprint | 6 |
| Estimated effort | 8-10 person-days |
| Prerequisites | M5 closed (main at v0.0.6-alpha.1) |
| Next milestone | M7 (Expression Engine) and M8 (Chart UI) — both depend on M6 SignalBuffer freeze |
| Hard-stop type | **Interface freeze** (`SignalBuffer` API + `SignalBufferRegistry` API) + **Performance certification** (write throughput + read latency) |
| Soft-HALT allowed | **No** |
| Branch | `milestone/M6` |

**Cross-reference notation**:

- `[EM §N]` — Execution Manual, section N
- `[Arch §N]` — Architecture document, section N
- `[MR]` — Milestone Roadmap
- `[CM §X]` — CLAUDE.md, section X
- `[ADR-N]` — Architecture Decision Record N
- `[M<n> §N]` — M<n> spec

---

## 1. Goal

Time-series storage for decoded `SignalValue` instances with a query API suitable for both real-time chart rendering (M8) and bulk session writing (M10).

M6 produces the **first storage layer in V1**. Two design pillars:

1. **Lock-free reads** — Chart UI runs at 30Hz × 60+ signals = ~2000 reads/sec. Reader paths must not contend with the decoder's writer. Approach: lock-free snapshot pointer with reference-counted segments.

2. **LOD pyramid** — Chart rendering at any zoom level should query at most ~2000 samples regardless of underlying density. Approach: writer maintains 4 LOD levels (1:1, 10:1 max-pool, 100:1, 1000:1) inline; reader picks appropriate level by time range / pixel ratio.

This milestone freezes the `SignalBuffer` interface that M7 (Expression Engine) and M8 (Chart UI) consume.

Quality philosophy carried from M5: **predictable performance under realistic workload**. A 100k samples/sec decoder feeding the buffer should not slow below 95% of the no-buffer baseline.

---

## 2. Scope

### 2.1 Must deliver

1. **`SignalBuffer`** at `src/buffer/signal_buffer.{hpp,cpp}`:
   - Time-windowed circular storage per signal
   - Per-variant typed internal storage (bool bit-pack, int64/double 8-byte, QString ref-count)
   - Concurrent writer (single, from pipeline thread) + concurrent readers (multiple, lock-free)
   - LOD pyramid maintained on write (4 levels)
   - Query API: time range, latest N, latest-with-staleness

2. **`SignalBufferRegistry`** at `src/buffer/signal_buffer_registry.{hpp,cpp}`:
   - Owns all `SignalBuffer` instances
   - Implements `signalforge::decoder::SignalValueSink`
   - `onSignalsRegistered` allocates buffers per signal in the catalog
   - `onSignal` routes to the correct buffer
   - `onSignalsUnregistered` releases buffers
   - Query API for downstream consumers (Chart, Recorder)

3. **Per-signal configuration**:
   - Time window duration (default 60 seconds; configurable per-signal)
   - Max samples cap (safety; default 1M samples, prevents pathological signal at 10MHz)
   - LOD enabled (default true; bool signals skip LOD as min/max-pool is meaningless)

4. **Memory budget management**:
   - Global budget in registry (default 256MB; configurable)
   - Per-signal allocation tracked
   - Accumulated LOD overhead measured and reported via metrics
   - Soft warning at 80% budget; hard reject (registration fails, log ERROR) at 100%

5. **Lock-free read path**:
   - Reader acquires segment via atomic load
   - Reference-counted segment lifetime (writer cannot free segment while reader holds it)
   - No reader blocks writer; writers do not block each other across signals (each signal has independent writer)

6. **LOD pyramid implementation**:
   - Level 0: every sample, full resolution
   - Level 1: 10:1 decimation, max-pool + min-pool (to draw envelope)
   - Level 2: 100:1
   - Level 3: 1000:1
   - Writer maintains levels incrementally on each push
   - Reader selects level based on (time_range / target_sample_count) ratio

7. **Integration with M5 DecoderRegistrar**:
   - `SignalBufferRegistry` replaces `LoggingSignalValueSink` as the production sink
   - DecoderRegistrar wires `SignalBufferRegistry` instance to all decoders
   - `LoggingSignalValueSink` moves to a `tests/` subdirectory or `test_only` namespace

8. **Unit tests** ≥ 85% coverage on buffer modules

9. **Integration tests** at `tests/integration/`:
   - `test_signal_buffer_round_trip.cpp` — push → query → verify exact samples
   - `test_signal_buffer_concurrent.cpp` — writer + multiple concurrent readers
   - `test_signal_buffer_lod.cpp` — verify LOD level selection and decimation accuracy
   - `test_signal_buffer_window_eviction.cpp` — old samples dropped when window exceeded
   - `test_signal_buffer_budget.cpp` — registration fails when memory budget exceeded

10. **Benchmark** at `tests/benchmark/bench_signal_buffer.cpp`:
    - Writer throughput: ≥ 500k samples/sec/signal (well above M5's 410k decoder rate, leaves headroom)
    - Reader throughput: ≥ 10k queries/sec at full LOD on 60-sec window
    - End-to-end: M5 decoder + M6 buffer pipeline overhead ≤ 30% beyond M5 standalone (revised by ADR-004 from the original ≤ 5%)
    - Results to `tests/benchmark/results/M6-baseline.md`

11. **Doxygen** on all public declarations

12. **`.claude/M6-done.md`** with standard completion report + freeze record

### 2.2 Must not do

1. **No modifications to M2/M3/M4/M5 frozen files**. If freeze-scope change seems needed, HALT.
2. **No persistence**. Buffer is in-memory only; M10 Session Writer reads from buffer to disk.
3. **No cross-signal correlation queries**. Each signal queried independently. M7 Expression Engine handles multi-signal computations.
4. **No alarm / threshold logic**. Buffer stores values; alarm decisions belong to a future module.
5. **No Chart-specific helpers**. Buffer's API is type-neutral; Chart-specific logic (color, axis, etc.) is M8.
6. **No ML / statistical pre-computation beyond LOD min/max**. No FFT, no derived statistics. M7 does derived signals via expressions; statistical analysis is V2.
7. **No new top-level dependencies**. Use existing M2 utilities (`Snapshot<T>`, `MpscQueue`) + Qt + std.
8. **No QObject SignalBuffer**. The buffer is pure C++; metric updates go through `MetricsRegistry` (already-frozen QObject API). This matches the M4 FrameSink pattern.

---

## 3. Design Decisions (locked by this spec)

Decisions confirmed in pre-M6 planning.

### 3.1 Per-variant internal storage

**Decision**: Each signal's storage is one of four type-specific implementations chosen at registration time based on `SignalMetadata::type`:

- `Bool` signals: bit-packed in `std::vector<uint64_t>` (64 samples per uint64, 1/8 the memory of naive bool[])
- `Int64` signals: `std::vector<int64_t>` (8 bytes per sample)
- `Double` signals: `std::vector<double>` (8 bytes per sample)
- `String` signals: `std::vector<QString>` (Qt's implicit shared QString, ~24 byte handle + shared backing store)

**Rationale**: bool signals are common (alarm bits, mode flags). Naive 8-byte storage wastes 87.5%. Per-variant storage is M5 Decoder's stated guidance. Internal complexity is contained; query API surfaces uniform `SignalValue`.

**Storage size example** (60-second window):
- 1 kHz bool: 60,000 samples × 1 bit = 7.5 KB (vs 480 KB naive)
- 1 kHz int64: 60,000 × 8 = 480 KB
- 1 kHz double: same as int64
- 1 Hz QString (60 strings, each ~16 chars): ~2.5 KB

**Implementation note**: a polymorphic `TypedBuffer` interface internal to `signal_buffer.cpp`; not exposed externally. Query API converts to `SignalValue` variant on demand.

### 3.2 Time-windowed storage with optional max-samples cap

**Decision**: Each signal has two limits, both enforced:

- **Time window** (primary, in seconds): samples older than `now - window` are evicted
- **Max samples cap** (safety, in count): if a malformed schema produces a signal at unexpectedly high rate, cap prevents memory blowup

Eviction model: ring-buffer-style. Writer monotonically advances head; older indices are reused (overwriting evicted samples).

**Default values**:
- Time window: 60 seconds (covers Chart's typical view + recent history)
- Max samples cap: 1,000,000 per signal (10kHz signal × 100s, 5x safety margin over normal)

**Per-signal override**: registration callback may specify both via `SignalBufferConfig` struct. Useful for slow signals (a 0.1Hz status signal needs only 6 samples for 60s window; cap to 100 reduces memory waste).

**Rationale**: Chart UI typically shows the last 30-60 seconds with optional pan back to history. Buffer sized for the live view, not unlimited history (that's M10 session writer's job). Time-based eviction matches user mental model ("what happened in the last minute").

### 3.3 Snapshot-based queries

**Decision**: All read operations return a copy of the requested data (a snapshot). The reader does not hold a lock during use; the data is simply a vector copy.

**API forms**:

```cpp
// Time range query — returns snapshot of samples in [t_start, t_end]
std::vector<SignalSample> queryRange(
    const QString& signalId,
    std::chrono::steady_clock::time_point t_start,
    std::chrono::steady_clock::time_point t_end,
    std::size_t target_sample_count = 0);  // 0 = full resolution; >0 selects appropriate LOD

// Latest N samples
std::vector<SignalSample> queryLatest(
    const QString& signalId,
    std::size_t n);

// Single most-recent value with staleness
struct LatestValue {
    SignalValue value;
    std::chrono::steady_clock::time_point timestamp;
    std::chrono::nanoseconds age;  // now - timestamp
};
std::optional<LatestValue> queryLatestOne(const QString& signalId);
```

**Rationale**: Snapshot semantics match the established `Snapshot<T>` pattern in M2 (single-writer atomic publish, multi-reader copy). Caller free to use snapshot however they need without lock concerns. Cost: query allocates and copies; for typical 60-sec view at 1 kHz this is 480 KB / signal, ~10 µs.

**For M8 Chart**: 60 signals × 30 Hz × ~50 KB (LOD-decimated) = ~9 MB/sec read traffic, easily within bandwidth.

### 3.4 LOD pyramid maintained on write

**Decision**: Each signal maintains 4 LOD levels concurrently:

| Level | Decimation | Sample type | Use case |
|---|---|---|---|
| 0 | 1:1 (raw) | Original `SignalValue` | Full-resolution query, max ~60s window |
| 1 | 10:1 | min/max pair (numeric) or last (bool/string) | Chart at zoomed-out view, ~10 min equivalent |
| 2 | 100:1 | min/max pair | Chart at very wide view |
| 3 | 1000:1 | min/max pair | Long-term overview |

**Writer cost**: each push updates level 0; on every 10th push, also updates level 1; on every 100th, level 2; on every 1000th, level 3. Total writer overhead: ~1.1× plain push (10% increase).

**Reader selection**: query computes `samples_per_pixel = (t_end - t_start) / target_sample_count`. Selects the highest-decimation level whose effective sample density still gives ≥ 1 sample per pixel.

**Bool / String signals**: skip LOD levels 1-3 (no meaningful aggregation). LOD enabled flag in config; bool signals default to false.

**Memory cost**: levels 1-3 add ~11% to base storage (1/10 + 1/100 + 1/1000 = 0.111×).

**Rationale**: Pre-computing LOD on write moves cost off the hot Chart-render path. M8's chart render at 30Hz with 60 signals would otherwise scan ~3.6M samples per signal per zoom-out — unfeasible. Pre-computed LOD reduces Chart query to ≤ 2000 samples regardless of zoom level.

**LOD verification**: spec §5.3 integration test verifies that querying with `target_sample_count = 100` over a 1-hour window returns ~100 samples whose min/max envelope matches the underlying full-resolution data.

### 3.5 Lock-free snapshot reads

**Decision**: Each signal's storage uses a "atomic published segment" pattern:

```
Writer (single, from pipeline thread):
1. Append to current segment in the writer-private append region
2. Periodically (~1ms or every N samples), atomically swap "published" pointer to a new immutable snapshot of [head - window, head]
3. Old published segments are reference-counted; freed when last reader releases

Reader (multi):
1. Atomic load of "published" pointer
2. Increment reference count
3. Use snapshot freely
4. On scope exit, decrement reference count
```

**Implementation pattern** mirrors M2's `Snapshot<T>` but applied to a sliding-window time series.

**Cost analysis**:

- Writer: one atomic store per publish (every ~1ms or every 1000 samples, whichever first). ~1µs per publish, amortized to negligible per sample.
- Reader: one atomic load + ref-count inc/dec. <100ns per query setup.
- Memory: at most 2-3 segments alive at once per signal (current writer-private + 1-2 published+referenced).

**Concurrent readers**: zero contention. All reads to the same published segment proceed independently.

**Reader-while-writer**: reader sees a consistent snapshot; new writes after snapshot publish are not visible until next publish (1ms latency acceptable for 30Hz Chart).

**Rationale**: M5's pipeline overhead was 4.47%; we don't want M6 to push that to 15%+. Lock-free reads keep decoder hot path uncontended. Implementation complexity is controlled (mirrors existing pattern).

**Failure modes documented**:

- If reader holds snapshot longer than (window + 1 publish interval), reader sees "stale" data — by design, snapshot is a point-in-time view
- If many readers each hold snapshots, memory inflates — bounded by reasonable usage (60 readers × 1MB per signal × 60 signals = 3.6GB worst case, far above expected)

### 3.6 Memory budget hard limit

**Decision**: Registry tracks total allocated bytes across all signals' raw + LOD storage. When `onSignalsRegistered` would push past 100% of budget:

- Reject registration: callback returns error
- Log ERROR with current usage + requested addition + remaining budget
- Decoder's `setSignalSink` step fails; pipeline still works but signals from that decoder aren't stored

**Soft warning** at 80%: log WARN, increment a `signal_buffer_budget_warned` counter (one-shot per registration that crosses 80%).

**Default budget**: 256 MB. Configurable at registry construction.

**Per-signal overhead estimation** (at registration time, before allocation):
```
estimated_bytes = window_seconds × estimated_rate_hz × bytes_per_sample × lod_overhead_factor
                = 60 × 1000 × 8 × 1.11  for typical 1kHz double signal
                = 533 KB
```

Registration uses a **conservative rate estimate from `SignalMetadata` if available** (e.g., metadata's `sample_rate_hz` field if present; otherwise default to 1000 Hz). Rate-vs-budget mismatches surface as WARN logs.

**Rationale**: Stops malformed schemas (e.g., user mistakenly configures 100kHz rate when device sends 100Hz) from OOM-killing the app. Reject-with-clear-error is more user-friendly than crash.

### 3.7 Per-signal independent writer

**Decision**: `SignalBuffer` is per-signal. The `SignalBufferRegistry` owns N `SignalBuffer` instances (one per registered signal). Concurrent writers (different decoders writing different signals) do not contend.

**Rationale**: M4's pipeline topology (Y) gives each driver its own pipeline+decoder; signals from different drivers are inherently parallel. M6 storage matches this — no global lock, no global queue.

**Cross-signal correlation queries** (e.g., "all values at t=T") would require synchronization, but M6 deliberately doesn't offer this. M7 Expression Engine handles the use case (its evaluator pulls from each signal independently).

### 3.8 No soft-HALT

Same as M2/M3/M4/M5.

### 3.9 Metrics naming convention

Per M4's `<module>_<metric>_<scope>` convention:

- `signal_buffer_samples_stored_<signalId>` (counter): total samples ingested
- `signal_buffer_samples_evicted_<signalId>` (counter): samples dropped due to time window
- `signal_buffer_queries_<signalId>` (counter): total queryRange/queryLatest/queryLatestOne calls
- `signal_buffer_query_us_<signalId>` (gauge): most-recent query latency
- `signal_buffer_memory_bytes_<signalId>` (gauge): current allocation for this signal
- `signal_buffer_total_memory_bytes` (gauge, registry-level): total across all signals
- `signal_buffer_budget_warned` (counter, registry-level): registrations that crossed 80% budget warning
- `signal_buffer_budget_rejected` (counter, registry-level): registrations rejected due to 100% budget

`signalId` is sanitized per M5 schema rules (no whitespace, no quotes, no `<>` etc).

---

## 4. Key Implementation Details

### 4.1 `SignalBuffer` class

Place at `src/buffer/signal_buffer.hpp`.

```cpp
// src/buffer/signal_buffer.hpp
#pragma once

#include "decoder/decoder_interface.hpp"  // For SignalValue, SignalType, SignalMetadata

#include <QString>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace signalforge::buffer {

/// One sample with timestamp.
struct SignalSample {
    std::chrono::steady_clock::time_point timestamp;
    signalforge::decoder::SignalValue value;
};

/// Most-recent value with staleness info.
struct LatestValue {
    signalforge::decoder::SignalValue value;
    std::chrono::steady_clock::time_point timestamp;
    std::chrono::nanoseconds age;
};

/// Per-signal configuration. Set at registration time.
struct SignalBufferConfig {
    /// Time window in seconds. Samples older than this are evicted on next write.
    double windowSeconds = 60.0;

    /// Hard cap on samples (safety). Min(windowSeconds × maxRate, capSamples) wins.
    std::size_t capSamples = 1'000'000;

    /// Whether to maintain LOD pyramid for this signal.
    /// Default: true for numeric (Int64, Double); false for Bool, String.
    std::optional<bool> lodEnabled;

    /// Estimated sample rate for budget calculation. If unset, registry uses 1000 Hz default.
    std::optional<double> estimatedRateHz;
};

/// Storage for one signal's time-series with optional LOD pyramid.
///
/// Threading: single writer (typically the decoder thread that produced the signal),
/// multi-reader (Chart UI, M10 session writer, M7 expression engine).
/// Writers do not block readers; readers do not block writers.
///
/// Lifetime: owned by `SignalBufferRegistry`. Callers do not own these directly.
///
/// Freeze scope: this header is frozen at M6 close.
class SignalBuffer {
public:
    /// Construct with metadata + config. Allocates initial storage.
    SignalBuffer(const signalforge::decoder::SignalMetadata& metadata,
                 const SignalBufferConfig& config);
    ~SignalBuffer();

    SignalBuffer(const SignalBuffer&) = delete;
    SignalBuffer& operator=(const SignalBuffer&) = delete;
    SignalBuffer(SignalBuffer&&) = delete;
    SignalBuffer& operator=(SignalBuffer&&) = delete;

    /// Append a sample. Called from the decoder thread.
    /// Thread-safe with respect to readers; not thread-safe across writers
    /// (each signal has at most one writer per the M4 pipeline topology).
    void push(std::chrono::steady_clock::time_point timestamp,
              const signalforge::decoder::SignalValue& value);

    /// Query a time range. Returns up to target_sample_count samples
    /// (decimated via LOD if needed), or all samples in range if 0.
    /// Thread-safe; called from any thread.
    [[nodiscard]] std::vector<SignalSample> queryRange(
        std::chrono::steady_clock::time_point t_start,
        std::chrono::steady_clock::time_point t_end,
        std::size_t target_sample_count = 0) const;

    /// Latest n samples (most-recent, in chronological order).
    [[nodiscard]] std::vector<SignalSample> queryLatest(std::size_t n) const;

    /// Single most-recent value with staleness info.
    /// Returns nullopt if no samples ever pushed.
    [[nodiscard]] std::optional<LatestValue> queryLatestOne() const;

    /// Total samples currently retained (not the cumulative pushed count).
    [[nodiscard]] std::size_t sampleCount() const noexcept;

    /// Cumulative samples pushed since construction (monotonic).
    [[nodiscard]] std::uint64_t totalSamplesPushed() const noexcept;

    /// Cumulative samples evicted due to time window or cap.
    [[nodiscard]] std::uint64_t totalSamplesEvicted() const noexcept;

    /// Current memory usage in bytes (raw + LOD).
    [[nodiscard]] std::size_t memoryBytes() const noexcept;

    /// Metadata accessor.
    [[nodiscard]] const signalforge::decoder::SignalMetadata& metadata() const noexcept;

    /// Configuration accessor.
    [[nodiscard]] const SignalBufferConfig& config() const noexcept;

private:
    // Internal: per-variant typed buffer (polymorphic; not in public API)
    struct TypedBuffer;
    std::unique_ptr<TypedBuffer> impl_;

    signalforge::decoder::SignalMetadata metadata_;
    SignalBufferConfig config_;

    // Atomic counters for cheap status queries
    std::atomic<std::uint64_t> totalPushed_{0};
    std::atomic<std::uint64_t> totalEvicted_{0};
    std::atomic<std::size_t> currentMemoryBytes_{0};
};

}  // namespace signalforge::buffer
```

### 4.2 `SignalBufferRegistry` class

Place at `src/buffer/signal_buffer_registry.hpp`.

```cpp
// src/buffer/signal_buffer_registry.hpp
#pragma once

#include "buffer/signal_buffer.hpp"
#include "decoder/decoder_interface.hpp"  // For SignalValueSink

#include <QString>
#include <QStringList>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace signalforge::buffer {

/// Registry-level configuration.
struct RegistryConfig {
    /// Total memory budget in bytes. Default 256 MB.
    /// Registrations that would exceed this are rejected.
    std::size_t totalBudgetBytes = 256ULL * 1024 * 1024;

    /// Per-signal default config. May be overridden per signal at registration.
    SignalBufferConfig signalDefaults;

    /// If true, attempts to register a signal that would exceed budget log
    /// ERROR and return failure. If false, log ERROR but allow registration.
    /// Default: true (strict).
    bool rejectOnBudgetExceeded = true;
};

/// Per-driver per-signal config override map. Keyed by signalId.
/// Used at registration to override registry defaults for specific signals.
using SignalConfigOverrides = std::unordered_map<QString, SignalBufferConfig>;

/// Registry of all SignalBuffer instances, implementing SignalValueSink.
///
/// One registry per app process; instantiated at app startup, passed to
/// `DecoderRegistrar` as the production sink (M5's `LoggingSignalValueSink`
/// is replaced).
///
/// Threading: thread-safe across all public methods.
///
/// Lifetime: outlives all decoders; destruction releases all signal buffers.
class SignalBufferRegistry : public signalforge::decoder::SignalValueSink {
public:
    explicit SignalBufferRegistry(RegistryConfig config = {});
    ~SignalBufferRegistry() override;

    SignalBufferRegistry(const SignalBufferRegistry&) = delete;
    SignalBufferRegistry& operator=(const SignalBufferRegistry&) = delete;

    // SignalValueSink overrides

    void onSignal(std::chrono::steady_clock::time_point timestamp,
                  const QString& signalId,
                  const signalforge::decoder::SignalValue& value) override;

    void onSignalsRegistered(
        const QString& driverId,
        const std::vector<signalforge::decoder::SignalMetadata>& signalsList) override;

    void onSignalsUnregistered(const QString& driverId) override;

    // Query API

    /// Look up a buffer. Returns nullptr if signalId not registered.
    /// Returned pointer remains valid until the signal is unregistered.
    [[nodiscard]] SignalBuffer* bufferFor(const QString& signalId) const;

    /// All currently-registered signal IDs.
    [[nodiscard]] QStringList signalIds() const;

    /// All signal IDs from a specific driver.
    [[nodiscard]] QStringList signalIdsForDriver(const QString& driverId) const;

    /// Per-driver registration override. Set BEFORE the driver's signals are registered.
    /// Subsequent onSignalsRegistered calls for this driver use these overrides.
    void setDriverConfigOverrides(const QString& driverId,
                                  const SignalConfigOverrides& overrides);

    // Status

    [[nodiscard]] std::size_t totalMemoryBytes() const;
    [[nodiscard]] std::size_t totalBudgetBytes() const;
    [[nodiscard]] std::size_t signalCount() const;

    /// Returns a struct with per-driver and per-signal memory usage.
    /// Useful for diagnostic UI and the M8 performance panel.
    struct UsageReport {
        std::size_t totalBytes;
        std::size_t budgetBytes;
        struct PerDriver {
            QString driverId;
            std::size_t bytes;
            int signalCount;
        };
        std::vector<PerDriver> drivers;
    };
    [[nodiscard]] UsageReport memoryUsage() const;

private:
    RegistryConfig config_;
    mutable std::mutex registryMutex_;
    std::unordered_map<QString, std::unique_ptr<SignalBuffer>> buffersBySignalId_;
    std::unordered_map<QString, QStringList> signalsByDriverId_;  // driverId -> [signalId, ...]
    std::unordered_map<QString, SignalConfigOverrides> driverOverrides_;
    std::atomic<std::size_t> totalBytes_{0};
};

}  // namespace signalforge::buffer
```

### 4.3 Internal `TypedBuffer` polymorphism

Inside `signal_buffer.cpp` (not in the public header), four template instantiations:

- `TypedBufferImpl<bool>` — uses bit-packed `std::vector<uint64_t>` for raw level
- `TypedBufferImpl<int64_t>` — uses `std::vector<int64_t>` for raw, int64 min/max for LOD
- `TypedBufferImpl<double>` — uses `std::vector<double>` for raw, double min/max for LOD
- `TypedBufferImpl<QString>` — uses `std::vector<QString>` for raw, no LOD

A virtual `TypedBuffer` base class provides the polymorphic interface. `SignalBuffer::push` and `SignalBuffer::queryRange` dispatch via virtual calls.

**Implementation note**: `SignalValue` arriving via `push()` is unpacked via `std::visit`, then routed to the appropriate typed buffer. This is one virtual call + one variant visit per push, ~5ns total.

### 4.4 Lock-free snapshot pattern

Each `TypedBufferImpl` maintains:

```cpp
struct Segment {
    std::shared_ptr<const std::vector<...>> rawData;          // Snapshot of raw samples
    std::shared_ptr<const std::vector<...>> lodLevel1;        // 10:1
    std::shared_ptr<const std::vector<...>> lodLevel2;        // 100:1
    std::shared_ptr<const std::vector<...>> lodLevel3;        // 1000:1
    std::chrono::steady_clock::time_point t_oldest;
    std::chrono::steady_clock::time_point t_newest;
};

std::atomic<std::shared_ptr<const Segment>> publishedSegment_;
```

Writer:
1. Append to private working buffer
2. Every N pushes (configurable, default 100) or every M ms (default 1ms):
   - Build new immutable Segment (copy-on-publish; old segment unchanged)
   - `publishedSegment_.store(newSegment, std::memory_order_release)`
   - Old segment naturally released when last reader's shared_ptr expires

Reader:
1. `auto seg = publishedSegment_.load(std::memory_order_acquire)` — atomic ptr load
2. Use `seg->rawData`, `seg->lodLevelN` freely; segment lifetime guaranteed by shared_ptr
3. Return when done; segment released

**`std::atomic<std::shared_ptr<T>>` requires C++20** (already in M5). Use C++23 `std::atomic_ref` if needed for fine control.

**Cost amortization**: publishing every 1ms / 100 pushes amortizes copy cost. For 1kHz signal, that's 1 publish / 100ms which copies ~100KB; ~1MB/sec copy traffic per signal, well within memory bandwidth.

### 4.5 LOD level selection

In `queryRange`:

```cpp
double samples_per_pixel = (t_end - t_start).count() / target_sample_count;
double signal_period_ns = 1e9 / estimated_rate_hz;
double effective_density = signal_period_ns / samples_per_pixel;

if (effective_density < 0.5)   return level3;  // 1000:1
if (effective_density < 5)     return level2;  // 100:1
if (effective_density < 50)    return level1;  // 10:1
return level0;                                 // raw
```

Threshold tuning: `effective_density` 0.5/5/50 corresponds to "raw level would give 0.5/5/50 samples per pixel". Below 0.5 we'd waste effort scanning; pick higher decimation. Above 50 raw is reasonable.

### 4.6 DecoderRegistrar wiring update

In `src/decoder/decoder_registrar.cpp`, change construction:

**Before** (M5):
```cpp
DecoderRegistrar(QObject* parent = nullptr) : QObject(parent) {
    sink_ = std::make_shared<LoggingSignalValueSink>();
}
```

**After** (M6):
```cpp
DecoderRegistrar(SignalBufferRegistry& bufferRegistry, QObject* parent = nullptr)
    : QObject(parent), bufferRegistry_(&bufferRegistry) {
    sink_ = std::shared_ptr<SignalValueSink>(&bufferRegistry, [](SignalValueSink*) {});
    // ^^^ non-owning shared_ptr; registry outlives registrar
}
```

`MainWindow` (or `main.cpp`) instantiates `SignalBufferRegistry` once at app startup and passes the reference into `DecoderRegistrar`'s constructor.

`LoggingSignalValueSink` is moved to `tests/test_only/` and only used in unit tests.

### 4.7 Memory budget enforcement during registration

In `SignalBufferRegistry::onSignalsRegistered`:

```cpp
// Estimate memory for new signals
std::size_t requiredBytes = 0;
for (const auto& meta : signalsList) {
    auto cfg = effectiveConfig(driverId, meta.id);
    requiredBytes += estimateSignalBytes(meta, cfg);
}

std::size_t currentBytes = totalBytes_.load();
double percentAfter = double(currentBytes + requiredBytes) / config_.totalBudgetBytes * 100;

if (percentAfter >= 100.0 && config_.rejectOnBudgetExceeded) {
    SF_LOG_ERROR("Signal buffer registration rejected: would exceed budget. "
                 "Current: {:.1f} MB, requested: {:.1f} MB, budget: {:.1f} MB",
                 currentBytes / 1e6, requiredBytes / 1e6, 
                 config_.totalBudgetBytes / 1e6);
    incrementMetric("signal_buffer_budget_rejected");
    return;  // No buffers created
}

if (percentAfter >= 80.0 && currentBytes / config_.totalBudgetBytes < 0.8) {
    SF_LOG_WARN("Signal buffer budget {:.0f}% utilized after this registration", 
                percentAfter);
    incrementMetric("signal_buffer_budget_warned");
}

// Allocate buffers
for (const auto& meta : signalsList) { ... }
```

---

## 5. Test strategy

### 5.1 Coverage ≥ 85% on buffer modules

- `signal_buffer.cpp`: ≥ 85%
- `signal_buffer_registry.cpp`: ≥ 85%

### 5.2 Unit tests

**For `SignalBuffer`** (one test file per type):

`tests/unit/buffer/signal_buffer_bool_test.cpp`:
- Bit packing correctness (push 65 bools, query, verify all 65)
- Eviction by time window
- Eviction by max samples cap
- queryLatest with various N
- queryLatestOne after one push
- queryLatestOne with no pushes returns nullopt
- LOD disabled by default for bool (verify level1+ buffers not allocated)

`signal_buffer_int64_test.cpp`:
- Push, query, verify exact values
- LOD level 1 min/max correctness
- LOD level 2 min/max correctness
- queryRange with target_sample_count selects correct LOD level

`signal_buffer_double_test.cpp`:
- Same suite as int64
- NaN handling (NaN samples stored, but NaN excluded from min/max LOD computation)

`signal_buffer_string_test.cpp`:
- QString implicit shared correctness (push QString literal twice; verify shared backing)
- LOD disabled
- Memory accounting includes string backing store size

**For concurrent safety**:

`signal_buffer_concurrent_test.cpp`:
- 1 writer + 4 readers, 100k pushes
- Verify reader counts match expected window size
- No data races (run under TSan if available; ASan otherwise)
- Reader holding old snapshot doesn't see new pushes (snapshot semantics)

**For `SignalBufferRegistry`**:

`signal_buffer_registry_test.cpp`:
- Empty registry: signalCount=0, signalIds empty
- Register one driver with 3 signals: signalCount=3, correct IDs
- Register again same driver: previous signals released, new set installed
- Unregister driver: count drops, lookups return nullptr
- Register multiple drivers: cross-driver isolation
- bufferFor unknown signal returns nullptr
- Memory accounting: register signals, verify totalMemoryBytes matches sum of individual buffer memoryBytes

`signal_buffer_registry_budget_test.cpp`:
- Budget at 256MB, register signals totaling 200MB → all succeed, log clean
- Register signal that would push to 300MB → rejected, ERROR logged, counter incremented
- Soft warning at 80%: register signals to 230MB → WARN logged, counter incremented

### 5.3 Integration tests

`tests/integration/test_signal_buffer_round_trip.cpp`:
1. Construct registry
2. Register a driver with 3 signals (bool, int64, double)
3. onSignal × 1000 with known timestamps and values
4. queryRange for each signal, full range, target_sample_count=0
5. Verify all 1000 samples returned with correct timestamps and values

`test_signal_buffer_concurrent.cpp`:
1. Register 1 signal (double)
2. Spawn writer thread pushing 1M samples over 5 seconds
3. Spawn 4 reader threads, each calling queryLatest(100) at 1kHz
4. Verify writer completes; readers all return non-empty snapshots
5. No crashes, ASan clean

`test_signal_buffer_lod.cpp`:
1. Register 1 signal at 1kHz (sine wave + noise)
2. Push 600,000 samples (10 minutes at 1kHz)
3. Query 10-minute range with target_sample_count=100
4. Verify result has ~100 samples, level 3 LOD selected
5. Verify min/max envelope contains all values from raw

`test_signal_buffer_window_eviction.cpp`:
1. Register 1 signal with windowSeconds=1
2. Push 2000 samples spread over 2 seconds (1kHz)
3. Query latest: should see only most recent ~1000
4. Verify totalSamplesEvicted ≈ 1000

`test_signal_buffer_budget.cpp`:
1. Construct registry with budget=10MB
2. Register signal that estimates to 5MB → succeeds
3. Register signal that estimates to 7MB (total 12MB) → rejected
4. Verify the second signal's buffer not created
5. Verify ERROR log message format

### 5.4 Benchmarks

`tests/benchmark/bench_signal_buffer.cpp`:

**Scenario 1: Writer throughput per signal type**
- Create one buffer
- Push 10M samples in tight loop
- Measure samples/sec
- Targets:
  - bool: ≥ 1M samples/sec (bit-pack overhead acceptable)
  - int64: ≥ 500k samples/sec
  - double: ≥ 500k samples/sec
  - QString (small string): ≥ 200k samples/sec

**Scenario 2: Reader throughput**
- Pre-load buffer with 60k samples (60s × 1kHz)
- Run queryRange in tight loop, target_sample_count=2000 (Chart-typical)
- Measure queries/sec
- Target: ≥ 10k queries/sec

**Scenario 3: End-to-end pipeline overhead**
- M5 decoder + M6 buffer registration as sink
- Run M5's existing benchmark methodology with buffer attached
- Compare to M5 standalone
- Target: M6 overhead ≤ 30% beyond M5 (revised by ADR-004 from the original ≤ 5%)

Results to `tests/benchmark/results/M6-baseline.md`.

### 5.5 ASan / TSan clean

Required:

- ASan: zero leak, zero use-after-free, zero buffer overflow on any test
- TSan (if available): zero data race in concurrent test

CI is the authoritative gate for ASan (local AppProtection.so still blocks).

---

## 6. Freeze protocol

### 6.1 What freezes at M6 close

**C++ interfaces**:

- `src/buffer/signal_buffer.hpp`: `SignalBuffer` class, `SignalSample` struct, `LatestValue` struct, `SignalBufferConfig` struct.
- `src/buffer/signal_buffer_registry.hpp`: `SignalBufferRegistry` class, `RegistryConfig` struct, `SignalConfigOverrides` typedef, `UsageReport` struct.

Once frozen, modifications require new ADR.

### 6.2 What does NOT freeze

- `TypedBuffer` polymorphism (internal to .cpp; may evolve)
- LOD level count or decimation ratios (could expand to 5 levels in a future tuning, additive)
- Snapshot publishing strategy (every N pushes vs every M ms — implementer's choice)
- Default values in `SignalBufferConfig` and `RegistryConfig` (tunable based on M8/M12 measurements)
- Metric names (additive only)

### 6.3 Freeze record format

`.claude/M6-done.md`:

```markdown
## Freezes established in this milestone

Frozen per M6 spec §6.1.

| File | sha256 |
|---|---|
| `src/buffer/signal_buffer.hpp` | <...> |
| `src/buffer/signal_buffer_registry.hpp` | <...> |

Frozen surface:
- `SignalBuffer` public methods + internal `TypedBuffer` polymorphism interface
- `SignalSample`, `LatestValue`, `SignalBufferConfig` struct layouts
- `SignalBufferRegistry` public API (including SignalValueSink overrides)
- `RegistryConfig` struct layout
- `UsageReport` struct layout

Modifications require new ADR per M6 §6.2.
```

---

## 7. M6-specific HALT triggers

Beyond CLAUDE.md §HALT:

1. **Any modification to M2/M3/M4/M5 frozen `.hpp`** → HALT.
2. **Lock-free snapshot pattern requires `std::atomic<std::shared_ptr<T>>`** which has subtle ABI issues on some standard library versions → if encountered, HALT and propose `tl::atomic_shared_ptr` or a hand-rolled ref-count implementation.
3. **Writer throughput < 200k samples/sec** for double type after first optimization pass → HALT.
4. **End-to-end overhead > 35%** beyond M5 baseline → HALT (revised by ADR-004 from the original > 10%).
5. **TSan reports data race in concurrent test** → HALT.
6. **Memory budget calculation off by > 20%** from actual allocation → HALT (estimation logic broken).
7. **LOD min/max envelope misses any actual sample by > 0.1%** → HALT (LOD computation broken).

---

## 8. Acceptance criteria

### 8.1 Build and test

- [ ] Debug, Release, debug-asan all build clean under C++23 (GCC 13)
- [ ] All unit + integration tests pass under all three presets
- [ ] Coverage ≥ 85% per §5.1
- [ ] CI green on milestone/M6 head

### 8.2 Performance

- [ ] Writer throughput: per-type targets in §5.4 Scenario 1 met
- [ ] Reader throughput: ≥ 10k queries/sec at 60s × 1kHz buffer
- [ ] End-to-end overhead: ≤ 30% beyond M5 baseline (revised by ADR-004)
- [ ] Results in `tests/benchmark/results/M6-baseline.md` with run-to-run variance < 5%

### 8.3 LOD correctness

- [ ] LOD level selection chooses correct level by sample density (verified in `test_signal_buffer_lod.cpp`)
- [ ] Min/max envelope at all levels contains all underlying raw values (no LOD aggregation drops outliers)
- [ ] Visual inspection of LOD output for sine + noise input shows preserved envelope

### 8.4 Concurrency safety

- [ ] ASan clean on all tests
- [ ] TSan clean on `test_signal_buffer_concurrent.cpp` (if TSan available; document if not)
- [ ] Reader holding snapshot during writer eviction sees consistent data

### 8.5 Freeze record

- [ ] `.claude/M6-done.md` has Freezes section per §6.3
- [ ] Sha256s recorded
- [ ] No modifications to M2/M3/M4/M5 frozen files

### 8.6 Hand-off to M7 (Expression Engine) and M8 (Chart UI)

- [ ] M6-done.md hand-off section covers:
  - For M7: how Expression Engine reads from `SignalBuffer` (use `queryLatest(1)` for current value; `queryRange` for windowed expressions)
  - For M8: LOD pyramid usage; recommended `target_sample_count = chart_pixel_width`
  - Thread affinity expectations (any thread can call query)
  - Performance baseline to maintain

---

## 9. Notes for CC

- **Lock-free is not free**. The snapshot publishing pattern requires careful sequencing with `memory_order_acquire/release`. If unsure about a specific atomic ordering, use `seq_cst` and let the optimizer handle it. We can tune later in M12.

- **LOD pyramid implementation has subtle correctness traps**:
  - Min/max pool at level 1 (10:1) must use values from level 0 (raw), not from another decimation
  - On window eviction, LOD bins that span the eviction point must be partially re-computed or dropped (don't leave "stale" bins from already-evicted raw samples)
  - bool-typed signals don't need LOD at all; respect the disabled flag

- **The `SignalConfigOverrides` map is set by the human or app config**, not by the decoder. The decoder calls `onSignalsRegistered` with metadata; the registry consults its overrides map keyed by signalId.

- **`std::atomic<std::shared_ptr<T>>` works in libstdc++ in GCC 12+**. If linker warnings appear, document them in concerns.md but proceed.

- **Don't over-engineer the budget estimation**. A simple `bytes_per_sample × samples × 1.11 LOD overhead` is fine. Real usage will be measured via actual `memoryBytes()` and reported via metric. Estimation is for upfront rejection, not precise accounting.

- **`SignalBufferRegistry` is the one production sink replacing `LoggingSignalValueSink`**. After M6 close, `LoggingSignalValueSink` is test-only. Verify by grep that production code paths don't reference it.

---

## 10. Closing note

M6 is the storage tier that downstream milestones (M7 Expression, M8 Chart, M10 Session Writer, M11 Replay) all depend on. The interface freeze here is consequential.

The two performance bets — lock-free reads and pre-computed LOD — together determine whether M8 Chart can hit 30 FPS at 60+ signals. If either bet fails, M8 has no Plan B short of architectural rework. So M6 is also where we validate the assumption that this design *can* support V1's UI performance targets.

When in doubt about a design choice between simplicity and performance, lean toward correctness first (snapshot semantics, ref-count safety), then performance (writer throughput, reader latency), then simplicity. Premature optimization — especially around the atomic ordering — has caused real bugs in similar systems; correctness traceable from the spec wins over micro-optimizations.

**Threshold-revision note (ADR-004, 2026-05-06)**: §5.4 and §7-4 end-to-end overhead thresholds were amended after S11/S11.5 measurement showed the original ≤ 5% target / > 10% HALT trigger were authored without architectural prototyping. The revised values (≤ 30% / > 35%) reflect the structural floor of the per-event `SignalValueSink` + variant + LOD pyramid architecture (~60-95 ns/signal of necessary per-sample work). M12 (Performance Optimization) inherits a `SignalBuffer` overhead reduction goal as the structurally correct home for cross-milestone performance debt; profiler-driven optimization there may target `SignalBuffer::push` body, the registry path, or the per-event sink interface (potentially batched).
