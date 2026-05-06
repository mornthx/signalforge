# M6 — Understanding

## 1. Restatement of the M6 goal

M6 delivers the **signal buffer layer**: time-series storage for the
`SignalValue` instances that M5's decoder produces, with a query API
suitable for both the M8 Chart UI (lock-free, LOD-aware reads at 30 Hz
across 60+ signals) and the M10 Session Writer (bulk dump to disk).

This is the **first storage layer in V1**. Two simultaneous design
pillars:

1. **Lock-free reads**. Chart UI reads ~2000 times/sec; readers must
   not contend with the decoder's writer. Approach: the M2
   `Snapshot<T>` pattern adapted to a sliding-window time series —
   atomically-published immutable segments, reference-counted lifetime.
2. **LOD pyramid maintained on write**. 4 levels (1:1, 10:1, 100:1,
   1000:1). Reader picks the level that gives ~1 sample per pixel for
   the requested time range. Pre-computing on the writer side keeps
   the chart-render path off scanning ~3.6 M samples per zoom-out.

Hard-stop types (concurrent):

1. **Interface freeze**: `SignalBuffer` API + `SignalBufferRegistry`
   API at M6 close.
2. **Performance certification**: writer throughput per type +
   reader throughput at 60s × 1 kHz buffer + end-to-end overhead ≤ 5%
   beyond M5 baseline.

**Soft-HALT is not allowed** (inherits M2/M3/M4/M5 stance).

Quality philosophy carried from M5: **predictable performance under
realistic workload**. Correctness traceable from the spec wins over
micro-optimizations (spec §10).

## 2. Observed repo state

```
$ git log --oneline origin/main -5
c7538aa Merge pull request #7 from mornthx/docs/m6-spec
6fc6c06 Merge pull request #6 from mornthx/milestone/M5
af4f7b6 docs: add M6 signal buffer spec
8183ff5 license: adopt MIT (replaces placeholder)
321b100 Merge pull request #5 from mornthx/milestone/M4
```

Phase 3 actions completed this session (report confirmed):

- PR #6 merged to main (merge commit `6fc6c06`).
- Tag `v0.0.6-alpha.1` annotated on `6fc6c06` and pushed.
- `milestone/M6` branch created from `6fc6c06` and pushed; later
  fast-forwarded to `c7538aa` to absorb the M6 spec.

`src/buffer/` does not yet exist; the M6 implementation creates it.

## 3. Scope reminder

### Must deliver (spec §2.1)

`SignalBuffer` + `SignalBufferRegistry` headers and implementations,
per-variant typed internal storage, lock-free snapshot reads, LOD
pyramid (4 levels) maintained on write, time-windowed + cap-bounded
eviction, memory-budget tracking with soft warn / hard reject,
`DecoderRegistrar` wiring update so the registry replaces
`LoggingSignalValueSink` as the production sink, ≥ 85% unit-test
coverage, 5 integration tests, a benchmark with results document,
Doxygen on all public declarations, and `.claude/M6-done.md` with
the freeze record.

### Must not do (spec §2.2)

- No modifications to M2/M3/M4/M5 frozen `.hpp` files (HALT trigger).
- No persistence (M10 Session Writer territory).
- No cross-signal correlation queries (M7 Expression Engine territory).
- No alarm/threshold logic.
- No Chart-specific helpers (M8).
- No ML / statistical pre-computation beyond LOD min/max.
- No new top-level dependencies. Use existing M2 `Snapshot<T>` /
  `MpscQueue`, plus Qt + std.
- No `QObject` `SignalBuffer`. Pure C++; metric updates go through the
  already-frozen `MetricsRegistry`.

## 4. M6 freeze surface (locks at M6 close)

Per spec §6.1:

- `src/buffer/signal_buffer.hpp`:
  - `SignalBuffer` class — public method signatures
  - `SignalSample` struct layout
  - `LatestValue` struct layout
  - `SignalBufferConfig` struct layout
- `src/buffer/signal_buffer_registry.hpp`:
  - `SignalBufferRegistry` class — public method signatures
    (including `SignalValueSink` overrides)
  - `RegistryConfig` struct layout
  - `SignalConfigOverrides` typedef
  - `SignalBufferRegistry::UsageReport` struct layout

What does **not** freeze (spec §6.2): internal `TypedBuffer`
polymorphism, LOD level count or decimation ratios (additive only),
snapshot publishing strategy (every N pushes vs every M ms), default
values in configs, metric names (additive only).

## 5. Locked design decisions (spec §3.1-§3.9, reflected verbatim)

These are **decisions confirmed in pre-M6 planning**. M6 implements
them as written; it does not re-evaluate.

### 5.1 Per-variant internal storage (§3.1)

- `Bool`: bit-packed in `std::vector<uint64_t>` (64 samples per word;
  ~1/8 the memory of naive `bool[]`).
- `Int64`: `std::vector<int64_t>` (8 bytes per sample).
- `Double`: `std::vector<double>` (8 bytes per sample).
- `String`: `std::vector<QString>` (Qt implicit-shared, ~24-byte
  handle plus shared backing).

Polymorphic internal `TypedBuffer` interface lives inside
`signal_buffer.cpp`; not exposed externally. `SignalValue` arriving via
`push()` is unpacked via `std::visit` and routed.

### 5.2 Time-windowed storage with optional max-samples cap (§3.2)

Each signal has two limits, both enforced:

- **Time window** (primary): samples older than `now - window` are
  evicted on next write.
- **Max samples cap** (safety): hard cap regardless of window.

Defaults: 60 s window, 1 000 000 sample cap.

Per-signal override via `SignalBufferConfig` at registration time.
Eviction is ring-buffer-style.

### 5.3 Snapshot-based queries (§3.3)

All reads return a copy of the requested data. The reader holds no
lock during use; the data is a `std::vector<SignalSample>` value.

API forms:

- `queryRange(signalId, t_start, t_end, target_sample_count = 0)`
- `queryLatest(signalId, n)`
- `queryLatestOne(signalId)` returning `std::optional<LatestValue>`

Cost note: a 60 s × 1 kHz double buffer copy is ~480 KB / ~10 µs;
60 signals × 30 Hz × ~50 KB LOD-decimated = ~9 MB/sec read traffic,
within memory bandwidth.

### 5.4 LOD pyramid maintained on write (§3.4)

| Level | Decimation | Sample type | Use case |
|---|---|---|---|
| 0 | 1:1 raw | original `SignalValue` | Full-resolution query |
| 1 | 10:1 | min/max pair (numeric) or last (bool/string) | Zoomed-out view |
| 2 | 100:1 | min/max pair | Wide view |
| 3 | 1000:1 | min/max pair | Long-term overview |

Bool and String signals skip LOD levels 1-3 (no meaningful aggregation).

Reader selects level by `samples_per_pixel = (t_end - t_start) /
target_sample_count` and the `effective_density` thresholds in spec
§4.5 (0.5 / 5 / 50).

Writer overhead target: ~10% beyond plain push. Memory overhead:
~11% beyond raw (1/10 + 1/100 + 1/1000 = 0.111).

### 5.5 Lock-free snapshot reads (§3.5)

Per-signal storage maintains a single
`std::atomic<std::shared_ptr<const Segment>>` published by the writer
and consumed by readers. The pattern mirrors the M2 `Snapshot<T>`
contract (already in `src/utils/snapshot.hpp`); M6 adapts it to a
sliding-window time series.

- **Writer**: append to private working buffer; periodically (default
  every 1 ms or every 100 pushes, whichever first) atomically publish
  a new immutable `Segment` containing the current view + the LOD
  pyramid.
- **Reader**: atomic load → ref-count inc via `shared_ptr` → use →
  ref-count dec on scope exit.
- Old segments are released when the last reader's `shared_ptr`
  expires.

Concurrent readers: zero contention. Reader-while-writer: reader sees
a consistent point-in-time snapshot; new writes are visible at next
publish (1 ms latency acceptable for 30 Hz Chart).

Memory safety failure mode (documented in spec): reader holding a
snapshot longer than `window + 1 publish interval` sees stale data —
by design.

### 5.6 Memory budget hard limit (§3.6)

Registry tracks total allocated bytes (raw + LOD). Behaviour:

- ≥ 100% of budget: registration rejected, `SF_LOG_ERROR` with
  current/requested/budget, `signal_buffer_budget_rejected` counter
  incremented; sink still functions but the rejected driver's signals
  are not stored.
- ≥ 80% (one-shot per crossing): `SF_LOG_WARN`,
  `signal_buffer_budget_warned` counter incremented.

Default budget: 256 MB. Configurable at registry construction.

Estimation formula (spec §3.6 / §4.7):
`window_seconds × estimated_rate_hz × bytes_per_sample × 1.11`.

### 5.7 Per-signal independent writer (§3.7)

`SignalBuffer` is per-signal. The registry owns N instances. Different
decoders (drivers) writing different signals do not contend.
Cross-signal correlation queries are M7's job, not M6's.

### 5.8 No soft-HALT (§3.8)

Inherits the M2/M3/M4/M5 stance.

### 5.9 Metrics naming (§3.9)

Per-signal: `signal_buffer_samples_stored_<signalId>`,
`samples_evicted_<signalId>`, `queries_<signalId>`,
`query_us_<signalId>`, `memory_bytes_<signalId>`.
Registry-level: `signal_buffer_total_memory_bytes`,
`budget_warned`, `budget_rejected`.

`signalId` sanitization follows the M5 schema rules (no whitespace,
no quotes/brackets) — already enforced upstream.

## 6. Performance targets (spec §5.4)

| Scenario | Target | Notes |
|---|---|---|
| Writer (bool) | ≥ 1 M samples/sec | bit-pack overhead acceptable |
| Writer (int64) | ≥ 500 k samples/sec | 8-byte storage |
| Writer (double) | ≥ 500 k samples/sec | 8-byte storage |
| Writer (QString small) | ≥ 200 k samples/sec | implicit-shared QString |
| Reader (queryRange, target=2000 over 60s × 1kHz) | ≥ 10 k queries/sec | Chart-typical |
| End-to-end (M5 decoder + M6 buffer) | ≤ 5% overhead beyond M5 standalone | regression gate |

Run-to-run variance budget: < 5% (per spec §8.2 acceptance).

The plan does **not** pre-allocate counter-mitigations. Strategy is
"measure first, optimize only on miss" (per session prompt).

## 7. M6 HALT triggers (spec §7, beyond CLAUDE.md §HALT)

These are encoded directly into plan §3 with measurement points.

1. Any modification to M2/M3/M4/M5 frozen `.hpp` → HALT.
2. Lock-free snapshot pattern requires `std::atomic<std::shared_ptr<T>>`;
   if subtle ABI/linker issues surface → HALT and propose alternative
   (`tl::atomic_shared_ptr` or hand-rolled refcount).
3. Writer throughput < 200 k samples/sec for `double` after one
   optimization pass → HALT.
4. End-to-end overhead > 10% beyond M5 baseline → HALT.
5. TSan reports a data race in the concurrent test → HALT.
6. Memory budget calculation off by > 20% from actual allocation → HALT
   (estimation logic broken).
7. LOD min/max envelope misses any actual sample by > 0.1% → HALT
   (LOD computation broken).

## 8. Risks and design notes

1. **`std::atomic<std::shared_ptr<T>>` correctness**. Already used in
   `src/utils/snapshot.hpp` (M2) on libstdc++ GCC 13.3 without
   incident; M6 reuses the proven path. Spec §9 says "if unsure about
   a specific atomic ordering, use `seq_cst`"; we will start with
   `acquire/release` mirroring `Snapshot<T>` and only escalate to
   `seq_cst` if a TSan finding warrants it.
2. **LOD eviction edge cases**. When a window-evicted raw sample
   leaves only partial coverage of an LOD bin, that bin must either be
   re-computed from currently-retained raw samples or dropped — never
   left with stale aggregates. Spec §9 flags this. The plan's S5
   (LOD) carries explicit unit tests for the eviction-vs-LOD boundary.
3. **`signalsList` parameter rename**. M5 already renamed
   `onSignalsRegistered`'s parameter to `signalsList` (M5 concerns #1)
   to avoid Qt's `signals` macro. M6 inherits this name; nothing to do.
4. **DecoderRegistrar API mismatch with spec §4.6**. The M6 spec's
   "Before/After" diff in §4.6 shows M5's `DecoderRegistrar` building
   a `LoggingSignalValueSink` internally. The actual M5 implementation
   (`src/decode/decoder_registrar.hpp:50-52`) already takes the sink
   as a constructor parameter (`std::shared_ptr<SignalValueSink>
   defaultSink`). This means the M6 wiring change is at the **call
   site** that constructs `DecoderRegistrar` (currently in `app/` or
   `main.cpp`), not in `decoder_registrar.cpp` — a smaller change
   than the spec's diff implies. Logging this in
   `.claude/M6-concerns.md` as deviation #1 (informational; no spec
   conflict, no HALT) once Phase 5 begins.
5. **`SignalMetadata::sample_rate_hz` does not exist**. Spec §3.6
   says "metadata's `sample_rate_hz` field if present". The actual
   M5 frozen `SignalMetadata` (id, name, unit, type, description,
   scale, offset) does **not** carry rate. We use only
   `SignalBufferConfig::estimatedRateHz` plus the registry default
   (1000 Hz) for budget estimation. No freeze-surface modification
   needed; logging as concerns #2.
6. **C++23 vs architecture.md C++20 statement**. Architecture §4.1
   states "Do not use C++23 features"; M5 already moved the project
   to C++23 in S1 (commit `d5e9317`). This is a pre-existing
   project-wide deviation, not introduced by M6, but M6 does rely on
   C++20 facilities (`std::atomic<std::shared_ptr<T>>`) that compile
   under both. No new contradiction.
7. **Local ASan blocked**. Per memory `host_asan_preload`, the host's
   `/etc/ld.so.preload` (`AppProtection.so`) prevents ASan-instrumented
   binaries from running locally. CI is the authoritative ASan/UBSan
   gate. Same protocol as M5.

## 9. Hand-off to downstream milestones

- **M7 (Expression Engine)**: reads via `queryLatest(signalId, 1)` for
  current value, `queryRange(...)` for windowed expressions. Thread
  affinity: any thread.
- **M8 (Chart UI)**: queries via `queryRange` with
  `target_sample_count = chart_pixel_width`. LOD pyramid handles the
  decimation.
- **M10 (Session Writer)**: bulk drains via `queryRange(t_start, t_end,
  0)` (full resolution, no decimation).
- **M11 (Replay)**: replay bypasses M6 (replays from disk via M10's
  format).

Performance baseline to maintain (per M5 hand-off): pipeline
throughput must not drop below 95% of M5's 410 k/398 k fps with M6
attached.

## 10. Open ambiguities (none blocking)

- Spec §3.6's reference to `SignalMetadata::sample_rate_hz` (see §8.5
  above) — resolved to "use config-supplied rate or registry default".
  Logged in concerns; no HALT.
- Spec §4.6's `DecoderRegistrar` diff (see §8.4 above) — resolved to
  "call-site update only". Logged in concerns; no HALT.

Both fall under CLAUDE.md §Ambiguity handling's "additive extensions
without HALT" exception (additive informational notes, no API change,
no new dependencies).

## 11. Definition of Done

A subtask is "done" when CLAUDE.md §Definition of Done is satisfied:
clean build under Debug + Release + debug-asan, all relevant tests
pass under all three (debug-asan via CI per the M5 protocol),
coverage target met, Doxygen on public declarations, clang-format /
clang-tidy clean, conforming commit, `.claude/M6-progress.md` updated.

The milestone is "done" when spec §8 acceptance criteria are
satisfied, the freeze record in `.claude/M6-done.md` is filed with
sha256s, and a PR is opened against main.
