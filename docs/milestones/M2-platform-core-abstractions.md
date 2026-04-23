# M2 — Platform + Core Abstractions

| Field | Value |
|---|---|
| Milestone ID | M2 |
| Sprint | 2 |
| Estimated effort | 5–7 person-days |
| Prerequisites | M1 closed (main at v0.0.2-alpha.1) |
| Next milestone | M3 |
| Hard-stop type | **Interface freeze** |
| Soft-HALT allowed | **No** — all failures are hard HALT |
| Branch | `milestone/M2` |

**Cross-reference notation**:

- `[EM §N]` — Execution Manual, section N
- `[Arch §N]` — Architecture document, section N
- `[MR M<n>]` — Milestone Roadmap, entry for milestone `M<n>`
- `[CM §X]` — CLAUDE.md, section X
- `[ADR-N]` — Architecture Decision Record N

---

## 1. Goal

Ship the foundation layer on which M3–M11 will build. Every public interface defined in M2 is subject to **freeze** at milestone close: subsequent milestones may add new interfaces alongside, but cannot modify signatures or semantics of M2's deliverables.

This is the most expensive-to-change milestone in V1. Precision at definition time is more important than speed.

---

## 2. Scope

### 2.1 Must deliver

1. **Platform layer** under `src/platform/`:
   - `time_source.{hpp,cpp}` — monotonic + wall-clock utilities
   - `thread_utils.{hpp,cpp}` — thread naming, optional CPU affinity hint
   - `crash_reporting.{hpp,cpp}` — Crashpad integration
   - `app_paths.{hpp,cpp}` — XDG-aware directory resolution for logs, crashdumps, config

2. **Driver interface** at `src/drivers/driver_interface.hpp`:
   - Abstract base class defining the driver contract
   - Associated value types: `DriverState`, `DriverError`, `DriverConfig` (tag-dispatched variant or base class)
   - Signals for frame delivery, error, state change
   - `RxStats` / `TxStats` / `DriverStatistics` struct families

3. **Frame layer** under `src/frame/`:
   - `raw_frame.{hpp,cpp}` — `RawFrame` value type
   - `frame_envelope.{hpp,cpp}` — optional metadata wrapper
   - `backpressure.{hpp,cpp}` — `BackpressureSignal` and queue-watermark utilities

4. **Utilities** under `src/utils/`:
   - `spsc_ring.{hpp,cpp}` — single-producer / single-consumer ring buffer
   - `mpsc_queue.{hpp,cpp}` — wrapper around `moodycamel::ConcurrentQueue`
   - `snapshot.{hpp,cpp}` — double-buffered snapshot utility for UI consumption

5. **Observability extension** to `src/observability/`:
   - Extend `logging.cpp`'s async sink with structured fields support
   - `metrics.{hpp,cpp}` — in-process counter / gauge registry feeding the performance panel (panel UI itself is M5+)

6. **Crash-trigger tool** at `tools/crash_test/`:
   - Standalone executable that deliberately crashes in several ways (null deref, abort, unhandled exception)
   - Produces minidumps into the standard crashdump directory
   - NOT integrated into unit tests (ASan would catch these before Crashpad does)
   - Documented README explaining manual verification procedure

7. **Unit tests** for every public declaration, with coverage target **≥ 85%** (raised from V1 default of 70% — this is foundation code)

8. **Doxygen documentation** on every public declaration, including thread-safety and ownership notes

9. **Freeze record** in `.claude/M2-done.md` enumerating every frozen interface

### 2.2 Must not do

1. **No concrete driver implementations.** Those belong to M3.
2. **No frame pipeline.** The frame-layer code defines `RawFrame` and backpressure primitives; wiring drivers to decoders is M4.
3. **No UI code.** Performance panel is M5; crash-trigger tool is a CLI.
4. **No migration from existing M0/M1 code except where spec explicitly requires modification.** Specifically, do not restructure `src/observability/logging.cpp` beyond what patch 5 of this spec asks.
5. **No new top-level dependencies.** If you think you need one, HALT.

---

## 3. Design Decisions (frozen by this spec)

These are the decisions the human made in pre-M2 planning. They are written here, not in the architecture document, because they are implementation-level contracts rather than architectural choices. Once M2 ships, they propagate into ADRs if M3+ milestones need to reference them in strategy discussions.

### 3.1 Driver interface uses Qt signals, not std::function callbacks

**Decision**: `DriverInterface` is a `QObject` subclass. Data flows outward via Qt signals with `Qt::QueuedConnection` for cross-thread delivery.

**Rationale**: Project is Qt-native end to end. Mixing `std::function` callbacks would require parallel cross-thread mechanics and violate `[Arch §5.5]`.

**Cost accepted**: `RawFrame` must be registered as a Qt metatype. Enum-like value types (`DriverState`, `DriverError`) must also be `Q_ENUM`-registered or metatype-registered.

### 3.2 `RawFrame` payload is `QByteArray` (implicit shared)

**Decision**: `RawFrame::payload` is a `QByteArray`. Implicit sharing (copy-on-write) provides zero-copy semantics for cross-thread signal delivery.

**Rationale**: Several thousand `RawFrame` per second will cross thread boundaries. `std::vector<std::byte>` would copy; `QByteArray` shares.

**Cost accepted**: `QByteArray` uses `char` internally, not `std::byte`. Documentation clearly states byte-order and signedness conventions at interface boundaries.

### 3.3 Backpressure mechanism is producer-pull, not broadcast

**Decision**: Each bounded queue exposes a watermark query. Producers (IO threads) poll the watermark before `push()` and adjust behavior per `[Arch §5.3]` policy. No global `BackpressureBroker` in V1.

**Observability**: Watermark events are logged via `SF_LOG_WARN` when crossing 80%, and the `metrics` registry tracks watermark peaks per queue.

**Rationale**: Simpler, lower indirection. A broadcast `BackpressureBroker` can be added in V1.5 if real-world workloads show cross-layer coordination needs.

### 3.4 Crashpad integration is complete but crash-test tool is standalone

**Decision**: M2 completes the full Crashpad init/shutdown lifecycle. A separate `tools/crash_test/` executable provides the deliberate-crash paths for manual verification. No deliberate-crash tests in `tests/unit/`.

**Rationale**: ASan in the `debug-asan` preset would catch deliberate crashes as violations, breaking the test. Crashpad's minidump path is complementary to ASan, not subject to it, so the verification lives outside the unit-test harness.

### 3.5 Statistics structs carry reserved fields from day one

**Decision**: `RxStats`, `TxStats`, `DriverStatistics` fields are populated based on `[Arch §14.2]` performance-panel requirements. Fields not yet read are marked `// reserved for M<N>` in code comments but are present in struct definition.

**Rationale**: Adding fields post-freeze is allowed but costly (touches every driver's stats emission). Defining once avoids drift.

### 3.6 Decode worker pool default is 4 threads

**Decision**: `DecodeWorkerPool::defaultThreadCount()` returns 4. Callers may override via constructor argument.

**Rationale**: Developer host is 8-core / 16-thread AMD Cezanne. 4 workers leaves cores for IO, UI, system. Scales to most dev and target hardware. Production tuning is M10's territory.

### 3.7 Hard HALT only — no soft-HALT / partial in M2

**Decision**: M2 does not adopt the soft-HALT mechanism introduced in M1. All failures are hard HALT. Rationale: interface-freeze milestones cannot have partial freezes — an interface is either defined and committed to, or it is not.

---

## 4. Key Implementation Details

### 4.1 `DriverInterface` declaration

Place at `src/drivers/driver_interface.hpp`. This is the most important file in M2 — every concrete driver in V1 and V1.5 will inherit from it.

Requirements:

```cpp
// src/drivers/driver_interface.hpp
#pragma once

#include <QByteArray>
#include <QObject>
#include <chrono>
#include <memory>

#include "frame/raw_frame.hpp"

namespace signalforge::drivers {

/// Lifecycle state of a driver. Monotonic where possible, but
/// Error → Idle transition is allowed after handling.
enum class DriverState : int {
    Idle = 0,       ///< Not yet configured or after close()
    Opening,        ///< open() in progress
    Open,           ///< Resource acquired, not yet reading
    Running,        ///< start() succeeded; data flowing
    Stopping,       ///< stop() in progress
    Closing,        ///< close() in progress
    Error,          ///< Terminal for the current lifecycle; recoverable via close() → open()
};

/// Error categories for driver failures. String detail lives in DriverError::message().
enum class DriverErrorCode : int {
    Success = 0,
    NotConfigured,
    ConfigInvalid,
    ResourceUnavailable,    ///< Port in use, device not found, etc.
    ResourceLost,           ///< Was open, became unavailable mid-run
    PermissionDenied,
    IoFailure,
    ProtocolFailure,
    BackpressureDrop,       ///< Data dropped due to downstream backpressure
    Timeout,
    Internal,               ///< Driver bug; always a defect
};

struct DriverError {
    DriverErrorCode code;
    QString message;
    std::chrono::steady_clock::time_point at;
};

/// Abstract driver base. One instance per logical device.
///
/// Thread affinity:
/// - Construction: any thread (typically main).
/// - open() / close() / start() / stop() / write(): must be called from the
///   thread that owns the driver (usually main or a dedicated coordinator).
/// - Signals are emitted from the driver's IO thread. Consumers connecting
///   slots on other threads MUST use Qt::QueuedConnection.
///
/// Ownership:
/// - Subclasses own all IO resources. open() acquires, close() releases.
/// - RawFrame is passed by value; QByteArray inside shares implicitly.
class DriverInterface : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(DriverInterface)

public:
    explicit DriverInterface(QObject* parent = nullptr);
    ~DriverInterface() override;

    /// Acquire the IO resource. Non-blocking; emits stateChanged() transitions.
    /// Returns Success immediately on accepted request; actual open may complete async.
    /// Precondition: state() == Idle.
    virtual DriverErrorCode open() = 0;

    /// Release the IO resource. Idempotent: no-op if already Idle.
    /// May block briefly for graceful shutdown (< 500ms recommended).
    virtual void close() = 0;

    /// Begin data flow. Frames emitted via frameReceived() until stop().
    /// Precondition: state() == Open.
    virtual DriverErrorCode start() = 0;

    /// Suspend data flow. Idempotent.
    /// Postcondition: no more frameReceived() after this returns, barring one
    /// in-flight frame already posted to the target thread.
    virtual void stop() = 0;

    /// Attempt to transmit payload. Returns accepted/rejected.
    /// Concrete drivers define whether this is synchronous (Serial) or queued (TCP).
    /// Precondition: state() == Running.
    virtual DriverErrorCode write(const QByteArray& payload) = 0;

    /// Current lifecycle state. Safe to call from any thread.
    [[nodiscard]] virtual DriverState state() const = 0;

    /// Health probe. Returns Success if driver is in a nominal state, else the
    /// primary error observed. Not a status snapshot — stats() gives that.
    [[nodiscard]] virtual DriverErrorCode health() const = 0;

    /// Snapshot of driver statistics. Safe to call from any thread.
    [[nodiscard]] virtual DriverStatistics statistics() const = 0;

signals:
    /// A frame was received. Emitted from the driver's IO thread.
    /// Consumers connect with Qt::QueuedConnection.
    void frameReceived(signalforge::frame::RawFrame frame);

    /// An error occurred. Emitted from the driver's IO thread.
    /// Consumers must decide whether to close() or continue.
    void errorOccurred(signalforge::drivers::DriverError error);

    /// Lifecycle state changed. Emitted whenever state() transitions.
    void stateChanged(signalforge::drivers::DriverState newState);
};

}  // namespace signalforge::drivers

Q_DECLARE_METATYPE(signalforge::drivers::DriverState)
Q_DECLARE_METATYPE(signalforge::drivers::DriverError)
```

`DriverStatistics` lives in `src/frame/` (next section).

### 4.2 `RawFrame` and statistics

Place at `src/frame/raw_frame.hpp`.

```cpp
// src/frame/raw_frame.hpp
#pragma once

#include <QByteArray>
#include <QString>
#include <chrono>
#include <cstdint>
#include <optional>

namespace signalforge::frame {

/// Nanosecond-resolution monotonic timestamp.
using SteadyTimestamp = std::chrono::steady_clock::time_point;

/// Wall-clock timestamp for display only. Never use for comparisons.
using WallTimestamp = std::chrono::system_clock::time_point;

/// Source identification. Strings are cheap because QByteArray shares.
using SourceId = QString;

/// A raw byte-level frame from a driver.
///
/// Value type. Copyable (QByteArray is implicit-shared, so copies are O(1)).
/// All timestamps are recorded by the producing driver as close to the IO
/// operation as possible.
struct RawFrame {
    SourceId sourceId;           ///< Stable ID of the originating driver
    QByteArray payload;          ///< Raw bytes; encoding is protocol-dependent
    SteadyTimestamp recvAt;      ///< When the bytes reached the IO thread
    std::optional<SteadyTimestamp> deviceAt;  ///< If the frame carries a device timestamp
    QString protocolHint;        ///< Optional; e.g., "serial", "tcp", "udp", "replay"

    // Reserved fields (populated M3+, present here to avoid post-freeze additions):
    std::uint64_t sequenceNumber = 0;  ///< Driver-local monotonic counter
    std::uint32_t flags = 0;            ///< Bit-reserved, see FrameFlags in future milestones
};

/// Receive-side statistics. All counters are monotonic increasing; rates are
/// derived by consumer.
struct RxStats {
    std::uint64_t framesTotal = 0;
    std::uint64_t bytesTotal = 0;
    std::uint64_t framesDropped = 0;
    std::uint64_t errors = 0;
    SteadyTimestamp lastFrameAt = {};
    std::uint32_t queueWatermarkPeak = 0;
    std::uint32_t queueWatermarkCurrent = 0;
};

/// Transmit-side statistics. Same conventions as RxStats.
struct TxStats {
    std::uint64_t framesTotal = 0;
    std::uint64_t bytesTotal = 0;
    std::uint64_t failures = 0;
    SteadyTimestamp lastFrameAt = {};
};

/// Combined snapshot for one driver. Returned by DriverInterface::statistics().
struct DriverStatistics {
    RxStats rx;
    TxStats tx;
    SteadyTimestamp snapshotAt = {};
};

}  // namespace signalforge::frame

Q_DECLARE_METATYPE(signalforge::frame::RawFrame)
```

### 4.3 `BackpressureSignal` and watermark utilities

Place at `src/frame/backpressure.hpp`.

```cpp
// src/frame/backpressure.hpp
#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

namespace signalforge::frame {

/// Reasons a backpressure event may fire.
enum class BackpressureReason : int {
    QueueFilling,       ///< >= 80% watermark
    QueueFull,          ///< 100%; drop decisions being made
    QueueRecovered,     ///< Back below 60% after being above 80%
};

/// Observed event. Produced when a queue crosses a watermark threshold.
/// Consumers: logger (always), metrics registry, UI performance panel.
struct BackpressureSignal {
    QString queueName;          ///< Stable identifier for the queue
    BackpressureReason reason;
    std::uint32_t watermarkPct;  ///< 0..100, rounded to integer percent
    std::uint64_t currentDepth;  ///< Current item count
    std::uint64_t capacity;      ///< Queue capacity
    std::chrono::steady_clock::time_point at;
};

/// Thread-safe watermark tracker. One per bounded queue.
/// Producer calls observe(depth) before each push; returns whether
/// a signal should be emitted (caller is responsible for emitting).
class WatermarkTracker {
public:
    explicit WatermarkTracker(std::uint64_t capacity,
                              std::uint32_t highPct = 80,
                              std::uint32_t recoverPct = 60);

    /// Update with current queue depth. Returns optional signal to emit.
    /// Thread-safe: callable from producer thread without external locking.
    [[nodiscard]] std::optional<BackpressureSignal> observe(
        std::uint64_t currentDepth,
        const QString& queueName);

    /// Current peak watermark observed since construction or last reset().
    [[nodiscard]] std::uint32_t peakPct() const noexcept;

    void reset() noexcept;

private:
    const std::uint64_t capacity_;
    const std::uint32_t highPct_;
    const std::uint32_t recoverPct_;
    std::atomic<std::uint32_t> peakPct_{0};
    std::atomic<bool> aboveHigh_{false};
};

}  // namespace signalforge::frame
```

### 4.4 `SpscRing`, `MpscQueue`, `Snapshot`

Place under `src/utils/`.

#### 4.4.1 `spsc_ring.hpp`

```cpp
// src/utils/spsc_ring.hpp
#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

namespace signalforge::utils {

/// Single-producer / single-consumer lock-free ring buffer.
///
/// Thread safety: exactly one thread may call push(); exactly one (possibly
/// different) thread may call pop(). Other operations (size, capacity, peakSize)
/// are safe to call from any thread but return approximate values.
///
/// The buffer is bounded; push() returns false when full. Dropping or
/// overwriting is the caller's decision, not the ring's.
template <typename T>
class SpscRing {
public:
    /// capacity must be > 0. Internally rounded up to next power of 2 for
    /// efficient masking.
    explicit SpscRing(std::size_t capacity);
    ~SpscRing() = default;

    SpscRing(const SpscRing&) = delete;
    SpscRing& operator=(const SpscRing&) = delete;
    SpscRing(SpscRing&&) = delete;
    SpscRing& operator=(SpscRing&&) = delete;

    /// Attempt to push. Returns false if full. Producer-only.
    [[nodiscard]] bool push(T item);

    /// Attempt to pop. Returns std::nullopt if empty. Consumer-only.
    [[nodiscard]] std::optional<T> pop();

    /// Approximate current size. Safe to call from any thread; value may lag.
    [[nodiscard]] std::size_t size() const noexcept;

    /// Capacity after power-of-2 rounding. Immutable.
    [[nodiscard]] std::size_t capacity() const noexcept;

    /// Peak size observed since construction. Monotonic.
    [[nodiscard]] std::size_t peakSize() const noexcept;

private:
    // Implementation detail: power-of-2 buffer, atomic head/tail, cache-line padded.
};

}  // namespace signalforge::utils
```

Implementation may use `boost::lockfree::spsc_queue` as a backing primitive if that simplifies code. If rolling your own, ensure cache-line padding on `head_` and `tail_` atomics.

#### 4.4.2 `mpsc_queue.hpp`

```cpp
// src/utils/mpsc_queue.hpp
#pragma once

#include <cstddef>
#include <memory>
#include <optional>

namespace signalforge::utils {

/// Multi-producer / single-consumer wrapper around moodycamel::ConcurrentQueue.
/// Thin adapter so callers don't depend directly on moodycamel API.
///
/// Thread safety: multiple producers may call push(); exactly one thread
/// may call pop().
template <typename T>
class MpscQueue {
public:
    explicit MpscQueue(std::size_t initialCapacity = 4096);
    ~MpscQueue();

    MpscQueue(const MpscQueue&) = delete;
    MpscQueue& operator=(const MpscQueue&) = delete;

    /// Push. Always succeeds (queue grows). Returns false only if out of memory.
    [[nodiscard]] bool push(T item);

    /// Pop. Returns std::nullopt if empty.
    [[nodiscard]] std::optional<T> pop();

    /// Approximate size. Safe from any thread.
    [[nodiscard]] std::size_t sizeApprox() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;  // pimpl so moodycamel headers don't leak
};

}  // namespace signalforge::utils
```

The `sizeApprox()` deliberately does not track a peak — M2 treats MPSC as unbounded in practice, with backpressure enforced at `SpscRing` upstream. Track peak via `WatermarkTracker` wired by the caller if needed.

#### 4.4.3 `snapshot.hpp`

```cpp
// src/utils/snapshot.hpp
#pragma once

#include <atomic>
#include <memory>

namespace signalforge::utils {

/// Double-buffered snapshot for single-writer / multi-reader fan-out.
///
/// Producer thread calls publish(T) to atomically swap in a new value.
/// Consumer threads call read() to get the most recent published T.
///
/// Reads are lock-free and never block the writer. Writes are lock-free
/// for the producer. The previous value's lifetime extends until the last
/// reader releases its snapshot.
template <typename T>
class Snapshot {
public:
    Snapshot() = default;
    ~Snapshot() = default;

    Snapshot(const Snapshot&) = delete;
    Snapshot& operator=(const Snapshot&) = delete;

    /// Publish a new value. Writer-only.
    void publish(T value);

    /// Read the current value. Safe from any thread.
    /// Returns a shared_ptr to ensure the value outlives the reader's use.
    [[nodiscard]] std::shared_ptr<const T> read() const;

private:
    std::shared_ptr<const T> current_;  // managed with atomic ops
    mutable std::atomic<std::shared_ptr<const T>*> currentPtr_{nullptr};
    // std::atomic<std::shared_ptr<T>> if C++20 and compiler support is sufficient;
    // otherwise custom implementation with atomic<T*> + ref counting.
};

}  // namespace signalforge::utils
```

Note: C++20's `std::atomic<std::shared_ptr<T>>` is the cleanest implementation. GCC 13 supports it. If any portability issue arises (unlikely on Ubuntu 24.04 + GCC 13), HALT — do not fall back to a custom implementation silently.

### 4.5 Platform-layer details

#### 4.5.1 `time_source.hpp`

```cpp
// src/platform/time_source.hpp
#pragma once

#include <chrono>

namespace signalforge::platform {

/// Monotonic time. Use for all internal timestamps.
[[nodiscard]] std::chrono::steady_clock::time_point monotonicNow() noexcept;

/// Wall-clock time. Use ONLY for display, file naming, and user-facing
/// formatting. Never for comparisons or ordering.
[[nodiscard]] std::chrono::system_clock::time_point wallClockNow() noexcept;

/// Pair recording monotonic-to-wall correspondence at a specific moment.
/// Used to embed in session file headers.
struct ClockOrigin {
    std::chrono::steady_clock::time_point monotonic;
    std::chrono::system_clock::time_point wall;
};

/// Capture both clocks as close together as possible. Call once at
/// session start.
[[nodiscard]] ClockOrigin captureOrigin() noexcept;

}  // namespace signalforge::platform
```

#### 4.5.2 `thread_utils.hpp`

```cpp
// src/platform/thread_utils.hpp
#pragma once

#include <QString>

namespace signalforge::platform {

/// Set the current thread's name, visible in debuggers / top / htop.
/// Max 15 characters on Linux; longer names are truncated with a warning log.
void setCurrentThreadName(const QString& name);

/// Suggest CPU affinity for the current thread. May be a no-op on some systems.
/// Advisory only — kernel may schedule elsewhere.
/// core: 0-based logical CPU index; if out of range, call is a no-op with log.
void suggestCpuAffinity(int core);

}  // namespace signalforge::platform
```

#### 4.5.3 `crash_reporting.hpp`

```cpp
// src/platform/crash_reporting.hpp
#pragma once

#include <QString>
#include <memory>

namespace signalforge::platform {

struct CrashReporterConfig {
    QString applicationName = "SignalForge";
    QString applicationVersion;   ///< From CMake-generated SIGNALFORGE_VERSION
    QString crashDumpDirectory;   ///< Must exist and be writable
    QString handlerExecutable;    ///< Path to crashpad_handler binary
};

/// Initialize Crashpad. Idempotent — subsequent calls log a warning and no-op.
/// On failure (handler missing, dump dir inaccessible), logs an error and
/// returns false; the application continues without crash reporting.
[[nodiscard]] bool initCrashReporting(const CrashReporterConfig& config);

/// Explicit shutdown. Crashpad's handler process continues running until
/// the parent exits naturally; this only releases the in-process registration.
void shutdownCrashReporting();

/// Whether crash reporting is currently active.
[[nodiscard]] bool crashReportingActive() noexcept;

}  // namespace signalforge::platform
```

`crashpad_handler` is built from Crashpad's source fetched via `FetchContent`. The build system discovers its path and passes via `SIGNALFORGE_CRASHPAD_HANDLER_PATH` CMake variable.

#### 4.5.4 `app_paths.hpp`

```cpp
// src/platform/app_paths.hpp
#pragma once

#include <QString>

namespace signalforge::platform {

/// Log directory. Resolution order:
/// 1. $SIGNALFORGE_LOG_DIR if set
/// 2. $XDG_STATE_HOME/signalforge/logs
/// 3. ~/.local/state/signalforge/logs
/// Directory is created if absent.
[[nodiscard]] QString logDirectory();

/// Crashdump directory. Resolution: $XDG_STATE_HOME/signalforge/crashdumps
/// or ~/.local/state/signalforge/crashdumps. Created if absent.
[[nodiscard]] QString crashDumpDirectory();

/// Config directory. Resolution: $XDG_CONFIG_HOME/signalforge or
/// ~/.config/signalforge. Created if absent.
[[nodiscard]] QString configDirectory();

}  // namespace signalforge::platform
```

### 4.6 Observability extensions

Extend the existing `src/observability/` with structured-field support and a minimal metrics registry.

#### 4.6.1 Structured log fields

The existing `SF_LOG_*` macros wrap spdlog. Add a lightweight field helper:

```cpp
// src/observability/logging.hpp — additions

namespace signalforge::observability {

/// Attach structured fields to the next log line. Thread-local; cleared after
/// use. Intended for one-off correlation, not high-volume logging.
///
/// Usage:
///   with_fields("device_id", deviceId, "queue", queueName);
///   SF_LOG_WARN("backpressure triggered");
void with_fields(...); // variadic key-value pairs

}
```

Exact implementation uses spdlog's custom formatter or `spdlog::details::backtracer`. If spdlog's API does not cleanly support this, HALT — do not implement string-concatenated fields inside the log message as a workaround (that breaks JSON-lines structure).

#### 4.6.2 `metrics.hpp`

```cpp
// src/observability/metrics.hpp
#pragma once

#include <QString>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace signalforge::observability {

/// Metric kinds. Names correspond to the performance panel rows in
/// [Arch §14.2].
enum class MetricKind : int {
    Counter,  ///< Monotonic increasing (e.g., frames_received)
    Gauge,    ///< Can go up or down (e.g., queue_watermark_pct)
};

class Metric {
public:
    Metric(QString name, MetricKind kind);

    /// Counter: increment by delta. Gauge: set to value.
    /// For Gauge use set(). For Counter use add().
    void add(std::int64_t delta) noexcept;
    void set(std::int64_t value) noexcept;

    [[nodiscard]] std::int64_t value() const noexcept;
    [[nodiscard]] const QString& name() const noexcept;
    [[nodiscard]] MetricKind kind() const noexcept;

private:
    QString name_;
    MetricKind kind_;
    std::atomic<std::int64_t> value_{0};
};

/// Singleton registry. Metrics are created once and referenced by pointer.
/// Thread-safe for registration and enumeration.
class MetricsRegistry {
public:
    static MetricsRegistry& instance();

    /// Register or fetch existing. Ownership retained by registry.
    Metric* getOrCreate(const QString& name, MetricKind kind);

    /// Snapshot all metrics. Safe for UI panel consumption.
    [[nodiscard]] std::vector<std::pair<QString, std::int64_t>> snapshot() const;

private:
    MetricsRegistry() = default;
    // impl
};

}  // namespace signalforge::observability
```

M5's performance panel will consume `MetricsRegistry::snapshot()`. M2 just defines the infrastructure.

### 4.7 Crash-trigger tool

Place at `tools/crash_test/`.

Structure:

```
tools/crash_test/
├── CMakeLists.txt
├── main.cpp
└── README.md
```

Standalone CMake, not wired into top-level `add_subdirectory`. Build command:

```
cd tools/crash_test
cmake -B build -S .
cmake --build build
```

`main.cpp` takes a subcommand:

```
./crash_test null_deref     # dereference nullptr
./crash_test abort          # std::abort()
./crash_test throw          # unhandled std::runtime_error
./crash_test stack_overflow # infinite recursion
```

Each mode first calls `initCrashReporting()` with the standard config, then triggers its crash. After the crash, the tool's README instructs the operator:

1. Check `~/.local/state/signalforge/crashdumps/` for a `.dmp` file
2. Verify file size > 0
3. Confirm `crashpad_handler` process spawned during crash (ps / journalctl)

`README.md` is short and operator-facing; includes a troubleshooting section for the case where AppProtection interferes with Crashpad's handler (same class of risk as M0's C2).

---

## 5. Test strategy

### 5.1 Coverage requirement: ≥ 85% line coverage on public surface

This is raised from the V1 default of 70%. Rationale: every interface here is load-bearing for M3–M11. A bug found at M5 costs 3× more to fix than if caught in M2's tests.

Use `cmake --build --target coverage` or gcovr to produce a coverage report. Include the report summary in `.claude/M2-done.md`.

### 5.2 Mandatory test categories

For each module, tests must cover:

**For all types**:
- Construction, move/copy semantics where applicable
- Default-constructed state is well-defined
- Comparison/equality if the type defines it

**For `DriverInterface`** (tested via a mock subclass `tests/mocks/mock_driver.hpp`):
- Lifecycle transitions: Idle → Opening → Open → Running → Stopping → Open → Closing → Idle
- Error path: any state → Error; Error → Closing → Idle
- Signal emission order and thread affinity
- `statistics()` returns a consistent snapshot (no torn reads)
- `write()` in wrong state returns NotConfigured / appropriate error

**For `RawFrame`**:
- Implicit sharing: copying a RawFrame with a large payload does not allocate (verify via counting allocator or mock `QByteArray` tracking)
- Metatype registration: `QVariant::fromValue(RawFrame{})` round-trips through `QVariant::value<RawFrame>()`

**For `WatermarkTracker`**:
- Cross-threshold events fire exactly once per crossing (no duplicate "filling" signals at 81%, 82%, 83%)
- Recovery threshold (60%) correctly resets the "above high" state
- Multi-producer concurrent observe() calls produce at most one "filling" signal across all threads

**For `SpscRing`**:
- Single-threaded: push/pop ordering, capacity behavior, peak tracking
- Two-threaded: stress test with 1M items, verify all received in order, no losses if within capacity
- Fill to capacity: next push fails cleanly, existing items retrievable

**For `MpscQueue`**:
- Single-producer, single-consumer: basic push/pop
- N-producer, single-consumer stress: 8 producers × 100k items each; consumer sees all items, any order
- `sizeApprox()` converges to zero after all popped

**For `Snapshot`**:
- Single write, single read: most recent wins
- Concurrent readers: all see a consistent, non-torn T
- Writer can publish new value while readers hold old value; old T is released when last reader done

**For platform utilities**:
- `monotonicNow()` is monotonic across calls
- `wallClockNow()` is not used for any internal ordering (test by convention — review)
- `setCurrentThreadName` truncates > 15 chars and logs a warning
- `logDirectory()` / `crashDumpDirectory()` / `configDirectory()` create dirs if absent

**For observability extensions**:
- Structured fields appear correctly in JSON log output
- `MetricsRegistry::getOrCreate` returns same pointer on repeat calls with same name
- Concurrent `add()` on a Counter produces correct total (stress test, 8 threads × 1M increments)

### 5.3 Integration test

One integration test: `tests/integration/driver_lifecycle_with_mock.cpp`.

Uses `MockDriver` to exercise the full interface surface:

1. Construct on main thread
2. `open()` → state Opening → Open (confirmed via signal)
3. `start()` → state Running
4. Inject 1000 mock frames over 1 second (mock driver uses `QTimer`)
5. Consumer (test thread) receives all 1000 via signal with QueuedConnection
6. `stop()` → state Open (confirmed)
7. `close()` → state Idle

Verify: signal emission thread = driver's IO thread, consumer thread = test main; no frame lost, no frame reordered.

### 5.4 What is NOT tested in M2

- Crashpad minidump actually triggers: manual verification via `tools/crash_test/`, not in unit tests
- Concrete driver behavior with real hardware: M3
- Frame pipeline wiring from driver to decoder: M4

---

## 6. Freeze protocol

M2 is the first milestone with `Interface freeze` as its hard-stop type. Formalize the protocol.

### 6.1 What freezes at M2 close

The following are **frozen** after `milestone/M2` merges to main and `v0.0.3-alpha.1` is tagged. Changes to these after freeze require a new ADR and human approval:

- `DriverInterface` class declaration — method signatures, signal signatures, Q_DISABLE_COPY_MOVE status
- `DriverState` and `DriverErrorCode` enum values and their numeric assignments (don't renumber)
- `DriverError` struct field layout
- `RawFrame` struct field layout (adding "reserved" fields later requires bumping a version marker if downstream code cares — M8 format spec will decide)
- `RxStats`, `TxStats`, `DriverStatistics` struct field layouts
- `BackpressureSignal` struct and `BackpressureReason` enum
- `WatermarkTracker` public API
- `SpscRing<T>`, `MpscQueue<T>`, `Snapshot<T>` public API
- `DriverInterface` thread-affinity contract (whatever is documented in the Doxygen)

### 6.2 What does NOT freeze

- Internal implementation details (private methods, member variable layouts, .cpp-file helpers)
- Test harness and mocks in `tests/`
- Crash-trigger tool in `tools/crash_test/`
- `metrics.hpp` registry API (may evolve through M5 when the performance panel first consumes it)
- Exact wording of Doxygen comments (clarifying language is welcome; changing semantics is not)

### 6.3 How to amend a frozen interface post-M2

1. CC encounters need → HALT with proposed amendment
2. Human evaluates impact across already-shipped milestones
3. If approved: new ADR under `docs/architecture/decisions/ADR-NNN-interface-amendment.md` documents the change, rationale, and migration path for consumers
4. `.claude/M2-done.md` is NOT retroactively edited — the freeze record there remains as historical truth
5. The amendment commit on main references the ADR in its body

### 6.4 Freeze record format

`.claude/M2-done.md` includes a section:

```markdown
## Freezes established in this milestone

The following are frozen per [M2-spec §6.1]. Modifications post-merge require
a new ADR per [M2-spec §6.3].

- `src/drivers/driver_interface.hpp`: DriverInterface class, all methods and signals as declared; DriverState enum; DriverErrorCode enum; DriverError struct
- `src/frame/raw_frame.hpp`: RawFrame struct layout; RxStats, TxStats, DriverStatistics struct layouts
- `src/frame/backpressure.hpp`: BackpressureSignal struct; BackpressureReason enum; WatermarkTracker public API
- `src/utils/spsc_ring.hpp`: SpscRing<T> public API
- `src/utils/mpsc_queue.hpp`: MpscQueue<T> public API
- `src/utils/snapshot.hpp`: Snapshot<T> public API
- `src/platform/time_source.hpp`: all declarations
- Thread-affinity contracts for DriverInterface as documented in its Doxygen

Sha256sum of frozen header files at close (for tamper detection):
[sha list generated via `find src -name '*.hpp' -path '*/drivers/*' -o -path '*/frame/*' -o -path '*/utils/*' -o -path '*/platform/*' | xargs sha256sum`]
```

---

## 7. M2-specific HALT triggers

In addition to general triggers in `CLAUDE.md`:

1. **Any ambiguity in interface design** (ownership, thread affinity, error propagation) that isn't resolved in this spec — HALT and ask. Do not guess.
2. **Crashpad FetchContent or build failure** — HALT. Crashpad is complex and host-specific; if it fails to build on Ubuntu 24.04, I want to know.
3. **C++20 `std::atomic<std::shared_ptr<T>>` unavailable or buggy on GCC 13** — HALT; do not fall back to custom implementation.
4. **Coverage < 85% on a module** after a good-faith test-writing pass — HALT and ask whether to raise threshold or add edge-case tests.
5. **Test flakiness under stress** — any stress test (see §5.2) that fails intermittently is a HALT, not a "retry until green" situation.

---

## 8. Acceptance criteria

### 8.1 Build and test

- [ ] Debug, Release, debug-asan all build clean with zero warnings
- [ ] ctest passes all three presets
- [ ] debug-asan passes in CI (local host may be blocked by AppProtection per OV-1)
- [ ] Coverage ≥ 85% on the union of all M2 modules, reported in `M2-done.md`
- [ ] `tools/crash_test/` builds independently and README is present

### 8.2 Interface quality

- [ ] Every public declaration has Doxygen with: one-sentence purpose, thread-safety note (or "N/A"), ownership note where applicable, precondition/postcondition for lifecycle methods
- [ ] Every enum has its numeric values explicit (defensive against accidental renumbering later)
- [ ] Every `Q_DECLARE_METATYPE`'d type is registered at program start (document where) and round-trips through `QVariant`
- [ ] `[[nodiscard]]` on all functions where ignoring the return is a bug

### 8.3 Freeze record

- [ ] `.claude/M2-done.md` has the Freezes section per §6.4
- [ ] Sha256sums of frozen headers are recorded
- [ ] ADR-002 optional — only if any design choice warrants a dedicated ADR (pre-M2 decisions are already in this spec; no additional ADR expected unless an unforeseen choice emerges)

### 8.4 Hand-off checklist content expected

`M2-done.md` hand-off section should explicitly call out:
- What M3 can start immediately (concrete driver implementations)
- What M3's first action should be (read DriverInterface Doxygen top-to-bottom)
- Known rough edges carried forward (e.g., if the crash_test tool is unverified on the dev host due to AppProtection)

---

## 9. Notes for CC

- **Precision over speed.** This milestone's code will be read by every subsequent milestone's CC. If a Doxygen sentence is ambiguous, rewrite it. If a contract needs three paragraphs to explain clearly, use three paragraphs.
- **Do not invent new types.** The types listed in §4 are exhaustive for M2. If you think you need a new public type, HALT and ask.
- **Follow the mandatory first action from CLAUDE.md §Required-9** (state observation). Verify that main is at `v0.0.2-alpha.1` merge commit before beginning.
- **The `DriverInterface` Q_OBJECT placement is architecturally load-bearing**. Do not "optimize" it to a non-QObject abstract class.
- **Use the additive-extension whitelist** ([EM §Ambiguity handling] exception) for report/documentation extensions only. Interface code changes are NEVER additive extensions in CC's autonomy — they are always HALT-worthy.
- **Your completion report is the freeze record.** Write it with the care of a legal contract, because that is effectively what it is.

---

## Appendix A — Session opening message for CC

```
You are Claude Code, working on the SignalForge project on branch milestone/M2.

This is the foundation milestone. Everything you define here will be used by M3 through M11. Precision matters.

Required reading, in this order:
1. CLAUDE.md
2. docs/claude-code/execution-manual.md (sections 1–7; pay attention to §5, §6)
3. docs/architecture/architecture.md §4.3, §5, §14
4. docs/architecture/decisions/ADR-001-rendering-approach.md (context)
5. docs/milestones/M2-platform-core-abstractions.md (your spec)

Before writing any code, observe current repo state: `git fetch origin --prune && git status && git log --oneline origin/main -5 && gh repo view --json defaultBranchRef`. Confirm main is at v0.0.2-alpha.1 merge commit and milestone/M2 is cleanly branched.

Then produce only these two files and stop:

Step 1 — .claude/M2-understanding.md:
- Restate M2's goal (3–5 sentences), with specific emphasis on what "interface freeze" means for subsequent milestones.
- List ambiguities or contradictions found in the spec. Include ambiguities in the interface designs themselves (ownership, thread affinity, error propagation) — these are the highest-risk type, even if the spec appears to address them.
- List HALT risks specific to this milestone, including Crashpad build, C++20 atomic shared_ptr availability, and test flakiness under stress.
- State explicitly how your plan will verify the thread-affinity contracts at test time (this is hard to test; I want to see your approach).

Step 2 — .claude/M2-plan.md:
- Break M2 into ordered subtasks (likely S1–S12 or so).
- For each: output files, rough effort, what "done" looks like for that subtask.
- Mark commit points — one commit per cohesive unit, not per file.
- Note which subtasks are highest-risk for HALT.
- Specifically call out: the order you will implement platform → frame → drivers → utils (or whatever order), and why.

After both files exist, reply "M2 understanding and plan ready for review" and stop.
Do not proceed to code until I say "approved, begin M2 execution".
```

---

## 10. Closing note

M2 is where the project transitions from "scaffolding" to "building". The interfaces here will outlive V1 — even if V1.5 adds CAN and Modbus drivers, they will inherit from `DriverInterface` as defined this week.

The value of M2's Doxygen documentation compounds every week. The cost of an ambiguous interface paragraph gets paid repeatedly, by every CC session that has to reread it. The cost of a well-written one is paid once.

Three person-days of extra review time during M2 prevents weeks of rework during M3–M11.

When in doubt, be precise, not fast.
