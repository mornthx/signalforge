# M10 — Session Writer

| Field | Value |
|---|---|
| Milestone ID | M10 |
| Sprint | 10 |
| Estimated effort | 5-7 person-days |
| Prerequisites | M9 closed (main at v0.0.10-alpha.1 or later) |
| Next milestone | M11 (Replay UX, builds on M9 ReplayDriver + M10 file format) |
| Hard-stop type | **Interface freeze** (`SessionWriter` API + SFREPLAY v1 file format spec) + **Format equivalence** (M10 writes a file that M9 ReplayDriver reads back identically) |
| Soft-HALT allowed | **No** |
| Branch | `milestone/M10` |

**Cross-reference notation**:

- `[EM §N]` — Execution Manual, section N
- `[Arch §N]` — Architecture document, section N
- `[MR]` — Milestone Roadmap
- `[CM §X]` — CLAUDE.md, section X
- `[ADR-N]` — Architecture Decision Record N
- `[M<n> §N]` — M<n> spec
- `[M9-done §X]` — M9-done.md section X (for SFREPLAY format hand-off)

---

## 1. Goal

Record signals to disk in real-time during user-initiated recording sessions. Files persist across app restarts and are replayable via M9's ReplayDriver (which already consumes the SFREPLAY format).

M9 implicitly defined the V1 replay file format when completing the ReplayDriver. M10 produces files in this format. The format is **frozen at M10 close** (the file format spec moves from M9-done.md hand-off into M10 spec §4).

After M10 the user can:

1. Connect to a device (M9)
2. Start recording (M10) — toolbar button starts a new session file
3. Watch signals flow into charts (M8) AND get written to disk (M10)
4. Stop recording — file is closed
5. Quit the app
6. Open the recorded file in a future session via Replay (M11) or replay it directly via the ReplayDriver (M9 + M10 format)

This milestone freezes:

1. The `SessionWriter` C++ API
2. The `SFREPLAY v1` file format specification

Quality philosophy from previous milestones: **format stability > optimization**. A frozen v1 file format must be readable by V2 and beyond. M10 specifies the format precisely; future versions extend additively without breaking v1 readers.

---

## 2. Scope

### 2.1 Must deliver

1. **`SessionWriter`** at `src/session/session_writer.{hpp,cpp}`:
   - QObject; lives on the main thread
   - Owns a worker QThread that does actual disk I/O
   - Provides `start(filePath)` / `stop()` / `isRecording()` API
   - Subscribes to `SignalBufferRegistry` to receive signal events
   - Lock-free or low-contention queue from main thread → worker thread
   - Writes SFREPLAY v1 format
   - Includes signal metadata + decoder schema reference in file header
   - Emits Qt signals for state changes + errors

2. **`SessionFileWriter`** at `src/session/session_file_writer.{hpp,cpp}`:
   - Internal class running on the worker QThread
   - Receives signal events via thread-safe queue
   - Writes binary records to file
   - Periodic flush (configurable, default 1 second)
   - Atomic finalization (write footer + close on stop)

3. **SFREPLAY v1 file format spec** at `docs/format/sfreplay-v1.md`:
   - Frozen format definition (header + signals catalog + records + footer)
   - Endianness, integer sizes, field meanings
   - Forward-compatibility rules (V2 can extend, V1 readers ignore extensions)

4. **`SessionMetadata`** struct (frozen at M10 close):
   - Recording start/end timestamps
   - Source connection info (driver type + display name)
   - Decoder schema reference (M5 schema ID)
   - Signal catalog (list of `SignalMetadata` snapshots at recording start)
   - Optional user-supplied annotations (description, tags)

5. **MainWindow integration**:
   - Toolbar button: "Record" (toggle: not recording / recording with red indicator)
   - On Record: opens save-file dialog → starts SessionWriter
   - Status bar shows recording state + file size
   - On Stop or app quit: gracefully closes file

6. **Recording lifecycle Qt signals**:
   - `SessionWriter::recordingStarted(QString filePath)`
   - `SessionWriter::recordingStopped(QString filePath, std::size_t bytesWritten)`
   - `SessionWriter::errorOccurred(QString errorMessage)`
   - `SessionWriter::flushed(std::size_t bytesFlushed)` — for diagnostics

7. **File rotation safety**:
   - On disk space exhaustion: log ERROR, stop recording gracefully, file remains valid up to last flush
   - On write error: log ERROR, attempt to close file cleanly, transition to Error state
   - On worker thread crash: detect via supervisor; log ERROR; recording state set to Idle

8. **All-signals recording** (per decision M10.4 R):
   - Records every signal currently in `SignalBufferRegistry`
   - V1.5+ may add user-selected signals filter

9. **Metadata in header** (per decision M10.5 U):
   - Signal catalog with full `SignalMetadata` (id, name, unit, type, description, scale, offset)
   - Decoder schema ID reference (allows M11 Replay to re-decode)
   - Connection info is **NOT** included (decision M10.5 U: signals + schema only, no connection config)

10. **Backpressure handling**:
    - Main thread → worker queue is bounded (default 10000 signal events)
    - On queue full: log WARN, drop oldest events (not the recent ones — recent data is what user just saw)
    - Increment `session_writer_dropped_events` counter
    - This is a fail-safe; in normal operation queue should never fill (worker writes 100k+ events/sec to disk)

11. **Worker thread lifecycle**:
    - Started on `SessionWriter::start()` in QThread
    - Stopped on `SessionWriter::stop()` via cooperative shutdown (signal, drain, exit)
    - Joined before SessionWriter destructor returns
    - Per-thread `QObject::moveToThread` pattern

12. **Integration tests** at `tests/integration/`:
    - `test_session_writer_basic_lifecycle.cpp` — start, push 1000 signal events, stop; verify file readable
    - `test_session_writer_replay_round_trip.cpp` — write file, read back via ReplayDriver, verify signal events match
    - `test_session_writer_metadata.cpp` — verify SignalMetadata in header matches registered signals
    - `test_session_writer_disk_full.cpp` — simulate disk full; verify graceful error
    - `test_session_writer_concurrent_access.cpp` — start recording, charts read concurrently, no contention
    - `test_session_writer_threading.cpp` — verify worker thread isolation; ASan/TSan clean
    - `test_session_writer_long_session.cpp` — 10 minute × 1kHz × 60 signals; verify file size + integrity

13. **Unit tests** ≥ 80% coverage on session modules

14. **Benchmark** at `tests/benchmark/bench_session_writer.cpp`:
    - 10 second × 1kHz × 60 signals (600,000 events) sustained recording
    - Target: queue never blocks main thread; worker keeps up
    - Disk write throughput: ≥ 500k events/sec (disk-bound, but should be far above)
    - Results to `tests/benchmark/results/M10-baseline.md`

15. **CLI tool** `sfreplay_inspect` at `tools/sfreplay_inspect/`:
    - Mirrors M5's `schema_lint` and M7's `expr_lint`
    - Inspects an SFREPLAY file: shows header, signal catalog, record count, time range
    - `--json` flag for machine-readable output
    - No modification to file (read-only inspector)

16. **Doxygen** on all public declarations

17. **`.claude/M10-done.md`** with standard completion report + freeze record

### 2.2 Must not do

1. **No modifications to M2/M3/M4/M5/M6/M7/M8/M9 frozen `.hpp`**.
2. **No replay UX**. M11 territory. M10 only writes; ReplayDriver (from M9) reads.
3. **No automatic recording on connect**. User-initiated only (decision M10.3 P).
4. **No selective signal recording**. All signals in registry recorded (decision M10.4 R).
5. **No connection config in file**. Format intentionally minimal (decision M10.5 U).
6. **No compression**. V1 raw binary. V1.5+ may add gzip/lz4 wrapper around the file.
7. **No encryption**. V1.5+ if needed. V1 plain binary.
8. **No live streaming protocol**. Files are local-only. Network sync is V2.
9. **No new top-level dependencies**. Use Qt + std + existing in-tree modules.
10. **No QML for recording UI**. Pure C++ Qt Widgets.
11. **No automatic rollover** (e.g., split file every N MB). V1 single-file-per-recording. V1.5+ may add.
12. **No concurrent multi-recorder**. V1 supports one active SessionWriter at a time. V1.5+ may add multi-recording.
13. **No edit-during-record**. File is append-only during recording. Cannot delete signals or modify metadata mid-stream.

---

## 3. Design Decisions (locked by this spec)

### 3.1 SFREPLAY v1 binary format (decision M10.1 Option A)

M9 ReplayDriver already defined a 16-byte header beginning with magic `"SFREPLAY"`, followed by length-prefixed records. M10 writes this format with full v1 specification.

**Rationale**:
- Avoid V1 internal format fragmentation
- Binary format is compact (millions of events per session)
- Custom format has zero dependencies (vs HDF5 / Parquet libraries)
- Simple readers can be written in any language

**Trade-off accepted**: not human-readable. Mitigated by `sfreplay_inspect` CLI tool.

### 3.2 Independent write thread (decision M10.2 Option Y)

`SessionWriter` (main thread) → bounded queue → `SessionFileWriter` (worker thread) → disk.

**Rationale**:
- Disk I/O latency must not block main thread (UI responsiveness)
- Per arch §"Session 写线程" — already planned
- Bounded queue prevents memory blowup if disk slows

**Implementation**: `QThread` + `moveToThread` pattern; `QQueue<SessionEvent>` with `QMutex`.

### 3.3 User-initiated recording (decision M10.3 Option P)

Toolbar button: "Record" → save-file dialog → `SessionWriter::start(path)`.
"Stop" button (same toolbar position, toggle) → `SessionWriter::stop()`.

**Rationale**:
- User control: avoids surprise disk fill
- Simple mental model: explicit start/stop
- Auto-on-connect (Option Q) was considered but creates surprise behavior

V1.5+ may add: configurable auto-record-on-connect with disk space check.

### 3.4 All signals recorded (decision M10.4 Option R)

Recording captures every signal in `SignalBufferRegistry` at recording time. New signals registered mid-recording are added to the file's signal catalog and start being recorded immediately.

**Rationale**:
- Simple: no per-signal selection UI
- Complete: replay can show anything that was happening
- V1.5+ may add user-selected filter for recording specific signals

**Implementation note**: `SessionWriter` registers as a `SignalValueSink` (M5 frozen interface) on the registry. New signals trigger `onSignalsRegistered` callback; M10 writes them to the file's catalog and proceeds.

### 3.5 Signals + schema metadata (decision M10.5 Option U)

File header includes:
- Recording start time
- Signal catalog (list of `SignalMetadata` at start)
- Decoder schema ID (M5 schema reference)
- Optional user description

Does NOT include:
- Connection config (driver type, port, baud rate, etc.)
- Pipeline config (decoder routing details)
- Application config (chart layouts, etc.)

**Rationale**:
- Minimal format → fewer compatibility issues V1 → V2
- Replay needs signals + schema (to re-decode), not connection info
- Connection details are not "signal data" — separating concerns

### 3.6 Format frozen at M10 close (not M9 close)

Although M9 ReplayDriver reads SFREPLAY format, the **format spec itself** is frozen at M10. M9-done.md has a hand-off note describing the format informally; M10 produces the canonical spec at `docs/format/sfreplay-v1.md`.

**Rationale**:
- M9 implementation has the format only as code (replay_driver.cpp)
- M10 is where the format becomes a contract: writers must produce it, readers must consume it
- Documentation centralization matters for future implementers (V1.5+ replay UX, V2 file conversions)

### 3.7 No soft-HALT (inherits M2-M9)

### 3.8 Metric naming

- `session_writer_state` (gauge, 0=Idle, 1=Recording, 2=Error)
- `session_writer_events_recorded_total` (counter)
- `session_writer_bytes_written_total` (counter)
- `session_writer_dropped_events_total` (counter): backpressure overflow
- `session_writer_disk_errors_total` (counter)
- `session_writer_queue_depth` (gauge)
- `session_writer_flush_latency_ms` (gauge)

---

## 4. Key Implementation Details

### 4.1 SFREPLAY v1 binary format

Place format spec at `docs/format/sfreplay-v1.md`.

```
File Layout:
+------------------+
| Header (variable)|
+------------------+
| Signal Catalog   |
+------------------+
| Records (stream) |
+------------------+
| Footer (16 bytes)|
+------------------+
```

#### Header (variable size, starts at offset 0)

```
Offset  Size  Field          Description
0       8     magic          ASCII "SFREPLAY" (8 bytes, no null)
8       4     formatVersion  uint32_le, currently 1
12      4     headerLen      uint32_le, total header bytes (incl. catalog)
16      8     recordedAt     int64_le, ns since Unix epoch (recording start)
24      4     descLen        uint32_le, length of optional description string
28      N     description    UTF-8 bytes, length descLen (no null terminator)
28+N    4     schemaIdLen    uint32_le, length of decoder schema ID
32+N    M     schemaId       UTF-8 bytes, length schemaIdLen
32+N+M  4     signalCount    uint32_le, number of signals in catalog
```

#### Signal Catalog (variable size, follows header)

Per signal, a frozen-shape SignalMetadata snapshot:

```
Offset    Size  Field           Description
0         4     signalIdLen     uint32_le
4         N1    signalId        UTF-8 bytes
4+N1      4     nameLen         uint32_le
4+N1+4    N2    name            UTF-8 bytes
4+N1+4+N2 4     unitLen         uint32_le
...                              (similar pattern for: name, unit, description, type, scale, offset)
```

Type encoding: 1 byte enum
- 0: bool
- 1: int64
- 2: double
- 3: string

scale, offset: 8-byte little-endian doubles

The catalog is **growable**: if new signals register mid-recording, they're appended to the catalog at the boundary between records and the next batch (a special "catalog extension" record type — see below).

#### Records (stream of typed records)

Each record:

```
Offset  Size  Field          Description
0       4     recordType     uint32_le, type tag
4       4     payloadLen     uint32_le, byte length of payload
8       N     payload        N bytes
```

Record types:

```
Type 1: Signal Value
  payload: { uint32_le signalIdx, int64_le timestampNs, value (typed) }
  - Bool: 1 byte (0 or 1)
  - Int64: 8 bytes int64_le
  - Double: 8 bytes double_le
  - String: 4 bytes uint32_le strLen + strLen bytes UTF-8

Type 2: Catalog Extension
  payload: signalCount + N more SignalMetadata entries (same shape as initial catalog)

Type 3: Marker
  payload: 8 bytes int64_le timestampNs + UTF-8 description
  Used for user annotations (V1.5+); V1 may emit but readers can ignore.

Type 4: Heartbeat
  payload: empty
  Written every 10 seconds during recording. Allows readers to detect truncated files.
```

#### Footer (16 bytes, last 16 bytes of file)

```
Offset  Size  Field            Description
0       8     magic            ASCII "REPLAYEOF"
8       4     totalRecords     uint32_le, count of all records (any type)
12      4     reserved         must be 0 (V2 may use)
```

A file without the footer is "incomplete" — recording was interrupted before clean stop. M9 ReplayDriver tolerates this (plays whatever records are present).

#### Format compatibility rules

- V2 readers may extend recordType > 4; V1 readers ignore unknown types
- V2 may add new fields to header (after `signalCount`) gated by `headerLen > 32+N+M`; V1 readers stop at `signalCount`
- V2 may extend signal catalog entries (additional fields at end); V1 readers ignore extra bytes per signal (use entry length from declared sizes)
- V1 footer magic and size are immutable; V2 may use the reserved 4 bytes additively

### 4.2 `SessionWriter` class

Place at `src/session/session_writer.hpp`.

```cpp
// src/session/session_writer.hpp
#pragma once

#include "buffer/signal_buffer_registry.hpp"
#include "decoder/decoder_interface.hpp"  // For SignalValue, SignalMetadata, SignalValueSink

#include <QObject>
#include <QString>
#include <QThread>
#include <chrono>
#include <memory>

namespace signalforge::session {

class SessionFileWriter;  // Forward; internal worker class

/// Recording state.
enum class RecordingState {
    Idle,
    Recording,
    Error,
};

/// Public API for recording sessions.
///
/// Lives on main thread. Owns a worker QThread that does disk I/O.
/// All signal events flow main → worker via lock-free queue.
///
/// Threading: caller (main thread) invokes start/stop. Internal worker
/// runs on a dedicated QThread. Lock-free queue between them.
///
/// Freeze scope: this class is frozen at M10 close.
class SessionWriter : public QObject, public signalforge::decoder::SignalValueSink {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SessionWriter)

public:
    explicit SessionWriter(
        signalforge::buffer::SignalBufferRegistry& registry,
        QObject* parent = nullptr);
    ~SessionWriter() override;

    /// Begin recording to filePath. Creates file (overwrites if exists).
    /// Returns false if already recording or filePath invalid.
    /// Optionally include user description and decoder schema ID.
    [[nodiscard]] bool start(const QString& filePath,
                              const QString& description = {},
                              const QString& decoderSchemaId = {});

    /// Stop recording. Flushes worker, closes file, joins worker thread.
    /// Returns total bytes written.
    [[nodiscard]] std::size_t stop();

    /// Currently recording?
    [[nodiscard]] bool isRecording() const noexcept;

    /// Current state.
    [[nodiscard]] RecordingState state() const noexcept;

    /// Total events recorded since start.
    [[nodiscard]] std::size_t eventsRecorded() const noexcept;

    /// Total bytes written to file (cumulative).
    [[nodiscard]] std::size_t bytesWritten() const noexcept;

    /// Total events dropped due to queue backpressure.
    [[nodiscard]] std::size_t droppedEvents() const noexcept;

    // SignalValueSink overrides — forward signal events to worker

    void onSignal(std::chrono::steady_clock::time_point timestamp,
                  const QString& signalId,
                  const signalforge::decoder::SignalValue& value) override;

    void onSignalsRegistered(
        const QString& driverId,
        const std::vector<signalforge::decoder::SignalMetadata>& signalsList) override;

    void onSignalsUnregistered(const QString& driverId) override;

signals:
    void recordingStarted(const QString& filePath);
    void recordingStopped(const QString& filePath, std::size_t bytesWritten);
    void errorOccurred(const QString& errorMessage);
    void flushed(std::size_t bytesFlushed);

private:
    signalforge::buffer::SignalBufferRegistry* registry_;
    std::unique_ptr<SessionFileWriter> fileWriter_;
    std::unique_ptr<QThread> workerThread_;
    RecordingState state_ = RecordingState::Idle;
    
    QString currentFilePath_;
    std::atomic<std::size_t> eventsRecorded_{0};
    std::atomic<std::size_t> bytesWritten_{0};
    std::atomic<std::size_t> droppedEvents_{0};
};

}  // namespace signalforge::session
```

### 4.3 `SessionFileWriter` class

Place at `src/session/session_file_writer.hpp`.

```cpp
// src/session/session_file_writer.hpp
#pragma once

#include "decoder/decoder_interface.hpp"

#include <QObject>
#include <QString>
#include <QFile>
#include <QQueue>
#include <QMutex>
#include <chrono>
#include <variant>

namespace signalforge::session {

/// Internal queue event types.
struct WriteSignalEvent {
    std::chrono::steady_clock::time_point timestamp;
    QString signalId;
    signalforge::decoder::SignalValue value;
};

struct CatalogExtensionEvent {
    QString driverId;
    std::vector<signalforge::decoder::SignalMetadata> newSignals;
};

struct StopEvent {
    // Sentinel; tells worker to flush + close + exit
};

using SessionEvent = std::variant<WriteSignalEvent, CatalogExtensionEvent, StopEvent>;

/// Internal worker class. Lives on a dedicated QThread.
/// Receives events from SessionWriter via thread-safe queue, writes to file.
///
/// Not part of M10 freeze surface (internal implementation detail).
class SessionFileWriter : public QObject {
    Q_OBJECT

public:
    explicit SessionFileWriter(QObject* parent = nullptr);
    ~SessionFileWriter() override;

    /// Open file for writing. Writes header + initial catalog.
    [[nodiscard]] bool openFile(
        const QString& filePath,
        const QString& description,
        const QString& decoderSchemaId,
        const std::vector<signalforge::decoder::SignalMetadata>& initialCatalog);

    /// Enqueue an event for writing. Thread-safe; called from main thread.
    /// Returns false if queue is full (backpressure).
    bool enqueue(SessionEvent event);

    /// Flush queue to disk + close file. Called via Qt::QueuedConnection
    /// from SessionWriter::stop() to run on worker thread.
    void flushAndClose();

    /// Current file size.
    [[nodiscard]] std::size_t bytesWritten() const noexcept;

signals:
    void error(const QString& errorMessage);
    void flushed(std::size_t bytesFlushed);

public slots:
    /// Worker thread entry point. Processes queue until StopEvent.
    void processQueue();

private:
    // ... internal write logic
    QFile file_;
    QQueue<SessionEvent> queue_;
    QMutex queueMutex_;
    std::atomic<std::size_t> bytesWritten_{0};
    std::vector<signalforge::decoder::SignalMetadata> currentCatalog_;
    std::unordered_map<QString, std::uint32_t> signalIdToIndex_;  // For record encoding
    
    static constexpr std::size_t kQueueCapacity = 10000;
    static constexpr std::chrono::seconds kFlushInterval{1};
};

}  // namespace signalforge::session
```

### 4.4 Worker thread lifecycle

Standard Qt pattern:

```cpp
// In SessionWriter::start()
fileWriter_ = std::make_unique<SessionFileWriter>();
if (!fileWriter_->openFile(filePath, description, schemaId, initialCatalog)) {
    return false;
}

workerThread_ = std::make_unique<QThread>();
fileWriter_->moveToThread(workerThread_.get());

connect(workerThread_.get(), &QThread::started, 
        fileWriter_.get(), &SessionFileWriter::processQueue);
connect(fileWriter_.get(), &SessionFileWriter::error, 
        this, &SessionWriter::errorOccurred);

workerThread_->start();
state_ = RecordingState::Recording;
emit recordingStarted(filePath);
return true;

// In SessionWriter::stop()
fileWriter_->enqueue(StopEvent{});  // Sentinel
workerThread_->quit();
workerThread_->wait();
auto bytes = fileWriter_->bytesWritten();
emit recordingStopped(currentFilePath_, bytes);

state_ = RecordingState::Idle;
fileWriter_.reset();
workerThread_.reset();
return bytes;
```

### 4.5 Record encoding

Worker thread writes records with little-endian encoding:

```cpp
void SessionFileWriter::writeSignalRecord(const WriteSignalEvent& evt) {
    // Lookup signal index
    auto it = signalIdToIndex_.find(evt.signalId);
    if (it == signalIdToIndex_.end()) {
        SF_LOG_WARN("Signal {} not in catalog; skipping record", evt.signalId);
        return;
    }
    
    auto signalIdx = it->second;
    auto timestampNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        evt.timestamp.time_since_epoch()).count();
    
    // Build payload
    QByteArray payload;
    payload.reserve(64);  // Approximate
    appendLe(payload, signalIdx);    // 4 bytes
    appendLe(payload, timestampNs);  // 8 bytes
    
    // Type-specific value encoding
    std::visit([&payload](auto&& val) {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, bool>) {
            payload.append(val ? '\x01' : '\x00');
        } else if constexpr (std::is_same_v<T, std::int64_t>) {
            appendLe(payload, val);
        } else if constexpr (std::is_same_v<T, double>) {
            appendLe(payload, val);
        } else if constexpr (std::is_same_v<T, QString>) {
            auto utf8 = val.toUtf8();
            appendLe(payload, static_cast<std::uint32_t>(utf8.size()));
            payload.append(utf8);
        }
    }, evt.value);
    
    // Write record header + payload
    QByteArray recordHeader;
    appendLe(recordHeader, std::uint32_t(1));   // recordType: SignalValue
    appendLe(recordHeader, std::uint32_t(payload.size()));
    
    file_.write(recordHeader);
    file_.write(payload);
    bytesWritten_ += recordHeader.size() + payload.size();
}
```

### 4.6 Backpressure and queue overflow

```cpp
bool SessionFileWriter::enqueue(SessionEvent event) {
    QMutexLocker locker(&queueMutex_);
    if (queue_.size() >= kQueueCapacity) {
        // Drop oldest (FIFO retention of recent data)
        if (!queue_.isEmpty() && std::holds_alternative<WriteSignalEvent>(queue_.head())) {
            queue_.dequeue();
            // signal counter updated by SessionWriter on the way out
        } else {
            // Queue is full of important events (catalog extensions, stops)
            // Drop the new event instead
            return false;
        }
    }
    queue_.enqueue(std::move(event));
    return true;
}
```

The drop-oldest strategy preserves recent events (what user just saw on chart) over historic events. For 1kHz × 60 signals = 60k events/sec, queue 10000 fills in < 200ms — should never happen in practice unless disk is severely constrained.

### 4.7 MainWindow integration

Add to MainWindow:
- Toolbar button "Record" (toggle)
- Click → `QFileDialog::getSaveFileName` + `SessionWriter::start(path)`
- During recording: button shows red dot indicator + "Stop" text
- Status bar shows file size (updates every 1s on `flushed` signal)
- On app close while recording: prompt user "Stop recording first?" or auto-stop

### 4.8 `sfreplay_inspect` CLI tool

Place at `tools/sfreplay_inspect/`. Mirrors M5 / M7 lint patterns.

```
$ sfreplay_inspect session_2026-05-08_14-30.sfreplay
File: session_2026-05-08_14-30.sfreplay (12,488,231 bytes)
Format: SFREPLAY v1
Recorded: 2026-05-08 14:30:15 UTC
Duration: 5m 23s
Description: "Dev board burn-in test"
Decoder schema: dev-board-frame-v1

Signal catalog (60 signals):
  voltage (V) [Double]
  current (A) [Double]
  temperature (°C) [Double]
  ...

Total records: 1,245,678
  Type 1 (Signal Value): 1,245,632
  Type 2 (Catalog Extension): 0
  Type 3 (Marker): 0
  Type 4 (Heartbeat): 32

Footer: present (file complete)

$ sfreplay_inspect session.sfreplay --json
{
  "file": "session.sfreplay",
  "size": 12488231,
  "format_version": 1,
  ...
}
```

---

## 5. Performance gates

### 5.1 Throughput

| Metric | Target | HALT |
|---|---|---|
| Sustained recording rate (60 signals × 1kHz × 10 sec) | ≥ 60k events/sec | < 30k events/sec |
| Worker queue never blocks main thread | yes | main thread blocked > 5ms |
| Worker thread CPU | < 30% of one core | > 80% of one core |
| File size for 10s × 60sig × 1kHz | ~30MB | > 60MB (compression issue) |

### 5.2 File integrity

| Metric | Target | HALT |
|---|---|---|
| M9 ReplayDriver reads M10 file 100% identically | yes | even one event mismatch |
| File without footer (interrupted) | readable up to last record | unreadable |
| Multiple simultaneous readers (M11 future) | yes | locked file |

### 5.3 Latency

| Metric | Target | HALT |
|---|---|---|
| Time from `start()` to `recordingStarted` signal | < 100ms | > 500ms |
| Time from `stop()` to file closed | < 1s (drain queue) | > 5s |
| Backpressure drop on disk slowdown | log warning, continue | crash or block main thread |

### 5.4 Memory

| Metric | Target | HALT |
|---|---|---|
| Worker queue size in steady state | < 100 events | sustained > 5000 |
| Total memory per recording (excluding file) | < 50MB | > 200MB |

---

## 6. Freeze protocol

### 6.1 What freezes at M10 close

**C++ interfaces**:
- `src/session/session_writer.hpp`: `SessionWriter` class, `RecordingState` enum
- `src/session/session_metadata.hpp`: `SessionMetadata` struct (frozen as the in-memory representation of file header)

**File format**:
- `docs/format/sfreplay-v1.md`: complete v1 binary format specification
- Magic bytes "SFREPLAY" + version 1 = inviolable contract

Once frozen, modifications require new ADR.

### 6.2 What does NOT freeze

- Internal `SessionFileWriter` implementation (worker, queue strategies)
- Default values (queue capacity, flush interval, file path conventions)
- `sfreplay_inspect` CLI tool internals
- V1.5+ may add: V2 format extensions (additive, V1 readers ignore)

### 6.3 Freeze record format

`.claude/M10-done.md` lists sha256 of:
- `src/session/session_writer.hpp`
- `src/session/session_metadata.hpp`
- `docs/format/sfreplay-v1.md`

---

## 7. M10-specific HALT triggers

Beyond CLAUDE.md §HALT:

1. **Modification to M2-M9 frozen `.hpp`** → HALT
2. **M9 ReplayDriver cannot read M10-written file identically** (round-trip mismatch) → HALT (format integration broken)
3. **Worker thread blocks main thread > 5ms** at any time → HALT (threading wrong)
4. **Worker can't keep up with 1kHz × 60 signals after one optimization pass** → HALT (architectural)
5. **File written with footer but ReplayDriver reads incomplete events** → HALT (write ordering bug)
6. **Memory leak in 30-min recording** (>10% growth) → HALT
7. **Backpressure drops events but doesn't increment counter** → HALT (silent data loss)

---

## 8. Acceptance criteria

### 8.1 Build and test

- [ ] Debug, Release, debug-asan all build clean under C++23
- [ ] All unit + integration tests pass under all three presets
- [ ] Coverage ≥ 80% per §2.1-13
- [ ] CI green on milestone/M10 head

### 8.2 Performance (per §5)

- [ ] Sustained 60k events/sec for 10 seconds
- [ ] Main thread never blocks > 5ms
- [ ] 30-min recording memory growth < 10%
- [ ] File size matches expected (30MB ± 10% for 10s × 60sig × 1kHz)

### 8.3 Format correctness

- [ ] M9 ReplayDriver round-trip: write file in M10, read in M9, verify all events match
- [ ] `sfreplay_inspect` correctly parses all written files
- [ ] Truncated file (no footer) readable up to last complete record
- [ ] Multiple readers can open same file simultaneously (read-only)

### 8.4 Lifecycle correctness

- [ ] Start → Stop → Start cycle works
- [ ] Worker thread joins before SessionWriter destructor returns
- [ ] App close during recording: file closed gracefully
- [ ] Disk full simulation: error logged, recording stopped, file remains valid

### 8.5 Threading safety

- [ ] ASan clean
- [ ] TSan clean (or local-only documented)
- [ ] No data race between writer / chart / expression engine accessing registry simultaneously

### 8.6 Freeze record

- [ ] M10-done.md has Freezes section per §6.3
- [ ] Sha256s recorded for 3 files (2 hpp + 1 format spec)
- [ ] No modifications to M2-M9 frozen files

### 8.7 Hand-off

- [ ] M10-done.md hand-off section covers:
  - For M11 Replay UX: SFREPLAY v1 format spec + `sfreplay_inspect` available; M11 reads files via ReplayDriver
  - For M12 Performance: session writer is bounded by disk I/O; possible future optimization area
  - For M13 Packaging: ensure file format spec ships with docs

---

## 9. Notes for CC

- **Format spec authoring is critical**. Spec at `docs/format/sfreplay-v1.md` is V1's first frozen file format. Errors here become permanent. Test with multiple readers (M9 + new M11 + future hand-rolled python reader).

- **Round-trip test is the acceptance gate**. Don't trust that "writer writes 100 events" = "reader reads 100 events". Build the test first; iterate the writer until it passes.

- **Worker thread lifecycle is fragile**. Use `moveToThread` + `Qt::QueuedConnection` carefully. Test with multiple start/stop cycles. ASan + TSan if available.

- **Backpressure is a fail-safe, not a feature**. Normal operation should never see queue overflow. If tests routinely show overflow at 1kHz × 60 signals, the worker is too slow — investigate.

- **`sfreplay_inspect` doubles as a debug tool**. Run it on every test-generated file to verify format compliance.

- **No premature optimization**. V1 plain binary format. Compression is V1.5+. Encryption is V1.5+. Network sync is V2.

- **Don't modify M9 ReplayDriver**. M9 is frozen. M10's write side must produce files that M9's reader handles. If the reader has bugs, file an M9 hotfix (parallel to M6 ADR-005 pattern), not an M10 spec amendment.

- **Document V2 forward-compatibility carefully**. The format extensibility rules in §4.1 are critical — V2 must be able to add features without breaking V1 readers. Examples in spec help future implementers.

---

## 10. Closing note

M10 makes SignalForge sessions **persistent**. Combined with M9 (Connection Manager), M11 (Replay UX), the user workflow becomes:

1. Connect device (M9)
2. **Record session (M10)** — file on disk
3. Quit
4. **Re-open file (M11)** in another session — replay through charts (M8)
5. Compare with live data (M9 reconnect)

The frozen file format is the inflection point. After M10, V1 has a permanent contract about what session files look like — a contract that V2, V3, etc. must honor (with extensions, never breaks).

Quality discipline:
- Format spec must be **complete** (no "TBD" sections)
- Round-trip test must be the **acceptance gate** for every commit touching the writer
- Threading must be **provably safe** (ASan/TSan clean)
- Backpressure must be **transparent** (counter exposed, log message clear)

When in doubt about format choices (record type values, field widths, endianness), choose the **most predictable / least surprising** option. Future implementers will thank you.

Performance is bounded by disk I/O. Worst-case 1kHz × 60 signals × ~50 bytes = 3MB/sec — fast for any modern disk. Optimization is V1.5+ if real workloads exceed this.
