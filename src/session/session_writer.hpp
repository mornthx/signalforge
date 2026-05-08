// src/session/session_writer.hpp
#pragma once

#include "buffer/signal_buffer_registry.hpp"
#include "decode/decoder_interface.hpp"
#include "session/session_metadata.hpp"

#include <QObject>
#include <QString>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>

class QThread;

namespace signalforge::session {

class SessionFileWriter;  // Forward declaration; internal worker class.

/// Public API for recording sessions to disk in SFREPLAY v1 format.
///
/// Lives on the main thread. Owns a worker `QThread` that does
/// disk I/O. All signal events flow main-thread → worker via a
/// bounded queue; the main thread is never blocked by disk
/// latency (per spec §3.2 + §5.1). Backpressure on a full queue
/// is implemented per ADR-007 / `M10-concerns.md` C3:
///
///   1. New droppable event + queue full → drop the new event.
///   2. New non-droppable event + queue full → drop the oldest
///      droppable event in the queue first.
///   3. Queue full of non-droppable events → block on enqueue
///      with a 10 ms timeout.
///   4. Timeout exceeded → log ERROR, return false from enqueue,
///      transition to `RecordingState::Error`.
///
/// This class registers itself as a `SignalValueSink` on the
/// supplied `SignalBufferRegistry` for the duration of a recording,
/// ensuring it observes every signal the registry receives. Per
/// spec §3.4, V1 records all signals — there is no per-signal
/// filter. New signals registered mid-recording are added to the
/// file's catalog via Catalog Extension records and immediately
/// recorded thereafter.
///
/// Format hand-off (per ADR-007): the file format frozen at M10
/// close is `docs/format/sfreplay-v1.md`. Round-trip integration
/// testing uses the new `signalforge::session::SessionReader`
/// class (M10 S6), not the M9 `signalforge::drivers::ReplayDriver`
/// (which retains scope as a raw-frame log replayer).
///
/// Threading: caller (main thread) invokes `start` / `stop` /
/// state queries. Internal worker runs on a dedicated `QThread`.
/// All cross-thread communication uses lock-free counters or
/// `Qt::QueuedConnection` for events. The destructor blocks until
/// the worker thread joins.
///
/// Freeze scope: this class (its public API + signals) is frozen
/// at M10 close. Modifications post-freeze require a new ADR.
class SessionWriter : public QObject, public signalforge::decoder::SignalValueSink {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SessionWriter)

public:
    /// Construct attached to a `SignalBufferRegistry`. The writer
    /// will register itself as a sink for the duration of any
    /// active recording (i.e., between `start()` returning true and
    /// `stop()` being called).
    explicit SessionWriter(signalforge::buffer::SignalBufferRegistry& registry, QObject* parent = nullptr);

    /// Joins the worker thread (blocking) if a recording is in
    /// flight. Calling `stop()` explicitly before destruction is
    /// recommended so the caller gets the byte count.
    ~SessionWriter() override;

    /// Begin recording to `filePath`. Creates the file (truncates
    /// if it already exists). Writes the SFREPLAY v1 header +
    /// initial signal catalog before returning.
    ///
    /// `description` is an optional human-readable annotation
    /// embedded in the file header. `decoderSchemaId` is the M5
    /// schema reference; an empty string is valid and means "no
    /// schema known to the writer".
    ///
    /// Returns false if a recording is already in flight, the path
    /// is invalid, or the file open / header write fails. On a
    /// false return the writer's `state()` is unchanged from
    /// pre-call (`Idle` if no recording was active; otherwise the
    /// existing `Recording` is preserved).
    [[nodiscard]] bool start(const QString& filePath, const QString& description = {},
                             const QString& decoderSchemaId = {});

    /// Stop recording. Enqueues a stop sentinel, waits for the
    /// worker to drain the queue + write the footer + close the
    /// file, and joins the worker thread. Returns the total bytes
    /// written. Safe to call from `Idle` (no-op, returns 0).
    [[nodiscard]] std::size_t stop();

    /// Currently recording? Equivalent to
    /// `state() == RecordingState::Recording`.
    [[nodiscard]] bool isRecording() const noexcept;

    /// Current recording state.
    [[nodiscard]] RecordingState state() const noexcept;

    /// Path of the in-flight recording (or last completed if
    /// `state() == Idle` and a recording has finished). Empty
    /// before any recording starts.
    [[nodiscard]] QString currentFilePath() const;

    /// Snapshot of the in-memory metadata (origin timestamps,
    /// description, schemaId, live signal catalog). Returned by
    /// value to keep thread-safety simple.
    [[nodiscard]] SessionMetadata metadata() const;

    /// Total signal events recorded since `start()`. Reset on each
    /// `start()` call. Atomic; safe to read from any thread.
    [[nodiscard]] std::size_t eventsRecorded() const noexcept;

    /// Total bytes written to file (cumulative since `start()`).
    /// Atomic; safe to read from any thread.
    [[nodiscard]] std::size_t bytesWritten() const noexcept;

    /// Total events dropped due to queue backpressure since
    /// `start()`. Mirrors the `session_writer_dropped_events_total`
    /// metric. Atomic; safe to read from any thread.
    [[nodiscard]] std::size_t droppedEvents() const noexcept;

    // SignalValueSink overrides — forward to worker via the queue.

    void onSignal(std::chrono::steady_clock::time_point timestamp, const QString& signalId,
                  const signalforge::decoder::SignalValue& value) override;

    void onSignalsRegistered(const QString& driverId,
                             const std::vector<signalforge::decoder::SignalMetadata>& signalsList) override;

    void onSignalsUnregistered(const QString& driverId) override;

signals:
    /// Emitted on the main thread after the file is created and the
    /// header + initial catalog are written. `state()` will already
    /// be `Recording` when this fires.
    void recordingStarted(const QString& filePath);

    /// Emitted on the main thread after the worker has drained,
    /// written the footer, closed the file, and joined.
    /// `bytesWritten` is the final cumulative byte count.
    void recordingStopped(const QString& filePath, std::size_t bytesWritten);

    /// Emitted on the main thread (via `Qt::QueuedConnection` from
    /// the worker) on any unrecoverable disk / queue / lifecycle
    /// fault. The writer transitions to `Error` before this signal
    /// is delivered.
    void errorOccurred(const QString& errorMessage);

    /// Emitted periodically (default every 1 s while recording) on
    /// the main thread with the cumulative byte count after a
    /// flush. Used by the status-bar UI to show the file growing.
    void flushed(std::size_t bytesFlushed);

private:
    signalforge::buffer::SignalBufferRegistry* registry_;
    std::unique_ptr<SessionFileWriter> fileWriter_;
    std::unique_ptr<QThread> workerThread_;
    RecordingState state_ = RecordingState::Idle;
    QString currentFilePath_;
    SessionMetadata metadata_;
    std::atomic<std::size_t> eventsRecorded_{0};
    std::atomic<std::size_t> bytesWritten_{0};
    std::atomic<std::size_t> droppedEvents_{0};
};

}  // namespace signalforge::session
