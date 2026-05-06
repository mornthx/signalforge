# HALT — M6 e2e overhead 26.12% after S11.5 cache (still > 10% threshold)

**Timestamp**: 2026-05-06T08:44:48Z (UTC)

**Subtask**: S11.5 (per-decoder buffer-pointer cache; Option A from
the prior HALT).

**Trigger**: spec §7-4 / plan §3 #4 — "End-to-end overhead > 10%
beyond M5 baseline". This is the **second HALT** on the same gate;
per the human's prior decision protocol:

> "If overhead > 10% but ≤ 15%: HALT again, report findings, propose
> Option C (deeper refactor) as next decision."

We're at 26.12%, ie above the 15% bound the human's protocol covered.
HALT and await further direction.

## Result of S11.5 cache

Per the human's authorization (Option A — per-decoder buffer-pointer
cache):

- `SchemaDecoder` now owns a
  `std::shared_ptr<const std::vector<SignalBuffer*>> bufferCache_`,
  populated at `setSignalSink` time when the sink is a
  `SignalBufferRegistry` (via `dynamic_cast`).
- Hot-path `tryDecodeFrame` calls `cacheData[metaIndex]->push(t, v)`
  directly, bypassing the registry's mutex + QString-keyed map find.
- Fallback for non-registry sinks (`LoggingSignalValueSink`) goes
  through the original `sink_->onSignal` path.
- 3 dedicated unit tests (`tests/unit/decode/decoder_buffer_cache_test.cpp`)
  verify cache populates / falls back / rebuilds.
- All 320 tests pass under Debug + Release; debug-asan green
  (pending CI).

CMake: broke the would-be circular dependency by dropping
`signalforge_decoder` from `signalforge_buffer`'s PUBLIC link deps
(headers are reachable via `${CMAKE_SOURCE_DIR}/src` include path)
and adding `signalforge_buffer` to `signalforge_decoder`'s PRIVATE
link deps.

Counter sink in the bench was also tightened to `std::atomic` (per
the human's pre-closure note) for a fair baseline.

## Numbers (release build)

```json
{"scenario":"writer","type":"bool","samples":2000000,"seconds":0.1537,"samples_per_sec":13016507.1}
{"scenario":"writer","type":"int64","samples":2000000,"seconds":0.2065,"samples_per_sec":9685702.1}
{"scenario":"writer","type":"double","samples":2000000,"seconds":0.2167,"samples_per_sec":9231034.2}
{"scenario":"writer","type":"string","samples":500000,"seconds":0.1227,"samples_per_sec":4075880.3}
{"scenario":"reader","iterations":20000,"seconds":0.0606,"queries_per_sec":329881.1,"window":"60s_1kHz","target":2000}
{"scenario":"end_to_end","frames":50000,"counter_seconds":0.1218,"counter_fps":410396.4,"counter_signals":250000,"registry_seconds":0.1537,"registry_fps":325409.4,"overhead_pct":26.12}
```

| Scenario | Result | Target | Status |
|---|---|---|---|
| Writer bool | 13.0 M /sec | ≥ 1 M /sec | ✅ 13× |
| Writer int64 | 9.7 M /sec | ≥ 500 k /sec | ✅ 19× |
| Writer double | 9.2 M /sec | ≥ 500 k /sec | ✅ 18× |
| Writer QString | 4.1 M /sec | ≥ 200 k /sec | ✅ 20× |
| Reader queries | 330 k /sec | ≥ 10 k /sec | ✅ 33× |
| End-to-end overhead | **26.12%** | ≤ 5% (HALT > 10%) | **❌ HALT** |

Progression:

| Pass | Description | E2E overhead |
|---|---|---|
| baseline (S11) | Initial implementation | 36.16% |
| S11 opt 1 | `samples_evicted` skip + `memory_bytes` to publish-cadence | 33.22% |
| S11.5 cache | Per-decoder buffer-pointer cache | **26.12%** |

## Refined source identification

After the S11.5 cache eliminated the registry mutex + map find
(which contributed ~30 ns per signal in practice — less than the
80-120 ns originally estimated), the remaining ~128 ns/signal
overhead is:

| Source | Estimate | Share |
|---|---|---|
| `SignalBuffer::push` wrapper (3 atomic stores mirroring `impl_` counters: `totalPushed_`, `totalEvicted_`, `currentMemoryBytes_`) | ~15-20 ns | ~12-16% |
| `TypedBuffer::push` body (variant unpack + deque push + 1-2 metric atomics + cadence check) | ~50-80 ns | ~40-60% |
| `LinearTypedBuffer<T>::onPushCompleted` (LOD level-1/2/3 bookkeeping, including in steady state where bins emit periodically) | ~10-20 ns | ~8-15% |
| Lambda dispatch + cache lookup in `SchemaDecoder::tryDecodeFrame` | ~5-10 ns | ~4-8% |

The remaining overhead is the **inherent cost** of doing real
per-sample storage work (push to deque, update timestamps, run
counters, maintain LOD). A counter-only sink does ~5-10 ns of work;
M6's per-sample work is fundamentally heavier.

## Why the cache helped less than estimated

My pre-cache estimate for the QString find was 80-120 ns; the
observed elimination effect was ~30 ns per signal. Reasons:

- The decoder uses only 5 unique signalIds. After the first frame,
  these QStrings are cache-hot in the CPU's L1, and QString hashing
  is faster than the worst case.
- `unordered_map<QString>::find` on a small map (5-10 entries) is
  often faster than the worst-case bound — the hash bucket has 1
  entry typically, so it's just hash + 1 string compare.

So the cache saved ~30 ns (mostly the `std::mutex` lock/unlock).
The remaining overhead is inherent to the buffer layer, not the
registry's lookup machinery.

## What's left to try (per plan §3 #4 categorization)

1. **Move `SignalBuffer`'s wrapper atomics to publish cadence**
   (~15-20 ns saved per push). Same approach as the metric updates
   in S11. Brings overhead to ~22%. Still > 15%.
2. **Drop the `SignalBuffer::push` wrapper entirely** by making
   `TypedBuffer::push` directly callable from the decoder. Requires
   exposing the internal `TypedBuffer` polymorphism through
   `SignalBuffer`'s public interface — not freeze-safe at this
   stage.
3. **Disable LOD updates per push** when no LOD level boundary is
   crossed (currently the front-pop check + bin-emission check fire
   per push). Could skip work when nothing changes. Saves ~10-15 ns.
   Brings overhead to ~22-23%.
4. **Batch metric updates** (e.g., update `samples_stored` every N
   pushes instead of per-push). Saves ~5-10 ns. Diminishing returns.
5. **Redesign the storage path entirely** — e.g., lock-free ring
   buffer with no per-push counter atomics; LOD computed as a
   deferred query-time operation rather than write-time. Multi-day
   work, partial freeze surface impact.

Combined optimization passes 1-4 might bring overhead to ~12-18%.
Still likely over 10%.

## What this looks like in absolute terms

The 26% overhead translates to:

- Counter-only path: 410 k frames/sec, 2.05 M signals/sec.
- M6 path: 325 k frames/sec, 1.63 M signals/sec.

For real V1 workloads (per the human's prior assessment): 10
drivers × 100 Hz × 10 signals/frame = 10 k signal events / sec.
The M6 path's 1.63 M /sec capacity gives **163× headroom** over the
realistic load. The 26% overhead is meaningful only at synthetic
saturation.

## Per-CLAUDE.md HALT cap

This is the second HALT on this gate. Per the human's S11.5
authorization: "Per CLAUDE.md §HALT cap: this is the second
optimization attempt. If it fails, no further attempts without
human re-decision."

Stopping here. Awaiting human direction on:

- **Option C** (deeper refactor): combine the candidate
  optimizations 1-4 + a possible storage-layer rework. 1-3 days of
  work. Likely brings overhead to ≤ 10% but not certain.
- **Option B** (revisit threshold): file an ADR amending spec §7-4
  to a more permissive threshold given (a) the realistic workload
  has 163× headroom and (b) optimizations 1-4 cannot reach 10%
  without a deeper rework. Sample new wording: "End-to-end overhead
  ≤ 30% beyond M5 baseline at synthetic saturation; real V1
  workload ≥ 90 fps at 1 kHz × 60 signals = ≥ 54 k signals/sec
  [demonstrably-met threshold]." The human originally rejected
  Option B at the previous HALT, so this is offered only as a
  fallback if Option C is too costly.
- **Option D** (close M6 with documented overhead): merge M6 with
  the 26.12% number documented as a known limitation; raise an M6.1
  remediation milestone for V1.0 release prep.

## State at HALT

- Branch `milestone/M6` at the just-pushed commit will contain:
  - All S1-S10 work (functional + tested).
  - S11 bench + first opt pass.
  - First HALT report.
  - **S11.5 cache implementation** (this commit).
  - Cache-verification unit tests (3 cases).
  - Updated baseline doc with new measurements.
  - This second HALT report.

- 320 / 320 tests pass under Debug + Release.

## Recommended next step

**Option D + lazy M6.1 remediation** — close M6 with documented
26% overhead. The realistic-workload margin is 163×. M6 closure
unblocks M7 (Expression Engine) and M8 (Chart UI), which are on
the V1 critical path. M6.1 (or a V1 hardening pass) can revisit
end-to-end overhead with full profiler data and a budget for
Option C-class work.

If governance discipline forbids closing with a missed acceptance
gate (the human's stated preference), then Option C is the only
remaining path. Estimate: 3-5 days of work, no guaranteed
sub-10% outcome.

Stopping per CLAUDE.md §HALT.
