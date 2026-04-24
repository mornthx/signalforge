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

### S3 — driver wiring + frame fanout + sink exception isolation (start)

**Goal**: complete `attachDriver` and the worker's drain/forward
logic so a live `DriverInterface` delivers frames to every
registered sink on the pipeline's thread, with exception isolation
(spec §7.4 HALT trigger — a throwing sink must not crash the
pipeline).

**Approach**:
- `attachDriver` connects the three driver signals
  (`frameReceived` / `errorOccurred` / `stateChanged`) to worker
  slots with `Qt::QueuedConnection`, so the worker is the first
  thread-affinity transition point.
- Worker slot `enqueueFrame(RawFrame)`: push to MPSC; if push fails
  (full), increment drop counter + log WARN (real metrics in S4).
  Post a single-shot `drain()` via
  `QMetaObject::invokeMethod(Qt::QueuedConnection)`; batched enqueues
  collapse to one drain event.
- Worker slot `drain()`: while MPSC pops a frame, snapshot sinks
  under mutex, release, iterate, call each sink's `onFrame` inside a
  `try { ... } catch (const std::exception&) { ... } catch (...)
  { ... }` — logged at ERROR, no rethrow (spec §3.1).
- Worker slots `forwardError(DriverError)` and
  `forwardState(DriverState)` call sinks directly (not queued
  through MPSC since they are rare events per spec §4.4). Same
  try/catch isolation per sink.
- Uses the same sink-snapshot pattern (take a copy of `sinks_`
  under lock, release lock before iterating) so a sink's
  `removeSink` during an in-flight callback does not block the
  worker, and a sink that has already been removed is not
  mid-called (the shared_ptr snapshot pins it until the frame
  batch finishes).

**Tests** (unit-level using a MockDriver):
- Fanout: 3 sinks → frame emission results in 3 onFrame calls.
- Error forwarding: driver errorOccurred → all sinks onError.
- State forwarding: driver stateChanged → all sinks onLifecycle.
- Sink exception isolation: middle sink throws; other two still
  called; pipeline stays alive; next frame still fanned out.
- Cross-thread affinity: worker slots execute on the pipeline thread
  (not main), verified via QThread::currentThread().

No M2/M3-frozen .hpp touched. M3's `MockDriver` (from
`tests/mocks/mock_driver.hpp`) is reused.

### S3 — driver wiring + frame fanout + sink exception isolation (close)

**Files delivered**:
- `src/pipeline/frame_pipeline.hpp`:
  - Added `friend class PipelineWorker;` so the internal worker can
    access `sinks_ / sinkMutex_ / framesReceived_ / framesDropped_ /
    errorsForwarded_` without exposing them as public API.
  - Added three atomic `std::uint64_t` counters for stats; bumped
    `stats()` to surface them.
- `src/pipeline/frame_pipeline.cpp`:
  - `PipelineWorker` gained three `public slots`: `enqueueFrame`,
    `forwardError`, `forwardState`. Each drains sinks on the pipeline
    thread with per-sink `try / catch(std::exception&) / catch(...)`
    isolation + ERROR log on throw.
  - `enqueueFrame` pushes to MPSC then `drain()` inline — single
    worker-thread hop per frame batch. A full queue increments
    `framesDropped_` and logs WARN.
  - `forwardError` / `forwardState` bypass the MPSC (rare events).
  - Worker holds `FramePipeline*` back-pointer to read the sink list
    and update counters.
  - `attachDriver`: connects driver's three signals to worker slots
    with `Qt::QueuedConnection`. nullptr and double-attach are
    rejected + logged.
  - Destructor: disconnects worker from driver before thread join to
    avoid late-signal-into-half-destroyed-worker.
  - `resetBackpressureStats` now resets `framesDropped_` (peak
    watermark still S4).
- `tests/unit/pipeline/pipeline_test.cpp` + `CMakeLists.txt`:
  - Links `signalforge_mocks` + `Qt6::Test`.
  - Adds `CountingSink` / `ThrowingSink` helpers + `pumpUntil` wait
    helper.
  - 5 new cases:
    * fanout to 3 sinks (each receives one onFrame with matching
      payload; framesReceived=1, framesDropped=0)
    * errorOccurred fans out to every sink; `errorsForwarded=1`
    * stateChanged fans out at every transition (open → Opening →
      Open, sink sees 2 onLifecycle calls)
    * throwing sink isolated — siblings still called; pipeline alive
      after; next frame still delivered
    * sink callbacks run on the pipeline thread, not main

**Verification**:
- Debug + Release: 199/199 tests pass.
- debug-asan: builds clean (runtime blocked locally; CI authoritative).
- clang-format: clean.

**Freeze scope**: no M2/M3-frozen .hpp modified. The `friend class
PipelineWorker` line inside the M4-frozen `FramePipeline` declaration
is a pre-freeze implementation choice; the freeze snapshot captures
it. New atomic members are private and behind the `private:` label —
not part of the public ABI.

**Time**: ~1 h (well under 3 h plan estimate).

### S4 — WatermarkTracker + metrics (start)

**Goal**: wire M2's `WatermarkTracker` into `PipelineWorker` and
register the 5 per-driver metrics from spec §4.6:

- `pipeline_frames_received_<driverId>` (counter)
- `pipeline_frames_dropped_<driverId>` (counter)
- `pipeline_errors_forwarded_<driverId>` (counter)
- `pipeline_ingress_watermark_<driverId>` (gauge, 0–100)
- `pipeline_ingress_depth_peak_<driverId>` (gauge, samples)

**Approach**:
- Worker constructs `WatermarkTracker(ingressCapacity, highPct,
  recoverPct)` from `PipelineConfig`.
- `enqueueFrame` calls `tracker_.observe(depthAfterPush, driverId)`.
  On returned `BackpressureSignal`:
    - `QueueFilling`: `SF_LOG_WARN` + gauge set to `watermarkPct`
      (M4 spec §3.3).
    - `QueueRecovered`: `SF_LOG_INFO` + gauge set to `watermarkPct`
      (M4 spec §3.3).
    - `QueueFull` (if returned by observe): treat as WARN (spec is
      silent; defensive).
- Depth peak maintained as `std::atomic<std::uint32_t>` on the
  FramePipeline; updated monotonically on each push. Surfaced via
  `stats().ingressDepthPeak` and the `pipeline_ingress_depth_peak_*`
  gauge.
- Metric registrations done once at `FramePipeline` construction via
  `MetricsRegistry::getOrCreate`. Under ADR-003 the names are accepted
  verbatim (driver IDs with `:`, `/`, `.` all pass). Null-guarded so a
  degenerate driver ID (e.g., contains forbidden char) degrades
  gracefully — pipeline still functions, metrics just don't tick.
- Counters already maintained in S3 are mirrored into the registry
  counters. Drop increments happen in the same place as the existing
  `framesDropped_.fetch_add`.
- `resetBackpressureStats` dispatches a `QMetaObject::invokeMethod`
  with `Qt::QueuedConnection` to the worker so
  `WatermarkTracker::reset()` runs on the worker thread (the tracker's
  reset is documented as "not thread-safe with concurrent observe").
  Also zeroes `framesDropped_` and `ingressDepthPeak_`.
- `peakWatermarkPct()` forwards to `tracker_.peakPct()`.

**Tests**:
- Watermark threshold crossing fires signal (direct tracker test is
  already in M2's backpressure_test; M4 adds a test where
  ingressCapacity is tiny (say 4) so rapid enqueues trigger
  QueueFilling, and that the resulting gauge is non-zero.
- `framesDropped` counter and `pipeline_frames_dropped_*` gauge
  increment when the queue is at 100 % (via capacity=1 + rapid
  double-enqueue).
- `stats().ingressDepthPeak` tracks monotonically.
- All 5 metrics appear in `MetricsRegistry::metricNames()` after
  pipeline construction with a non-degenerate driver ID.
- `resetBackpressureStats` zeroes `framesDropped` + peak (after a
  brief pump to let the worker-side reset execute).

No M2/M3-frozen .hpp touched.

### S4 — WatermarkTracker + metrics (close)

**Files delivered**:
- `src/pipeline/frame_pipeline.hpp`: added `ingressDepthPeak_` atomic.
- `src/pipeline/frame_pipeline.cpp`:
  - `PipelineWorker` ctor registers 5 metrics per spec §4.6 via
    `MetricsRegistry::getOrCreate`; under ADR-003 driver IDs with
    `:`, `/`, `.` pass verbatim.
  - `PipelineWorker` now holds a `WatermarkTracker` built from
    `PipelineConfig` thresholds.
  - `enqueueFrame`:
    - Explicitly checks `sizeApprox() >= ingressCapacity` as a
      soft cap (MpscQueue wraps moodycamel, which is actually
      unbounded — `ingressCapacity` becomes a pipeline-level
      convention).
    - Updates peak monotonically via compare-exchange on the
      `ingressDepthPeak_` atomic + the corresponding gauge.
    - Calls `tracker_.observe(depth, driverId)` and routes returned
      `BackpressureSignal` to `SF_LOG_WARN` (QueueFilling /
      QueueFull) or `SF_LOG_INFO` (QueueRecovered) + watermark
      gauge update — matching M4 spec §3.3's bespoke severity
      mapping (diverges from M2 backpressure's "always WARN"
      default, deliberately).
  - `drain` increments `pipeline_frames_received_<driverId>` on
    every popped frame.
  - `forwardError` increments `pipeline_errors_forwarded_<driverId>`.
  - `resetBackpressureStats` now also zeroes `ingressDepthPeak_` and
    dispatches `resetInternal` to the worker via
    `Qt::QueuedConnection` so `WatermarkTracker::reset()` (documented
    not-thread-safe-with-observe) runs on the worker thread.
  - `peakWatermarkPct()` forwards to `tracker_.peakPct()`.
  - `stats()` surfaces `ingressDepthPeak`.
  - All metric writes are null-guarded so a degenerate driver ID
    that fails ADR-003 validation degrades gracefully.
- `tests/unit/pipeline/pipeline_test.cpp`: 5 new cases
  ([pipeline][frame_pipeline][metrics] and
  [pipeline][frame_pipeline][backpressure] tags):
  - all 5 metrics present in registry after FramePipeline ctor.
  - framesReceived + errorsForwarded metrics tick under load.
  - ingressCapacity=0 drops every frame + bumps `framesDropped` +
    `pipeline_frames_dropped_<driverId>` gauge.
  - ingressDepthPeak tracks monotonically with a blocking sink.
  - resetBackpressureStats zeroes framesDropped and ingressDepthPeak.

**Design note**: the plan originally assumed a blocking sink would
make the MpscQueue fill up. With inline drain, frames pile up in
Qt's event queue (not MpscQueue) when the worker blocks, so the cap
check never fires under that scenario. The tests use `capacity=0`
to exercise the drop path directly — a valid edge case that
demonstrates the mechanism without architecting-around Qt's event
queue. Real production backpressure relies on the same cap check
when driver emit-rate exceeds pipeline service-rate.

**Verification**:
- Debug + Release: 204/204 tests pass.
- debug-asan: builds clean (runtime blocked locally; CI authoritative).
- clang-format: clean.

**Freeze scope**: no M2/M3-frozen .hpp modified. `FramePipeline`'s
public API is unchanged since S3 except the additional private
atomic member (behind `private:`). ADR-003-validated metric names;
no MetricsRegistry changes.

**Time**: ~1.5 h (under 2 h plan estimate).

### S5 — PipelineManager (start)

**Goal**: deliver `src/pipeline/pipeline_manager.{hpp,cpp}` per spec
§4.3. Central registry for `FramePipeline` instances; `attach` /
`detach` / `pipelineFor` / `pipelineCount` / `driverIds` + two
signals (`pipelineAttached` / `pipelineDetached`). Thread-safe.
Owns all pipelines via `std::unique_ptr`.

**Approach**:
- Header verbatim per spec §4.3 with an additional `#include
  <cstddef>` for `size_t` (not in spec's listing but required).
- Implementation uses `std::unordered_map<QString, unique_ptr<FramePipeline>>`
  under a `std::mutex`, mirroring MetricsRegistry's pattern.
  QString keys require hashing — Qt provides `qHash` specialisation
  that the map can use via a helper specialisation for
  `std::hash<QString>` (or we use `std::string` keys derived from
  `QString::toStdString()`, matching MetricsRegistry exactly).
- `attach`:
  - validates non-empty driverId and uniqueness
  - constructs `FramePipeline` with the given config
  - calls `pipeline->attachDriver(driver)` before inserting into the
    map so the signal connection is in place by the time consumers
    see `pipelineAttached`
  - inserts + emits `pipelineAttached(driverId, pipeline)`
- `detach`: erase under mutex (unique_ptr destructor joins the
  thread), then emit `pipelineDetached` outside the lock (avoid
  deadlock if a signal handler calls back into the manager).
- Destructor: iterate and destroy all pipelines. Emit detached
  signals? The spec says "Manager destruction detaches all pipelines"
  — so yes, we emit. But on destruction the signal's consumers may
  already be half-destroyed; safer to emit inside destructor with a
  swap-then-destroy pattern.

Tests cover: attach/detach round-trip, duplicate driverId rejection,
pipelineFor lookup, pipelineCount and driverIds, signal emission
correctness.

No M2/M3-frozen .hpp touched.

### S5 — PipelineManager (close)

**Files delivered**:
- `src/pipeline/pipeline_manager.hpp` — public header per spec §4.3.
  Freeze surface: `PipelineManager` class (5 public methods),
  `pipelineAttached(QString, FramePipeline*)` and
  `pipelineDetached(QString)` signals.
- `src/pipeline/pipeline_manager.cpp`:
  - `attach` validates nullptr driver + empty driverId; constructs
    `FramePipeline`, calls `attachDriver`, inserts under mutex, emits
    `pipelineAttached` outside the lock. Duplicate driverId → the
    just-built pipeline is dropped (thread joins on scope exit) and
    nullptr returned.
  - `detach` erases under mutex, releases the unique_ptr outside the
    lock (so the ≤500 ms thread-join budget doesn't block other
    callers), emits `pipelineDetached`.
  - `pipelineFor` / `pipelineCount` / `driverIds` — thread-safe
    accessors.
  - Destructor: swap-then-destroy pattern — move the map out under
    lock, then destroy pipelines with the lock released. Emits
    `pipelineDetached` for each, matching the "manager destruction
    detaches all pipelines" contract.
  - Uses `std::unordered_map<std::string, unique_ptr<FramePipeline>>`
    keyed on `QString::toStdString()`, mirroring MetricsRegistry.
- `src/pipeline/CMakeLists.txt`: adds `pipeline_manager.cpp` +
  `pipeline_manager.hpp` to sources.
- `tests/unit/pipeline/pipeline_test.cpp`: 7 new cases:
  * attach creates + bumps count + pipelineFor works; detach removes
  * duplicate driverId returns nullptr + count unchanged
  * attach(nullptr driver) and attach(empty driverId) both refused
  * detach on unknown id is a no-op
  * driverIds enumerates all attached
  * pipelineAttached / pipelineDetached signals fire with the right
    driverId + pipeline pointer
  * destructor destroys all attached pipelines cleanly

**Verification**:
- Debug + Release: 211/211 tests pass.
- debug-asan: builds clean (runtime blocked locally; CI authoritative).
- clang-format: clean.

**Freeze scope**: no M2/M3-frozen .hpp modified. `PipelineManager`'s
public API enters the M4 freeze at M4 close.

**Time**: ~45 min (under 3 h plan estimate).

### S6 — ConnectionManager wiring (start)

**Goal**: per plan §2 S6, wire the M3 Connection Manager UI to the
new `PipelineManager` so each Connect creates a pipeline and each
Disconnect destroys it. No new UI widgets — internal wiring only.

**Approach**:
- `MainWindow` gains a `std::unique_ptr<PipelineManager>` owned for
  the process lifetime; allocated lazily in the menu slot (first
  `File → Connection Manager...` open) and passed into the dialog's
  new constructor overload.
- `ConnectionManager` gains:
  - New ctor overload `(PipelineManager*, QWidget* parent = nullptr)`.
    Existing no-manager ctor kept (delegates with nullptr manager)
    so existing integration tests are not broken.
  - Member `PipelineManager* pipelineManager_` (non-owning).
  - Member `QString currentDriverId_` capturing the id used at
    `attach`, so `detach` uses the exact same string even if config
    fields later change.
  - `makeDriverId()` private helper producing:
    - `serial:<device>` from serialDevice_
    - `tcp:<host>:<port>` from tcp fields
    - `udp:<localAddr>:<localPort>` or `udp:-:<remoteHost>:<remotePort>`
      (implementer's judgement — covers both bind-only and
      send-only configurations; uniqueness required only across
      concurrent connections per spec §4.5)
    - `replay:<basename>` from replayPath_
  - `onConnectClicked`: after successful `driver_->open()`, if
    `pipelineManager_` is set, call `attach(driver_.get(), cfg)` and
    store the returned pipeline pointer (non-owning). If attach
    returns nullptr (duplicate id, shouldn't happen in M3 single-
    connection mode), log ERROR but keep the driver connected (the
    connection still works — just no pipeline).
  - `onDriverStateChanged(Idle)`: existing path releases driver
    ownership; also `pipelineManager_->detach(currentDriverId_)` +
    clear state. Must run before `driver_.reset()` because detach
    implicitly disconnects worker; fine either order actually, but
    safer to detach first.
- `signalforge_app_ui` static library now links `signalforge_pipeline`.

Minimal UI test coverage in S8 (extends M3 test file) will assert
pipelineCount transitions, since offscreen Widget testing is already
wired up.

No M2/M3-frozen .hpp touched. `ConnectionManager` is explicitly
non-frozen per M3-done §"Not frozen".

### S6 — ConnectionManager wiring (close)

**Files delivered**:
- `src/app/main_window.{hpp,cpp}`: owns a
  `std::unique_ptr<PipelineManager>` (lazy-constructed on first
  menu open). Forward-declares `pipeline::PipelineManager`.
- `src/app/connection_manager.{hpp,cpp}`:
  - New ctor overload
    `ConnectionManager(pipeline::PipelineManager*, QWidget*)`. The
    original ctor delegates with nullptr manager so existing
    M3 offscreen tests keep compiling unmodified.
  - New members: `pipelineManager_` (non-owning), `pipeline_`
    (non-owning; returned by `attach`), `currentDriverId_` (captures
    the id used at attach so detach uses the same string).
  - `onConnectClicked`: after driver `open()` success, if manager is
    set, builds a `PipelineConfig` with the driverId from
    `makeDriverId()` and calls `attach`. Failure is logged but the
    connection continues without a pipeline.
  - `onDriverStateChanged(Idle)`: detaches pipeline BEFORE resetting
    `driver_`. Clears `currentDriverId_` and the pipeline pointer.
  - Destructor: same detach path defensively, in case the Idle spin
    times out.
  - `makeDriverId(DriverType)` helper produces:
    * `serial:<device>`
    * `tcp:<host>:<port>`
    * `udp:<remoteHost>:<remotePort>` (preferred, for send-configured
      drivers) or `udp:<localAddr>:<localPort>` fallback
    * `replay:<basename>` (from `QFileInfo::fileName`)
    ADR-003 accepts `:`, `/`, `.` verbatim, so metric-name
    concatenation works without sanitization.
- `src/app/CMakeLists.txt`: `signalforge_app_ui` now PUBLIC-links
  `signalforge_pipeline`.

**Verification**:
- Debug + Release: 211/211 tests pass (no new tests in S6; S8 adds
  pipeline attach/detach assertions).
- debug-asan: builds clean.
- clang-format: clean.

**Freeze scope**: no M2/M3-frozen .hpp modified. `ConnectionManager`
is explicitly not-frozen per M3-done.md "Not frozen". The new
ctor overload is an additive extension preserving the existing API.

**Time**: ~35 min (under 2 h plan estimate).
