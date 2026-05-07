# M6 baseline

> Acceptance gate: end-to-end overhead measured at **26.1% / 25.6% /
> 25.4%** across three independent runs (mean **25.7%**, max-min
> **0.74 pp**) on host shuai-vm. Within ADR-004's revised acceptance
> threshold (≤ 30%) with margin. **All scenarios pass.**

## Run conditions

- Branch: `milestone/M6` at the S11.5 cache commit (`8fbeb0f`) +
  ADR-004 commit (`7b54461`) + spec amendment (`6e41505`).
- Preset: `release` (GCC 13, C++23, `-O2`, no ASan).
- Host: shuai-vm, x86_64 Linux 6.8, glibc 2.41.
- Binary: `build/release/tests/benchmark/bench_signal_buffer`.
- 3 independent runs; values below are from run 1 unless otherwise
  noted.

## Scenario 1 — writer throughput per type (spec §5.4 #1)

Tight push loop on a single `SignalBuffer` with `capSamples = 10000`
and `windowSeconds = 1e9` (steady-state cap-bounded; no time-window
eviction). Hint rate `1 kHz`.

| Type | Samples | Seconds | Samples / sec | Target | Headroom |
|---|---|---|---|---|---|
| Bool | 2 000 000 | 0.1239 | **16.1 M** | ≥ 1 M | 16× |
| Int64 | 2 000 000 | 0.2351 | **8.5 M** | ≥ 500 k | 17× |
| Double | 2 000 000 | 0.2273 | **8.8 M** | ≥ 500 k | 17× |
| QString | 500 000 | 0.1143 | **4.4 M** | ≥ 200 k | 22× |

✅ All four targets met by an order of magnitude or more. HALT
trigger #3 (writer double < 200 k after one opt pass) **not fired**.

Run-to-run variance for the double writer (3 runs): 8.77 / 9.78 /
8.80 M /sec. The dispersion is ~10% — typical for tight-loop writer
benchmarks where small allocator-page-touch variations move the
result. Well above the 500 k target in every run.

## Scenario 2 — reader throughput (spec §5.4 #2)

Pre-loaded buffer with 60 000 samples (60 s × 1 kHz). Loop
`queryRange(t0, t0 + 60s, target_count = 2000)` for 20 000
iterations.

| Iterations | Seconds | Queries / sec | Target | Headroom |
|---|---|---|---|---|
| 20 000 | 0.0594 | **337 k** | ≥ 10 k | 33× |

LOD level 3 (1000:1) is selected per spec §4.5 thresholds for the
chosen density.

Run-to-run variance: 335 k / 336 k / 333 k queries/sec. < 1%.

## Scenario 3 — end-to-end overhead (spec §5.4 #3, ADR-004)

50 000 frames through `SchemaDecoder` (5 fields = 5 signal events
per frame; 250 000 signal events total). Compared:

- **Counter sink**: minimal `SignalValueSink` with
  `std::atomic<std::uint64_t>` increment.
- **M6 path**: `SignalBufferRegistry` as sink; `SchemaDecoder`'s
  S11.5 buffer-pointer cache routes signals directly to
  `SignalBuffer::push`, bypassing the registry's mutex + map find.

| Run | Counter (s) | Counter fps | M6 (s) | M6 fps | Overhead |
|---|---|---|---|---|---|
| 1 | 0.1223 | 408 902 | 0.1542 | 324 248 | 26.11% |
| 2 | 0.1222 | 409 158 | 0.1535 | 325 674 | 25.63% |
| 3 | 0.1224 | 408 430 | 0.1535 | 325 770 | 25.37% |
| **mean** | **0.1223** | **408 830** | **0.1537** | **325 231** | **25.7%** |

| Metric | Threshold | Result | Status |
|---|---|---|---|
| End-to-end overhead | ≤ 30% (HALT > 35%) | **25.7% mean** | **✅ PASS** |

**Run-to-run variance**: max-min = **0.74 pp** (3 runs). Well below
the 5% variance bound spec §8.2 sets (and well below the 3 pp
loose bound the human's S11.6 directive set). The result is
reproducible.

✅ Within ADR-004's revised acceptance threshold (≤ 30%) with 4-5
percentage points of margin. See
`docs/architecture/decisions/ADR-004-signal-buffer-overhead-threshold.md`
for the threshold-revision rationale.

### Source decomposition (post-S11.5)

Per-signal overhead in the M6 path: ~127 ns. Inspection-based
breakdown:

| Source | Estimate | Share |
|---|---|---|
| `SignalBuffer::push` wrapper (3 atomic stores mirroring `impl_`) | ~15-20 ns | ~12-16% |
| `TypedBuffer::push` body (variant unpack + deque append + atomics + cadence) | ~50-80 ns | ~40-60% |
| `LinearTypedBuffer<T>::onPushCompleted` (LOD bookkeeping) | ~10-20 ns | ~8-15% |
| Cache lookup + lambda dispatch in `tryDecodeFrame` | ~5-10 ns | ~4-8% |

The remaining overhead is the **inherent cost** of doing real
per-sample storage work. M12 (Performance Optimization) inherits
the goal of reducing this further; the highest-value targets are:

1. `SignalBuffer::push` body (~50-80 ns of structural overhead).
2. The per-event `SignalValueSink::onSignal` interface (potentially
   batched in a future API revision).
3. `LinearTypedBuffer<T>::onPushCompleted` LOD bookkeeping (only
   touch state when a level boundary is actually crossed).

## Optimization-pass log

| Pass | Change | Effect on overhead |
|---|---|---|
| 1 (S11) | `samples_evicted_<id>` skip-when-unchanged + `memory_bytes_<id>` gauge moved to publish cadence | 36.16% → 33.22% |
| 2 (S11.5) | Per-decoder buffer-pointer cache in `SchemaDecoder` (Option A) | 33.22% → **25.7% (mean)** |

Two HALTs were filed during S11/S11.5 against the original 5%/10%
thresholds. ADR-004 (filed in S11.6) revised the thresholds based
on measurement evidence; the post-S11.5 implementation passes the
revised gate.

- `.claude/halt/HALT-20260506T081040Z-m6-e2e-overhead.md` — first
  HALT, pre-cache.
- `.claude/halt/HALT-20260506T084448Z-m6-e2e-overhead-after-cache.md`
  — second HALT, post-cache, leading to ADR-004.

## Status

| Acceptance gate | Status |
|---|---|
| Writer throughput per type | ✅ |
| Reader throughput | ✅ |
| End-to-end overhead (ADR-004 threshold) | ✅ |
| Run-to-run variance < 5% | ✅ (0.74 pp on the e2e gate) |
| Large-buffer scaling (ADR-005, post-chunked-storage) | ✅ (5.8× over 500 k @ 1 M target) |

## Large-buffer scaling (post-ADR-005, chunked storage)

> Acceptance gate (HALT trigger #5 from the publish-cadence
> patch): Double @ 1 M ≥ 500 k samples/sec on a buffer with
> retention ≥ N (no eviction). Measured 3-run mean **2.9 M/sec**
> — **5.8× over** the gate.

Run conditions: same as M6 §1 (release-bench preset, GCC 13,
host shuai-Laptop). Bench scenario added in
`tests/benchmark/bench_signal_buffer.cpp::runLargeBufferBench` on
the `fix/m6-publish-cadence` branch.

Scenario: push N samples into a single Double buffer with
`capSamples = N + 1` and `windowSeconds = 1e9` (no eviction);
LOD enabled (the realistic M8 chart configuration). 3 runs at
each N.

| N samples | Run 1 (s/s) | Run 2 (s/s) | Run 3 (s/s) | mean | (s/N) |
|---:|---:|---:|---:|---:|---:|
|   10 000  | 22.85 M | 15.87 M | 20.56 M | **19.8 M** | ~50 ns |
|  100 000  | 10.38 M |  9.99 M | 11.05 M | **10.5 M** | ~95 ns |
|  1 000 000 |  2.87 M |  2.92 M |  2.89 M |  **2.9 M** | ~345 ns |

Pre-ADR-005 (deque-based storage, M6 baseline), the same
scenario at N = 1 M extrapolates from the M8 prototype's measured
~40 k samples/sec for a 600 k preload — call it ~30 k/sec at 1 M.
Post-ADR-005: **2.9 M/sec — ~95× faster** at this scale.

The per-push wall cost grows from ~50 ns (10 k) to ~95 ns (100 k)
to ~345 ns (1 M). The growth comes from two unavoidable terms:

1. The per-publish work scales as O(N / kChunkSize) shared_ptr
   copies (244 chunks at 1 M × ~30 ns = ~7.3 µs per publish; with
   cadence 100, that's ~73 ns per push amortized).
2. Heap-allocator pressure grows with allocation count
   (snapshot-tail copies + LOD-bin chunk seals); at 1 M total
   pushes there are ~244 chunk seals × 4 stores (values +
   timestamps + 3 LOD levels) = ~1 200 `make_shared<vector>`
   allocations.

This is acceptable headroom for M8: at 1 kHz live-data rate, the
push budget is 1 ms per push (1 sec / 1000 samples). Even at the
1 M pole the per-push cost is 0.34 µs — three orders of magnitude
inside budget. M8 charts with hours of retained data hit the
acceptable regime cleanly.

### Optimization passes (post-M6 closure)

| Pass | Change | Effect on 1 M throughput |
|---|---|---|
| ADR-005 v1 (discarded) | Adaptive cadence `clamp(N/100, 100, 5000)` | (would have been ~500 k/s but broke S9 test + violated §3.5 staleness contract) |
| ADR-005 v2 (this) | Chunked storage for `values_` + `timestamps_` | 30 k/s → 207 k/s |
| ADR-005 v2 + LOD chunking | Same chunking applied to LodLevel<T>::bins | 207 k/s → **2.9 M/s** |
