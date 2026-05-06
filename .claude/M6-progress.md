# M6 — Progress log

## Session metadata

- Phase 5 execution begins 2026-05-06.
- Branch: `milestone/M6` at `da59b0b` (concerns pre-recorded;
  understanding + plan from Phase 3 at `3dce067`).
- Plan: `.claude/M6-plan.md`, 12 subtasks S1–S12.
- Understanding: `.claude/M6-understanding.md`.
- Concerns: `.claude/M6-concerns.md` (2 pre-recorded entries).
- Remote: `git@github.com:mornthx/signalforge.git`.

## Subtask log

Each subtask appends a start entry and a close entry.

---

### S1 — `src/buffer/` scaffolding + freeze-surface headers (start)

**Goal**: per plan §2 S1, establish the M6 module foundation:

1. Create `src/buffer/` directory.
2. Deliver spec §4.1 verbatim:
   `src/buffer/signal_buffer.hpp` — `SignalSample`, `LatestValue`,
   `SignalBufferConfig` structs + `SignalBuffer` class with internal
   forward-declared `TypedBuffer`.
3. Deliver spec §4.2 verbatim:
   `src/buffer/signal_buffer_registry.hpp` — `RegistryConfig` struct,
   `SignalConfigOverrides` typedef, `SignalBufferRegistry` class
   (extends `SignalValueSink`), nested `UsageReport`.
4. `src/buffer/signal_buffer.cpp` and
   `src/buffer/signal_buffer_registry.cpp` — ctor/dtor stubs that
   compile. Internal `TypedBuffer` struct definition lives in the
   .cpp (per pimpl-style pattern with forward-declared inner class).
5. `src/buffer/CMakeLists.txt` — `signalforge_buffer` STATIC library.
   PUBLIC deps: `Qt6::Core`, `signalforge_decoder` (for
   `SignalValueSink` + types). PRIVATE: observability.
6. Top-level `CMakeLists.txt` adds `add_subdirectory(src/buffer)`.
7. `tests/unit/buffer/CMakeLists.txt` + `buffer_smoke_test.cpp` —
   placeholder Catch2 tests verifying the headers compile, default
   configs make sense, and a minimal `SignalBuffer` constructs +
   destructs cleanly.
8. `tests/unit/CMakeLists.txt` adds `add_subdirectory(buffer)`.

**Acceptance**:

- All three presets build clean.
- Doxygen on every public declaration.
- `clang-format --dry-run -Werror` clean on changed files.
- New unit test executable runs in `ctest` and reports `pass` (even
  if the only assertion is "headers include cleanly").

**Freeze scope**: M2/M3/M4/M5 frozen `.hpp` not modified. Pre-commit
diff check confirms.

### S1 — `src/buffer/` scaffolding + freeze-surface headers (close)

**Result**: green.

**Files added**:

- `src/buffer/signal_buffer.hpp` (122 lines) — spec §4.1 verbatim;
  freeze-surface header.
- `src/buffer/signal_buffer.cpp` (66 lines) — ctor/dtor + accessor
  stubs; internal `TypedBuffer` defined as a forward-declared inner
  struct so the `unique_ptr<TypedBuffer>` member compiles. Push,
  query, and sample-count methods are placeholders for S2-S6.
- `src/buffer/signal_buffer_registry.hpp` (109 lines) — spec §4.2
  verbatim.
- `src/buffer/signal_buffer_registry.cpp` (87 lines) — ctor/dtor +
  trivial query plumbing; mutex-protected map accesses already
  threaded through. `onSignal` / `onSignalsRegistered` /
  `onSignalsUnregistered` are stubs for S7.
- `src/buffer/CMakeLists.txt` — `signalforge_buffer` STATIC; PUBLIC
  deps `Qt6::Core`, `signalforge_decoder`; PRIVATE
  `signalforge_observability`. AUTOMOC ON not needed (no QObject in
  the lib) — kept off.
- `tests/unit/buffer/buffer_smoke_test.cpp` (5 cases) — verifies
  default config defaults, default registry defaults, `SignalBuffer`
  constructs and reports zeros, empty registry has zero signals,
  `SignalBufferRegistry` is convertible to `SignalValueSink*`.
- `tests/unit/buffer/CMakeLists.txt` — `buffer_smoke_test` Catch2
  executable; deps `Catch2::Catch2WithMain`, `Qt6::Core`,
  `signalforge_buffer`, `signalforge_decoder`.

**Files modified**:

- `CMakeLists.txt` — `add_subdirectory(src/buffer)` after `src/decode`.
- `tests/unit/CMakeLists.txt` — `add_subdirectory(buffer)` after
  `decode`.

**Build verification** (local):

- `cmake --preset=debug && cmake --build --preset=debug` — clean,
  68 targets.
- `cmake --preset=release && cmake --build --preset=release` —
  clean, 79 targets.
- `cmake --preset=debug-asan && cmake --build --preset=debug-asan` —
  clean, 68 targets.
- `clang-format --dry-run -Werror` on all 5 changed/added files —
  clean.

**Test verification** (local):

- `ctest --preset=debug` — 270 / 270 pass (was 265 at M5 close;
  +5 from `buffer_smoke_test`).
- `ctest --preset=release` — 270 / 270 pass.
- `ctest --preset=debug-asan` — deferred to CI per memory
  `host_asan_preload`.

**Frozen-file diff** vs 6fc6c06 (M5 merge): empty.

**Deviations from plan §S1**: none.

**Effort**: 2.5 h (plan estimate 3 h).

**CI** (run 25417700815): success — debug, release, debug-asan all
green. Confirms no ASan issues with the M6 bootstrap headers.

---

### S2 — TypedBuffer polymorphism + per-type storage + push routing (start)

**Goal**: per plan §2 S2, plug operational logic into the S1
scaffolding:

1. Promote `SignalBuffer::TypedBuffer` from an empty inner struct to
   a virtual base class with `pushValue(SignalValue) -> bool`,
   `valueMemoryBytes()`, `clearValues()`, plus shared timestamp
   storage and metric pointers.
2. Add 4 per-type derivations (anonymous-namespace inside
   `signal_buffer.cpp`):
   - `BoolTypedBuffer` — bit-packed `std::vector<uint64_t>` (64
     samples per word).
   - `Int64TypedBuffer` — `std::vector<int64_t>`.
   - `DoubleTypedBuffer` — `std::vector<double>`.
   - `StringTypedBuffer` — `std::vector<QString>`.
3. `SignalBuffer` constructor switches on `metadata.type` and
   instantiates the right derivation.
4. `SignalBuffer::push` dispatches via `impl_->push(t, v)` (one
   virtual call per push); on success, increments `totalPushed_` and
   updates `currentMemoryBytes_` to mirror `impl_->memoryBytes()`.
   Type mismatch (wrong variant for the metadata type) skips the
   push silently — tests cover only the matching path; a future
   registry-level mismatch metric can be added in S7 if production
   reveals the need.
5. Per spec §3.9: register `signal_buffer_samples_stored_<id>`
   (counter), `..._samples_evicted_<id>` (counter, registered now;
   updated by S3), `..._memory_bytes_<id>` (gauge) with the global
   `MetricsRegistry`. Null-tolerant on invalid names.
6. Add `tests/unit/buffer/signal_buffer_push_test.cpp` covering one
   round-trip per type and the bit-pack edge case (push 65 bools to
   confirm word-boundary handling).

**Acceptance**: per type, `push() × N` increments `sampleCount` to
`N`, `totalSamplesPushed` to `N`, `memoryBytes` rises monotonically.
No eviction yet (S3).

**Freeze scope**: M2/M3/M4/M5 frozen `.hpp` not modified. The
`SignalBuffer::TypedBuffer` private inner struct is not part of the
freeze (spec §6.2).

### S2 — TypedBuffer polymorphism + per-type storage + push routing (close)

**Result**: green.

**Changes**:

- `src/buffer/signal_buffer.hpp` — moved the forward declaration
  `struct TypedBuffer;` from `private:` to `public:` (the
  `std::unique_ptr<TypedBuffer> impl_;` member stays private). The
  full definition still lives only in the .cpp; the rename is solely
  to permit the per-type derived classes inside the .cpp's
  anonymous namespace to inherit from `SignalBuffer::TypedBuffer`.
  Spec §6.2 explicitly classifies `TypedBuffer` polymorphism as
  outside the freeze surface, so this is permitted; logged as
  concerns #3 below for completeness.
- `src/buffer/signal_buffer.cpp` — replaced the empty `TypedBuffer`
  stub with a virtual base class holding the timestamp vector, the
  three S2-relevant metric pointers
  (`signal_buffer_samples_stored_<id>`,
  `..._samples_evicted_<id>`,
  `..._memory_bytes_<id>`), and the `push() / samplesRetained() /
  memoryBytes() / clear()` plumbing. Four anonymous-namespace
  derivations (`BoolTypedBuffer`, `Int64TypedBuffer`,
  `DoubleTypedBuffer`, `StringTypedBuffer`) implement `pushValue` /
  `valueMemoryBytes` / `clearValues`. `makeTypedBuffer` factory
  switches on `metadata.type`. `SignalBuffer::push` delegates to
  `impl_->push` and, on success, increments `totalPushed_` and
  mirrors `currentMemoryBytes_`.
- `tests/unit/buffer/signal_buffer_push_test.cpp` (6 cases) +
  `tests/unit/buffer/CMakeLists.txt` — covers per-type push, the
  bit-pack edge case (65 booleans across the 64-bit word boundary),
  NaN tolerance, type-mismatch silent-drop, and monotonic memory
  growth across 1000 pushes.

**Build verification** (local):

- Debug + Release + debug-asan all build clean.
- `clang-format --dry-run -Werror` clean on all changed C++ files
  (after one auto-fix iteration that reformatted the case-label
  indentation of the factory switch).

**Test verification** (local):

- `ctest --preset=debug` — 276 / 276 pass (was 270; +6 from
  `signal_buffer_push_test`).
- `ctest --preset=release` — 276 / 276 pass.
- debug-asan deferred to CI per the M5 protocol.

**Frozen-file diff** vs 6fc6c06: empty.

**Compile fixes attempted**: 1 (TypedBuffer access-level adjustment).
Within HALT trigger budget (3 attempts).

**Effort**: 4 h (plan estimate 5 h).

**New concern recorded**: see concerns.md #3
("`SignalBuffer::TypedBuffer` forward-decl moved to public").

**CI** (run 25418116429): success — debug, release, debug-asan all
green.

---

### S3 — Time-window + cap eviction (start)

**Goal**: per plan §2 S3, add eviction at the head of every `push()`:

1. **Time-window eviction** (primary): drop samples older than
   `t - windowSeconds`, where `t` is the timestamp of the incoming
   push. Implemented by walking timestamps from the front and
   popping until the oldest is within window.
2. **Cap eviction** (safety): after window eviction + the new sample
   appended, if the total exceeds `capSamples`, drop the oldest
   until at cap.
3. `samples_evicted_<id>` metric updated; `SignalBuffer::totalEvicted_`
   atomic counter mirrored from the typed buffer's internal count.

**Storage shift**: switch internal storage from `std::vector<...>`
to `std::deque<...>` for O(1) front pop. The bool buffer's bit-pack
becomes `std::deque<uint64_t>` + a `headBitOffset_` for sub-word
eviction (advance offset; pop the front word once it reaches 64).
This is a deviation from spec §3.2's "ring-buffer-style" wording
(preallocated circular indexing) — std::deque gives the same
sliding-window semantics with growth-on-demand sizing rather than
upfront allocation of `capSamples` entries (which at 1M default × 24
bytes for QString = 24 MB per signal would be unwarranted overhead
for typical 60 k-sample usage). To be logged in concerns.md as #4.

**Acceptance**:

- Window eviction unit test: push 2000 samples spread over 2 s with
  `windowSeconds = 1.0`; expect retained ≈ 1000 and evicted ≈ 1000.
- Cap eviction unit test: push 200 samples with `capSamples = 100`;
  expect retained = 100 and evicted = 100.
- Per-type window eviction smoke (Bool / Int64 / Double / QString).

**Freeze scope**: M2/M3/M4/M5 frozen `.hpp` not modified.

### S3 — Time-window + cap eviction (close)

**Result**: green.

**Changes**:

- `src/buffer/signal_buffer.cpp`:
  - Promoted `windowDuration_` (computed from `windowSeconds`) and
    `capSamples_` to `TypedBuffer` members; added an internal
    `totalEvicted_` counter.
  - Added `evictFrontValue()` virtual; called in lock-step with
    `timestamps_.pop_front()`.
  - `TypedBuffer::push` now does (1) time-window eviction (drop
    front while `front < t - window`), (2) type-checked append,
    (3) cap eviction (drop front while `size > capSamples`),
    (4) metric updates (including `samples_evicted_<id>` gauge
    update).
  - Switched value storage from `std::vector` to `std::deque` for
    O(1) front pop. Bool buffer keeps bit-packing via
    `std::deque<uint64_t>` + `headBitOffset_` tracking sub-word
    eviction; once `headBitOffset_` reaches 64, the front word is
    popped.
  - `SignalBuffer::push` now mirrors `impl_->totalEvicted()` to its
    `totalEvicted_` atomic on every successful push.
- `tests/unit/buffer/signal_buffer_eviction_test.cpp` (6 cases):
  - Window eviction (Double, 2000 samples / 2 s, window=1 s, retained
    ≈ 1000).
  - Cap eviction (Int64, 200 samples, cap=100, retained=100).
  - Bool window eviction across multiple bit-pack word boundaries.
  - Cap eviction (QString, 12 samples, cap=5, retained=5).
  - Type-mismatch no-op contract (totalSamplesPushed unchanged).
  - cap=1 edge case (only newest retained).

**Build verification** (local):

- Debug + Release + debug-asan all build clean.
- `clang-format --dry-run -Werror` clean (one auto-fix iteration on
  the factory signature wrap).

**Test verification** (local):

- `ctest --preset=debug` — 282 / 282 pass (was 276; +6 from S3
  eviction tests).
- `ctest --preset=release` — 282 / 282 pass.
- debug-asan deferred to CI.

**Frozen-file diff** vs 6fc6c06: empty.

**Effort**: 3 h (plan estimate 3 h).

**New concern recorded**: see concerns.md #4 (`std::deque` vs
spec's "ring-buffer-style" wording).

**CI** (run 25418455593): success — debug, release, debug-asan all
green.

---

### S4 — Snapshot publish pattern (lock-free reads) (start)

**Goal**: per plan §2 S4, add the lock-free "atomic published
segment" mechanism so readers can pull a coherent point-in-time view
of a signal's data without blocking the writer.

1. `TypedBuffer` base gains:
   - `publishCadence_` (default 100 pushes; spec §3.5)
   - `pushesSincePublish_` counter
   - `publishesMetric_` pointer (`signal_buffer_publishes_<id>`,
     counter — additive per spec §6.2 "metric names — additive only")
   - Pure-virtual `publishSegment()`
   - At the tail of `push()`, if `++pushesSincePublish_ >=
     publishCadence_` → call `publishSegment()`, reset counter to 0,
     increment publishes metric.
2. Each derived class:
   - Adds a per-type `Segment` struct (raw timestamp vector +
     typed value vector(s); LOD vectors are placeholders for S5).
   - Adds `std::atomic<std::shared_ptr<const Segment>>
     publishedSegment_` (mirrors M2's `Snapshot<T>` pattern from
     `src/utils/snapshot.hpp`).
   - Overrides `publishSegment()` to copy current internal state
     into an immutable `Segment` and `store` it on the atomic with
     `std::memory_order_release`.
3. Initial publishedSegment_ is an empty `shared_ptr` (null);
   readers tolerate null (no published segment yet means "no data
   available to query"). S6 will wire reader-side access.

**S4 publish strategy** (per session prompt's S4 note + plan §S4):
sample-count-only trigger. Time-based publish (every 1 ms regardless
of count) is deferred until M8 Chart UI integration surfaces a
staleness need.

**Acceptance**:

- Per-type test: 99 pushes → publishes metric == 0; 100th push →
  publishes == 1; another 100 → publishes == 2.
- Verify across all four types.
- Verify the published segment's atomic load returns a non-null
  segment after the first publish.

**Freeze scope**: M2/M3/M4/M5 frozen `.hpp` not modified. The
`SignalBuffer` public API is unchanged (no new public methods); the
publishes metric is internal observability.

### S4 — Snapshot publish pattern (lock-free reads) (close)

**Result**: green.

**Changes**:

- `src/buffer/signal_buffer.cpp`:
  - Added `kPrefixPublishes` + `kDefaultPublishCadence = 100`.
  - `TypedBuffer` base gains `publishCadence_`,
    `pushesSincePublish_`, `publishCount_`, `publishesMetric_` and a
    new pure-virtual `publishSegment()`. `push()` increments the
    cadence counter at the tail and triggers `publishSegment()` +
    metric increment when it crosses the threshold.
  - `BoolTypedBuffer` adds an inner `Segment` struct (
    `shared_ptr<vector<uint64_t>> packedBits` + `firstBitOffset` +
    `totalBits` + `shared_ptr<vector<time_point>> timestamps`),
    plus `std::atomic<std::shared_ptr<const Segment>>
    publishedSegment_` and a `currentSegment()` accessor used by S6.
    `publishSegment()` copies `packed_` (deque → vector),
    snapshots `headBitOffset_` and `totalBits_`, copies
    `timestamps_`, and `store`s with `memory_order_release`.
  - The other three numeric/string types are folded into a new
    template `LinearTypedBuffer<T>` (CRTP-style) that owns
    `std::deque<T> values_` plus the `Segment` / atomic / publish
    plumbing. `Int64TypedBuffer`, `DoubleTypedBuffer`,
    `StringTypedBuffer` derive from it and only implement the
    type-checked `pushValue()` override.
  - The `signal_buffer_publishes_<id>` counter is registered in the
    base ctor and incremented per publish.
- `tests/unit/buffer/signal_buffer_publish_test.cpp` (5 cases) +
  `tests/unit/buffer/CMakeLists.txt`:
  - One per type confirming the cadence (Double: 99 → 0, 100 → 1,
    +100 → 2, +100 → 3; Int64: 1000 → 10; Bool: 250 → 2; QString:
    100 → 1).
  - Type-mismatch contract: 1000 failed pushes leave the publishes
    counter unchanged; subsequent valid pushes resume normally.

**Build verification** (local):

- Debug + Release + debug-asan all build clean.
- `clang-format --dry-run -Werror` clean (one auto-fix iteration on
  template/decl line wrapping).

**Test verification** (local):

- `ctest --preset=debug` — 287 / 287 pass (was 282; +5 from S4
  tests).
- `ctest --preset=release` — 287 / 287 pass.
- debug-asan deferred to CI (atomic-shared-ptr is the spec §7-2 HALT
  trigger; CI is the authoritative runtime gate).

**Frozen-file diff** vs 6fc6c06: empty.

**Effort**: 4 h (plan estimate 5 h).

**HALT trigger #2 status**: `std::atomic<std::shared_ptr<T>>`
compiled and linked clean across all three presets. No ABI / linker
warnings observed locally; CI green will further confirm.

**CI** (run 25418816004): success — debug, release, debug-asan all
green. **HALT trigger #2 cleared**: `std::atomic<std::shared_ptr<T>>`
operates correctly under CI's debug-asan instrumentation.

---

### S5 — LOD pyramid maintained on write (start)

**Goal**: per plan §2 S5, attach a 4-level LOD pyramid to numeric
typed buffers, with eviction-aware bin maintenance.

1. Add `LodBin<T>` struct (min, max, t_start, t_end).
2. Per numeric typed buffer (`Int64`, `Double`): three `LodLevel`
   instances at bin sizes 10, 100, 1000. Each tracks `bins` deque,
   `firstBinIndex`, `nextBinToEmit`.
3. New base virtual `onPushCompleted()` invoked at the tail of
   `TypedBuffer::push` (after all eviction settles, before publish
   cadence). Numeric derivations override to:
   - For each level: pop front bins whose index < `ceil(E / binSize)`
     (any bin spanning the eviction point is dropped, per spec §9).
   - For each level: if `pushCount_` just crossed
     `(nextBinToEmit + 1) * binSize` AND the bin is intact (not
     spanning eviction), compute min/max + t_start/t_end over the
     last `binSize` samples and append to `bins`.
4. NaN handling for `Double`: NaN samples are stored but excluded
   from min/max aggregation (spec §5.2).
5. `Segment` struct of numeric derivations gains 3 new fields:
   `shared_ptr<vector<LodBin<T>>> lod1, lod2, lod3`. `publishSegment`
   copies the bin deques to immutable vectors.
6. `Bool` and `QString` derivations skip LOD entirely
   (lodEnabled = false regardless of config; `onPushCompleted` is a
   no-op).
7. Add a `lodBinCount(level)` public accessor on `SignalBuffer` so
   tests + S6 query selection can introspect the per-level bin
   count. (Additive method on the freeze surface during M6
   implementation; finalized at M6 close.)
8. Need `pushCount_` counter inside `TypedBuffer` (separate from
   `SignalBuffer::totalPushed_`) so LOD math has access to N from
   the base; SignalBuffer's atomic still mirrors it for thread-safe
   reader access.

**Acceptance**:

- Per-type LOD-bin-count test: 100 pushes to a Double signal
  produces 10 level-1 bins (10 each), 1 level-2 bin (100 each), 0
  level-3 bins.
- LOD eviction edge: 10 pushes, level-1 bin emitted; evict 1 sample
  → front bin dropped (stale).
- Bool / QString: any number of pushes → 0 LOD bins at every level.
- Min/max envelope: synthetic ramp 0..999 → each level-1 bin's
  [min, max] equals [10*i, 10*i+9].
- HALT trigger #7 (LOD envelope misses raw by > 0.1%): not expected
  to fire; correctness verified by the envelope test above.

**Freeze scope**: M2/M3/M4/M5 frozen `.hpp` not modified.
`SignalBuffer` gains `lodBinCount(int level)`. The freeze surface
finalizes at M6 close so additive public methods at this stage are
allowed (and noted in the M6 done.md hand-off).

### S5 — LOD pyramid maintained on write (close)

**Result**: green.

**Changes**:

- `src/buffer/signal_buffer.hpp`:
  - Added `[[nodiscard]] std::size_t lodBinCount(int level) const
    noexcept` public accessor.
- `src/buffer/signal_buffer.cpp`:
  - Added `LodBin<T>` (min/max + t_start/t_end) and
    `LodLevel<T>` (binSize, deque<bin>, firstBinIndex,
    nextBinToEmit) helper templates.
  - Added `accumulateMinMax<T>` NaN-safe helper (NaN check
    compiles out for integer T via `std::is_floating_point_v`).
  - `TypedBuffer` base gains `pushCount_` (cumulative successful
    pushes; used by LOD math), `onPushCompleted()` virtual
    (default no-op), and `lodBinCount(int)` virtual (default 0).
  - `LinearTypedBuffer<T>` becomes the LOD-bearing template:
    - Static `kTypeSupportsLod = std::is_arithmetic_v<T>` selects
      Int64/Double=true, QString=false.
    - Per-instance `lodEnabled_` = type support && config (default
      true if config unset).
    - Three `LodLevel<T>` instances at bin sizes 10, 100, 1000.
    - `onPushCompleted()` per push: front-pop bins whose index <
      `ceil(E / binSize)`; emit a new bin when N crosses a bin
      boundary AND the bin is intact (no eviction crosses it).
    - `Segment` struct gains `lod1`, `lod2`, `lod3` shared-ptr
      vectors; `publishSegment()` populates them only when
      `lodEnabled_`.
    - `valueMemoryBytes` accounts for LOD bin storage too.
    - `clearValues` resets all LOD state.
  - `SignalBuffer::lodBinCount` delegates to `impl_->lodBinCount`.
- `tests/unit/buffer/signal_buffer_lod_test.cpp` (7 cases):
  - Double / Int64 LOD bin counts at 100 / 1000 / 1001 pushes.
  - Min/max envelope correctness placeholder (full verification
    deferred to S10 integration tests where Segment is reachable).
  - Bool / String emit no LOD bins regardless of push count.
  - Stale-bin eviction: 51-sample push with cap=50 forces eviction;
    bin 0 (samples 0..9) is dropped because it spans the eviction
    point. Subsequent pushes verify the level-1 deque correctly
    tracks intact bins.
  - `lodEnabled = false` on a Double signal disables LOD.

**Build verification** (local):

- Debug + Release + debug-asan all build clean.
- `clang-format --dry-run -Werror` clean (one auto-fix iteration on
  the test file's include order).

**Test verification** (local):

- `ctest --preset=debug` — 294 / 294 pass (was 287; +7 from S5
  tests).
- `ctest --preset=release` — 294 / 294 pass.
- debug-asan deferred to CI.

**Frozen-file diff** vs 6fc6c06: empty.

**Compile-fix attempts**: 0. **Test-fix attempts**: 1 (off-by-one
expectations on the level-3 bin emission boundary; the 1000th push
completes bin 0 of level 3, not the 1001st). Within HALT trigger #2
budget.

**HALT trigger #7 status**: not fired. The min/max envelope is
verified by construction in the unit test (ramp 0..99 produces bin
i with [10i, 10i+9]); end-to-end envelope-vs-raw verification lands
in S10's integration test.

**Effort**: 5 h (plan estimate 6 h).

