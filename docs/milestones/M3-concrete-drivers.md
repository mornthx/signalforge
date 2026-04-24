# M3 — Concrete Drivers + Connection Manager Preview

| Field | Value |
|---|---|
| Milestone ID | M3 |
| Sprint | 3 |
| Estimated effort | 10–14 person-days (wide allocation; quality priority) |
| Prerequisites | M2 closed (main at v0.0.3-alpha.1) |
| Next milestone | M4 |
| Hard-stop type | **Implementation correctness** (with freeze-alignment verification) |
| Soft-HALT allowed | **No** — drivers either work per contract or they do not |
| Branch | `milestone/M3` |

**Cross-reference notation**:

- `[EM §N]` — Execution Manual, section N
- `[Arch §N]` — Architecture document, section N
- `[MR M<n>]` — Milestone Roadmap
- `[CM §X]` — CLAUDE.md, section X
- `[ADR-N]` — Architecture Decision Record N
- `[M2 §N]` — M2 spec, section N (the frozen contract M3 implements)

---

## 1. Goal

Implement four concrete `DriverInterface` subclasses — `SerialDriver`, `TcpDriver`, `UdpDriver`, `ReplayDriver` — plus a minimal Connection Manager UI to exercise them interactively. Deliver performance baselines and error-injection coverage such that M10 optimization and M4 pipeline integration both have a verified, trustworthy foundation.

M3's quality philosophy is **user-facing polish first, runtime speed second**. A driver that handles disconnect cleanly, produces a human-readable error message, and never blocks the UI thread is more valuable in V1 than one that squeezes an extra 20% throughput.

---

## 2. Scope

### 2.1 Must deliver

1. **SerialDriver** at `src/drivers/serial_driver.{hpp,cpp}`:
   - Inherits `DriverInterface`
   - Uses `QSerialPort` via a dedicated IoWorker on a worker thread
   - `SerialConfig` struct with minimum fields (see §4.1)
   - Loopback integration test using socat virtual serial pair

2. **TcpDriver** at `src/drivers/tcp_driver.{hpp,cpp}`:
   - Inherits `DriverInterface`
   - Uses `QTcpSocket` via IoWorker on worker thread
   - `TcpConfig` struct with connect timeout, reconnect policy placeholders
   - Localhost echo-server integration test

3. **UdpDriver** at `src/drivers/udp_driver.{hpp,cpp}`:
   - Inherits `DriverInterface`
   - Uses `QUdpSocket`; unicast and multicast support
   - `UdpConfig` struct with local + remote endpoint specification
   - Localhost bidirectional integration test

4. **ReplayDriver skeleton** at `src/drivers/replay_driver.{hpp,cpp}`:
   - Inherits `DriverInterface`
   - `ReplayConfig` struct with session file path
   - Lifecycle methods complete and correct: `open()` opens the file (verifies existence only, no parsing), `close()` releases, `start()` transitions state but emits no frames, `stop()` idempotent
   - File format parsing + frame emission deferred to M9; clearly documented via `// TODO(M9): parse sessions and emit frames` comments at the exact insertion points

5. **Shared IoWorker pattern** at `src/drivers/io_worker_base.{hpp,cpp}`:
   - Not a public interface (not part of freeze scope) but a utility class that all four drivers inherit from or compose with
   - Encapsulates QThread lifecycle (construct, moveToThread, quit, wait) so each driver doesn't re-implement it

6. **Connection Manager UI preview** at `src/app/connection_manager.{hpp,cpp}` + dialog UI file:
   - A QDialog with driver type selector (Serial/TCP/UDP/Replay)
   - Conditional form fields based on selected type
   - "Connect" / "Disconnect" buttons
   - Real-time state display (Idle/Opening/Open/Running/Error badge)
   - Scrolling log of received frames (decoded as hex + ASCII, truncated to last 200 entries)
   - Last error message display
   - **Not**: configuration persistence, multi-connection, yaml save/load, favorites (all M7)

7. **Integration tests** under `tests/integration/`:
   - `test_serial_driver_loopback.cpp` — socat virtual pair
   - `test_tcp_driver_echo.cpp` — localhost echo server
   - `test_udp_driver_loopback.cpp` — localhost UDP
   - `test_replay_driver_skeleton.cpp` — lifecycle correctness, no frame emission
   - `test_driver_error_paths.cpp` — error injection (extended set per §5.3)

8. **Performance benchmarks** at `tests/benchmark/`:
   - `bench_driver_throughput.cpp` — raw throughput per driver
   - `bench_driver_latency.cpp` — latency profile
   - `bench_driver_footprint.cpp` — resource footprint
   - Results written to `tests/benchmark/results/M3-baseline.md` and committed to repo
   - Results are evaluated against hard thresholds (§5.4); infrastructure bottleneck exemption possible (§5.5)

9. **Unit tests** with coverage ≥ 85% on each driver module, via mock peers where applicable

10. **Doxygen** on all public declarations, plus inline `TODO(M9)` markers at the exact insertion points for ReplayDriver's future completion

11. **Hand-off record** in `.claude/M3-done.md` including: test pass matrix, performance baseline numbers, error-injection coverage list, known rough edges, and M4 hand-off notes

### 2.2 Must not do

1. **No modifications to any frozen interface from M2.** `DriverInterface`, `RawFrame`, statistics structs, BackpressureSignal, WatermarkTracker, SpscRing, MpscQueue, Snapshot, platform utilities — all are read-only from M3's perspective. If you think any of them needs to change, HALT and ask.

2. **No frame pipeline (decode) wiring.** Drivers produce `RawFrame`; what decodes them is M4.

3. **No Connection Manager full features.** Multi-connection, yaml config, favorites — all M7.

4. **No session recording or replay content.** ReplayDriver is skeleton only; writing sessions is M8, replaying content is M9.

5. **No Modbus or CAN driver stubs.** Reserved for V1.5 per `[Arch §3.3]`.

6. **No DriverFactory pattern.** Concrete drivers are constructed directly by the Connection Manager UI with their specific config struct. Factory abstraction is deferred (M7 is the natural place for it if needed).

7. **No new top-level dependencies beyond sentry-native.** If you think you need a new dependency (e.g., a TCP framing library, a benchmark framework beyond Catch2), HALT. `socat` is a runtime test tool, not a library dependency.

8. **No changes to CI workflow beyond adding `socat` to apt-install.** Benchmarks should run locally or in a dedicated `performance-baseline.yml` workflow (not pull_request-triggered; manual / scheduled only) — but this workflow is optional for M3 and may be deferred to M10. M3 pull requests run unit + integration tests only, not benchmarks.

---

## 3. Design Decisions (locked by this spec)

These decisions resulted from pre-M3 planning. They are implementation-level contracts; downstream milestones may reference them but they are not architecture changes.

### 3.1 Every driver runs its IO on a dedicated QThread via moveToThread

**Decision**: Each concrete driver constructs a private `IoWorker` QObject and a dedicated `QThread` in its constructor. The IoWorker is moved to the thread and connected via signals/slots. The driver itself lives on the caller's thread (typically main).

**Rationale**: This is the Qt-idiomatic pattern. It isolates blocking IO (Serial syscalls, socket operations) from the UI thread. `QueuedConnection` on signals makes RawFrame delivery cross-thread safe, matching `[M2 §4.1]` thread-affinity contract.

**Cost accepted**: Each driver consumes one OS thread. For V1 (≤10 concurrent drivers typical), this is fine. V2/embedded targets may revisit.

**Cross-reference**: All concrete drivers compose or inherit from `IoWorkerBase` (§4.5) to avoid duplicating QThread lifecycle boilerplate.

### 3.2 `write()` semantics are uniformly asynchronous

**Decision**: `write(payload)` returns `Success` when the payload is queued for transmission. Actual byte-level success (TCP ack, Serial fd write) is reflected in `TxStats::framesTotal` / `TxStats::failures` over time. A hard transmit failure (TCP disconnect, Serial device lost) is reported via `errorOccurred` signal.

**Rationale**: TCP's natural semantic is already "queue for send"; forcing Serial to match avoids caller-side type-of-driver branching. Small overhead on Serial (one queue push per write) is acceptable.

**Implementation note**: Serial driver internally maintains a small `QByteArray` queue on its IoWorker; `write()` emits a signal to the worker which drains the queue.

### 3.3 Connection Manager dialog is single-instance, single-connection

**Decision**: The Connection Manager dialog (§4.6) manages one driver instance at a time. Selecting a different driver type or disconnecting releases the current driver. No persistent connection list, no save/restore.

**Rationale**: Preview-level integration. Full multi-connection management is M7. The goal in M3 is to exercise driver interfaces end-to-end through a real Qt event loop, not to ship a production connection manager.

### 3.4 Performance baselines are measured, recorded, and hard-gated with infrastructure exemption

**Decision**: Benchmarks produce a results file that is committed to the repo. Each metric has a threshold:

- Exceeding threshold → subtask passes
- Missing threshold → HALT; CC investigates cause
  - If cause is identifiably in CC's code → fix and retry
  - If cause is demonstrably in Qt, Linux kernel, socat, or host environment → document the investigation, note the infrastructure bottleneck, and proceed with the actual measurement as baseline

**Rationale**: M10 optimization needs a trustworthy baseline. Measurements without context are noise; measurements with bottleneck analysis are data.

**Thresholds are in §5.4.** They are targets for local loopback / stub / modest baud rates, not end-hardware throughput ceilings.

### 3.5 Error injection coverage is the extended set

**Decision**: Error-injection tests cover:

- Normal disconnect (peer closes)
- Mid-run disconnect (disconnect during active transmission)
- Invalid configuration (bad device path, port out of range, unreachable host)
- Buffer pressure (producer outpaces consumer; WatermarkTracker fires)
- Malformed peer behavior (zero-length UDP, truncated TCP stream)
- Race conditions (open()→close() without wait; rapid state transitions)

This is the extended set identified in pre-M3 planning.

**Rationale**: The stated quality philosophy is "handles edge cases gracefully and reports human-readable errors." Error injection is how that property is verified. Unit tests with mocks can't replicate these without socat or OS-level tricks.

### 3.6 ReplayDriver skeleton means lifecycle-correct, emission-empty

**Decision**: ReplayDriver implements all `DriverInterface` methods with correct state transitions and idempotency. `open()` opens the session file (verifies existence, basic header sanity check, but no full parse). `start()` transitions to Running state but emits zero `frameReceived` signals. `stop()` and `close()` are idempotent.

**Rationale**: M4 needs a driver instance it can plumb through the pipeline without real data. A fully-functional ReplayDriver is M9. The skeleton lets M4 work; the real content lives in M9. The exact insertion points for M9 to fill in parsing + emission are marked with `// TODO(M9): ...` comments at line-precision.

### 3.7 Configuration structs are minimal

**Decision**: Each `*Config` struct contains only the fields that have a concrete V1 use case:

- `SerialConfig`: device path, baud rate, data bits, parity, stop bits, flow control
- `TcpConfig`: host, port, connect timeout
- `UdpConfig`: local bind endpoint, remote send endpoint (for unicast); multicast group (optional)
- `ReplayConfig`: session file path

Fine-grained Qt-specific options (read buffer size, DTR/RTS state, write timeout) use defaults from Qt itself. If a future milestone needs more control, the field is added then — not preemptively.

**Rationale**: Minimal config surface reduces test matrix, reduces user confusion in the UI, and makes yaml config in M7 simpler.

### 3.8 No soft-HALT

**Decision**: M3 inherits M2's hard-HALT-only stance. Implementation correctness is binary for driver semantics. If a driver passes 95% of its tests, it is not "mostly done" — the 5% failing tests represent real bugs.

---

## 4. Key Implementation Details

### 4.1 Configuration structs

Place at `src/drivers/driver_configs.hpp`. All config structs live in this single header for discoverability.

```cpp
// src/drivers/driver_configs.hpp
#pragma once

#include <QString>
#include <QtGlobal>
#include <chrono>
#include <cstdint>

namespace signalforge::drivers {

/// Serial port configuration.
struct SerialConfig {
    QString device;                           ///< e.g. "/dev/ttyUSB0" or "/tmp/ttyV0"
    qint32 baudRate = 115200;                 ///< Standard rates: 9600, 38400, 115200, 921600
    int dataBits = 8;                         ///< 5, 6, 7, or 8
    QString parity = QStringLiteral("none");  ///< "none", "even", "odd", "mark", "space"
    int stopBits = 1;                         ///< 1 or 2
    QString flowControl = QStringLiteral("none");  ///< "none", "hardware", "software"
};

/// TCP connection configuration.
struct TcpConfig {
    QString host;                                            ///< Hostname or IP (resolved at open())
    quint16 port = 0;                                        ///< 1..65535; 0 rejected in open()
    std::chrono::milliseconds connectTimeout{5000};          ///< open() timeout
    // Reserved for future: std::chrono::milliseconds reconnectBackoff;
};

/// UDP endpoint configuration.
struct UdpConfig {
    QString localBindAddress = QStringLiteral("0.0.0.0");    ///< "0.0.0.0", "127.0.0.1", or specific
    quint16 localBindPort = 0;                               ///< 0 = OS-assigned
    QString remoteHost;                                      ///< Target for write()
    quint16 remotePort = 0;
    QString multicastGroup;                                  ///< Optional; e.g. "224.0.0.1"
    quint32 multicastTtl = 1;                                ///< Valid only if multicastGroup nonempty
};

/// Session replay configuration.
struct ReplayConfig {
    QString sessionFilePath;                                 ///< Path to .sf or .sfreplay session file
    // M9 adds: playback rate, loop mode, start offset
};

}  // namespace signalforge::drivers
```

**Validation**: Each driver's `open()` validates its config. Invalid values (negative port, empty host for TcpDriver, nonexistent device for SerialDriver's device path) return `DriverErrorCode::ConfigInvalid` and do NOT transition state.

**Config struct equality / comparison**: Not required for M3. M7 (Connection Manager) may add these when needed.

### 4.2 SerialDriver

Place at `src/drivers/serial_driver.{hpp,cpp}`.

Public interface:

```cpp
// src/drivers/serial_driver.hpp
#pragma once

#include "drivers/driver_configs.hpp"
#include "drivers/driver_interface.hpp"

#include <QObject>
#include <QThread>
#include <memory>

namespace signalforge::drivers {

class SerialIoWorker;  // pimpl-like; defined in .cpp

/// Serial port driver. Uses QSerialPort on a dedicated IO thread.
///
/// Typical usage:
///   SerialDriver driver{SerialConfig{.device="/dev/ttyUSB0", .baudRate=115200}};
///   connect(&driver, &DriverInterface::frameReceived, ...);
///   driver.open();   // async; wait for stateChanged(Open)
///   driver.start();  // frames flow via frameReceived
///   driver.write(payload);
///   driver.stop();
///   driver.close();  // blocks until IO thread joins (< 500ms)
class SerialDriver : public DriverInterface {
    Q_OBJECT

public:
    explicit SerialDriver(SerialConfig config, QObject* parent = nullptr);
    ~SerialDriver() override;

    DriverErrorCode open() override;
    void close() override;
    DriverErrorCode start() override;
    void stop() override;
    DriverErrorCode write(const QByteArray& payload) override;

    [[nodiscard]] DriverState state() const override;
    [[nodiscard]] DriverErrorCode health() const override;
    [[nodiscard]] signalforge::frame::DriverStatistics statistics() const override;

    /// Current configuration. Returned by value.
    [[nodiscard]] const SerialConfig& config() const noexcept;

private:
    SerialConfig config_;
    std::unique_ptr<QThread> thread_;
    std::unique_ptr<SerialIoWorker> worker_;
    // Statistics are held in atomics inside worker_; exposed via statistics()
};

}  // namespace signalforge::drivers
```

**Thread ownership**: `SerialDriver` owns `thread_` and `worker_`. `worker_` is moved to `thread_` in constructor. On destruction, driver issues `thread_->quit()` and `thread_->wait()` before releasing.

**Signal flow**:
- Driver's public signals (`frameReceived`, `errorOccurred`, `stateChanged`) are the `DriverInterface` signals.
- Worker has its own internal signals (`workerFrameReceived`, etc.) connected to driver's public signals via `Qt::QueuedConnection`. Driver re-emits them to consumers.

**Open sequence**:
1. Validate `config_`. If invalid → return `ConfigInvalid`, no state change.
2. Emit `stateChanged(Opening)`.
3. Post `openRequest()` signal to worker.
4. Worker on IO thread: constructs QSerialPort, applies config, calls `QSerialPort::open(ReadWrite)`.
   - Success: worker emits `workerStateChanged(Open)` → driver emits public `stateChanged(Open)`.
   - Failure: worker emits `workerError(DriverError{ResourceUnavailable, detail, now()})` → driver emits public `errorOccurred` then `stateChanged(Error)`.

**Start sequence**: Driver emits `stateChanged(Running)` after the worker confirms `readyRead` signal is connected (one round-trip through QueuedConnection). Frames flow via worker's `readyRead → read → emit workerFrameReceived` chain.

**Write sequence**: Driver's `write()` enqueues to `worker_->writeQueue_` (private SPSC ring) and signals the worker to drain. Always returns `Success` if state is `Running`; actual Serial failure reflected in `TxStats`.

**Close sequence**: Driver emits `stateChanged(Closing)`, posts `closeRequest()` to worker, waits for `workerStateChanged(Idle)`, joins thread. Total time budget ~500ms.

**Implementation note**: The entire SerialIoWorker class lives in `serial_driver.cpp`. It is not public and not part of the freeze scope. Internal refactoring is allowed post-M3.

### 4.3 TcpDriver

Place at `src/drivers/tcp_driver.{hpp,cpp}`.

Same pattern as SerialDriver: public `TcpDriver : DriverInterface`, private `TcpIoWorker` on a dedicated QThread.

**Open sequence for TCP**:
1. Validate `TcpConfig`. Empty host or port=0 → `ConfigInvalid`.
2. Emit `stateChanged(Opening)`.
3. Worker calls `socket_->connectToHost(host, port)`.
4. Use `QTcpSocket::waitForConnected(connectTimeout.count())` on the worker thread; or connect to `connected` / `errorOccurred` signals.
5. Success: `stateChanged(Open)`. Failure: appropriate error code:
   - Host unresolvable → `ResourceUnavailable`
   - Connection refused → `ResourceUnavailable`
   - Timeout → `Timeout`
   - Permission denied → `PermissionDenied`

**Mid-run disconnect**: QTcpSocket emits `disconnected()`. Worker treats this as `ResourceLost` error, emits `stateChanged(Error)`. Driver does NOT auto-reconnect in M3.

**Write**: payload enqueued to worker; worker calls `socket_->write()`. If returns -1 → error. If socket is in `UnconnectedState` when write() called → `errorOccurred(ResourceLost)`.

**Framing**: TCP is a byte stream; M3's TcpDriver does not impose framing. Every call to `write(payload)` results in one or more bytes written. Read side: every `readyRead` produces a `RawFrame` with `payload = socket->readAll()`. Downstream (M4 decoders) handle re-framing.

### 4.4 UdpDriver

Place at `src/drivers/udp_driver.{hpp,cpp}`.

**Open sequence for UDP**:
1. Validate `UdpConfig`. Both local and remote endpoints optional but at least one must be specified:
   - If `localBindPort != 0` or `localBindAddress != "0.0.0.0"` → bind
   - If `remoteHost` non-empty → prepare for outbound write
   - If neither → `ConfigInvalid`.
2. Worker calls `socket_->bind(localAddress, localBindPort)`.
3. Multicast: if `multicastGroup` non-empty, `joinMulticastGroup(group)` after bind.
4. Emit `stateChanged(Open)` on successful bind.

**Read**: Every `readyRead` reads all pending datagrams. Each datagram becomes one `RawFrame` (unlike TCP, UDP framing is preserved).

**Write**: `writeDatagram(payload, remoteHost, remotePort)`. If remote endpoint not configured → `ConfigInvalid` returned synchronously.

**Error handling**: UDP is connectionless. "Destination unreachable" is reported via ICMP errors; these may or may not propagate back. Treat the absence of error as success. `TxStats::failures` counts datagrams that returned -1 from `writeDatagram`.

### 4.5 IoWorkerBase (shared lifecycle)

Place at `src/drivers/io_worker_base.{hpp,cpp}`.

```cpp
// src/drivers/io_worker_base.hpp
#pragma once

#include <QObject>
#include <QString>

namespace signalforge::drivers {

/// Common base for IO worker classes that live on a dedicated QThread.
///
/// Subclasses (SerialIoWorker, TcpIoWorker, UdpIoWorker, ReplayIoWorker)
/// add driver-specific setup slots and signals.
///
/// Not part of the DriverInterface freeze scope. Internal abstraction.
class IoWorkerBase : public QObject {
    Q_OBJECT

public:
    explicit IoWorkerBase(QString threadName, QObject* parent = nullptr);
    ~IoWorkerBase() override = default;

    /// Thread name (for debug logs, set via platform::setCurrentThreadName
    /// at thread start).
    [[nodiscard]] const QString& threadName() const noexcept;

protected:
    /// Subclass override. Called on the IO thread via QTimer::singleShot(0, ...)
    /// from the thread's started() signal. Subclasses set thread name, initialize
    /// socket/serial objects, etc.
    virtual void onThreadStart() = 0;

private:
    QString threadName_;
};

}  // namespace signalforge::drivers
```

Each concrete driver's constructor:

```cpp
SerialDriver::SerialDriver(SerialConfig config, QObject* parent)
    : DriverInterface{parent}, config_{std::move(config)}
{
    thread_ = std::make_unique<QThread>();
    thread_->setObjectName(QStringLiteral("SerialIO-") + config_.device);
    worker_ = std::make_unique<SerialIoWorker>(thread_->objectName());
    worker_->moveToThread(thread_.get());

    connect(thread_.get(), &QThread::started,
            worker_.get(), [this]() { worker_->onThreadStart(); });

    // Wire worker signals to driver's public signals.
    connect(worker_.get(), &SerialIoWorker::workerFrameReceived,
            this, &DriverInterface::frameReceived, Qt::QueuedConnection);
    connect(worker_.get(), &SerialIoWorker::workerErrorOccurred,
            this, &DriverInterface::errorOccurred, Qt::QueuedConnection);
    connect(worker_.get(), &SerialIoWorker::workerStateChanged,
            this, &DriverInterface::stateChanged, Qt::QueuedConnection);

    thread_->start();
}

SerialDriver::~SerialDriver() {
    if (thread_ && thread_->isRunning()) {
        thread_->quit();
        thread_->wait(500);  // 500ms budget
        if (thread_->isRunning()) {
            SF_LOG_ERROR("SerialDriver IO thread did not exit within 500ms; forcing terminate");
            thread_->terminate();
            thread_->wait();
        }
    }
}
```

### 4.6 Connection Manager UI preview

Place at `src/app/connection_manager.{hpp,cpp}` and `src/app/connection_manager.ui` (Qt Designer form, or hand-coded — your judgment).

**Widget structure**:

```
QDialog "Connection Manager"
├── QComboBox "Driver Type" [Serial, TCP, UDP, Replay]
├── QStackedWidget (one page per driver type)
│   ├── SerialConfigForm:
│   │   ├── QLineEdit "Device"
│   │   ├── QComboBox "Baud" [9600, 38400, 115200, 921600, custom]
│   │   ├── QSpinBox "Data Bits" (5-8)
│   │   ├── QComboBox "Parity"
│   │   ├── QSpinBox "Stop Bits" (1-2)
│   │   └── QComboBox "Flow Control"
│   ├── TcpConfigForm:
│   │   ├── QLineEdit "Host"
│   │   ├── QSpinBox "Port" (1-65535)
│   │   └── QSpinBox "Connect Timeout (ms)"
│   ├── UdpConfigForm:
│   │   ├── QLineEdit "Local Bind" ("address:port")
│   │   ├── QLineEdit "Remote" ("host:port")
│   │   └── QLineEdit "Multicast Group" (optional)
│   └── ReplayConfigForm:
│       └── QLineEdit "Session File" + QPushButton "Browse"
├── QHBoxLayout
│   ├── QPushButton "Connect"
│   └── QPushButton "Disconnect"
├── QLabel "State: Idle" (color-coded: gray/yellow/green/red)
├── QLabel "Last error: (none)"
├── QTextBrowser "Frame log" (read-only, shows hex+ascii of last 200 frames)
│   Format: "2026-04-24 10:32:45.123 [42 bytes] 01 02 AB CD ...  .ü.Í"
└── QStatusBar with throughput estimate ("47 frames/s, 1.2 KB/s")
```

**Behavior**:

- "Connect" validates current form, constructs the appropriate driver, calls `open()`. Tracks state via `stateChanged` signal.
- "Disconnect" calls `close()`. Tracks state transition back to Idle.
- Driver type change while a connection is active: disable dropdown; must Disconnect first.
- Log display: truncate to last 200 frames to avoid unbounded memory.
- Throughput: update every 1s from `statistics()`.
- Hex format: `xxd` style (16 bytes per line, offset + hex + ascii).

**Wire into MainWindow**: Add a menu item "File → Connection Manager..." that opens the dialog (modal or modeless — your judgment, I lean toward modeless so the user can interact with other parts of the main window while connected).

**Not in scope**:
- Saving favorite connections
- Loading config from yaml
- Multi-connection (this dialog manages one at a time)
- Frame decoder (the log shows raw bytes, not decoded signal values)

### 4.7 ReplayDriver skeleton

Place at `src/drivers/replay_driver.{hpp,cpp}`.

```cpp
// src/drivers/replay_driver.hpp
#pragma once

#include "drivers/driver_configs.hpp"
#include "drivers/driver_interface.hpp"

#include <QObject>
#include <QThread>
#include <memory>

namespace signalforge::drivers {

class ReplayIoWorker;

/// Session file replay driver.
///
/// **M3 status: SKELETON.** Lifecycle methods are complete and correct:
/// open() verifies the session file exists, close() releases, start()
/// transitions to Running but emits no frames, stop() is idempotent.
///
/// Actual session file parsing and frame emission is M9. The insertion
/// points are marked with `// TODO(M9):` comments at line-precision.
class ReplayDriver : public DriverInterface {
    Q_OBJECT

public:
    explicit ReplayDriver(ReplayConfig config, QObject* parent = nullptr);
    ~ReplayDriver() override;

    DriverErrorCode open() override;
    void close() override;
    DriverErrorCode start() override;
    void stop() override;
    DriverErrorCode write(const QByteArray& payload) override;

    [[nodiscard]] DriverState state() const override;
    [[nodiscard]] DriverErrorCode health() const override;
    [[nodiscard]] signalforge::frame::DriverStatistics statistics() const override;

private:
    ReplayConfig config_;
    std::unique_ptr<QThread> thread_;
    std::unique_ptr<ReplayIoWorker> worker_;
};

}  // namespace signalforge::drivers
```

**open() behavior**:
1. Validate `config_.sessionFilePath` non-empty → else `ConfigInvalid`
2. `QFile::exists(sessionFilePath)` → else `ResourceUnavailable`
3. Open file read-only, read first header bytes (TBD by M9 spec, but at minimum 16 bytes)
4. Close file (we're just verifying; M9 will keep it open for streaming)
5. Transition to Open

`// TODO(M9): parse full session metadata (clock origin, format version, stream descriptor)`

**start() behavior**:
1. Transition to Running
2. `// TODO(M9): open file for reading, start replay timer, begin emitting frames at recorded intervals`
3. (M3: do nothing more; driver sits in Running state emitting no frames)

**write() behavior**: Always return `NotConfigured` (ReplayDriver is read-only; write semantics are meaningless for a replayer). Alternative: return `Success` and silently discard. **Decision**: return `NotConfigured` — explicit failure makes wrong usage obvious.

**close() behavior**: Symmetric; idempotent.

**Statistics**: All counters remain at 0 throughout M3 (no frames emitted, no bytes received). `snapshotAt` updates on each call.

### 4.8 Error taxonomy

Map concrete error scenarios to `DriverErrorCode`:

| Scenario | Code | Driver | Trigger |
|---|---|---|---|
| Device path nonexistent | ResourceUnavailable | Serial | open() |
| Device path exists but no permission | PermissionDenied | Serial | open() |
| Device path valid, baud unsupported | ConfigInvalid | Serial | open() |
| Device valid, port in use | ResourceUnavailable | Serial | open() |
| USB cable unplugged mid-run | ResourceLost | Serial | after start() |
| Hostname unresolvable | ResourceUnavailable | TCP | open() |
| Connection refused | ResourceUnavailable | TCP | open() |
| Connection timeout | Timeout | TCP | open() |
| Peer closes gracefully | ResourceLost | TCP | after start() |
| Peer closes with RST | ResourceLost | TCP | after start() |
| Bind port in use | ResourceUnavailable | UDP | open() |
| Invalid multicast address | ConfigInvalid | UDP | open() |
| Multicast join failed | ResourceUnavailable | UDP | open() |
| Session file missing | ResourceUnavailable | Replay | open() |
| Session file exists but empty | ProtocolFailure | Replay | open() |
| Invalid session file magic | ProtocolFailure | Replay | open() |
| write() on non-Running driver | NotConfigured | all | write() |
| write() on replay driver | NotConfigured | Replay | always |
| Internal logic bug | Internal | all | (assertion context) |

Each driver writes tests covering its row in this table.

### 4.9 DriverError::message content

`DriverError::message` is a `QString` shown to users (potentially in the UI error badge). Style guidance:

- Human-readable, not errno translation
- Context-rich: "Failed to open /dev/ttyUSB0: Permission denied. Try adding your user to the 'dialout' group."
- Consistent capitalization and punctuation across drivers

CC judges specific wording but must avoid:
- "Error -13"
- "EACCES"
- "QSerialPort::PermissionError"

### 4.10 Thread naming convention

Each driver's IO thread has a descriptive name via `QThread::setObjectName()`:

- SerialDriver: `"SerialIO-/dev/ttyUSB0"` (include device path)
- TcpDriver: `"TcpIO-host:port"` (include connection target)
- UdpDriver: `"UdpIO-bind:port"` (include local bind)
- ReplayDriver: `"ReplayIO-<filename>"` (include file name, not full path)

This appears in `top`, `htop`, `strace`, gdb, and spdlog output. Debugging multi-driver scenarios without thread names is painful.

At thread start, each IoWorker calls `signalforge::platform::setCurrentThreadName(threadName_)` to set the name at OS level (not just Qt's tracked name).

---

## 5. Test strategy

### 5.1 Coverage ≥ 85% on driver modules

Same threshold as M2. Target by module:

- `serial_driver.cpp`: ≥ 85%
- `tcp_driver.cpp`: ≥ 85%
- `udp_driver.cpp`: ≥ 85%
- `replay_driver.cpp`: ≥ 75% (skeleton; some code paths are TODO-stubbed)
- `io_worker_base.cpp`: ≥ 70% (abstract base; direct instantiation may be limited)
- `connection_manager.cpp`: ≥ 60% (UI code is historically hard to unit-test; compensate with integration test)

ReplayDriver's lower bar is intentional; 75% on a skeleton is more meaningful than gaming the metric.

### 5.2 Unit tests

Per driver, unit tests exercise:

**Construction**:
- Construct with valid config; does not start thread until open()
- Construct with clearly-invalid config; config stored, open() will validate

**Lifecycle state machine**:
- Idle → Opening → Open → Running → Stopping → Open → Closing → Idle
- Error recovery: any state → Error → (via close()) → Idle
- Idempotent close(): close() on Idle driver is no-op
- Idempotent stop(): stop() when already stopped is no-op
- Double open() when already Open → what happens? (Decision: logs warning, returns Success silently; does not re-open. Document in tests.)

**write()**:
- write() in Idle → NotConfigured
- write() in Open (not Running) → NotConfigured (must be Running)
- write() in Running → Success, payload enters queue
- Large write (>10KB) handled without truncation (Serial/TCP/UDP have different limits; test each)

**statistics()**:
- Fresh driver: all counters zero
- After known payloads: counters reflect expected totals
- Cross-field consistency NOT asserted (per M2 §4.2 clarification 3)

**Error paths**:
- Every row in the §4.8 error taxonomy table has at least one test

**Thread affinity**:
- `frameReceived` signal arrives on consumer's thread (test with QueuedConnection)
- Emitting thread ID captured; should match driver's IO thread ID

### 5.3 Integration tests

Place at `tests/integration/`.

#### 5.3.1 `test_serial_driver_loopback.cpp`

**Prerequisites**: socat installed.

**Setup**: CMake custom target or test fixture spawns socat as child process:
```
socat -d pty,raw,echo=0,link=/tmp/sf_ttyV0 pty,raw,echo=0,link=/tmp/sf_ttyV1
```

This creates `/tmp/sf_ttyV0` ↔ `/tmp/sf_ttyV1` virtual serial pair (anything written to V0 appears on V1's readable end).

**Test scenarios**:
1. Open SerialDriver on `/tmp/sf_ttyV0` at 115200. Peer SerialDriver on `/tmp/sf_ttyV1`.
2. Write 100-byte payload to V0; verify V1 receives it as `frameReceived`.
3. Reverse direction: V1 writes, V0 receives.
4. Throughput: Write 1 MB of random data from V0; verify V1 receives all 1 MB (possibly as multiple frames).
5. Disconnect socat mid-test; verify both drivers emit `errorOccurred(ResourceLost)`.

**Teardown**: Kill socat.

#### 5.3.2 `test_tcp_driver_echo.cpp`

**Setup**: Test spawns a local echo server (CMake target or Qt-based internal helper — your judgment). Echo server listens on 127.0.0.1 at OS-assigned port, echoes every received byte back.

**Test scenarios**:
1. TcpDriver connects to echo server; sends 100 bytes; receives 100 bytes back.
2. Throughput: Send 100 MB; verify correct echo and measure rate.
3. Echo server closes connection; verify TcpDriver emits `errorOccurred(ResourceLost)`.
4. Echo server closes with RST (TCP reset); verify same.
5. Connection timeout: TcpDriver connects to firewalled port (OS-unassigned high port); verify `Timeout` error.

#### 5.3.3 `test_udp_driver_loopback.cpp`

**Setup**: Two UdpDrivers bound to different local ports on 127.0.0.1, configured to send to each other.

**Test scenarios**:
1. Driver A writes → Driver B receives (bidirectional).
2. Datagram boundaries preserved (each write(bytes[N]) → one frameReceived with N bytes).
3. Multicast: Both drivers join 224.0.0.1:port; writes from A arrive at both A and B (multicast loopback).
4. Large datagram (~60 KB, near UDP limit); verify received as one frame.
5. Datagram to unbound port (127.0.0.1:65000 with nothing listening); verify `TxStats::failures == 0` (UDP doesn't reliably detect this; ICMP may or may not propagate).

#### 5.3.4 `test_replay_driver_skeleton.cpp`

**Setup**: Pre-baked minimal session file (16-byte header only, no frames) at `tests/integration/fixtures/minimal_session.sfreplay`.

**Test scenarios**:
1. Construct ReplayDriver with valid path; open() succeeds, state → Open.
2. Open with nonexistent file → `ResourceUnavailable`.
3. Open with empty file → `ProtocolFailure`.
4. start() → state Running; wait 1 second; verify zero frames emitted.
5. write() → NotConfigured (always).
6. Stop + close: clean transition to Idle.

#### 5.3.5 `test_driver_error_paths.cpp`

Concentrated error injection tests per §3.5. One test case per scenario:

- Mid-run Serial disconnect (kill socat)
- Mid-run TCP disconnect (close echo server)
- Rapid open/close cycles (100 iterations; no leaks)
- open() → close() before Open state reached (race; state machine must be consistent)
- start() without open() → NotConfigured
- stop() without start() → no-op, no error
- Writing to a driver whose state just transitioned to Error → appropriate error

### 5.4 Performance baselines

Place at `tests/benchmark/`. These are separate executables (not linked into `ctest` directly — run manually or via a dedicated target).

**Build targets**:
- `bench_driver_throughput`
- `bench_driver_latency`
- `bench_driver_footprint`

**Run script**: `tests/benchmark/run_baselines.sh` (or `.py`) which runs all benchmarks and writes results to `tests/benchmark/results/M3-baseline.md`.

#### 5.4.1 Throughput benchmark

For each driver:

| Driver | Configuration | Measurement | Threshold |
|---|---|---|---|
| Serial | /tmp/sf_ttyV0, 115200 baud | Receiver bytes/sec sustained over 10s | ≥ 11 KB/s (≥ 95% of 115200/10 theoretical) |
| Serial | /tmp/sf_ttyV0, 921600 baud | Receiver bytes/sec sustained | ≥ 90 KB/s |
| TCP | localhost echo | Receiver bytes/sec sustained over 10s | ≥ 100 MB/s |
| UDP | localhost unicast | Receiver datagrams/sec (1KB each) | ≥ 50000 /s |

Each driver also records:
- Dropped frames (BackpressureDrop count)
- Average frame size actually delivered
- Peak watermark percentage reached

#### 5.4.2 Latency benchmark

For each driver:
- Producer sends 1 KB packet every 100ms for 100 seconds (1000 samples)
- Receiver timestamps on `frameReceived`
- Compute p50, p90, p99, p99.9, max

Thresholds (localhost / virtual serial):

| Driver | p99 | max |
|---|---|---|
| Serial 115200 | ≤ 50 ms | ≤ 100 ms |
| TCP localhost | ≤ 2 ms | ≤ 10 ms |
| UDP localhost | ≤ 1 ms | ≤ 5 ms |

ReplayDriver is excluded from latency benchmarks in M3 (skeleton has no timing behavior).

#### 5.4.3 Resource footprint

For each driver:
- Baseline RSS of idle process
- Construct driver → RSS delta
- open() → number of OS threads (expect +1)
- start() + idle 10s → RSS delta
- 100 cycles of open/close → RSS delta (leak indicator; expect < 1 MB growth)

Thresholds:

| Driver | Construct RSS Δ | 100-cycle RSS growth |
|---|---|---|
| Serial | ≤ 500 KB | ≤ 1 MB |
| TCP | ≤ 500 KB | ≤ 1 MB |
| UDP | ≤ 500 KB | ≤ 1 MB |
| Replay | ≤ 200 KB | ≤ 500 KB |

### 5.5 Infrastructure bottleneck exemption procedure

If a benchmark misses its threshold, CC investigates and categorizes the cause:

**Category 1 — CC code**: Driver logic, buffer management, signal handling. → Fix and retry.

**Category 2 — Qt framework**: QSerialPort overhead, QTcpSocket read buffer defaults, signal/slot queue latency. → Document the specific Qt call chain suspected; accept the measurement if the call chain is uncontestable. M10 may revisit via alternative Qt APIs or non-Qt IO.

**Category 3 — Linux kernel**: Socket buffer sizing, serial driver latency, scheduling jitter. → Document; accept.

**Category 4 — socat / echo server**: Test harness overhead. → Document; accept; consider alternate harness in M10 if critical.

**Category 5 — Host-specific**: AppProtection interference, cgroup limits, CPU frequency governor. → Document; accept if the benchmark runs are stable but below threshold.

**Decision rule**: If categories 2/3/4/5 dominate, record the measurement as-is and proceed. The `M3-baseline.md` file must include the category classification for any threshold miss.

**HALT**: If no clear category applies (measurement unstable, reasons unclear), HALT and ask the human. Do not guess.

### 5.6 Test execution under ASan

All unit tests and integration tests run under debug-asan preset. No leaks in TxStats/RxStats structures, no use-after-free in thread teardown. Benchmarks are NOT required to run under ASan (ASan overhead would distort timing).

### 5.7 CI matrix addition

The main `.github/workflows/ci.yml` gains:
- `apt-get install -y socat` (alongside existing libcurl4-openssl-dev)
- Integration tests that require socat are tagged so they run only when socat is available
- Benchmarks are NOT run in CI (manual/local only)

---

## 6. M3 Hand-off to M4

M4 (Frame Pipeline) begins immediately after M3 closes. M3 must hand off these assets:

1. **Concrete drivers that emit `frameReceived`** — M4 connects decoders to this signal.
2. **ReplayDriver skeleton** — M4 uses for pipeline end-to-end testing without real drivers.
3. **Performance baseline numbers** — M4 inherits the "no regression" expectation for its pipeline overhead.
4. **Connection Manager UI** — M4 optionally integrates decoded values into the preview UI (or leaves that for M5).

`.claude/M3-done.md` includes a "M4 hand-off" section explicitly covering these.

---

## 7. M3-specific HALT triggers

Beyond CLAUDE.md §HALT:

1. **Any attempted modification to an M2-frozen file** (public `.hpp` in src/drivers/driver_interface.hpp, src/frame/*.hpp, src/utils/*.hpp, src/platform/*.hpp). → HALT. If M2 freeze is wrong for M3's needs, that's an ADR-worthy discussion, not unilateral action.

2. **Performance benchmark misses threshold without clear category** (§5.5). → HALT.

3. **socat unavailable on build host at test time.** → HALT. (socat should be pre-installed per §5.7; if missing, CI config or host setup is off.)

4. **Integration test flakiness under stress.** Same stance as M2 §7-5: any intermittent test failure is a HALT, not a retry-until-green scenario.

5. **Connection Manager UI blocks the main thread for >200ms under any operation.** M3's quality goal is a responsive UI. Any discovered blocking call in the GUI thread is a HALT.

6. **Qt signal connection with direct-connection across threads (discovered via review).** This would indicate a thread-affinity violation. HALT.

7. **Driver destruction leaves zombie thread** (QThread::wait() returns false indicating the thread did not exit). → HALT. Driver destruction must be clean.

---

## 8. Acceptance criteria

### 8.1 Build and test

- [ ] Debug, Release, debug-asan all build clean, zero warnings
- [ ] All unit tests pass under all three presets
- [ ] All integration tests pass under Debug and Release (debug-asan for integration is nice-to-have, not required if local AppProtection blocks it — CI-authoritative only)
- [ ] Coverage thresholds per §5.1 met or documented with rationale
- [ ] socat-dependent tests are tagged and skip cleanly when socat is absent (must NOT fail the suite in that case; this is about portability)

### 8.2 Benchmarks

- [ ] All three benchmarks produce results files
- [ ] Results committed to `tests/benchmark/results/M3-baseline.md`
- [ ] Every threshold either passes or has category classification per §5.5
- [ ] No threshold missed with category "CC code" (else means unfixed bug)

### 8.3 UI preview

- [ ] Connection Manager dialog opens from MainWindow menu
- [ ] All four driver types selectable from dropdown
- [ ] Conditional form fields appear per driver type
- [ ] Connect button creates driver, opens successfully (test each driver type at least once manually — log in hand-off checklist)
- [ ] State badge reflects live driver state
- [ ] Frame log displays received frames in hex format
- [ ] Disconnect button releases driver cleanly; reconnect possible
- [ ] Changing driver type while connected is prevented (UI disables dropdown)

### 8.4 Documentation

- [ ] Every public class and method has Doxygen covering purpose, thread safety, preconditions
- [ ] `// TODO(M9):` markers are present at all ReplayDriver skeleton insertion points; each marker briefly describes what M9 must add
- [ ] Each error-taxonomy row (§4.8) has a corresponding unit test (verifiable via grep of test file names/content)

### 8.5 Hand-off

- [ ] `.claude/M3-done.md` includes:
   - All completion-report standard sections (timing, deliverables, acceptance self-check)
   - PR and merge state per `[EM §6.2]`
   - Benchmark results summary (inline or linked)
   - Error-injection coverage matrix (which scenarios tested, which deferred)
   - Known rough edges (e.g., "TCP reconnect is not implemented; driver stays in Error state until close()+open()")
   - Hand-off checklist for the human (manual UI tests to verify, real-hardware tests deferred)
   - M4 hand-off notes per §6
   - Impact analysis for driver contract (freeze-compatibility confirmation)

---

## 9. Notes for CC

- **M3 is implementation work, not design work.** The interfaces are frozen from M2. Your job is to make the concrete drivers satisfy the `DriverInterface` contract, not to redesign the contract.

- **User experience matters.** Error messages are user-facing. A DriverError with message `"Could not open /dev/ttyUSB0: Permission denied"` is good. A message `"QSerialPort::PermissionError (13)"` is bad. Spend time on these.

- **QThread lifecycle is a common bug source in Qt.** Pay careful attention to destruction order. The `IoWorkerBase` abstraction exists to centralize this pattern — use it consistently across drivers.

- **The performance benchmarks are not a race.** Thresholds are set to catch gross problems, not measure peak performance. A throughput 20% below theoretical ceiling is fine if ASan-clean and not wasteful; a throughput 50% below needs investigation.

- **Error injection tests are the hardest tests to write.** Budget time for them. Flakiness means the test is poorly designed, not that the code is wrong.

- **M3 is the first milestone where a real user could open the app and do something.** Connection Manager UI + a working Serial driver = interactive debugging session. This is a big step for the project. Take it seriously.

- **Do not anticipate M4 decoders.** Deliver raw bytes per driver contract. Decoded values are M4's domain.

---

## 10. Closing note

M3 is where SignalForge transitions from "foundation" to "functional". The driver contract defined in M2 becomes driver implementations; the UI scaffolding from M0/M1 becomes an interactive tool; the performance assumptions become measured baselines.

The quality philosophy for this milestone is explicit: **user experience over raw speed**. A driver that reports errors clearly, shuts down without hanging, and emits `frameReceived` on a sane thread schedule delivers more V1 value than one that maxes out throughput but leaks threads or produces cryptic error codes.

When in doubt, optimize for the person sitting at the keyboard debugging a real device. That person's day is better when drivers fail visibly, recover cleanly, and tell the truth via `DriverError::message`.
