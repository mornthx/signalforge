# M6 — Plan

## 0. Execution ground rules

- Branch: `milestone/M6` (already pushed at `c7538aa`).
- Per-subtask discipline (CLAUDE.md §Required #2 + §Git operation
  protocol), identical to M5:
  1. Append start entry to `.claude/M6-progress.md`.
  2. Implement per plan.
  3. Build all three presets clean (Debug, Release, debug-asan).
  4. `ctest` Debug + Release clean.
  5. `clang-format --dry-run -Werror` on changed files.
  6. Append close entry to progress.md with counts + deviations.
  7. Commit with `<module>: <imperative verb> <object>`; body states
     "Freeze scope: no M2/M3/M4/M5-frozen .hpp modified."
  8. Push `milestone/M6`.
  9. Watch CI via `gh run watch`; report result before starting the
     next subtask. No silent retries.
- No new top-level dependencies (spec §2.2-7).
- Locked design decisions (spec §3.1-§3.9) are reflected in
  `.claude/M6-understanding.md §5` and implemented as written; not
  re-evaluated in this plan.
- Performance HALT gates (spec §7) and the 7 spec-defined HALT
  triggers are pre-encoded in §3 below with their measurement points.
- Strategy: **measure first, optimize only on miss**. No
  pre-allocated counter-mitigations beyond what the spec already
  prescribes.

## 1. Subtask sequence overview

| # | Subtask | Prereqs | Effort | Commit | Notes |
|---|---|---|---|---|---|
| S1 | `src/buffer/` scaffolding + `SignalBuffer` / `SignalBufferRegistry` headers + CMake wiring | — | 3 h | Yes | Establishes the freeze surface. No `.cpp` logic yet beyond ctor/dtor stubs that compile. |
| S2 | `TypedBuffer` polymorphism + per-type storage classes (bool bit-pack, int64, double, QString) + `SignalBuffer::push` routing via `std::visit` + atomic counters | S1 | 5 h | Yes | No LOD, no snapshot publish yet — flat-vector storage with naive eviction stub. |
| S3 | Time-window + cap eviction logic for raw level | S2 | 3 h | Yes | Ring-buffer-style. Counters update `totalEvicted_`, metric updates. |
| S4 | Snapshot publish pattern (lock-free reads): `Segment` struct + `std::atomic<std::shared_ptr<const Segment>>` + writer-side periodic publish (default 1 ms / 100 pushes) | S3 | 5 h | Yes | Cost: 1 atomic store per publish; reader = atomic load + ref-count. |
| S5 | LOD pyramid (4 levels) maintained on write + level selection in queryRange + LOD eviction edge handling | S4 | 6 h | Yes | Three numeric LOD types (level 1/2/3 min/max pairs); bool/QString skip per spec §3.4. |
| S6 | Query API: `queryRange`, `queryLatest`, `queryLatestOne` consuming the published segment + LOD level selection per §4.5 thresholds | S5 | 4 h | Yes | Query metrics (`queries_<id>`, `query_us_<id>`) updated here. |
| S7 | `SignalBufferRegistry`: multi-buffer ownership, `SignalValueSink` overrides, per-driver overrides map, budget tracking + soft-warn / hard-reject (spec §3.6 / §4.7) | S6 | 5 h | Yes | Mutex-protected map; `bufferFor` returns raw pointer (registry-owned). `UsageReport` aggregates per-driver bytes. |
| S8 | `DecoderRegistrar` call-site update so production builds wire `SignalBufferRegistry` instead of `LoggingSignalValueSink` + relocate `LoggingSignalValueSink` to `tests/test_only/` | S7 | 3 h | Yes | The `DecoderRegistrar` constructor already takes `defaultSink`; only the call site changes. Verify by grep that no production code references `LoggingSignalValueSink` post-S8. |
| S9 | Unit tests ≥ 85% coverage: per-type buffers (bool, int64, double, string), concurrent (1W + 4R, 100k pushes), registry (lifecycle, lookup), budget (warn/reject) | S2-S7 | 6 h | Yes | One test file per spec §5.2 enumeration: `signal_buffer_bool_test.cpp`, `_int64_test.cpp`, `_double_test.cpp`, `_string_test.cpp`, `_concurrent_test.cpp`, `signal_buffer_registry_test.cpp`, `_budget_test.cpp`. |
| S10 | Integration tests (5 files per spec §5.3): round trip, concurrent, LOD, window eviction, budget | S8 | 5 h | Yes | All in `tests/integration/`. Concurrent test uses 1 writer × 4 readers × 1 M samples; verifies under ASan via CI. |
| S11 | Benchmark `bench_signal_buffer.cpp` + `tests/benchmark/results/M6-baseline.md` | S6 | 3 h | Yes | 3 scenarios per spec §5.4. Runs under Release. End-to-end scenario reuses M5 decoder + buffer wired as sink. |
| S12 | `.claude/M6-done.md` + freeze record + PR against main | S1-S11 green + CI green | 3 h | Yes | Mirrors M5 closure flow. SHA256s for `signal_buffer.hpp` + `signal_buffer_registry.hpp`. |

**Total estimated effort**: 51 h, well within spec's 8-10
person-day (64-80 h) budget. Slack reserved for §S5 LOD edge cases
(highest-risk subtask) and §S11 perf-target tuning.

## 2. Subtask details

### S1 — Scaffolding + freeze-surface headers

**Deliverables**:

- `src/buffer/CMakeLists.txt` adding `signalforge_buffer` static lib.
- `src/buffer/signal_buffer.hpp` matching spec §4.1 byte-for-byte.
- `src/buffer/signal_buffer_registry.hpp` matching spec §4.2.
- `src/buffer/signal_buffer.cpp`, `signal_buffer_registry.cpp` —
  ctor/dtor stubs that allocate `impl_` (still no operational logic).
- Top-level `CMakeLists.txt` + `tests/unit/CMakeLists.txt` +
  `tests/integration/CMakeLists.txt` updates so the new lib is
  buildable and a placeholder unit test compiles.

**Acceptance**:

- All three presets build clean (no operational logic to test yet).
- Doxygen on every public declaration (spec §2.1-11).
- `clang-format --dry-run -Werror` clean.

### S2 — TypedBuffer + per-type storage + push routing

**Deliverables**:

- Internal `TypedBuffer` virtual base in `signal_buffer.cpp`:
  `push(timestamp, value)`, `samplesRetained()`, `memoryBytes()`,
  `clear()`. No query yet.
- 4 instantiations: `BoolTypedBuffer` (bit-packed
  `std::vector<uint64_t>`), `Int64TypedBuffer`, `DoubleTypedBuffer`,
  `StringTypedBuffer`.
- `SignalBuffer::push` dispatches via `std::visit` to the right
  typed buffer.
- `totalPushed_` / `totalEvicted_` / `currentMemoryBytes_` atomic
  counters wired.
- Per-signal metric registration (spec §3.9):
  `signal_buffer_samples_stored_<id>` etc.

**Acceptance**:

- Unit test push / count / memoryBytes round-trip per type.
- No eviction yet (window/cap not enforced).

### S3 — Time-window + cap eviction

**Deliverables**:

- `evictExpired(now)` on each `TypedBuffer`, called at the head of
  every `push()`. Drops samples where `timestamp < now - window`.
- Cap enforcement: if `samplesRetained() == capSamples`, drop the
  oldest before append.
- `samples_evicted_<id>` metric updated.

**Acceptance**:

- Unit test for window eviction (push 2000 over 2 s, window = 1 s,
  retained ≈ 1000).
- Unit test for cap eviction (window = ∞, cap = 100, push 200,
  retained = 100).

### S4 — Snapshot publish pattern (lock-free reads)

**Deliverables**:

- Internal `Segment` struct (one per typed buffer):
  - `std::shared_ptr<const std::vector<...>> rawData`
  - `std::shared_ptr<const std::vector<...>> lodLevel{1,2,3}`
    (placeholders; populated in S5)
  - `t_oldest`, `t_newest`
- `std::atomic<std::shared_ptr<const Segment>> publishedSegment_`
  per typed buffer.
- Writer publishes a new `Segment` every 100 pushes or every 1 ms,
  whichever first. Initial impl: simple sample-count trigger
  (`pushesSincePublish_ >= 100`). Time-based trigger added if benches
  show staleness issues; deferred until measured.
- Atomic-load helper `currentSegment()` for readers.

**Acceptance**:

- Unit test: writer pushes 1000, publish triggers 10 times.
- Concurrent test (will be expanded in S9): reader can load segment
  while writer pushes; reader's view is internally consistent.

### S5 — LOD pyramid

**Deliverables**:

- Numeric typed buffers maintain three running aggregators:
  - Level 1 (every 10 raw): min/max pair
  - Level 2 (every 100 raw): min/max pair
  - Level 3 (every 1000 raw): min/max pair
- Bool / String buffers do **not** allocate LOD vectors when
  `lodEnabled` resolves to false (default per spec §3.1). Effective
  default: bool=false, string=false, numeric=true.
- LOD eviction handling (spec §9): when raw eviction crosses an LOD
  bin boundary, the partially-evicted bin is dropped (not retained
  with stale aggregate).
- `Segment` populated with LOD vectors.
- LOD level selection in queryRange per spec §4.5 thresholds (0.5 / 5
  / 50).

**Acceptance**:

- LOD min/max envelope contains every raw sample within the bin
  (verified at unit and integration level).
- Sine + noise integration test (spec §5.3 `test_signal_buffer_lod`)
  passes.
- HALT gate: any sample outside LOD envelope by > 0.1% → HALT
  (spec §7-7).

### S6 — Query API

**Deliverables**:

- `queryRange(t_start, t_end, target_sample_count)`:
  - Acquire current segment.
  - If `target_sample_count == 0`: return all raw samples in range.
  - Else: compute density per spec §4.5 and pick LOD level; return
    decimated samples in chronological order.
  - Update `queries_<id>` and `query_us_<id>` metrics (timer wraps
    the body).
- `queryLatest(n)`: tail of raw segment.
- `queryLatestOne()`: most recent sample's timestamp + age = `now -
  timestamp`. Returns `std::nullopt` if zero samples ever pushed.

**Acceptance**:

- Round-trip unit tests per type.
- Latency budget: one queryRange of 60 s × 1 kHz buffer with
  target=2000 ≤ 100 µs (allows ≥ 10 k queries/sec target).

### S7 — SignalBufferRegistry

**Deliverables**:

- `signal_buffer_registry.cpp` implementing all public methods.
- `onSignalsRegistered`: estimate memory per signal using
  `SignalBufferConfig::estimatedRateHz` (or 1 kHz default since
  `SignalMetadata` lacks a rate field — see understanding §8.5),
  budget check (soft 80% / hard 100%), allocate `SignalBuffer`s,
  insert into the map.
- `onSignal`: lookup by signalId (mutex), forward to the right
  `SignalBuffer`. Hot path; lookup time matters — use
  `std::unordered_map<QString>` with a single-shot `find()`.
- `onSignalsUnregistered`: release all buffers for that driverId,
  decrement `totalBytes_`.
- `setDriverConfigOverrides`: thread-safe; consumed at next
  `onSignalsRegistered` for the matching driver.
- `bufferFor(signalId)` raw-pointer accessor; valid until
  unregistration.
- `UsageReport memoryUsage()` aggregates per-driver totals.

**Acceptance**:

- Registry unit tests (spec §5.2 last block).
- Budget tests (separate file).

### S8 — DecoderRegistrar wiring + LoggingSignalValueSink relocation

**Deliverables**:

- Update the call site that constructs `DecoderRegistrar` (currently
  in `src/app/` or `main.cpp` — to be located in S8 itself) to
  construct a process-singleton `SignalBufferRegistry` and pass
  `std::shared_ptr<SignalValueSink>(&registry, [](auto*) {})`
  (non-owning, registry outlives registrar).
- Move `LoggingSignalValueSink` from `src/decode/` to
  `tests/test_only/` (or a `signalforge::test_only` namespace under
  `tests/`); update test includes.
- Verify by grep that no production CMake target references
  `logging_signal_value_sink.{hpp,cpp}` post-relocation.
- **No modification to the M5-frozen `decoder_interface.hpp` or
  `decoder_registrar.hpp` constructor signatures** (HALT trigger if
  attempted).

**Acceptance**:

- Production build still links (no missing-symbol errors from the
  relocation).
- Unit / integration tests that previously used
  `LoggingSignalValueSink` continue to compile and pass.

### S9 — Unit tests ≥ 85% coverage

**Deliverables** (one file per logical unit per spec §5.2):

- `tests/unit/buffer/signal_buffer_bool_test.cpp`
- `tests/unit/buffer/signal_buffer_int64_test.cpp`
- `tests/unit/buffer/signal_buffer_double_test.cpp`
- `tests/unit/buffer/signal_buffer_string_test.cpp`
- `tests/unit/buffer/signal_buffer_concurrent_test.cpp` (1W + 4R,
  100 k pushes)
- `tests/unit/buffer/signal_buffer_registry_test.cpp`
- `tests/unit/buffer/signal_buffer_registry_budget_test.cpp`

**Acceptance**:

- Coverage ≥ 85% on `signal_buffer.cpp` + `signal_buffer_registry.cpp`
  (verified in CI when quota permits; locally via gcov on Debug if
  needed).
- Concurrent test runs under ASan (CI) without warnings.

### S10 — Integration tests

**Deliverables** (per spec §5.3):

- `tests/integration/test_signal_buffer_round_trip.cpp`
- `tests/integration/test_signal_buffer_concurrent.cpp` (1 M samples
  / 5 s, 4 readers @ 1 kHz)
- `tests/integration/test_signal_buffer_lod.cpp` (sine + noise,
  600 k samples, target = 100, level 3 selected)
- `tests/integration/test_signal_buffer_window_eviction.cpp`
- `tests/integration/test_signal_buffer_budget.cpp`

**Acceptance**:

- All five pass under Debug + Release.
- Concurrent test + LOD test pass under debug-asan in CI.

### S11 — Benchmark + M6-baseline.md

**Deliverables**:

- `tests/benchmark/bench_signal_buffer.cpp` with 3 scenarios:
  1. Writer per type (bool / int64 / double / QString) — 10 M
     pushes in tight loop, samples/sec.
  2. Reader queryRange at target = 2000 over 60 s × 1 kHz buffer —
     queries/sec.
  3. End-to-end: M5 decoder + M6 registry as sink — overhead vs M5
     standalone (reuse M5's `bench_decoder_throughput` methodology).
- `tests/benchmark/results/M6-baseline.md` with run-to-run variance
  < 5%.

**Acceptance gates** (HALT triggers per spec §7):

- Writer throughput per type meets §5.4 targets after one
  optimization pass; if `double` < 200 k samples/sec → HALT.
- End-to-end overhead ≤ 5% target; > 10% → HALT.

### S12 — Closure: M6-done.md + PR

**Deliverables**:

- `.claude/M6-done.md` per spec §6.3:
  - Deliverables checklist
  - Acceptance self-check per spec §8
  - Test count matrix
  - Benchmark summary
  - Freezes section with sha256s of `signal_buffer.hpp` +
    `signal_buffer_registry.hpp`
  - Commit manifest
  - CI verification status
  - Hand-off to M7 / M8 / M10 / M11
  - HALT resolution trail
  - Deviations and concerns
- PR against `main` titled `milestone/M6: signal buffer`.
- Stop and announce per CLAUDE.md §Phase 1 step 6:
  "M6 ready. Awaiting approval to merge M6 and begin M7 bootstrap."

## 3. Pre-encoded HALT statements (spec §7)

Each HALT trigger is paired with its measurement point and the
report path.

| # | Trigger | Measurement point | Action |
|---|---|---|---|
| 1 | Modification to M2/M3/M4/M5 frozen `.hpp` | Pre-commit check: `git diff origin/main...HEAD -- src/utils/snapshot.hpp src/utils/mpsc_queue.hpp src/utils/spsc_ring.hpp src/observability/metrics.hpp src/pipeline/frame_sink.hpp src/decode/decoder_interface.hpp src/decode/schema_validator.hpp` returns empty | HALT report `.claude/halt/HALT-<ts>-frozen-modified.md`; revert change |
| 2 | `std::atomic<std::shared_ptr<T>>` ABI/linker issue | S4 build under all three presets | HALT report propose alt (`tl::atomic_shared_ptr` or hand-rolled refcount); concerns.md entry; await human |
| 3 | Writer throughput (double) < 200 k samples/sec after one opt pass | S11 benchmark + one optimization iteration | HALT report with profiler output (perf record on hot path) |
| 4 | End-to-end overhead > 10% beyond M5 baseline | S11 scenario 3 | HALT report; identify whether overhead is in writer push, snapshot publish, or registry mutex |
| 5 | TSan reports a data race in concurrent test | S10 concurrent test under TSan (if available; else CI debug-asan) | HALT report; record race location + threads |
| 6 | Memory budget calc off > 20% from actual | S7 budget test: register, then read `memoryBytes()`; ratio `actual / estimated` outside [0.8, 1.2] | HALT report; estimation formula needs revision |
| 7 | LOD min/max envelope misses any raw sample by > 0.1% | S5 / S10 LOD test: for every raw sample at level 0, verify `min ≤ sample ≤ max` at all higher levels covering it | HALT report; LOD computation broken |

CLAUDE.md §HALT triggers (compile error after 3 fixes, test fail
after 3 fixes, etc.) apply at every subtask.

## 4. Risk register

| Rank | Risk | Mitigation built into the plan |
|---|---|---|
| 1 | LOD eviction edge cases (partial bin retention) | S5 includes explicit eviction-vs-LOD boundary unit tests; spec §9 explicitly flags this |
| 2 | `std::atomic<std::shared_ptr<T>>` ABI issues on libstdc++ | M2's `Snapshot<T>` already uses this pattern in production; M6 reuses the same pattern — proven path |
| 3 | Per-signal mutex in registry hot path | `onSignal` uses a single map lookup per call; if profiling shows contention in S11, swap to per-signal `std::shared_mutex` (deferred until measured) |
| 4 | Memory-budget estimation accuracy | HALT trigger #6 catches > 20% drift; concerns.md will record real-vs-estimated ratios |
| 5 | Writer throughput target for QString (≥ 200 k) | QString is ref-counted; small-string optimization helps; if miss, profile string-allocator path before optimizing |
| 6 | CI quota | M5 closed under billing-blocked CI; M6 begins post-reset, but if quota exhausts again, follow the M5 protocol (local verification + retroactive CI on next quota reset) |

## 5. Performance budget breakdown

Per spec §5.4 + §3.4:

| Component | Per-push cost target | Per-query cost target |
|---|---|---|
| `std::visit` on `SignalValue` | ~5 ns | n/a |
| Push to typed buffer | ~5 ns | n/a |
| LOD update (every 10th / 100th / 1000th push) | amortized ~5 ns | n/a |
| Atomic publish (every 100 pushes) | ~10 ns amortized | n/a |
| Atomic load + ref-count | n/a | < 100 ns |
| Vector copy (60 s × 1 kHz double = 480 KB) | n/a | ~10 µs |
| LOD level selection arithmetic | n/a | ~50 ns |

Total push budget: ~25 ns / sample → ~40 M pushes/sec ceiling per
type. Achievable rate per spec §5.4 (1 M for bool, 500 k for numeric)
leaves > 40× headroom; the typical bottleneck will be memory
bandwidth on the publish copy, not arithmetic.

Total query budget: ~10 µs / query × 10 k queries/sec = 100 ms/sec
of CPU time, ~10% of one core — well within target.

## 6. Out of scope (spec §2.2 reminder; HALT if encountered)

If any of the following becomes seemingly necessary, HALT:

- Modifications to M2/M3/M4/M5 frozen `.hpp` files.
- Persistence of any kind.
- Cross-signal correlation queries.
- Alarm / threshold logic.
- Chart-specific helpers (color, axis, etc).
- ML / statistical pre-computation beyond LOD min/max.
- New top-level dependencies.
- `QObject SignalBuffer`.

## 7. Closure flow

Per CLAUDE.md §Git operation protocol Phase 1:

1. S1-S12 complete on `milestone/M6`.
2. `git push origin milestone/M6`; report.
3. Wait for CI green; report.
4. `gh pr create` against `main`; report PR # + URL.
5. File `.claude/M6-done.md` with merge SHA placeholder + CI status.
6. Stop and announce: "M6 ready. Awaiting approval to merge M6 and
   begin M7 bootstrap."

Phase 2 (human approval) → Phase 3 (CC merges, tags `v0.0.7-alpha.1`,
bootstraps M7) follow the standard protocol. **No tag, no merge, no
M7 bootstrap is started without explicit Phase 2 approval.**
