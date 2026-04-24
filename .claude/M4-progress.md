# M4 — Progress log

## Session metadata

- Phase 5 execution begins 2026-04-24.
- Branch: `milestone/M4`.
- Plan: `.claude/M4-plan.md`, 10 subtasks S1–S10.
- Understanding: `.claude/M4-understanding.md`.
- Remote: `git@github.com:mornthx/signalforge.git`.

## Subtask log

Each subtask appends a start entry and a close entry. Do not overwrite.

---

### S1 preflight HALT — MetricsRegistry sanitization (opened)

Spec §7.2 gate hit at first action: `MetricsRegistry::getOrCreate`
accepts any UTF-8 string verbatim; no sanitization. HALT report at
`.claude/halt/HALT-20260424T153722Z-m4-s1-metric-sanitization.md`.

Commit `e619588`: HALT report.

### S1 preflight HALT — resolution (close)

Human reviewed the three options and chose a **relaxed validator** on
`MetricsRegistry` (Option B variant): accept any non-empty UTF-8 that
does NOT contain whitespace, control chars, or `"`, `'`, `\`, `<`, `>`.
FramePipeline will use driver IDs verbatim in metric names — no
sanitization helper needed.

**Changes**:
- `src/observability/metrics.hpp`: declare
  `signalforge::observability::isValidMetricName(const QString&)`;
  updated `getOrCreate` Doxygen to state the `nullptr`-on-invalid
  contract.
- `src/observability/metrics.cpp`: implement the validator; plumb into
  `getOrCreate` (log `SF_LOG_ERROR` once on reject, return nullptr).
- `tests/unit/observability/metrics_test.cpp`: 4 new cases covering
  the valid/invalid tables from the user's policy + the registry's
  nullptr-on-invalid behaviour + separator-rich driver ID acceptance.
- `docs/architecture/decisions/ADR-003-metric-name-validation.md`:
  decision + rejected alternatives (Prometheus-style, sanitization) +
  rationale per class of rejected character + cross-refs.

**Verification**:
- Debug + Release: 183/183 tests pass.
- debug-asan: builds clean (runtime blocked locally; CI run
  `24898696564` confirmed green across the debug/release/debug-asan
  matrix).
- clang-format: clean.

**Freeze scope**: no M2/M3-frozen .hpp modified. `MetricsRegistry` is
explicitly not in the M2 freeze surface (`metrics.hpp` line 69).
ADR-003 filed under `docs/architecture/decisions/` per the
ADR-001/ADR-002 precedent.

Commit `d3fd6ff`.

---

### S1 — pipeline scaffolding + FrameSink header (start)

**Goal**: per plan §2 S1, now that the sanitization gate is clear,
produce the M4 module foundation: `src/pipeline/` directory with its
own `CMakeLists.txt` + `signalforge_pipeline` static library, root
`CMakeLists.txt` gains `add_subdirectory(src/pipeline)`, and the
frozen `FrameSink` header at `src/pipeline/frame_sink.hpp` per spec
§4.1.

**Approach**: hand-crafted per spec §4.1, with an explicit
`#include <QString>` and `#include <QtGlobal>` (`QString` is returned
by `sinkName()` and `QChar::Other_Control` usage triggers transitively
in metrics.hpp but not here). The header is the only M4 deliverable
in this subtask that participates in the interface freeze.

A placeholder unit test at `tests/unit/pipeline/pipeline_test.cpp`
verifies that a concrete sink deriving from `FrameSink` can be
constructed and that the default `onError` / `onLifecycle` overrides
are non-throwing no-ops.

No M2/M3-frozen .hpp touched.

### S1 — pipeline scaffolding + FrameSink header (close)

**Files delivered**:
- `src/pipeline/frame_sink.hpp` — FrameSink abstract base per spec §4.1.
  Three virtuals (`onFrame` pure, `onError`/`onLifecycle` with default
  no-op bodies) + `sinkName()` pure. Explicit `#include <QString>`.
- `src/pipeline/frame_pipeline.cpp` — S1 placeholder (empty
  namespace) so `signalforge_pipeline` is a linkable STATIC library
  from the start. The actual implementation lands in S2.
- `src/pipeline/CMakeLists.txt` — defines `signalforge_pipeline`
  STATIC library with public deps `Qt6::Core`, `signalforge_drivers`,
  `signalforge_frame`; private deps `signalforge_observability`,
  `signalforge_platform`, `signalforge_utils`.
- Root `CMakeLists.txt`: adds `add_subdirectory(src/pipeline)` after
  `src/utils` so the dependency chain resolves.
- `tests/unit/pipeline/CMakeLists.txt` + `pipeline_test.cpp` —
  4 unit cases verifying TestSink construction, default no-op
  behaviour of `onError`/`onLifecycle`, and `onFrame` override
  callability.

**Verification**:
- Debug + Release: 187/187 tests pass.
- debug-asan: builds clean (runtime blocked locally; CI authoritative).
- clang-format: clean on all new files.

**Freeze scope**: no M2/M3-frozen .hpp modified. `FrameSink` is a new
header that enters the M4 freeze surface at M4 close.

**Time**: ~30 min (well under the 2 h plan estimate — scope was
already scoped tightly in the plan).

### S2 — FramePipeline skeleton (start)

**Goal**: per plan §2 S2, deliver the public `FramePipeline` header
verbatim from spec §4.2, plus a skeleton `FramePipeline`
implementation that:
- owns a dedicated `QThread` named `PipelineWorker-<driverId>` (via
  `IoWorkerBase` pattern reused from M3 drivers)
- wraps an internal MPSC queue for frames
- exposes `addSink` / `removeSink` / `sinkCount` with idempotent add
- exposes `stats()` / `peakWatermarkPct()` / `resetBackpressureStats`
  as no-op-ish stubs (real backpressure wiring in S4)
- has `attachDriver` as a stub (real signal wiring in S3)
- destructor joins the thread within 500 ms with `terminate()`
  fallback, matching M3 driver pattern

No M2/M3-frozen .hpp touched.

### S2 — FramePipeline skeleton (close)

**Files delivered**:
- `src/pipeline/frame_pipeline.hpp` — public header per spec §4.2.
  Freeze surface: `FramePipeline` class (ctor, `attachDriver`, sink
  add/remove/count, `stats`, `peakWatermarkPct`,
  `resetBackpressureStats`, `driverId`), `PipelineConfig` + `Stats`
  struct layouts.
- `src/pipeline/frame_pipeline.cpp` — S2 implementation with:
  - internal `PipelineWorker` deriving from M3's `IoWorkerBase`,
    holding an `MpscQueue<RawFrame>` as the ingress buffer.
  - `FramePipeline` ctor spawns the worker thread named
    `PipelineWorker-<driverId>` and starts it. Dtor quits + waits
    500 ms with a `terminate()` fallback (M3 driver pattern).
  - Sink registry: `std::vector<shared_ptr<FrameSink>>` guarded by
    `std::mutex`. Idempotent addSink + nullptr addSink + noop-remove
    all handled.
  - `attachDriver` is a stub (driver signal wiring lands in S3).
  - `stats`, `peakWatermarkPct`, `resetBackpressureStats` are stubs
    returning zeros (backpressure + metric wiring lands in S4).
- `src/utils/mpsc_queue.cpp` — adds
  `template class MpscQueue<RawFrame>;` explicit instantiation, per
  the file's own documented extension pattern (line 42–43 comment
  invites this). MpscQueue's frozen **header** (M2-done.md) is
  untouched; only the .cpp instantiation list is extended. Also
  adds `#include "frame/raw_frame.hpp"` for the RawFrame type.
- `src/pipeline/CMakeLists.txt` — adds `frame_pipeline.hpp` to
  sources for AutoMOC visibility.
- `tests/unit/pipeline/pipeline_test.cpp` — 7 new cases covering
  FramePipeline construction, driverId round-trip, sink registry
  (round-trip, idempotent addSink, nullptr ignore, unregistered
  removeSink, multi-sink coexistence), and S2-skeleton Stats.

**Verification**:
- Debug + Release: 194/194 tests pass (11 cases now in pipeline_test).
- debug-asan: builds clean.
- clang-format: clean.

**Freeze scope**: no M2/M3-frozen .hpp modified. The MpscQueue
extension lives in .cpp only; `src/utils/mpsc_queue.hpp` is bit-
identical to M2. `FramePipeline` + `PipelineConfig` + `Stats` enter
the M4 freeze at M4 close.

**Time**: ~50 min (well under 4 h plan estimate).
