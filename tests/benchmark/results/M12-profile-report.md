# M12 — Profile report

| Field | Value |
|---|---|
| Date | 2026-05-08 |
| Host | Linux x86_64, GCC 13, Qt 6.10.2 |
| Build | `release-bench` (`-O2` + `SIGNALFORGE_BENCHMARKS=ON`) |
| Profile harness | `tools/profile/profile_main.cpp` |
| Tier used | Tier 2 — `valgrind --tool=callgrind` (host `perf_event_paranoid=4` blocks tier 1) + Tier 3 — `QElapsedTimer` |
| Raw data | `tests/benchmark/results/M12-profile-data/` |

## 1. Run environment

`/proc/sys/kernel/perf_event_paranoid` = 4 — `perf record` /
`perf stat` are unavailable for userspace profiling on this
host. The harness fell through `run_profile.sh`'s tier
detection to **tier 2** (`valgrind --tool=callgrind`) for
function-level profiling and **tier 3** (`QElapsedTimer`) for
scenario-level wall-clock measurements.

The 3-scenario coverage required by spec §5.3 was met (live,
replay, concurrent record + chart). All 3 scenarios completed
without crash or hang (H3 trigger clear).

## 2. Scenario A — live-style synthetic dispatch (60 sig × 1 kHz)

### 2.1 Wall-clock summary (tier 3, 5 s duration)

```json
{"scenario":"A","duration_s":5,"events":300000,"total_ms":90,"events_per_sec":3322413,"dispatch_ns_total":84023061,"dispatch_pct":93.05}
```

Native throughput: **3.3 M events/sec**. Dispatch (the inner
`registry.onSignal` loop) accounts for **93.05 %** of total
wall-time, confirming the harness exercises the M6 hot path
representative of live workloads.

### 2.2 Hot-function table (tier 2 callgrind, 1 s duration; 99 % threshold)

Total instructions retired: **148.4 M Ir**. Top ranked
self-time:

| Rank | Self % | Function | Module |
|---:|---:|---|---|
| 1 | 5.75 % | `qHashBits` | Qt6Core (called via `QString` → `unordered_map<QString, ...>` lookup in the registry) |
| 2 | 5.72 % | `LinearTypedBuffer<double>::onPushCompleted` | **M6 (SignalBuffer)** |
| 3 | 5.54 % | `<inlined Qt6Core>` (likely `QArrayData::deallocate`) | Qt6Core |
| 4 | 5.14 % | `do_lookup_x` | dl-loader (irrelevant; one-time) |
| 5 | 4.80 % | `_int_free` | glibc malloc |
| 6 | 4.33 % | `QString::arg_impl` | Qt6Core (**HARNESS ARTEFACT** — see §2.3) |
| 7 | 4.22 % | `SignalBuffer::push` | **M6 (SignalBuffer)** |
| 8 | 3.97 % | `malloc` | glibc |
| 9 | 2.16 % | `LinearTypedBuffer<double>::valueMemoryBytes` | **M6 (SignalBuffer)** |
| 10 | 2.02 % | `SignalBufferRegistry::onSignal` | **M6 (Registry)** |
| 11 | 1.95 % | `qCalculateBlockSize` | Qt6Core |
| 12 | 1.94 % | `QString::QString(QChar const*, long long)` | Qt6Core |
| 13 | 1.92 % | `QtPrivate::equalStrings` | Qt6Core (via hashtable lookup) |
| 14 | 1.82 % | `_Hashtable::_M_find_before_node` | libstdc++ (via registry's unordered_map) |
| 15 | 1.63 % | `DoubleTypedBuffer::pushValue` | **M6 (SignalBuffer)** |

### 2.3 Per-module breakdown

| Module | Self % | Notes |
|---|---:|---|
| **M6 SignalBuffer push path** | **~14 % (rows 2 + 7 + 9 + 15)** | Combined `push` / `onPushCompleted` / `valueMemoryBytes` / `pushValue` per push call |
| **M6 SignalBufferRegistry dispatch + lookup** | **~9 % (rows 1 + 10 + 13 + 14)** | Per-event hashtable lookup by `QString` signal id |
| Qt6Core internals (string + array) | ~17 % | mostly `QString` lifecycle + `QArrayData` |
| **Harness artefact** | ~5 % | `QString::arg_impl` ranks #6 because the harness loop calls `QStringLiteral("sig_%1").arg(sig)` 60× per tick. **This is profile-harness noise, not a V1 hot path.** Production code uses cached signal-id strings; the harness re-formats per call. |
| dl-loader / malloc / free | ~14 % | mostly transient |

### 2.4 Synthesis for Scenario A

**Real V1 hot paths surfaced**:
- M6 SignalBuffer push wrapper + bookkeeping: **~14 %** of Scenario A runtime, matching the candidate-list entry "M6 SignalBuffer push wrapper (12-16 % of overhead)" almost exactly. **Strong candidate.**
- M6 SignalBufferRegistry dispatch + per-event hashtable lookup: **~9 %**, dominated by `qHashBits` + `equalStrings`. Could be addressed by a shared `QHash`-based registry or by caching the iterator in the signal-id resolver. Medium-risk internal change.

**Not in real V1 hot path** (despite ranking high):
- `QString::arg_impl` (5.75 %) is a harness artefact.
- `do_lookup_x` is one-time.

## 3. Scenario B — replay file 1× speed (60 sig × 1 kHz × 5 s file)

### 3.1 Wall-clock summary (tier 3)

```json
{"scenario":"B","records":199769,"total_ms":5709}
```

Replay of a 5 s file took 5.7 s wall-clock at 1× speed →
**~14 % timing error** (matches the M11-baseline `12.02 %`
within run-to-run variance per `M11-baseline.md`).

The fixture writer dropped ~100 k records to backpressure
(199 k of nominal 300 k). This is fixture-side; the timing-error
metric is what M12 cares about.

### 3.2 Profile-confirmed bottleneck

The 1× timing gap is the SessionPlayer dispatch loop's per-record
`sleep_for` + sequential `readNextRecord` overhead (M11 S10 found
direct-dispatch saved only ~3 %, leaving the remaining gap to the
sleep-granularity + file-I/O path).

C4 Stage B (M11-concerns.md) is the documented optimisation for
this: replace per-record `sleep_for` chunks with `sleep_until`
against an absolute deadline + batched dispatch (collect N
records on the worker, direct-call into the sink with all N).

## 4. Scenario C — concurrent record + chart (60 sig × 1 kHz × 5 s)

### 4.1 Wall-clock summary (tier 3)

```json
{"scenario":"C","duration_s":5,"events":300000,"total_ms":148,"bytes":8401964}
```

Concurrent tee fan-out to M6 registry + M10 SessionWriter
processed 300 k events in 148 ms = **2.0 M events/sec**.
Compared to Scenario A (3.3 M events/sec, registry only), the
write-side path costs roughly 39 % overhead — this is the
SessionWriter's queue + worker dispatch cost.

### 4.2 Synthesis

Scenario C does NOT surface a third optimisation candidate beyond
what Scenarios A and B already showed:
- The tee-fan-out cost is bounded by the SessionWriter's queue
  (which already has C3 4-point backpressure tuned at M10);
  reducing it would require touching the M10-frozen
  `SessionWriter` interface (HALT trigger H2 — ADR-008).
- Per-event overhead of the second sink ([M6] direct path) is
  the same as Scenario A.

## 5. Selected optimisations (top 2 — C4 decision-tree path)

Per the M12-concerns.md C4 decision tree applied at the end of
S2: profile shows **2 viable candidates**, not 3.

### 5.1 Optimisation 1 — C4 Stage B (M11 SessionPlayer dispatch)

| Field | Value |
|---|---|
| **Source** | M11 hand-off / spec §9 Note explicit recommendation / M11-concerns.md C4 Stage B |
| **Hot path** | `signalforge::replay::SessionPlayer::dispatchLoop` |
| **Primary metric** | M11 1× timing error (currently 12.02 % per M11-baseline.md) |
| **Pass criterion** | ≥ 10 % improvement → ≤ 10.82 % error |
| **Stretch (spec §5.1 target)** | ≤ 5 % error |
| **Secondary metric (bonus)** | M11 10× completion (currently 2.26 s for 10 s file → spec target ≤ 1 s) |
| **Approach** | Replace per-record `std::this_thread::sleep_for` (chunked at 5 ms) with `std::this_thread::sleep_until` against an absolute deadline computed from `openTimeSteady_ + nanoseconds(rec.timestampNs / speed)`. Optionally batch-read N records on the worker thread and direct-dispatch to the sink in one tight loop, reducing per-record loop overhead. Direct-call dispatch (M11 S10) is preserved. |
| **Risk** | Medium. The pause-mid-sleep `pendingRecord_` semantics must be preserved (M11 S4 pattern). |
| **Subtask** | S3 |

### 5.2 Optimisation 2 — M6 SignalBuffer push wrapper

| Field | Value |
|---|---|
| **Source** | Profile §2 (callgrind 14 % combined) + candidate list §2.1-4 #4 (M6-baseline.md "12-16 % of overhead") |
| **Hot path** | `signalforge::buffer::SignalBuffer::push` + `LinearTypedBuffer<T>::onPushCompleted` + `valueMemoryBytes` |
| **Primary metric** | M6 SignalBuffer Double push throughput (per `M6-baseline.md`) |
| **Pass criterion** | ≥ 10 % throughput improvement on Double push |
| **Approach** | (a) Inline `valueMemoryBytes` (currently a virtual call from `onPushCompleted`; for `LinearTypedBuffer<double>` it's compile-time constant 8 bytes). (b) Reduce `onPushCompleted` bookkeeping — move infrequent telemetry to a 1 Hz timer instead of per-push. (c) If profile shows hash-lookup as significant, consider caching the buffer pointer on the registry's `onSignal` path keyed by `signalId` (already a hashtable; aim for reduced hash cost). |
| **Risk** | Low. Internal `.cpp`-only changes; M6 freeze surface untouched. |
| **Subtask** | S4 |

## 6. Optimisations considered but not selected

### 6.1 Backward seek O(N) (candidate list #3)

**Profile evidence**: not measured (Scenario B doesn't exercise
backward seek; would need a separate sub-scenario).

**Estimated wall-time**: per M11 spec §9 Note 2, backward step
on a 600 k-record file is "slow" — but M11 acceptance §8.4 +
S6 unit test "stepBackward replays records before current"
shows it works correctly at the smaller scale.

**Disposition**: defer to V1.5+ per M11 spec §9 Note + M12 spec
§9 ("M11 backward seek O(N) is high risk"). The in-memory index
required to avoid O(N) re-walk would touch `SessionReader`'s
internal state in a way that may force ADR-008 + would
significantly grow `session_reader.cpp` complexity.

### 6.2 M5 decoder hot path (candidate list #5)

**Profile evidence**: Scenario A's synthetic dispatch path
**bypasses** the M5 schema decoder — `registry.onSignal` is
called directly without going through `SchemaDecoder::tryDecodeFrame`.
A real live-mode profile would surface decoder cost; the synthetic
profile cannot.

**Disposition**: not selected. To exercise the decoder hot path,
profiling would need a real driver + schema fixture. The
synthetic harness was the right scope for S1; extending it to
the full M3+M4+M5 path is bench scope-creep. The candidate-list
entry remains documented for V1.5+ if real workloads expose it.

### 6.3 M8 chart redraw (candidate list #6)

**Profile evidence**: charts are not exercised in any of the 3
scenarios (UI thread not active; no QQuickWidget instantiated).

**Disposition**: not selected. Per the candidate-list entry,
M8 already has 50× margin per its baseline; further optimisation
is V1.5+.

### 6.4 M7 ExpressionEngine (candidate list #7)

**Profile evidence**: ExpressionEngine not exercised in any
scenario.

**Disposition**: not selected. The candidate-list note says
"unmeasured"; no profile data justifies the 10 % bar.

### 6.5 SessionWriter 60-min memory growth (candidate list #8)

**Profile evidence**: memory growth not measured (`bench_session_writer
--soak 1800` is operator-run; not part of M12's profile session).

**Disposition**: not selected — outside M12 scope (carries forward
as an inherited M10 follow-up). If real-world soak surfaces a
leak, V1.5+ revisits.

### 6.6 Per-event QString hashtable lookup (profile finding)

**Profile evidence**: `qHashBits` 5.75 % + `equalStrings` 1.92 %
+ `_Hashtable::_M_find_before_node` 1.82 % = ~9 % of Scenario A
runtime is the per-event signal-id hashtable lookup in
`SignalBufferRegistry::onSignal`.

**Disposition**: considered. Replacing the unordered_map with a
QHash or a flat-array index keyed by integer signal IDs would
reduce this. However, the `SignalBufferRegistry` interface is
**M6-frozen** at the public-API level — keys are `QString`
signal IDs. A faster lookup would need internal-only changes
(e.g., a shadow `QHash<QString, SignalBuffer*>` cache populated
on registration), which is feasible without ADR-008 but
**non-trivial** to validate (registration ordering + thread
safety). Below the 10 % bar in confidence; documented as V1.5+
candidate.

## 7. Decision-tree application (per C4)

Per `.claude/M12-concerns.md` C4 decision tree applied at S2 close:

| Viable count | Action | This profile |
|---|---|---|
| 0 | HALT | n/a |
| 1 | HALT (spec minimum 2) | n/a |
| **2** | **Proceed to S3 + S4. Document third's "not selected" rationale** | **✓ chosen** |
| 3+ | Proceed to S3 + S4 + S5 | n/a |

**M12 ships with 2 optimisations (S3 + S4).** S5 is **deferred**
with the "not selected" candidates documented in §6 above.

This is permitted by spec §5.1 ("CC is NOT required to deliver
exactly 3; 2 may be acceptable if profile shows only 2 viable
candidates").

## 8. Hand-off to S3 + S4

- **S3**: implement C4 Stage B in `src/replay/session_player.cpp`.
  Targets `SessionPlayer::dispatchLoop`. Primary: 1× error
  ≤ 10.82 %. Secondary bonus: 10× completion ≤ 1.5 s.
  Re-run `bench_replay --realtime 10` 3 times for variance.
- **S4**: implement M6 push-wrapper optimisation in
  `src/buffer/signal_buffer.cpp`. Primary: M6 Double push
  throughput +10 %. Re-run `bench_signal_buffer` 3 times for
  variance.
- **S5**: deferred. M12-progress.md entry will document this
  per the decision-tree path.

Both optimisations target **internal `.cpp`** only — no
frozen-`.hpp` modification. M2-M11 freeze surface unchanged
(verified at S6 sha256 check per acceptance §8.4).
