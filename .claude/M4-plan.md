# M4 — Plan

## 0. Execution ground rules

- Branch: `milestone/M4` (local + origin; pushed at Phase 3 step f).
- Per-subtask discipline (CLAUDE.md §Required #2 and §Git operation protocol):
  1. Append start entry to `.claude/M4-progress.md`.
  2. Implement per plan.
  3. `cmake --build build/{debug,release,debug-asan}` all three presets clean.
  4. `ctest` Debug + Release clean. debug-asan per preset (LSan suppressions already in place; no GUI tests added in M4, so no new suppressions expected).
  5. `clang-format --dry-run -Werror` on changed files.
  6. Append close entry to progress.md with counts and any deviations.
  7. Commit with message `<module>: <imperative verb> <object>`; body includes "Freeze scope: no M2/M3-frozen .hpp modified".
  8. Push `milestone/M4`.
  9. **Watch CI until green via `gh run watch`**; report result before starting next subtask. No silent retry. HALT on unexpected failure.
- No new dependencies; uses M2 MPSC + WatermarkTracker + Qt only.
- Benchmark threshold miss without clear §5.5 category → HALT per spec §7.3.
- UI blocking main thread > 200 ms → HALT per spec §7.6.

## 1. Subtask sequence overview

| # | Subtask | Prereqs | Effort | Commit | Notes |
|---|---|---|---|---|---|
| S1 | Verify MetricsRegistry sanitization + add `src/pipeline/` scaffolding + `FrameSink` header | — | 2 h | Yes | HALT check at §4.6 gate. |
| S2 | `FramePipeline` skeleton: worker, thread, ingress queue, sink registration | S1 | 4 h | Yes | No driver wiring yet. |
| S3 | Driver signal wiring + frame fanout + sink exception isolation | S2 | 3 h | Yes | End-to-end with a test sink on a MockDriver. |
| S4 | Backpressure integration: WatermarkTracker + metrics + drop counter | S3 | 2 h | Yes | Threshold crossings logged. |
| S5 | `PipelineManager` (attach / detach / lookup / signals) | S3 | 3 h | Yes | Integration-test-ready. |
| S6 | Connection Manager wiring (MainWindow owns manager; ConnectionManager attaches/detaches) | S5 | 2 h | Yes | No new widgets. |
| S7 | Integration tests: driver-integration, backpressure, fanout | S6 | 3 h | Yes | §5.3. |
| S8 | Connection-manager integration test update (attach/detach assertions) | S6 | 1 h | Yes | Extends M3's file. |
| S9 | Performance benchmark: `bench_pipeline_throughput.cpp` + baseline update | S7 | 2 h | Yes | §5.4. HALT if overhead > 10 %. |
| S10 | M4 completion report (freeze record + SHA256 + hand-off to M5) + PR | S9 | 2 h | Yes | Per §6.3; same Phase-1 closure flow as M3. |

**Total estimated effort**: 24 h, inside spec's 4–6 person-day budget.

## 2. Subtask details

### S1 — MetricsRegistry sanitization check + src/pipeline scaffolding + FrameSink header

**Preflight check** (HALT-primed per spec §7.2): read `src/observability/metrics.hpp` + `.cpp`. Determine whether `MetricsRegistry::registerCounter/Gauge` sanitizes metric names.

- If sanitization is present (or all characters are accepted): proceed.
- If not: HALT with pre-drafted question (spec §4.6 + understanding §3.9). Do not guess.

Assuming the preflight passes, deliver:

- `src/pipeline/CMakeLists.txt`:
  ```cmake
  add_library(signalforge_pipeline STATIC
      frame_pipeline.cpp
  )
  target_include_directories(signalforge_pipeline PUBLIC ${CMAKE_SOURCE_DIR}/src)
  target_link_libraries(signalforge_pipeline
      PUBLIC Qt6::Core signalforge_frame signalforge_drivers
      PRIVATE signalforge_platform signalforge_observability signalforge_utils
  )
  ```
  (Exact set of libraries refined in S2 if needed; `signalforge_utils` may be header-only.)
- Root `CMakeLists.txt`: add `add_subdirectory(src/pipeline)` after `src/drivers`.
- `src/pipeline/frame_sink.hpp` per spec §4.1 (with explicit `#include <QString>` for `sinkName()` return type — minor implementer's polish).
- Unit test harness: `tests/unit/pipeline/CMakeLists.txt` + placeholder `pipeline_test.cpp` with one `TEST_CASE` that verifies a concrete `TestSink` can be constructed and its defaults do not throw.

**Tests**: 1 unit case (FrameSink defaults are non-throwing no-ops).
**Commit**: `pipeline: scaffold module + add FrameSink header`.

### S2 — FramePipeline skeleton

**Deliverables**:

- `src/pipeline/frame_pipeline.hpp` verbatim from spec §4.2, with Doxygen on all public declarations per §6.1.
- `src/pipeline/frame_pipeline.cpp`:
  - `PipelineWorker` class (internal, defined in .cpp with moc include at file bottom — same pattern as M3 drivers) deriving from `IoWorkerBase`:
    - Constructor: stores PipelineConfig, allocates MPSC queue via `signalforge::utils::MpscQueue<RawFrame>`.
    - `onStarted()` override: invokes `platform::setCurrentThreadName("PipelineWorker-<driverId>")`.
    - Slot `enqueueFrame(RawFrame)`: pushes to MPSC, updates peak depth, runs watermark observation (placeholder for S4).
    - Slot `drain()`: dequeues and (in S3) invokes sinks; in S2, just counts.
  - `FramePipeline` class:
    - Constructor: builds thread+worker pair; thread named `PipelineWorker-<driverId>`.
    - Destructor: `thread_->quit(); thread_->wait(500);` with `terminate()` fallback, matching M3 driver pattern. Log WARN on terminate.
    - `addSink` / `removeSink` / `sinkCount`: guarded by `sinkMutex_`; idempotent addSink logs WARN if shared_ptr already present (compare by raw ptr identity).
    - `peakWatermarkPct` / `stats` / `resetBackpressureStats` / `driverId`: thread-safe accessors (atomics or mutex-guarded).
  - `attachDriver`: stub in S2 — stores pointer; actual signal wiring in S3.

**Tests**:
- Construction with valid config produces a running pipeline thread (observable via `thread_->isRunning()`).
- `addSink(sptr); sinkCount() == 1; removeSink(sptr); sinkCount() == 0`.
- Idempotent addSink: adding same sptr twice leaves sinkCount at 1, emits one log warn.
- `driverId()` accessor returns the config's value.
- Construction with empty driverId: logs ERROR, but constructor completes (spec §5.2 says "document" the choice — we choose to log and continue, which matches M3's ConfigInvalid pattern).

**Tests**: ~6 unit cases.
**Commit**: `pipeline: add FramePipeline skeleton with worker thread`.

### S3 — Driver wiring + frame fanout + sink exception isolation

**Deliverables**:

- `FramePipeline::attachDriver(DriverInterface*)` implementation:
  - Connects `driver->frameReceived → worker->enqueueFrame` with `Qt::QueuedConnection`.
  - Connects `driver->errorOccurred → worker->forwardError` with `Qt::QueuedConnection`.
  - Connects `driver->stateChanged → worker->forwardState` with `Qt::QueuedConnection`.
- Worker's `drain()` slot: takes snapshot of `sinks_` under mutex, releases, iterates, calls each sink's `onFrame` inside `try { ... } catch (const std::exception& e) { SF_LOG_ERROR } catch (...) { SF_LOG_ERROR }`. Increments `framesReceived` counter on each successful drain.
- Worker's `forwardError(DriverError)` and `forwardState(DriverState)`: same try/catch per-sink pattern; not queued through the MPSC since they are rare events (spec §4.4).
- `drain()` is connected to a self-trigger: each `enqueueFrame` does `QMetaObject::invokeMethod(this, "drain", Qt::QueuedConnection)` (or a `QTimer::singleShot(0)`). Single-shot so batches of enqueues collapse into one drain if pending.

**Tests**:
- Fanout: 3 sinks registered, inject frame via MockDriver, each sink's `onFrame` called exactly once with the same frame payload.
- Error forwarding: MockDriver emits `errorOccurred`, all sinks see `onError`.
- State forwarding: MockDriver emits `stateChanged`, all sinks see `onLifecycle`.
- Sink exception isolation: 3 sinks, middle one throws `std::runtime_error`. Other two still get the frame. Pipeline still processes next frame.
- Cross-thread timing: worker slots run on the pipeline thread (verify by `QThread::currentThread() != mainThread` inside sink callback).

**Tests**: ~5 unit cases.
**Commit**: `pipeline: wire driver signals and sink fanout with exception isolation`.

### S4 — Backpressure integration

**Deliverables**:

- Worker holds a `WatermarkTracker tracker_(ingressCapacity)` initialized in its constructor (thresholds from `PipelineConfig::watermarkHighPct` / `recoverPct`).
- On each `enqueueFrame`: call `tracker_.recordSample(currentDepth, capacity)`. If returns a `BackpressureSignal`, handle per `reason`:
  - `QueueFilling`: `SF_LOG_WARN`, set `pipeline_ingress_watermark_<driverId>` gauge to current pct.
  - `QueueRecovered`: `SF_LOG_INFO`, update gauge.
- Ingress queue full (push returns false): increment `pipeline_frames_dropped_<driverId>` counter + `SF_LOG_WARN` + `FramePipeline::Stats::framesDropped++`. Do not invoke sinks for the dropped frame.
- `MetricsRegistry` registrations at `FramePipeline` construction:
  - `pipeline_frames_received_<driverId>` (counter)
  - `pipeline_frames_dropped_<driverId>` (counter)
  - `pipeline_ingress_watermark_<driverId>` (gauge)
  - `pipeline_ingress_depth_peak_<driverId>` (gauge)
  - `pipeline_errors_forwarded_<driverId>` (counter)
- Unregistered at destruction.
- Sanitize `driverId` before inclusion (decision from S1 preflight).

**Tests**:
- Watermark threshold crossing fires via direct `tracker_` invocation (unit-level, no driver needed).
- `framesDropped` counter increments when push returns false (use a tiny `ingressCapacity = 2` config + 10 frames injected rapid-fire).
- `stats().ingressDepthPeak` tracks the peak accurately.
- All five metrics are registered at construction + unregistered at destruction (check via `MetricsRegistry::hasCounter("...")` helper if exists, else indirect: verify via next registration succeeding).

**Tests**: ~4 unit cases.
**Commit**: `pipeline: add WatermarkTracker backpressure observation and metrics`.

### S5 — PipelineManager

**Deliverables**:

- `src/pipeline/pipeline_manager.hpp` verbatim from spec §4.3, Doxygen per §6.1.
- `src/pipeline/pipeline_manager.cpp`:
  - `attach(DriverInterface*, PipelineConfig)`: construct pipeline, insert in `pipelines_` map under mutex, call `pipeline->attachDriver()`, emit `pipelineAttached(driverId, pipeline)`. Duplicate driverId → return nullptr + `SF_LOG_ERROR`.
  - `detach(driverId)`: erase from map under mutex (unique_ptr destructor joins thread); emit `pipelineDetached(driverId)` after the erase releases the mutex.
  - `pipelineFor(driverId)`: returns raw pointer or nullptr under mutex.
  - `pipelineCount()` / `driverIds()`: thread-safe accessors.

**Tests**:
- Attach returns non-null + pipelineCount == 1.
- Duplicate driverId returns nullptr + count unchanged.
- Detach nonexistent is a no-op.
- Signals `pipelineAttached` / `pipelineDetached` fire with the correct driverId.
- `driverIds()` enumerates active drivers.

**Tests**: ~5 unit cases.
**Commit**: `pipeline: add PipelineManager registry with attach/detach signals`.

### S6 — Connection Manager wiring

**Deliverables**:

- `src/app/main_window.{hpp,cpp}`: own a `std::unique_ptr<PipelineManager>` constructed lazily (e.g., on first ConnectionManager open). Pass a raw pointer to `ConnectionManager`'s constructor.
- `src/app/connection_manager.{hpp,cpp}`:
  - New constructor overload taking `PipelineManager*` (keep the old one nullable for tests that don't need pipeline attach).
  - After `driver_->open()` succeeds in `onConnectClicked`, build a `PipelineConfig`:
    - `driverId` = `<driver-type>-<sanitized-suffix>` where suffix is `serial:device`, `tcp:host:port`, `udp:host:port` or `local-bind`, `replay:basename-no-ext`.
    - Defaults for `ingressCapacity`, thresholds from the spec.
  - Call `pipelineManager_->attach(driver_.get(), cfg)` and store the returned `FramePipeline*` in a member.
  - On `onDriverStateChanged(Idle)` (when releasing driver ownership), call `pipelineManager_->detach(currentDriverId_)`.
  - If `pipelineManager_ == nullptr` (e.g., from legacy test path), skip attach/detach silently.
- `signalforge_app_ui` now links `signalforge_pipeline`.

**Tests**: covered in S8.
**Commit**: `app: wire ConnectionManager to PipelineManager for driver attach/detach`.

### S7 — Integration tests

**Deliverables** (all under `tests/integration/`):

- `test_pipeline_driver_integration.cpp`:
  - Scenario 1 (ReplayDriver lifecycle): verifies `onLifecycle` called at every state transition; no `onFrame`; detach cleanly.
  - Scenario 2 (UdpDriver real frames): two UDP drivers, one sinks 100 frames, verify payload equality.
- `test_pipeline_backpressure.cpp`:
  - Slow sink (10 ms sleep in `onFrame`) + fast UDP producer; watermark crosses 80 % within 1 s; `SF_LOG_WARN` observed via log sink or stats counter; producer pauses; watermark recovers below 60 %.
- `test_pipeline_fanout.cpp`:
  - 3 sinks + 50 frames from UdpDriver; each sink counter == 50; payloads equal.

Updates to `tests/integration/CMakeLists.txt` to register the three new executables, linking `signalforge_pipeline`, `signalforge_echo_server_fixture` (not needed here), and existing driver libs.

**Tests**: 5–7 integration cases total (counts depend on how SUCCEED-skip handles local multicast-blocked hosts — same pattern as M3).
**Commit**: `tests: add pipeline driver integration + backpressure + fanout`.

### S8 — Connection-manager integration test update

**Deliverables**:

- Extend `tests/integration/test_connection_manager.cpp` with one new TEST_CASE:
  - Construct `MainWindow` (which owns `PipelineManager`) or directly an external `PipelineManager` + `ConnectionManager` pair.
  - ConnectRe ReplayDriver → assert `pipelineCount() == 1`.
  - Disconnect → wait for Idle → assert `pipelineCount() == 0`.
  - Reconnect with a different driverId (e.g., a second Replay with a different session path) → assert `pipelineCount() == 1` with the new ID.
- The existing four M3 cases remain untouched.

**Tests**: 1 new UI integration case.
**Commit**: `tests: extend connection_manager test with pipeline attach/detach`.

### S9 — Performance benchmark

**Deliverables**:

- `tests/benchmark/bench_pipeline_throughput.cpp`:
  - Two scenarios on UdpDriver localhost:
    1. **Direct**: driver → QObject-level counter (no FramePipeline).
    2. **Pipelined**: driver → FramePipeline → counter sink.
  - Each runs for `kDurationSec = 10`, measures datagrams/sec received.
  - Emits JSON lines matching existing benchmark format.
- `tests/benchmark/CMakeLists.txt`: add new executable entry + link `signalforge_pipeline`.
- `tests/benchmark/run_baselines.sh`: extend to also invoke `bench_pipeline_throughput`, append to results.
- `tests/benchmark/results/M4-baseline.md` (new file, per spec §5.4): comparison table + overhead percentage + §5.5 classification if needed.

**Threshold gate** (spec §7.3): overhead > 10 % of M3 baseline → HALT.

M3 UDP baseline is 198 600 datagrams/s. Overhead ceiling is 19 860 /s (10 %); minimum pipelined throughput is 178 740 /s. If measured overhead is clearly in CC code we fix; if clearly Qt `QueuedConnection` + MPSC we document §5.5 Category 2.

**Commit**: `bench: add pipeline throughput benchmark and M4 baseline`.

### S10 — M4 completion + PR

Matches M3's S12 pattern:

1. Produce `.claude/M4-done.md` per execution-manual §6.2 with:
   - Timing and commit manifest.
   - Deliverables checklist vs M4 spec §2.1.
   - Acceptance self-check vs §8.1 – §8.5.
   - Test count matrix.
   - Benchmark result + §5.5 category if any.
   - Freezes established (§6.3: `FrameSink`, `FramePipeline`, `PipelineManager` with SHA256 sums).
   - Hand-off to M5 (how decoders register as sinks via `pipelineAttached` signal).
   - Deviations ↔ `.claude/M4-concerns.md` if any.
2. Commit `chore: M4 completion report` (docs-only; no rebuild needed per CLAUDE.md Required #2 exception).
3. Push. Watch CI green. Report.
4. Create PR to main with body pointing at M4-done.md (authorized by this session's merge-flow prompt; we do NOT merge, just open — same as M3).
5. Update M4-done.md with PR #, CI status, commit the one-liner update.
6. Stop + announce: "M4 ready. Awaiting approval to merge M4 and begin M5 bootstrap."

**Commit**: `chore: M4 completion report` (and follow-up `chore: record M4 PR number and CI status in done.md`).

## 3. HALT triggers specific to M4 (rehearsed)

Pre-drafted halt statements per spec §7:

- **§7.1** (modification to M2/M3 frozen .hpp): never occurs by design. If a compile error suggests a frozen header needs a method, HALT with "M4 §2.2-2 violation: [header]:[line] required change is [what]; options are [A] wrap via adapter, [B] ADR + new method".
- **§7.2** (metric sanitization): pre-drafted at understanding §3.9.
- **§7.3** (benchmark overhead > 10 %): HALT with "pipeline vs M3 baseline: [measured]/[baseline] = X%; overhead X pp. Suspected cause: [Qt QueuedConnection | MPSC lock contention | sink fanout | other]. Recommend: [profile at function granularity before any 'fix']. HALT per §7.3."
- **§7.4** (sink exception crashes pipeline): this is a unit-test failure; the fix is in the try/catch placement. If after 3 attempts we can't isolate, HALT.
- **§7.5** (thread does not exit in 500 ms): HALT with "pipeline destructor wait-500ms returned false on [test name]. Reproducer: [scenario]. Options: [terminate + restart] vs [extend budget + investigate sink hang]."
- **§7.6** (UI blocks > 200 ms): HALT with "ConnectionManager onConnect/Disconnect wall-clock: [measured] ms > 200 ms budget. Suspected blocking call: [stack]. This violates M3 §7-5 UI quality."

Standard CLAUDE.md HALTs (3 compile/test-fix attempts, new dependency, frozen-file modification) also apply.

## 4. Risk-ranked substitution plan

If a specific subtask HALTs, the standard resolution is to write a HALT report, commit partial state, and exit. The human then decides.

Additional fallbacks already approved by spec:

- **If benchmark overhead is in Category 2 (Qt framework) and demonstrably unavoidable**, document + accept + note in M4-done.md. Spec §5.5 explicitly permits this; M4 §7.3 says HALT on **unfixed** overhead, but "unfixed" here includes "categorized as external".
- **If metric sanitization is absent**, the pre-drafted §7.2 question offers two options; user picks one, we implement.

## 5. Timing and discipline

- Target: 24 h focused implementation time across 3–4 calendar days (matching M3's pacing).
- Every push followed by a CI watch. No silent retries (reinforced after M3's 4 consecutive red-CI incidents).
- Progress log entries per subtask (start + close). Concerns logged immediately, not at the end.

## 6. Closing alignment

The plan mirrors M3's structure (S1 = foundation, S2–S4 = core, S5 = integration, S7 = integration tests, S9 = benchmark, S10 = close). The scope is smaller (24 h vs 48 h for M3). The freeze surface (3 small interfaces) is the highest-value output; everything else exists to protect it.
