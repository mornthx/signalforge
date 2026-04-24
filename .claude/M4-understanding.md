# M4 — Understanding

## 1. Restatement of the M4 goal

M4 delivers the **routing layer** between M3's concrete drivers and future downstream consumers (M5 Decoder, M10 Session Writer, and any other `RawFrame` sink). M4 is a **connective-tissue milestone**: it freezes three interfaces (`FrameSink`, `FramePipeline`, `PipelineManager`) that M5–M11 depend on, but it deliberately does *no* frame decoding. `RawFrame` is forwarded byte-for-byte.

Hard-stop type is **Interface freeze + Implementation correctness**. Soft-HALT not allowed. The spec calls this a "thin" milestone — simplicity is the highest-order virtue. Anything beyond routing + fanout + backpressure observation + lifecycle coordination is scope creep.

Quality philosophy carried from M3: per-frame latency ≤ 20 µs from driver emission to sink callback; pipeline overhead ≤ 10 % of M3 driver baseline.

## 2. Observed repo state

State reconciled at Phase 3 start, before M4 understanding was drafted:

```
$ git log --oneline origin/main -5
7d0a78c Merge pull request #4 from mornthx/milestone/M3        # M3 merge commit
70460b0 docs: merge v1 structural content into v2 roadmap (v2.1)
438c6df docs: merge v1 structural content into v2 roadmap (v2.1)
89bb948 docs: re-split roadmap M4-M11 into M4-M13 for finer granularity
5bd73b6 Merge pull request #3 from mornthx/milestone/M2
```

Phase 3 actions already completed:

- PR #4 merged to main (merge commit `7d0a78c`).
- Tag `v0.0.4-alpha.1` annotated on the merge commit, pushed.
- `milestone/M4` branch created from main at `7d0a78c` and pushed with upstream tracking.

Spec `docs/milestones/M4-frame-pipeline.md` is present on main (committed prior to M3 merge by the human per the session 2 task: "Next session is M4 spec commit; session after is M3 merge").

**Incoming from M3** (frozen, read-only — verified by diff below):

- `src/drivers/driver_interface.hpp` — `DriverInterface`, `DriverState`, `DriverErrorCode`, `DriverError` (M2 freeze, preserved through M3).
- `src/drivers/{serial,tcp,udp,replay}_driver.hpp` — concrete drivers emitting `frameReceived` / `errorOccurred` / `stateChanged`.
- `src/drivers/driver_configs.hpp` — the four `*Config` value types.
- `src/drivers/io_worker_base.hpp` — IoWorker pattern (utility, not frozen, reused internally by M4's PipelineWorker).
- `src/frame/raw_frame.hpp` — `RawFrame`, stats types (M2 frozen).
- `src/frame/backpressure.hpp` — `WatermarkTracker`, `BackpressureSignal`, `BackpressureReason` (M2 frozen).
- `src/utils/mpsc_queue.hpp` — MPSC queue primitive (M2 frozen; M4's ingress queue).
- `src/observability/metrics.hpp` — `MetricsRegistry` (M2; M4 adds pipeline gauges/counters to it — additive, not modification).
- `src/observability/logging.hpp` — `SF_LOG_*` macros (M2; unchanged).
- `src/platform/thread_utils.hpp` — `setCurrentThreadName` (M2; used by PipelineWorker).
- `src/app/connection_manager.{hpp,cpp}` — M3 preview UI; M4 modifies to wire in pipeline attach/detach (spec §4.5 permits this).
- `src/app/main_window.{hpp,cpp}` — M3 menu wiring (unchanged in M4; pipeline manager is instantiated here or elsewhere at implementer's judgment).
- `signalforge_app_ui` static library (M3).

**No M4 spec ambiguities surfaced**. Every design decision is locked (§3.1 – §3.7), every file location is named (§4.1 – §4.5), every metric is named (§4.6), every HALT trigger is listed (§7), every acceptance criterion is explicit (§8).

## 3. Key constraints

### 3.1 New top-level module `src/pipeline/`

The spec places all new code at `src/pipeline/` — a new top-level directory. This requires:
- `add_subdirectory(src/pipeline)` in the root `CMakeLists.txt`
- A new `src/pipeline/CMakeLists.txt` defining a `signalforge_pipeline` static library
- Tests at `tests/unit/pipeline/` and new integration tests at `tests/integration/`

### 3.2 `FrameSink` is pure C++, not QObject

Per §3.1, `FrameSink` has three pure-virtual / default-virtual methods and a pure-virtual `sinkName()` returning `QString`. It is NOT a `QObject` — this is deliberate: M5's Decoder and M10's Session Writer are pure-C++ classes with no Qt inheritance need, and forcing `QObject` would introduce metatype overhead.

The header as written in the spec (§4.1) returns `QString` but does not include `<QString>`. The spec's `#include "frame/raw_frame.hpp"` transitively pulls `QString` via `QByteArray`, so the code compiles; nonetheless we will add an explicit `#include <QString>` in `frame_sink.hpp` per best-practice "include what you use". This is implementer's-judgment polish, not a spec deviation.

### 3.3 `FramePipeline` IS a QObject, owns a QThread

Per §3.2, the pipeline is a `QObject` whose inner worker runs on a dedicated `QThread`. Driver → worker signal crossing uses `Qt::QueuedConnection`. Thread naming follows §4.7 (`PipelineWorker-<driverId>` via `platform::setCurrentThreadName`). We reuse the `IoWorkerBase` pattern from M3 for consistency.

### 3.4 Backpressure is ingress-only

Per §3.3, a single `WatermarkTracker` per pipeline observes the ingress-queue depth. No per-sink watermark in M4. Threshold crossing only logs + updates the metric; it does **not** drop frames. Frame drops happen only when the ingress queue is at 100 % capacity (spec §3.3 closing paragraph).

### 3.5 Sink exception isolation

Per §3.1 closing and HALT-4 in §7, a sink that throws from `onFrame` / `onError` / `onLifecycle` must be caught, logged at ERROR level, and the pipeline must continue with the remaining sinks. This is a **safety property**, tested explicitly (§5.2 unit tests and §8.1).

### 3.6 Sink registration uses `std::shared_ptr<FrameSink>`

Per §4.2's `addSink(std::shared_ptr<FrameSink>)` signature. This allows the pipeline to keep the sink alive during an in-flight callback even if the caller releases its last strong reference — preventing the classic "sink freed mid-onFrame" use-after-free. Caller responsibility is documented in the Doxygen for `FrameSink` (§4.1): "sinks must outlive the pipeline they are registered with, or be removed via `removeSink` before destruction".

### 3.7 `PipelineManager` owns all `FramePipeline` instances

Per §3.5: callers (Connection Manager) do not own `FramePipeline` instances. They call `PipelineManager::attach(driver, config)` and receive a non-owning raw pointer valid until `detach(driverId)`. This centralizes lifecycle management and prevents accidental double-destruction. Per §4.3, the manager is itself a `QObject` and emits `pipelineAttached(QString, FramePipeline*)` and `pipelineDetached(QString)` signals so future sink registrars (M5 Decoder) can auto-attach.

### 3.8 Connection Manager integration is minimal

Per §4.5, on Connect-after-Open the dialog calls `pipelineManager_->attach(driver.get(), PipelineConfig{.driverId=makeDriverId(driver)})` and stores the pointer. On Disconnect-after-Idle it calls `detach(driverId)`. **No new UI widgets**. Driver ID format per §4.5 is implementer's judgment with the constraint that it be unique across concurrent connections; we adopt `<driver-type>-<suffix>` where suffix is a sanitized form of the primary-config field (device path / host:port / filename).

PipelineManager ownership: the spec is silent on exactly where `PipelineManager` lives. Our plan places it as a member of `MainWindow` (owned by main.cpp) and passed into `ConnectionManager`'s constructor. This matches how M3 already threads drivers through the dialog.

### 3.9 Metric sanitization concern

Spec §4.6 introduces five per-driver metrics whose names include `driverId`. The spec §7 HALT-2 reads: "MetricsRegistry does not sanitize driver IDs with special characters, and spec §4.6 requires sanitization → HALT (ask whether to implement sanitization here or add it to metrics registry per ADR)".

We will verify `MetricsRegistry`'s current behaviour as the first substep (before writing any pipeline code). If sanitization is absent, HALT immediately per §7. No guess.

### 3.10 Benchmark threshold is hard

Per §5.4 and §7 HALT-3: pipeline overhead > 10 % of M3 direct UDP baseline → HALT. M3's measured UDP localhost baseline was **198 600 datagrams/s** (see `tests/benchmark/results/M3-baseline.md`). With the pipeline the threshold is **≥ 178 740 /s**. We'll budget measurement noise at ±5 % during interpretation, but the hard line remains ≥ 90 % of baseline.

## 4. Freeze surface analysis

M4 freezes three public declarations. Each is small and has precedent:

1. **`FrameSink`** — 3 virtuals + `sinkName()`. Mirror of many sink/visitor interfaces in Qt/C++. Freeze cost: very low; future sinks only implement, never extend.
2. **`FramePipeline`** — one explicit constructor, ~8 public methods, `Stats` + `PipelineConfig` struct layouts. Freeze cost: medium. Internal layout (`PipelineWorker`, mutex, vectors) is explicitly **not** frozen (§6.2).
3. **`PipelineManager`** — one constructor, 5 methods, 2 signals. Freeze cost: low. The `pipelineAttached` signal is the key extension point for M5.

Frozen files' SHA256 sums go in `M4-done.md` §Freezes per §6.3.

**Additive compatibility**: new metrics added to `MetricsRegistry` are additive per §6.1 last paragraph and do not break the M2 metrics freeze. We verify: existing metrics remain registered, no renames.

## 5. Risks and mitigations

### Rank 1 — Benchmark overhead exceeds 10 % → HALT

The plan's primary latency cost is the extra `QueuedConnection` hop (driver thread → pipeline thread) plus the MPSC enqueue/dequeue. Qt `QueuedConnection` is measured in the low-microsecond range on modern hardware; MPSC push/pop is sub-microsecond. Total overhead expected: ~2–5 µs per frame; pipeline throughput should come within 2–3 % of driver-direct.

**Estimated probability of miss**: 15 %. **Mitigation**: take a quick measurement immediately after the first end-to-end wiring, before writing the full test suite, to catch any architectural miss early. If overhead > 10 %, investigate WatermarkTracker overhead (could be a lock-heavy path), sink fanout lock contention (rare with 1 sink), and QMetaObject::invokeMethod overhead (measurable).

### Rank 2 — Metric sanitization absent (HALT per §7.2)

**Estimated probability**: 25 % — we haven't yet read M2's `metrics.hpp` in detail. Mitigation: we check in the first substep (S1.a) before writing any pipeline code. If absent, HALT with the question pre-drafted: "spec §4.6 requires driverId in metric names; current MetricsRegistry rejects characters `[/.:@]`. Option A: sanitize in pipeline at metric registration time. Option B: add sanitization to MetricsRegistry via new ADR. Recommend A (keeps M2 freeze intact)."

### Rank 3 — UI integration blocks > 200 ms (HALT per §7.6)

Per M3, `ConnectionManager`'s disconnect is fully async (state-driven). Adding `PipelineManager::detach` during the Idle callback must also be non-blocking. The detach path joins the pipeline thread with a bounded budget (we adopt 500 ms to match M3's driver thread budget, but the UI itself is not blocked because detach runs on the UI thread's event loop after the driver's queued Idle signal has already landed).

**Estimated probability**: 5 %. Mitigation: benchmark `PipelineManager::detach()` wall-clock from UI thread in the integration test.

### Rank 4 — ReplayDriver does not emit frames

M3's ReplayDriver skeleton emits no frames (deferred to M11). Integration test scenario 1 (§5.3) is written specifically for lifecycle-only verification. Scenario 2 uses two UdpDrivers to verify real frame flow. No additional mitigation needed; the test design already handles it.

**Estimated probability of surprise**: 0 %.

### Rank 5 — Thread shutdown budget (HALT per §7.5)

Spec §7.5: pipeline thread must exit within 500 ms on destruction → HALT. M3 drivers use the same 500 ms budget with a `QThread::terminate()` fallback after timeout. We reuse the pattern. At test scale (≤ 10 K frames in ingress queue), normal shutdown is sub-millisecond; only a sink-that-hangs scenario would come close to 500 ms, and those tests are not in M4's scope.

**Estimated probability of miss under normal conditions**: 1 %.

## 6. Relationship to prior milestones' assets

- **M2 freeze surface**: every driver's `frameReceived` / `errorOccurred` / `stateChanged` signal is preserved. `RawFrame::payload` is `QByteArray` with implicit sharing; sinks copy the reference cheaply. `WatermarkTracker` has the M2 API `recordSample(depth, capacity)` returning a signal if threshold crossed — we use it exactly.
- **M3 frozen files**: `driver_interface.hpp`, `driver_configs.hpp`, concrete driver hpps — read-only. The Connection Manager is NOT part of M3's freeze scope per M3-done §"Not frozen"; we may modify it.
- **M3 test infrastructure**: `CoreAppHolder` pattern, `waitForState` helper, `EchoServer` and `SocatVirtualPair` fixtures — reused where helpful.
- **M3 benchmark harness**: `tests/benchmark/CMakeLists.txt` (gated by `SIGNALFORGE_BENCHMARKS=ON`), echo server fixture linked — we add `bench_pipeline_throughput.cpp` under this gate.
- **LSan suppressions (`tools/lsan_suppressions.txt`)**: pipeline tests do not construct `QApplication`, so Qt-Gui init leaks do not appear. Existing suppressions stand unchanged.

## 7. Assumptions taken as given

- `PipelineManager`'s ownership hierarchy lives in `MainWindow`. The instance is passed into `ConnectionManager` via pointer/reference. Alternative (a process-wide singleton) is rejected — singletons complicate lifetime in tests.
- Integration test `test_connection_manager.cpp` gains a new case for pipeline attach/detach assertion (§5.5). The existing four M3 cases remain.
- Pipeline `sinks_` vector is a `std::vector<std::shared_ptr<FrameSink>>` guarded by a mutex. Iteration in the worker takes a snapshot under lock, then releases, then iterates outside the lock — so a sink's `removeSink` does not block during an in-flight callback.
- On a throwing sink: we `catch (std::exception& e)` and log `e.what()` at ERROR; we also `catch (...)` with a generic message. Both cases do not re-throw (sink isolation).

## 8. What M4 does NOT do (scope control)

Copied from spec §2.2 for quick reference in later reviews:

1. No decoding (M5).
2. No modifications to M2 or M3 frozen files.
3. No new top-level dependencies.
4. No signal buffer (M6).
5. No expression evaluation (M7).
6. No UI changes beyond minimal Connection Manager wiring.
7. No cross-pipeline fanout.
8. No sink-side threading policy enforcement.

If we find any of these tempting during implementation, HALT.
