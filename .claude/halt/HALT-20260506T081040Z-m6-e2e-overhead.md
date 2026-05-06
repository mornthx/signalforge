# HALT — M6 end-to-end overhead exceeds spec §7-4 threshold

**Timestamp**: 2026-05-06T08:10:40Z (UTC)

**Subtask**: S11 (benchmark) of M6 plan.

**Trigger**: spec §7 #4 / plan §3 #4 — "End-to-end overhead > 10%
beyond M5 baseline".

**Trigger condition**: the M6 end-to-end benchmark scenario shows
**33.22% overhead** beyond the M5 standalone baseline (counter-only
sink).

## Measurement context

`tests/benchmark/bench_signal_buffer.cpp` runs three scenarios (see
plan §S11 + spec §5.4). The end-to-end scenario feeds 50 000 frames
through `SchemaDecoder` configured against the simple schema (5
fields, 4 numeric + 1 magic byte). Each frame produces 5 signal
events, totaling 250 000 signal events. The same decoder is
exercised twice:

1. **Counter sink** — a minimal `SignalValueSink` whose `onSignal`
   does a non-atomic increment.
2. **M6 path** — `SignalBufferRegistry` as the sink, with each
   signal routed to its `SignalBuffer` via `onSignal → mutex + map
   find → SignalBuffer::push`.

## Numbers

After one optimization pass that (a) skipped the
`samples_evicted` metric update unless eviction happened and
(b) moved the `memory_bytes` gauge update from per-push to the
publish cadence:

```json
{"scenario":"writer","type":"bool","samples":2000000,"seconds":0.1239,"samples_per_sec":16144289.0}
{"scenario":"writer","type":"int64","samples":2000000,"seconds":0.2351,"samples_per_sec":8507961.6}
{"scenario":"writer","type":"double","samples":2000000,"seconds":0.2376,"samples_per_sec":8417317.4}
{"scenario":"writer","type":"string","samples":500000,"seconds":0.1143,"samples_per_sec":4373344.9}
{"scenario":"reader","iterations":20000,"seconds":0.0594,"queries_per_sec":336532.0,"window":"60s_1kHz","target":2000}
{"scenario":"end_to_end","frames":50000,"counter_seconds":0.1200,"counter_fps":416726.0,"counter_signals":250000,"registry_seconds":0.1598,"registry_fps":312821.3,"overhead_pct":33.22}
```

| Target | Threshold | Result | Status |
|---|---|---|---|
| Writer bool | ≥ 1 M /sec | 16.1 M /sec | ✅ 16× |
| Writer int64 | ≥ 500 k /sec | 8.5 M /sec | ✅ 17× |
| Writer double | ≥ 500 k /sec | 8.4 M /sec | ✅ 17× |
| Writer QString | ≥ 200 k /sec | 4.4 M /sec | ✅ 22× |
| Reader queries | ≥ 10 k /sec | 337 k /sec | ✅ 33× |
| End-to-end overhead | ≤ 5% (HALT > 10%) | **33.2%** | **❌ HALT** |

The writer / reader targets are met by an order of magnitude or
more. Only the end-to-end overhead trips the HALT trigger.

## Source identification (per plan §3 #4 requirement)

Per-signal overhead in the M6 path vs the counter path is **~150 ns**
(from 480 ns/signal counter → 640 ns/signal M6 = 250 K signals × 150
ns ≈ 38 ms over a 120 ms baseline). Decomposition (estimates from
inspection, not profiler):

| Source | Estimate | % of 150 ns |
|---|---|---|
| `std::mutex` lock + unlock in `SignalBufferRegistry::onSignal` | ~25–30 ns | ~17–20% |
| `std::unordered_map<QString>::find` (QString hash + Unicode-aware compare) | ~80–120 ns | ~50–80% |
| `SignalBuffer::push` (variant unpack + deque push + counter atomics + cadence check + LOD logic) | ~50–80 ns | ~33–53% |

The largest single contributor is the **QString-keyed map find**.
The mutex is not the dominant cost.

## Optimization passes attempted

- **Pass 1** (committed in this branch's bench commit):
  - `samples_evicted_<id>` metric set only when eviction occurred.
  - `memory_bytes_<id>` gauge moved from per-push to per-publish.

  Effect: 36.16% → 33.22%. ~3 percentage-point reduction. Below the
  10% threshold not reached. CLAUDE.md §HALT #6 caps optimization at
  one pass before HALT.

## Candidate next-step optimizations (for human decision)

The user will likely want to choose between these (or accept the
overhead). None has been implemented; they are listed as
"identification of overhead source + remediation options" per spec
§7-4 wording.

1. **Replace QString find with a small-fixed-size cache.** The
   decoder repeatedly emits the same 5 signalIds per frame. Caching
   per-decoder a `signalId → SignalBuffer*` mapping (built at
   `setSignalSink` time when the catalog is known) would eliminate
   the hot-path map lookup. Estimated overhead reduction: ~80 ns
   per signal → e2e overhead ~7%, **under the 10% HALT threshold**.
   Cost: ~3-5 hours. Surface area: per-decoder cache, no public-API
   change, freeze-compatible.

2. **Replace `std::mutex` with `std::shared_mutex` + atomic-snapshot
   map for read paths.** The registry's `buffersBySignalId_` becomes
   a `std::atomic<std::shared_ptr<const Map>>` (mirroring M2's
   Snapshot pattern). Lock-free reads. Estimated reduction: ~25 ns
   per signal → e2e overhead ~28%. **Insufficient alone.**

3. **Pre-compute a numeric `signalId` (hash-cached or interned).**
   Add `SignalMetadata::idHash` populated by the decoder at
   registration time; registry indexes by hash. Requires extending
   `SignalMetadata` — **violates M5 freeze**. Not pursuable without
   ADR.

4. **Accept the overhead.** Per M5's `done.md` hand-off: "M6's
   per-signal storage should not drop below ~80% of these numbers".
   Counter baseline 416 K fps × 0.80 = 333 K fps minimum; M6 path
   measures 313 K fps = 75% of baseline, **5 percentage-points below
   the M5 hand-off floor**. So even relaxing to M5's hand-off
   tolerance still trips. Not viable.

## State of the milestone

S1-S10 are complete and CI-green:

- 317 unit + integration tests pass.
- All other HALT triggers (#1, #2, #5, #6, #7) verified clear.
- The buffer / registry / production wiring is functionally
  complete and correct. The only failing acceptance is the
  end-to-end overhead.

## Files committable at HALT time

- `tests/benchmark/bench_signal_buffer.cpp` — the benchmark itself.
- `src/buffer/signal_buffer.cpp` — the optimization-pass changes
  (samples_evicted skip + memory_bytes move-to-publish-cadence).
- `tests/benchmark/results/M6-baseline.md` — partial baseline doc
  capturing the failing measurement (will be authored in the
  closing commit alongside this HALT report).

CI on debug-asan is expected to remain green for the optimization
changes (they are pure simplification of the metric-update logic;
no new locks or shared state).

## Recommended next-step action

**Option A** — implement candidate optimization #1 (per-decoder
buffer-pointer cache). Highest probability of bringing overhead under
10% HALT threshold within 5 hours of work, no freeze-surface impact,
no spec amendment.

**Option B** — file an ADR amending the spec §7-4 threshold (e.g.,
to 25% or to "≥ 80% of M5 baseline"). The 5% target is aspirational
given the architecture (mutex + map lookup + per-event
SignalValueSink interface); 33% is too high but a more permissive
threshold may be defensible for V1.

**Option C** — implement a more invasive redesign (cache-friendly
signalId hashing, lock-free registry map, batched metric updates).
Multi-day work. Likely brings overhead to ~5%.

I recommend Option A for the next session — surgical, freeze-safe,
high probability of clearing the trigger.

## Stop

Per CLAUDE.md §HALT: "do not attempt to continue. Do not try
alternative approaches beyond the attempt limits below". One
optimization pass attempted; HALT.

Awaiting human decision on Option A / B / C / other.
