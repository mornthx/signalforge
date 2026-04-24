# M4 — Frame Pipeline

| Field | Value |
|---|---|
| Milestone ID | M4 |
| Sprint | 4 |
| Estimated effort | 4-6 person-days |
| Prerequisites | M3 closed (main at v0.0.4-alpha.1) |
| Next milestone | M5 (Decoder Layer) |
| Hard-stop type | **Interface freeze** (FrameSink + FramePipeline) + **Implementation correctness** |
| Soft-HALT allowed | **No** |
| Branch | `milestone/M4` |

**Cross-reference notation**:

- `[EM §N]` — Execution Manual, section N
- `[Arch §N]` — Architecture document, section N
- `[MR]` — Milestone Roadmap
- `[CM §X]` — CLAUDE.md, section X
- `[ADR-N]` — Architecture Decision Record N
- `[M2 §N]` — M2 spec frozen contracts
- `[M3 §N]` — M3 spec concrete drivers

---

## 1. Goal

Provide the routing layer between concrete drivers (M3) and downstream consumers (decoder, session writer, any other RawFrame sink). M4 deliberately excludes decoding — `RawFrame` is passed through as-is to registered sinks. The pipeline adds per-driver fanout, backpressure observation, and lifecycle coordination.

Design philosophy: **simple, observable, zero-overhead when not needed**. A driver with no sinks should impose no pipeline cost beyond signal emission. A driver with sinks should have per-frame latency under 20μs from emission to sink callback.

This milestone freezes two interfaces (`FrameSink`, `FramePipeline`) that M5–M11 depend on. Freeze discipline is M2-level: modifications post-merge require new ADR.

---

## 2. Scope

### 2.1 Must deliver

1. **`FrameSink` interface** at `src/pipeline/frame_sink.hpp`:
   - Abstract base class
   - Pure virtual `onFrame(const RawFrame&)` method
   - Optional `onError(const DriverError&)` method (default: no-op)
   - Optional `onLifecycle(DriverState)` method (default: no-op)

2. **`FramePipeline` class** at `src/pipeline/frame_pipeline.{hpp,cpp}`:
   - Attached to one `DriverInterface` instance
   - Runs on its own dedicated `QThread` (per pre-M4 decision 4.2 option B)
   - Manages 0..N `FrameSink` registrations
   - Routes `frameReceived` / `errorOccurred` / `stateChanged` from driver to all registered sinks
   - Applies backpressure observation on the ingress queue
   - Clean lifecycle teardown (thread join on destruction)

3. **Backpressure integration** using M2's `WatermarkTracker`:
   - One tracker per pipeline (ingress queue)
   - Thresholds configurable (defaults 80% high, 60% recover from M2)
   - Logs via `SF_LOG_WARN` on threshold crossing
   - Updates `MetricsRegistry` gauge `pipeline_ingress_watermark_<driver-id>`

4. **Pipeline manager** at `src/pipeline/pipeline_manager.{hpp,cpp}`:
   - Owns pipelines; provides `attach(DriverInterface*)` / `detach(driver-id)` API
   - Exposes all pipelines to sink registrars (e.g., M5 Decoder adds itself as sink)
   - Thread-safe registration from any thread

5. **Integration with M3 Connection Manager**:
   - When Connection Manager's `Connect` creates a driver, it also asks `PipelineManager` to attach a pipeline
   - On `Disconnect`, detach pipeline (which stops its thread and releases resources)
   - No new UI widgets; this is internal wiring only

6. **Unit tests** with coverage ≥ 85% on pipeline modules:
   - Sink registration / deregistration
   - Frame fanout to multiple sinks
   - Pipeline lifecycle (attach → running → detach)
   - Backpressure threshold firing and recovery
   - Error/state forwarding to sinks

7. **Integration tests** at `tests/integration/`:
   - `test_pipeline_driver_integration.cpp` — attach pipeline to ReplayDriver skeleton (no frames; lifecycle only), attach to UdpDriver (real frames via localhost loopback)
   - `test_pipeline_backpressure.cpp` — slow sink causes watermark crossing
   - `test_pipeline_fanout.cpp` — multiple sinks receive same frame

8. **Benchmarks** at `tests/benchmark/`:
   - `bench_pipeline_throughput.cpp` — compares driver-only throughput (M3 baseline) vs driver+pipeline throughput
   - Target: pipeline overhead ≤ 10% of M3 driver baseline
   - Results appended to `tests/benchmark/results/M4-baseline.md`

9. **Doxygen** on all public declarations

10. **`.claude/M4-done.md`** with standard completion report + freeze record (sha256sum of `frame_sink.hpp` and `frame_pipeline.hpp`)

### 2.2 Must not do

1. **No decoding**. `RawFrame` is forwarded byte-for-byte to sinks. Parsing into typed signals is M5.
2. **No modifications to M2 or M3 frozen files**. `DriverInterface`, `RawFrame`, `WatermarkTracker`, concrete drivers (`SerialDriver`, `TcpDriver`, `UdpDriver`, `ReplayDriver`), `IoWorkerBase` — all read-only. If modification seems needed, HALT.
3. **No new top-level dependencies**. Uses M2 utilities (MPSC queue, WatermarkTracker) + Qt.
4. **No signal buffer** (M6 territory).
5. **No expression evaluation** (M7).
6. **No UI changes beyond minimal Connection Manager wiring**. No new menus, no new widgets. The existing ConnectionManager dialog gains internal pipeline-attach logic but its UI is unchanged.
7. **No cross-pipeline fanout** (a sink registers with one specific pipeline; M5 Decoder will register one decoder per pipeline, not a global decoder).
8. **No sink-side threading policy enforcement**. Each `FrameSink::onFrame` is called on the pipeline's thread. If a sink wants to offload to another thread, that's the sink's responsibility and out of pipeline's scope.

---

## 3. Design Decisions (locked by this spec)

These resulted from pre-M4 planning. Written here as implementation-level contracts.

### 3.1 `FrameSink` is a C++ abstract class, not Qt signals

**Decision**: `FrameSink` has pure virtual `onFrame(const RawFrame&)` as its primary consumer interface. Not `QObject`, not Qt signals.

**Rationale**: Downstream consumers (M5 Decoder, M10 Session Writer) are pure C++ classes with no Qt inheritance needs. Using Qt signals would force them to inherit QObject and introduce metatype/queue overhead for zero benefit (they run on the pipeline's thread anyway). Sinks that want to bridge to Qt signals implement it themselves.

**Cost accepted**: FrameSink consumers that are Qt widgets (hypothetical future) wrap with a QObject adapter. No widget sinks in V1.

### 3.2 Pipeline runs on its own dedicated QThread

**Decision**: Each `FramePipeline` owns a `QThread` on which all sink callbacks execute. Driver → pipeline crossing uses `Qt::QueuedConnection`.

**Rationale**: Per decision 4.2 option B. Isolates slow sinks (e.g., future session writer flushing to disk) from the driver's IO thread. Costs one thread per pipeline (10 drivers = 10 pipeline threads, acceptable on 16-thread dev host).

**Implementation note**: Uses the same `IoWorkerBase` pattern as M3 drivers. Pipeline internally has a `PipelineWorker` moved to a dedicated thread. Public `FramePipeline` lives on the calling thread.

### 3.3 Backpressure only at pipeline ingress

**Decision**: One `WatermarkTracker` per pipeline, observing the ingress queue depth. No per-sink watermark in M4.

**Rationale**: Per decision 4.3 option a. Keeps surface small. If future milestones need sink-specific watermark, each sink can own its own tracker internally.

**Behavior**:
- On threshold crossing (default 80%): emit `SF_LOG_WARN` + update `pipeline_ingress_watermark_<driver-id>` metric
- On recovery (default 60%): emit `SF_LOG_INFO` + update metric
- Does NOT drop frames. Dropping is the driver's responsibility when the pipeline's queue is full (driver's `write()` path already has BackpressureDrop handling per M2).

**Ingress queue full**: if a frame arrives at the pipeline and the internal queue is at 100% capacity, the frame is dropped, logged at WARN level, and counted in `pipeline_frames_dropped_<driver-id>` metric. This should never happen in normal operation (WARN at 80% should have triggered downstream remediation), but we handle it gracefully.

### 3.4 Sinks are registered once per pipeline, not globally

**Decision**: A sink registers with a specific `FramePipeline` instance, not with a global registry. M5 Decoder creates one decoder per driver and registers it with that driver's pipeline.

**Rationale**: Per-driver pipeline isolation (decision 4.2/4.3 of earlier M3 planning). Prevents a crashing decoder for one driver from affecting others. Adds minor registration boilerplate — acceptable.

### 3.5 Pipeline lifetime is owned by PipelineManager, not caller

**Decision**: Callers (Connection Manager) do not own `FramePipeline` instances. They call `PipelineManager::attach(driver)` which returns a handle. Detach releases the pipeline.

**Rationale**: Centralizes pipeline lifecycle management. Prevents caller from accidentally destroying a pipeline while it has active sinks. Simplifies testing (manager provides a single place to enumerate active pipelines).

**Handle design**: Handle is an opaque weak reference (not a unique_ptr). Multiple callers can hold the same handle; pipeline is destroyed when `PipelineManager::detach(driver-id)` is called (not on handle destruction).

### 3.6 Frame copying / sharing semantics

**Decision**: `FrameSink::onFrame` receives `const RawFrame&`. The RawFrame payload (`QByteArray`) uses implicit sharing; sinks that need to store the frame for later processing copy the reference cheaply. Sinks that need to mutate must deep-copy via `QByteArray::detach()` or equivalent.

**Rationale**: Preserves M2's zero-copy intent for RawFrame while allowing sinks to extend lifetime if needed.

**Thread safety**: Since all sinks for a pipeline run on the pipeline's thread sequentially, sinks don't need mutexes for their own processing. Cross-pipeline data sharing (e.g., a sink that aggregates across drivers) is the sink implementer's responsibility.

### 3.7 No soft-HALT (inherits from M2/M3 stance)

Interface freeze is binary. Implementation correctness is binary. Any ambiguity → HALT and ask.

---

## 4. Key Implementation Details

### 4.1 `FrameSink` declaration

Place at `src/pipeline/frame_sink.hpp`.

```cpp
// src/pipeline/frame_sink.hpp
#pragma once

#include "drivers/driver_interface.hpp"
#include "frame/raw_frame.hpp"

namespace signalforge::pipeline {

/// Downstream consumer of driver frames.
///
/// A sink is registered with a `FramePipeline` and receives every frame,
/// error, and state change from the pipeline's attached driver.
///
/// Thread affinity:
/// - All three callbacks (`onFrame`, `onError`, `onLifecycle`) are invoked
///   on the pipeline's dedicated thread. Sinks do not need internal locking
///   for data accessed only within these callbacks.
/// - If a sink wants to propagate data to another thread (e.g., a UI
///   thread), the sink implements that dispatch itself (Qt signals with
///   QueuedConnection, or post to a queue).
///
/// Lifetime:
/// - Sinks must outlive the pipeline they are registered with.
/// - Sinks may deregister via `FramePipeline::removeSink(this)` before
///   destruction. If not explicitly removed, the pipeline's destructor
///   releases its sink references without calling them.
///
/// Error handling:
/// - Exceptions thrown from `onFrame` / `onError` / `onLifecycle` are
///   caught by the pipeline, logged at ERROR level, and the pipeline
///   continues with the next sink. A faulty sink does not crash the
///   pipeline.
///
/// Freeze scope: this class is frozen at M4 close. Modifications post-freeze
/// require a new ADR.
class FrameSink {
public:
    virtual ~FrameSink() = default;

    /// Called for every frame received from the attached driver.
    /// `frame` is valid only for the duration of this call; if the sink
    /// needs to retain the frame, it must copy `frame.payload` (implicitly
    /// shared, O(1)) or the fields it needs.
    virtual void onFrame(const signalforge::frame::RawFrame& frame) = 0;

    /// Called when the driver reports an error. Default: no-op.
    /// Override if the sink needs to react (e.g., record error in session,
    /// suspend processing).
    virtual void onError(const signalforge::drivers::DriverError& error) {
        (void)error;
    }

    /// Called when the driver's state changes. Default: no-op.
    /// Override if the sink needs lifecycle awareness (e.g., flush on
    /// Stopping, reset state on Open).
    virtual void onLifecycle(signalforge::drivers::DriverState newState) {
        (void)newState;
    }

    /// Human-readable sink name, used in logs and metrics.
    /// Should be stable across invocations (pipeline may log it
    /// repeatedly).
    [[nodiscard]] virtual QString sinkName() const = 0;
};

}  // namespace signalforge::pipeline
```

### 4.2 `FramePipeline` declaration

Place at `src/pipeline/frame_pipeline.hpp`.

```cpp
// src/pipeline/frame_pipeline.hpp
#pragma once

#include "drivers/driver_interface.hpp"
#include "frame/raw_frame.hpp"
#include "pipeline/frame_sink.hpp"

#include <QObject>
#include <QString>
#include <QThread>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace signalforge::pipeline {

class PipelineWorker;  // forward-declared; defined in .cpp

/// Configuration for a FramePipeline.
struct PipelineConfig {
    /// Human-readable driver identifier, used in logs and metric names.
    /// Expected format: `<driver-type>-<unique-suffix>`, e.g. "serial-ttyUSB0"
    /// or "tcp-localhost-9000". Must be non-empty and unique per
    /// PipelineManager.
    QString driverId;

    /// Ingress queue capacity. Frames arriving when the queue is at
    /// capacity are dropped (and counted in pipeline_frames_dropped metric).
    /// Default is sized for ~1 second of backlog at 10kHz frame rate.
    std::uint32_t ingressCapacity = 10000;

    /// Watermark thresholds for backpressure observation.
    /// `highPct`: depth above this triggers "QueueFilling" signal.
    /// `recoverPct`: depth below this (after being above highPct) triggers
    /// "QueueRecovered" signal.
    std::uint32_t watermarkHighPct = 80;
    std::uint32_t watermarkRecoverPct = 60;
};

/// Routing layer between a driver and downstream sinks.
///
/// One FramePipeline per driver instance. Manages:
/// - A dedicated QThread for sink callback execution
/// - An ingress MPSC queue (driver → pipeline)
/// - A vector of registered FrameSinks (fanout on pipeline thread)
/// - Backpressure observation on the ingress queue
/// - Driver signal wiring (frameReceived / errorOccurred / stateChanged)
///
/// Thread affinity:
/// - Public API (addSink, removeSink, sinkCount) is safe to call from any
///   thread, internally locked.
/// - Sinks' callbacks execute on the pipeline's dedicated thread.
///
/// Lifetime:
/// - Pipeline is owned by PipelineManager, not directly by callers.
/// - Caller receives a FramePipeline* (non-owning) from PipelineManager::attach
///   for sink registration. Do not store this pointer beyond the scope of
///   the driver's active connection.
///
/// Freeze scope: the public class interface (methods below) is frozen at M4
/// close. Internal implementation (PipelineWorker, queue, etc.) may evolve.
class FramePipeline : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(FramePipeline)

public:
    /// Construct a pipeline for a driver. Pipeline is created in a "ready"
    /// state but not yet connected to the driver; call `attachDriver()` to
    /// wire up and start processing.
    explicit FramePipeline(PipelineConfig config, QObject* parent = nullptr);
    ~FramePipeline() override;

    /// Connect pipeline to driver. Establishes signal/slot connections
    /// between driver's `frameReceived` / `errorOccurred` / `stateChanged`
    /// and the pipeline worker. Pipeline begins processing frames
    /// immediately upon driver emission.
    ///
    /// Precondition: pipeline not already attached.
    /// Thread: any thread (QueuedConnection used internally).
    void attachDriver(signalforge::drivers::DriverInterface* driver);

    /// Register a sink for frame fanout. Sink lifetime must outlast the
    /// pipeline's (or be removed via `removeSink` before destruction).
    /// Idempotent: registering the same sink twice logs a warning and
    /// has no additional effect.
    /// Thread: any thread (internally locked).
    void addSink(std::shared_ptr<FrameSink> sink);

    /// Deregister a sink. No-op if not registered. Any in-flight callback
    /// already dispatched on the pipeline thread completes; no new
    /// callbacks will be made to this sink after removeSink returns.
    /// Thread: any thread (internally locked).
    void removeSink(std::shared_ptr<FrameSink> sink);

    /// Number of registered sinks. Safe from any thread.
    [[nodiscard]] std::size_t sinkCount() const;

    /// Current backpressure watermark peak (percentage). Monotonic since
    /// construction or last `resetBackpressureStats()`. Safe from any thread.
    [[nodiscard]] std::uint32_t peakWatermarkPct() const;

    /// Current pipeline statistics snapshot. Safe from any thread.
    struct Stats {
        std::uint64_t framesReceived = 0;
        std::uint64_t framesDropped = 0;  ///< Dropped at ingress queue full
        std::uint64_t errorsForwarded = 0;
        std::uint32_t ingressDepthCurrent = 0;
        std::uint32_t ingressDepthPeak = 0;
        std::chrono::steady_clock::time_point snapshotAt{};
    };
    [[nodiscard]] Stats stats() const;

    /// Reset backpressure peak and drop counters. Does not affect
    /// framesReceived counter. Thread-safe; caller should quiesce sink
    /// registration if called concurrently with it.
    void resetBackpressureStats();

    /// Driver identifier from config. Stable across pipeline lifetime.
    [[nodiscard]] const QString& driverId() const noexcept;

private:
    PipelineConfig config_;
    std::unique_ptr<QThread> thread_;
    std::unique_ptr<PipelineWorker> worker_;

    // Sink registry. Guarded by sinkMutex_.
    mutable std::mutex sinkMutex_;
    std::vector<std::shared_ptr<FrameSink>> sinks_;

    // Driver connection state
    signalforge::drivers::DriverInterface* driver_ = nullptr;
};

}  // namespace signalforge::pipeline
```

### 4.3 `PipelineManager` declaration

Place at `src/pipeline/pipeline_manager.hpp`.

```cpp
// src/pipeline/pipeline_manager.hpp
#pragma once

#include "drivers/driver_interface.hpp"
#include "pipeline/frame_pipeline.hpp"

#include <QObject>
#include <QString>
#include <memory>
#include <unordered_map>
#include <mutex>

namespace signalforge::pipeline {

/// Central registry for FramePipeline instances.
///
/// Callers (typically the Connection Manager) attach drivers here to get
/// a FramePipeline managing their frame flow. Sink registrars (M5 Decoder,
/// future components) look up pipelines by driver ID to register sinks.
///
/// Thread safety: all methods safe from any thread.
///
/// Lifetime:
/// - PipelineManager owns all FramePipeline instances.
/// - Detaching a driver destroys its pipeline (joining its thread, releasing
///   its sinks).
/// - Manager destruction detaches all pipelines.
class PipelineManager : public QObject {
    Q_OBJECT

public:
    explicit PipelineManager(QObject* parent = nullptr);
    ~PipelineManager() override;

    /// Attach a driver to the manager, creating a pipeline. Returns a
    /// non-owning pointer to the pipeline for sink registration.
    ///
    /// `driverId` must be non-empty and unique across currently-attached
    /// pipelines. If a pipeline with the same ID already exists,
    /// returns nullptr and logs ERROR.
    ///
    /// Pipeline is created with the provided config, attached to the driver,
    /// and ready to receive frames.
    [[nodiscard]] FramePipeline* attach(
        signalforge::drivers::DriverInterface* driver,
        PipelineConfig config);

    /// Detach the pipeline for `driverId`. Stops and destroys the pipeline.
    /// Sinks registered with that pipeline are released; their destructors
    /// run on the caller's thread unless the sinks have their own lifetime
    /// management.
    ///
    /// Safe to call even if `driverId` is not attached (no-op).
    void detach(const QString& driverId);

    /// Look up a pipeline by driver ID. Returns nullptr if not attached.
    /// Returned pointer is valid until `detach` is called for this ID.
    [[nodiscard]] FramePipeline* pipelineFor(const QString& driverId) const;

    /// Current number of attached pipelines.
    [[nodiscard]] std::size_t pipelineCount() const;

    /// Enumerate active driver IDs. Useful for sink registrars that want
    /// to attach to all current pipelines.
    [[nodiscard]] QStringList driverIds() const;

signals:
    /// Emitted when a pipeline is attached. Sink registrars connect to this
    /// to auto-register sinks on newly-attached pipelines.
    void pipelineAttached(const QString& driverId, FramePipeline* pipeline);

    /// Emitted when a pipeline is detached (after sink release).
    void pipelineDetached(const QString& driverId);

private:
    mutable std::mutex mutex_;
    std::unordered_map<QString, std::unique_ptr<FramePipeline>> pipelines_;
};

}  // namespace signalforge::pipeline
```

### 4.4 Pipeline worker pattern

Place `PipelineWorker` definition in `src/pipeline/frame_pipeline.cpp` (internal, not frozen).

The worker:
- Lives on the pipeline's dedicated QThread
- Receives frames via a connected slot from the driver's `frameReceived` signal (Qt::QueuedConnection)
- Pushes frames into an internal MPSC queue (from M2 utils)
- Has a slot that processes the queue, iterating and calling each sink's `onFrame`
- Observes watermark on each push
- Catches exceptions from sinks (log + continue)

**Processing model**: worker processes one frame at a time, calling all sinks sequentially before moving to the next frame. This preserves frame ordering across sinks. Sinks that want parallelism do it themselves (offload to another thread).

**Error/state forwarding**: when the driver emits `errorOccurred` or `stateChanged`, the worker immediately iterates sinks and calls `onError` / `onLifecycle` (not queued through the frame MPSC — these are rare events).

### 4.5 Connection Manager integration

Modify `src/app/connection_manager.cpp` to:

1. Hold a `PipelineManager*` (from the main window or globally accessible; implementer's judgment)
2. On Connect (after driver successfully reaches Open state):
   - Call `pipelineManager_->attach(driver.get(), PipelineConfig{.driverId=makeDriverId(driver)})`
   - Store the returned `FramePipeline*` alongside the driver
3. On Disconnect (after driver reaches Idle):
   - Call `pipelineManager_->detach(driverId)`
   - Reset stored pipeline pointer to nullptr

The UI does not display pipeline state in M4 (no widget changes). Internally the pipeline is running; sinks for decoding (M5) will register themselves.

**Driver ID generation**: implementer's judgment. Suggested format: `<driver-type>-<disambiguating-suffix>` where suffix is device path for Serial, `host:port` for TCP/UDP, filename for Replay. Must be unique across concurrent connections. Since M3's Connection Manager is single-connection, the uniqueness constraint is automatically satisfied in M4, but the format must be prepared for multi-connection (M9).

### 4.6 Metrics naming convention

New metrics added to `MetricsRegistry`:

- `pipeline_frames_received_<driverId>` (counter)
- `pipeline_frames_dropped_<driverId>` (counter)
- `pipeline_ingress_watermark_<driverId>` (gauge, 0-100 integer percent)
- `pipeline_ingress_depth_peak_<driverId>` (gauge, samples)
- `pipeline_errors_forwarded_<driverId>` (counter)

These are registered at pipeline construction and unregistered at pipeline destruction. Metric names with special characters in `driverId` (e.g., slashes in paths) are sanitized per existing MetricsRegistry rules (implementer verifies; if the registry doesn't sanitize, HALT and ask).

### 4.7 Thread naming

Each pipeline thread is named `PipelineWorker-<driverId>` (e.g., `PipelineWorker-serial-ttyUSB0`) via `QThread::setObjectName()` + `platform::setCurrentThreadName()` on thread start.

---

## 5. Test strategy

### 5.1 Coverage ≥ 85% on pipeline modules

- `frame_pipeline.cpp`: ≥ 85%
- `pipeline_manager.cpp`: ≥ 85%
- `frame_sink.hpp`: interface header, not a coverage unit

### 5.2 Unit tests

**For `FramePipeline`** (tests use a `MockDriver` from M2 or a lightweight test fixture):

- Construction: valid config → ready state; invalid config (empty driverId) → log ERROR, construction continues (or throws per implementer choice — document)
- Sink registration: addSink + removeSink + idempotent addSink (same sink twice)
- Sink counting: empty pipeline has sinkCount=0, N registrations → N
- Fanout: with 3 registered sinks, a frame emission results in 3 onFrame calls, same frame
- Error forwarding: errorOccurred signal → all sinks' onError called
- State forwarding: stateChanged signal → all sinks' onLifecycle called
- Sink exception: a sink that throws from onFrame does not prevent other sinks from receiving the frame (exception logged at ERROR)
- Stats: framesReceived increments on each frame; framesDropped increments when ingress queue full
- Backpressure: watermark events fire when queue depth crosses thresholds

**For `PipelineManager`**:

- Attach: first attach returns non-null pipeline; pipelineCount becomes 1
- Attach duplicate driverId: returns nullptr; logs ERROR; pipelineCount unchanged
- Detach: pipelineCount decrements; pipelineFor returns nullptr for that ID
- Detach nonexistent: no-op
- Lookup: pipelineFor returns correct pipeline pointer
- DriverIds: enumerates all attached driver IDs (order not guaranteed)
- Signal emission: pipelineAttached / pipelineDetached fire at correct times

**For `FrameSink`** (interface tests):

- A concrete TestSink implementing all three overrides (onFrame, onError, onLifecycle)
- Verifying default implementations of onError and onLifecycle are no-ops (don't throw)

### 5.3 Integration tests

**`test_pipeline_driver_integration.cpp`**:

Scenario 1: ReplayDriver skeleton + pipeline lifecycle
1. Create PipelineManager
2. Create ReplayDriver with valid config
3. Attach driver via manager
4. Open + start driver → sink receives onLifecycle(Opening→Open→Running) via pipeline
5. No frames received (ReplayDriver skeleton)
6. Stop + close driver → sink receives onLifecycle(Stopping→Open→Closing→Idle)
7. Detach from manager → sink references released
8. Verify pipeline thread joined cleanly

Scenario 2: UdpDriver + pipeline with real frames
1. Create two UdpDrivers on localhost, configured to send to each other
2. Attach both via manager with distinct driverIds
3. Register a counting sink with driver A's pipeline
4. Driver B writes 100 datagrams
5. Sink receives 100 onFrame calls
6. Verify frame payloads match sent data
7. Detach both; verify sink is not called afterward

**`test_pipeline_backpressure.cpp`**:

1. Create a slow sink (onFrame sleeps 10ms)
2. Attach pipeline to fast driver (UDP flood)
3. Producer sends 1000 frames quickly
4. Verify watermark crosses 80%; SF_LOG_WARN observed
5. Producer pauses; watermark recovers to below 60%; SF_LOG_INFO observed
6. Final: pipeline.stats() reflects frames received + possibly dropped

**`test_pipeline_fanout.cpp`**:

1. Attach pipeline to UdpDriver
2. Register 3 distinct sinks
3. Drive 50 frames
4. Each sink receives 50 frames
5. Frame content identical across sinks

### 5.4 Benchmarks

**`bench_pipeline_throughput.cpp`**:

Measure throughput regression introduced by pipeline:
- Test A: UdpDriver direct (no pipeline) → frameReceived signal → counter sink
- Test B: UdpDriver → Pipeline → counter sink registered

Compare:
- Test A throughput (bytes/sec, datagrams/sec)
- Test B throughput
- Overhead percentage

**Threshold**: Pipeline overhead ≤ 10% of direct driver throughput. Miss → HALT per M4 §7.

Results appended to `tests/benchmark/results/M4-baseline.md` with comparison table.

### 5.5 Test for Connection Manager integration

Light integration test in `test_connection_manager.cpp` (extends M3's):
- Open Connection Manager dialog
- Connect via ReplayDriver
- Verify pipelineCount == 1 in global manager
- Disconnect
- Verify pipelineCount == 0
- Reconnect different driver type (TCP)
- Verify pipelineCount == 1 again with new driverId

---

## 6. Freeze protocol

### 6.1 What freezes at M4 close

Modifications after merge require new ADR:

- `FrameSink` class declaration (pure virtual method signatures, default-implementation signatures, sinkName)
- `FramePipeline` class declaration (constructor, public methods, `Stats` struct layout, `PipelineConfig` struct layout)
- `PipelineManager` class declaration (public methods, signals)
- Metrics naming convention (§4.6) — adding new metrics is additive (not a freeze violation); renaming existing ones is.

### 6.2 What does NOT freeze

- Internal implementation (`PipelineWorker`, private members, .cpp helpers)
- Thread naming convention (§4.7 — may evolve for readability)
- Default values in `PipelineConfig` (may be tuned based on M10/M12 observation)

### 6.3 Freeze record format in M4-done.md

```markdown
## Freezes established in this milestone

Frozen per M4 spec §6.1. Modifications post-merge require new ADR per §6.2 of execution manual's governance.

- `src/pipeline/frame_sink.hpp`: FrameSink class (3 virtuals: onFrame, onError, onLifecycle; sinkName accessor)
- `src/pipeline/frame_pipeline.hpp`: FramePipeline class, PipelineConfig struct, Stats struct
- `src/pipeline/pipeline_manager.hpp`: PipelineManager class, pipelineAttached/pipelineDetached signals

Sha256sum:
<output of:>
  find src/pipeline -name '*.hpp' -print0 | xargs -0 sha256sum
```

---

## 7. M4-specific HALT triggers

In addition to CLAUDE.md §HALT:

1. Any modification to M2- or M3-frozen `.hpp` files → HALT
2. `MetricsRegistry` does not sanitize driver IDs with special characters, and spec §4.6 requires sanitization → HALT (ask whether to implement sanitization here or add it to metrics registry per ADR)
3. Pipeline benchmark overhead > 10% of direct driver throughput → HALT
4. Sink exception isolation fails (a throwing sink crashes the pipeline) → HALT
5. Pipeline thread does not exit within 500ms on destruction → HALT
6. Connection Manager integration causes UI thread blocking > 200ms per M3 quality standard → HALT

---

## 8. Acceptance criteria

### 8.1 Build and test

- [ ] Debug, Release, debug-asan all build clean, zero warnings
- [ ] All unit tests pass under all three presets
- [ ] All integration tests pass under Debug + Release (debug-asan for integration tests is nice-to-have, per M2/M3 precedent)
- [ ] Coverage ≥ 85% on pipeline modules (per §5.1)

### 8.2 Benchmarks

- [ ] Pipeline throughput benchmark completes
- [ ] Overhead ≤ 10% of M3 baseline for UDP localhost (primary test driver)
- [ ] Results committed to `tests/benchmark/results/M4-baseline.md`

### 8.3 Integration

- [ ] Connection Manager's Connect/Disconnect successfully attaches/detaches pipelines
- [ ] PipelineManager exposes attached pipelines for future sink registrars (M5)
- [ ] Signal `pipelineAttached` fires for sink registration hook

### 8.4 Freeze record

- [ ] `.claude/M4-done.md` has Freezes section per §6.3
- [ ] Sha256sums of frozen headers recorded
- [ ] No modifications to M2 or M3 frozen files (verify via `git diff` against merge base + grep of modified file list)

### 8.5 Hand-off to M5

- [ ] `.claude/M4-done.md` hand-off section explicitly states:
  - What M5 can start immediately (DecoderInterface implementing FrameSink)
  - How M5 registers decoders with pipelines via PipelineManager::pipelineAttached signal
  - Known rough edges (e.g., if ingress queue drops happen in benchmarks, note conditions)

---

## 9. Notes for CC

- **Freeze discipline is the priority.** M4 defines three interfaces (FrameSink, FramePipeline, PipelineManager) that M5-M11 depend on. Doxygen must be clear about thread affinity, lifetime, and error propagation. Ambiguity in freeze scope = HALT.

- **This is a routing layer, not a processing layer.** Do not add "smart" features (frame aggregation, filtering, transformation). Those are sink responsibilities. If tempted to add a "SmartPipeline" feature, HALT and ask.

- **Per-pipeline thread costs one OS thread per driver.** At V1 scale (≤10 drivers) this is fine. If implementation finds the thread cost materially impacts something, HALT — don't "optimize" by pooling threads; that's architectural.

- **Connection Manager wiring is minimal.** Do not add pipeline-related UI widgets. Connection state display is already in M3. Adding anything here is scope creep.

- **Metric naming includes driverId** (e.g., `pipeline_ingress_watermark_serial-ttyUSB0`). If MetricsRegistry has rules against certain characters, sanitize or HALT.

- **Sink exception isolation is a safety property, not a nice-to-have.** A decoder in M5 might have bugs; the pipeline must not crash. Test this explicitly.

---

## 10. Closing note

M4 is a connective tissue milestone. Its value is making M5 simpler, not adding user-visible features. The `FrameSink` interface is small — three virtual methods plus a name accessor — but it's the boundary all future frame consumers cross. Freeze it precisely.

When in doubt, prefer less code over more. Pipeline complexity grows in every subsequent milestone (M10 session writer, M12 optimizations). M4 should be as thin as the contract demands.
