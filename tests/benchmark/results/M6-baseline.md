# M6 baseline (PRELIMINARY — HALT TRIGGERED)

> ⚠️ This file documents an **incomplete** baseline run. The end-to-end
> overhead scenario tripped HALT trigger #4 (spec §7-4 / plan §3 #4).
> See `.claude/halt/HALT-20260506T081040Z-m6-e2e-overhead.md` for the
> full HALT report and proposed remediation options.

## Run conditions

- Branch: `milestone/M6` at `14d9afb` plus the in-progress
  optimization-pass changes to `src/buffer/signal_buffer.cpp`.
- Preset: `release` (GCC 13, C++23, `-O2`, no ASan).
- Host: shuai-vm, x86_64 Linux 6.8, glibc 2.41.
- Binary: `build/release/tests/benchmark/bench_signal_buffer`.
- Single run (run-to-run variance to be characterized once
  performance lands within HALT thresholds).

## Scenario 1 — writer throughput per type (spec §5.4 #1)

Tight push loop on a single `SignalBuffer` with `capSamples = 10000`
and `windowSeconds = 1e9` (steady-state cap-bounded; no time-window
eviction). Hint rate `1 kHz`.

| Type | Samples | Seconds | Samples / sec | Target | Headroom |
|---|---|---|---|---|---|
| Bool | 2 000 000 | 0.1239 | **16.1 M** | ≥ 1 M | 16× |
| Int64 | 2 000 000 | 0.2351 | **8.5 M** | ≥ 500 k | 17× |
| Double | 2 000 000 | 0.2376 | **8.4 M** | ≥ 500 k | 17× |
| QString | 500 000 | 0.1143 | **4.4 M** | ≥ 200 k | 22× |

✅ All four targets met by an order of magnitude or more. HALT
trigger #3 (writer double < 200 k after one opt pass) **not fired**.

## Scenario 2 — reader throughput (spec §5.4 #2)

Pre-loaded buffer with 60 000 samples (60 s × 1 kHz). Loop
`queryRange(t0, t0 + 60s, target_count = 2000)` for fixed iterations.

| Iterations | Seconds | Queries / sec | Target | Headroom |
|---|---|---|---|---|
| 20 000 | 0.0594 | **337 k** | ≥ 10 k | 33× |

LOD level 3 (1000:1) is selected per spec §4.5 thresholds for the
chosen density.

## Scenario 3 — end-to-end overhead (spec §5.4 #3)

50 000 frames through `SchemaDecoder` (5 fields = 5 signal events
per frame; 250 000 signal events total). Compared:

- **Counter sink**: minimal `SignalValueSink` doing a non-atomic
  increment.
- **M6 path**: `SignalBufferRegistry` with `capSamples = 10 000`,
  rejected-budget ceiling `1 GB` (effectively unlimited).

| Path | Seconds | Frames / sec | Per-frame overhead |
|---|---|---|---|
| Counter | 0.1200 | 416 726 | (baseline) |
| M6 registry | 0.1598 | 312 821 | +127 ns |

| Metric | Target | Result | Status |
|---|---|---|---|
| End-to-end overhead | ≤ 5% (HALT > 10%) | **33.22%** | **❌ HALT** |

🛑 **HALT trigger #4 fired.** Per CLAUDE.md §HALT trigger #6, after
one optimization pass (samples_evicted-on-change + memory_bytes-
on-publish-cadence), overhead remains 33.22% ≫ 10%. See
`.claude/halt/HALT-20260506T081040Z-m6-e2e-overhead.md`.

### Identified overhead sources (per plan §3 trigger #4 requirement)

Per-signal overhead in the M6 path: ~127 ns. Inspection-based
breakdown:

| Source | Estimate | Share |
|---|---|---|
| `std::unordered_map<QString>::find` in `SignalBufferRegistry::onSignal` | ~80–120 ns | ~50–80% |
| `SignalBuffer::push` body | ~50–80 ns | ~33–53% |
| `std::mutex` lock + unlock in `onSignal` | ~25–30 ns | ~17–20% |

The QString-keyed map find dominates. Mutex is secondary.

## Optimization-pass log

| Pass | Change | Effect on overhead |
|---|---|---|
| 1 | `samples_evicted_<id>` skip-when-unchanged + `memory_bytes_<id>` gauge moved to publish cadence | 36.16% → 33.22% |
| 2 (S11.5) | Per-decoder buffer-pointer cache in `SchemaDecoder` (Option A from first HALT) | 33.22% → **26.12%** |

CLAUDE.md §HALT #6 caps further optimization at one pass before HALT;
the human authorized one additional attempt (Option A) at the prior
HALT. After both passes the gate still fails — second HALT filed at
`.claude/halt/HALT-20260506T084448Z-m6-e2e-overhead-after-cache.md`.

## S11.5 numbers (post-cache)

```json
{"scenario":"end_to_end","frames":50000,"counter_seconds":0.1218,"counter_fps":410396.4,"counter_signals":250000,"registry_seconds":0.1537,"registry_fps":325409.4,"overhead_pct":26.12}
```

| Path | Seconds | Frames / sec | Per-frame overhead |
|---|---|---|---|
| Counter (atomic) | 0.1218 | 410 396 | (baseline) |
| M6 registry + cache | 0.1537 | 325 409 | +98 ns |

Per-signal overhead (50 000 frames × 5 signals/frame = 250 000
signals): ~128 ns. Decomposition (post-cache):

| Source | Estimate |
|---|---|
| `SignalBuffer::push` wrapper (3 atomic stores mirroring `impl_`) | ~15-20 ns |
| `TypedBuffer::push` body (variant unpack + deque append + atomics + cadence) | ~50-80 ns |
| `LinearTypedBuffer<T>::onPushCompleted` (LOD bookkeeping) | ~10-20 ns |
| Cache lookup + lambda dispatch in `tryDecodeFrame` | ~5-10 ns |

## Status

| Acceptance gate | Status |
|---|---|
| Writer throughput per type | ✅ |
| Reader throughput | ✅ |
| End-to-end overhead | ❌ HALT |
| Run-to-run variance < 5% | ⏳ pending re-run after HALT resolution |

Variance characterization will be added once the overhead lands
within HALT thresholds and the baseline is finalized.
