# ADR-005 — SignalBuffer publish cadence scales with sample count

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

## Context

M6 set `kDefaultPublishCadence = 100` as a fixed constant
(`src/buffer/signal_buffer.cpp:40`). This was sized for the
benchmarked steady-state cap of 10 000 samples (M6-baseline.md
Scenario 1), where each publish snapshots ≤ 10 k elements
(~80 KB) and total work over the 2 M-sample writer benchmark is
manageable.

The M8 chart use case retains hours of data per signal — buffer
sizes routinely 0.6 M to 3.6 M samples. At those sizes:

- Per-publish copy work: O(N) memory and O(N) elements
- Per-push amortized cost: O(N / 100), which **grows with N**
- Total work for inserting M samples into a buffer that retains
  all of them: O(M²/100)

Measurement (M8 prototype Scenario 4 preload, 600 k samples):

| Metric | Value | M6 baseline reference |
|---|---:|---:|
| Wall throughput | ~40 k samples/sec | 8.8 M/sec (10 k cap) |
| Cause | publish copy at large N | (small steady-state cap) |
| Slowdown vs M6 baseline | 220× | — |

This is a **regime difference**, not a regression: M6 baseline never
measured large-buffer push throughput. The M8 chart use case is
the first one to exercise it.

## Reader-staleness tradeoff

`publishSegment` exists so that readers (`queryRange`, etc.) see an
immutable consistent snapshot without locking against the writer.
Increasing the cadence increases the time gap between when a sample
is pushed and when readers can observe it.

Current cadence (every 100 pushes):

| Sample rate | Latency to publish |
|---|---:|
| 1 kHz | 100 ms |
| 100 Hz | 1 sec |
| 30 Hz | 3.3 sec |

For interactive charts the relevant metric is "what does the user
see *now*?" The chart only redraws at 30 Hz anyway, so the
practical floor on visible latency is ~33 ms; any publish cadence
finer than that is wasted work.

## Decision

Replace the fixed `kDefaultPublishCadence = 100` with an **adaptive
cadence** computed at every push:

```
cadence = clamp( 100, sampleCount / kCadenceRatio, kMaxCadence )
```

with `kCadenceRatio = 100` and `kMaxCadence = 5000`.

Behavior:

| `sampleCount` | Cadence | Per-publish work | Amortized per-push |
|---:|---:|---:|---:|
| ≤ 10 k | 100 | O(10 k) | O(100) |
| 100 k | 1 000 | O(100 k) | O(100) |
| 500 k | 5 000 (capped) | O(500 k) | O(100) |
| 1 M | 5 000 (capped) | O(1 M) | O(200) |
| 3.6 M | 5 000 (capped) | O(3.6 M) | O(720) |

The cap at 5 000 bounds reader staleness:

| Sample rate | Worst-case staleness |
|---|---:|
| 1 kHz | 5 sec |
| 100 Hz | 50 sec |
| 30 Hz | 167 sec (!) |

For the M8 chart use case (1 kHz live data), 5 seconds is the
practical worst case — acceptable for a chart that already only
redraws at 33 ms cadence and is typically scrolling several
seconds of history per redraw. For slower-rate signals
(30 Hz / 100 Hz typical control-loop telemetry), the ceiling on
absolute time-staleness is dominated by `kMaxCadence`; downstream
components that want fresher snapshots can call `queryLatestOne()`
which always reflects the current writer state (it does not go
through the published Segment).

Note: `queryLatestOne` already bypasses the published Segment for
its return value (it reads `mostRecentValue_` directly under a
lightweight lock), so single-value liveness is **not** affected by
the cadence change. Only `queryRange` and `queryLatest(n)` see
delayed history.

## Acceptance criteria

1. **HALT trigger #5** (this task): 1 M-sample push throughput
   ≥ 500 k samples/sec on a single buffer with default config.
2. **HALT trigger #3** (this task): M6 e2e overhead remains
   ≤ 30% (ADR-004 threshold).
3. **HALT trigger #4**: every existing test in M6 + M7 still
   passes (no test loosening — if a test fails, root-cause it).
4. **HALT trigger #2**: `queryRange` / `queryLatest` /
   `queryLatestOne` return values bit-identical to pre-patch on
   the existing test fixtures.
5. **HALT trigger #1**: no `.hpp` changes (the cadence is an
   internal implementation detail; the public Segment layout is
   unchanged).

## Alternatives considered

### A. Segmented copy-on-write storage

Replace `std::deque<T>` with a segmented structure where each
chunk is `shared_ptr<const std::vector<T>>` of fixed size. Push
appends to a mutable tail chunk; when full, the chunk is frozen
and shared into a "spine". Publish snapshots only the spine
(O(N/chunkSize)).

**Pros**: Per-publish cost is O(N / chunkSize) — orders of
magnitude faster. Per-push amortized cost is O(1) regardless of
N. No reader-staleness tradeoff.

**Cons**: Substantial refactor of the buffer internals (`pushValue`,
`evictByWindow`, `clearValues`, the LOD bin emission, the eviction
front-trimming). Would touch ~500 lines of `signal_buffer.cpp`.
Higher risk for a measurement-driven follow-up patch.

**Verdict**: Right long-term solution. Out of scope for this
patch; revisit as ADR-006 if the adaptive-cadence approach proves
insufficient or the staleness ceiling is a problem.

### B. Lazily skip publishes when buffer hasn't grown

`pushesSincePublish_` only increments on successful push; if push
fails or evicts the just-pushed sample, no publish needed. This
already happens implicitly. Doesn't address the O(N) copy cost
per publish.

### C. Defer publish to a worker thread

Hand the snapshot construction off to a background thread so the
writer never pays the O(N) copy. Adds threading complexity (a per-
buffer worker, lifetime management, ordering guarantees) for a
benefit that's bounded by the worker's own throughput. Doesn't
address the underlying O(M²) total-work problem.

### D. Just bump the fixed cadence to 1000

Naive: doesn't scale with N. At N = 100 k, cadence = 1000 still
gives O(N/1000) = 100 amortized — acceptable. At N = 1 M, the
per-publish cost (O(1 M)) divided by cadence 1000 = O(1000), a
10× regression vs Option A in this ADR. Would also impose a fixed
1 sec staleness on the small-buffer case where 100 ms is fine.

## Consequences

- Push throughput on long-lived buffers improves from O(M²/100)
  total work to O(M × max(100, N/100)) = O(M × N/100), capped at
  O(M × 5000) for N > 500 k.
- Reader staleness ceiling rises from 100 ms (at 1 kHz) to 5 s
  (at 1 kHz) on the largest buffers. `queryLatestOne` is
  unaffected.
- No public-API change. No `.hpp` change. No test-fixture change.
- The `kCadenceRatio` and `kMaxCadence` tunables are documented
  in `signal_buffer.cpp` near the constants; adjusting them is
  an internal change subject only to the e2e-overhead and
  large-buffer-throughput acceptance gates.
- A future ADR-006 may revisit Option A above if the M8 chart
  subsystem hits the staleness ceiling in practice.

## References

- M6 spec §3.5, §4.4 (publish cadence design intent)
- M6-baseline.md (small steady-state-cap measurements)
- M8 prototype RESULTS.md (large-buffer measurements,
  600 k preload at 14.92 sec)
- ADR-004 (precedent for measurement-driven M6 buffer threshold
  revision)
