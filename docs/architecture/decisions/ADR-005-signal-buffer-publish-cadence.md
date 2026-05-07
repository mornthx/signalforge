# ADR-005 — SignalBuffer chunked storage

**Status**: Accepted
**Date**: 2026-05-07
**Context**: M8 prototype Scenario 4 — preloading 600 k samples
into a long-lived `SignalBuffer` measured ~40 k samples/sec wall
throughput (vs M6's bench-reported 8.8 M/sec on small steady-state
buffers). Root-cause: every `kDefaultPublishCadence = 100` pushes,
`publishSegment` copies the entire retained `values_` deque plus
the `timestamps_` deque (and the LOD pyramids) into a new immutable
`Segment`. Per-publish work is O(N); per-push amortized work is
O(N / 100), which grows linearly with buffer size.

## Decision

Replace `LinearTypedBuffer<T>`'s deque-based mutable storage with
**chunked shared storage**: a vector of immutable sealed chunks
(`std::shared_ptr<const std::vector<T>>`) plus a mutable tail
vector. Pushes append to the tail; when the tail fills
`kChunkSize` elements it is sealed (moved into a `shared_ptr<const>`)
and pushed onto the sealed-chunks vector. `publishSegment` then
copies only the sealed-chunks vector (a vector of
`shared_ptr<const>` — `O(N / kChunkSize)`) plus a snapshot of the
mutable tail (`O(kChunkSize)`). Cadence stays at the M6 baseline
value of 100 pushes per publish; M6 spec §3.5's "≤ 1 ms publish
latency for 30 Hz Chart" contract is preserved.

At `kChunkSize = 4096` and N = 1 M samples:

- Sealed-chunks vector: 244 entries × `sizeof(shared_ptr) = 16 B`
  → ~4 KB per publish
- Mutable tail snapshot: ≤ 4096 doubles × 8 B → ≤ 33 KB
- **Total per-publish work: ~37 KB at N = 1 M, O(1) growth with N**
  modulo the chunk-vector size

Compare to M6's deque-copy at N = 1 M: ~8 MB per publish — a
**>200× reduction** in per-publish work at this scale.

## Supporting evidence

The M6 baseline benchmark (`tests/benchmark/results/M6-baseline.md`)
was structured around steady-state `capSamples = 10 000` buffers,
where the deque-copy publish is ~80 KB and amortizes to 0.8 µs
per push at cadence 100 — well within the 30 Hz frame budget.

The M8 prototype Scenario 4 (`tools/m8_prototype/RESULTS.md`) is
the first measurement that exercised long-lived large-buffer push
throughput, the actual chart use case (1 kHz × minutes-to-hours).
At those sizes, per-publish O(N) copy dominates and total work
becomes O(M² / 100) for inserting M samples into a buffer
retaining all of them.

| Buffer size | M6 baseline copy size | M6 baseline rate | After chunked storage (this ADR) |
|---:|---:|---:|---:|
| 10 k cap | 80 KB / publish | 8.8 M/sec | 37 KB / publish, similar rate |
| 100 k retained | ~800 KB / publish | ~1 M/sec (extrapolated) | 37 KB / publish |
| 600 k retained | ~5 MB / publish | 40 k/sec (measured) | 37 KB / publish |
| 1 M retained | ~8 MB / publish | (not measured) | 37 KB / publish |
| 3.6 M retained | ~30 MB / publish | (not feasible) | 37 KB / publish |

## Evolution from prior attempt

This ADR's first draft proposed an **adaptive publish cadence**:
`cadence = clamp(samplesRetained() / 100, 100, 5000)`. That
draft was discarded after measurement-driven discovery on the
`fix/m6-publish-cadence` branch:

- The `S9: 1 writer + 4 readers, 100 k Double pushes, snapshot
  consistency` test failed under adaptive cadence with a 539-sample
  staleness gap. Investigation revealed that all three reader APIs
  (`queryRange`, `queryLatest`, `queryLatestOne`) read from the
  immutable published `Segment`. Increasing the cadence makes them
  all bounded-stale by `cadence - 1` samples — a substantive
  semantic change, not just a perf tuning.
- M6 spec §3.5 sets a "≤ 1 ms publish latency acceptable for
  30 Hz Chart" contract. Adaptive cadence at N = 100 k implies
  cadence = 1 000, which at 1 kHz is **1 second** of staleness —
  a 1000× violation of the spec contract.
- The HALT report at
  `.claude/halt/HALT-20260507T025419Z-publish-cadence-test-conflict.md`
  enumerates the design conflict in detail.

The discarded approach is data, not failure: it pinned down
exactly which design boundary the M8 finding pushes against (the
M6 spec §3.5 staleness contract), which then forced selection of
the architecturally correct fix that preserves both the staleness
contract and the lock-free read path. This evolution mirrors
ADR-002's path — the original Crashpad choice was superseded by
sentry-native after M2 S2 discovery surfaced an integration
constraint that the v0.6 architecture had not anticipated.

## Why not the alternatives

### Adaptive publish cadence (discarded; see "Evolution" above)

- Violates M6 spec §3.5 staleness contract.
- Breaks `S9` consistency test, with bounded-staleness semantics
  that ripple into `queryLatestOne` (and therefore M7
  ExpressionEngine's per-tick source-value reads).

### Mutex-protected live-state read path

- Violates M6 spec §3.5 lock-free reads guarantee.
- M7 ExpressionEngine and the M8 chart UI are documented as
  hot-path lock-free readers; introducing a mutex affects the
  ExpressionEngine 30 Hz tick and the M8 chart redraw path
  observably.

### Deferred publish to a worker thread

- Adds threading complexity (per-buffer worker, lifetime
  management, ordering guarantees) for a benefit that's bounded
  by the worker's own throughput. Doesn't address the underlying
  O(M²) total-work problem.

### Naive cadence bump (e.g., constant cadence = 1000)

- Doesn't scale. At N = 1 M, per-publish cost (O(1 M))
  divided by cadence 1000 = 1000 element ops per push amortized,
  10× regression vs the chunked-storage approach.
- Imposes fixed staleness on the small-buffer case where
  the M6 baseline is already meeting its targets.

## Acceptance criteria

1. **HALT trigger #1**: no `.hpp` changes — the chunked storage
   is an internal implementation detail; `Segment` shape and
   reader-API signatures are unchanged.
2. **HALT trigger #2**: `queryRange` / `queryLatest` /
   `queryLatestOne` return values bit-identical to pre-patch on
   the existing test fixtures (no staleness change at the M6
   cadence).
3. **HALT trigger #3**: M6 e2e overhead remains ≤ 30%
   (ADR-004 threshold).
4. **HALT trigger #4**: every existing test in M6 + M7 still
   passes — including the `S9` consistency test that the
   adaptive-cadence draft broke.
5. **HALT trigger #5**: 1 M-sample push throughput
   ≥ 500 k samples/sec on a single buffer with default config.
6. **HALT trigger #6**: total task time ≤ 12 hours.

## Implementation

### `LinearTypedBuffer<T>`

Replace the existing `std::deque<T> values_` and
`std::deque<int64_t> timestamps_` (effectively `time_point`)
with:

```cpp
// Sealed-and-shared chunks: each chunk is a fixed-size immutable
// vector. Once sealed, the same shared_ptr is captured by every
// Segment that publishes after that point — readers holding old
// segments do not block writer mutation.
std::vector<std::shared_ptr<const std::vector<T>>>            sealedValueChunks_;
std::vector<std::shared_ptr<const std::vector<TimePoint>>>    sealedTimestampChunks_;

// Mutable tail: appends happen here. Once size() == kChunkSize,
// the tail is moved into a `shared_ptr<const std::vector<T>>` and
// pushed onto sealedValueChunks_; the tail is then cleared and
// reserve(kChunkSize) is restored.
std::vector<T>          tailValues_;
std::vector<TimePoint>  tailTimestamps_;

std::size_t totalSamples_ = 0;  // running cumulative count for
                                 // the LOD pyramid index math
```

`kChunkSize = 4096` (suggested; the bench may tune).

### Push path

```cpp
tailValues_.push_back(v);
tailTimestamps_.push_back(t);
++totalSamples_;
if (tailValues_.size() == kChunkSize) {
    sealedValueChunks_.push_back(
        std::make_shared<const std::vector<T>>(std::move(tailValues_)));
    sealedTimestampChunks_.push_back(
        std::make_shared<const std::vector<TimePoint>>(std::move(tailTimestamps_)));
    tailValues_.clear();      tailValues_.reserve(kChunkSize);
    tailTimestamps_.clear();  tailTimestamps_.reserve(kChunkSize);
}
```

### Publish path

```cpp
auto seg = std::make_shared<Segment>();
seg->valueChunks     = sealedValueChunks_;        // shared_ptr vector copy
seg->timestampChunks = sealedTimestampChunks_;    // shared_ptr vector copy
seg->tailValues      = std::make_shared<const std::vector<T>>(tailValues_);
seg->tailTimestamps  = std::make_shared<const std::vector<TimePoint>>(tailTimestamps_);
seg->lod1 = ...; seg->lod2 = ...; seg->lod3 = ...;  // unchanged
publishedSegment_.store(std::move(seg), std::memory_order_release);
```

### Read path

Readers traverse sealed chunks then the tail snapshot as a
logical concatenation. A small helper makes existing index-based
code (LOD bin emission, `valueAt`-style accessors) cheap:

```cpp
[[nodiscard]] T valueAt(std::size_t logicalIndex) const noexcept {
    const std::size_t chunkIdx = logicalIndex / kChunkSize;
    const std::size_t chunkOff = logicalIndex % kChunkSize;
    if (chunkIdx < sealedValueChunks_.size()) {
        return (*sealedValueChunks_[chunkIdx])[chunkOff];
    }
    return tailValues_[logicalIndex - sealedValueChunks_.size() * kChunkSize];
}
```

### Eviction

When the time window or cap evicts the front:

1. If the entire oldest sealed chunk is out of the window, drop
   it (`sealedValueChunks_.erase(begin())`). Existing readers
   holding a Segment that still references the chunk keep the
   chunk alive via the `shared_ptr`; eviction does not block them.
2. If the eviction point falls inside a sealed chunk, **partially
   evict** by remembering an offset within the front chunk
   (`std::size_t firstChunkOffset_`). Readers then walk
   `*sealedValueChunks_[0]` starting at `firstChunkOffset_`.

### LOD pyramid

Algorithm unchanged. The only change is the data-access mode:
`values_[i]` becomes `valueAt(i)`. Per-bin work is O(binSize)
samples; the chunk-aware index lookup is O(1) per access.

### `BoolTypedBuffer`

Bit-packing makes the storage shape different (one `std::uint64_t`
holds 64 logical samples). Apply the same chunked pattern at the
**word** level: chunks of `std::vector<std::uint64_t>` of size
`kBoolChunkWords = 64` (= 4096 logical samples per chunk to match
the linear case). Tail is a partial word + headBitOffset/totalBits
metadata. Publish copies the sealed-words vector + a snapshot of
the partial-word tail.

If a future measurement shows `BoolTypedBuffer` is not on the M8
hot path (charts typically render bool as a scrolling indicator,
not via 1 kHz query streams), the bool refactor may be deferred
to a follow-up patch — gated by measurement, not assumption.

## Consequences

- Push throughput on long-lived buffers improves from
  O(M²/100) total work to O(M) total work (modulo the chunk-vector
  copy overhead, which is O(M / kChunkSize) total).
- Per-publish memory traffic drops by 200× at N = 1 M.
- Reader staleness is unchanged from M6 — cadence stays at 100.
- Reader API surfaces are unchanged: `queryRange`, `queryLatest`,
  `queryLatestOne` accept the same arguments and return the same
  shapes with the same freshness as M6.
- No `.hpp` change. No test-fixture change. No spec amendment.
- The HALT report from the discarded adaptive-cadence draft stays
  on this branch (`.claude/halt/HALT-20260507T025419Z-...`) as
  the historical record of the design boundary that pushed us
  here.
- A future ADR may revisit `kChunkSize` or the BoolTypedBuffer
  treatment if measurement on a real M8 chart workload surfaces a
  different optimum.

## References

- M6 spec §3.5 (publish-latency contract; lock-free-reads
  guarantee)
- M6-baseline.md (small steady-state-cap measurements,
  pre-chunked-storage)
- M8 prototype RESULTS.md (large-buffer measurement, 600 k preload
  at 14.92 sec, naming the design boundary)
- ADR-002 (precedent for ADR evolution after M2 S2 discovery)
- ADR-004 (precedent for measurement-driven M6 buffer threshold
  revision)
- HALT report `.claude/halt/HALT-20260507T025419Z-publish-cadence-test-conflict.md`
