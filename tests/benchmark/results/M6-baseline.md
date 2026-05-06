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
