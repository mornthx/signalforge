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

