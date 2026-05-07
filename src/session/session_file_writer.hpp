// src/session/session_file_writer.hpp
#pragma once

#include "decode/decoder_interface.hpp"
#include "session/session_metadata.hpp"

#include <QByteArray>
#include <QFile>
#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QString>
#include <QWaitCondition>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <variant>
#include <vector>

namespace signalforge::session {

/// Queue event: a signal value to write.
struct WriteSignalEvent {
    std::chrono::steady_clock::time_point timestamp;
    QString signalId;
    signalforge::decoder::SignalValue value;
};

/// Queue event: a new driver's signals registered mid-stream. The
/// worker writes a Catalog Extension record (file format type 2)
/// and updates its `signalIdToIndex_` table before processing
/// subsequent signal events.
struct CatalogExtensionEvent {
    QString driverId;
    std::vector<signalforge::decoder::SignalMetadata> newSignals;
};

/// Sentinel: tells the worker to drain the rest of the queue,
/// write the footer, and exit its event loop. Non-droppable.
struct StopEvent {};

using SessionEvent = std::variant<WriteSignalEvent, CatalogExtensionEvent, StopEvent>;

/// Internal worker class. Lives on a dedicated `QThread` owned by
/// `SessionWriter`. Receives events from the writer via a
/// thread-safe bounded queue and writes them to disk in SFREPLAY
/// v1 format.
///
/// **Not** part of M10's freeze surface — internal implementation
/// detail. Default values (queue capacity, flush interval) and the
/// internal threading layout may be changed in V1.5+ without an
/// ADR (see ADR-007 + spec §6.2).
class SessionFileWriter : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SessionFileWriter)

public:
    explicit SessionFileWriter(QObject* parent = nullptr);
    ~SessionFileWriter() override;

    /// Open file for writing. Writes the SFREPLAY v1 header +
    /// initial signal catalog. Called once on the worker thread
    /// (via `Qt::BlockingQueuedConnection`) before the queue
    /// processing loop starts.
    [[nodiscard]] bool openFile(const QString& filePath, const SessionMetadata& metadata);

    /// Enqueue an event for writing. Thread-safe; called from main
    /// thread. Implements the C3 backpressure policy. Returns
    /// false if the event was dropped (droppable overflow case) or
    /// rejected (queue full of non-droppable + 10 ms timeout).
    bool enqueue(SessionEvent event);

    /// Worker entry point. Loops popping events from the queue and
    /// dispatching by `std::variant` alternative. Exits when a
    /// `StopEvent` is processed (after which the footer is written
    /// and the file closed). Connected via `QThread::started`.
    void processQueue();

    /// Cumulative file size in bytes.
    [[nodiscard]] std::size_t bytesWritten() const noexcept;

    /// Total events dropped since open. Atomic.
    [[nodiscard]] std::size_t droppedEvents() const noexcept;

signals:
    /// Emitted on disk / queue / lifecycle fault. Caller's
    /// `SessionWriter` wires this to its own `errorOccurred` via
    /// `Qt::QueuedConnection`.
    void error(const QString& errorMessage);

    /// Emitted after each periodic flush with the cumulative byte
    /// count.
    void flushed(std::size_t bytesFlushed);

private:
    /// Encode + write a Signal Value (record type 1).
    void writeSignalRecord(const WriteSignalEvent& evt);

    /// Encode + write a Catalog Extension (record type 2). Also
    /// updates the in-memory `currentCatalog_` and
    /// `signalIdToIndex_` so subsequent signal records reference
    /// the right indices.
    void writeCatalogExtension(const CatalogExtensionEvent& evt);

    /// Encode + write a Heartbeat (record type 4) at the current
    /// time.
    void writeHeartbeat();

    /// Encode + write a record header + payload.
    void writeRecord(std::uint32_t recordType, const QByteArray& payload);

    /// Write the 16-byte SFREPLAY v1 footer with the running
    /// `totalRecords_` count.
    void writeFooter();

    QFile file_;
    QMutex queueMutex_;
    QWaitCondition queueNotFull_;
    QQueue<SessionEvent> queue_;
    std::unordered_map<QString, std::uint32_t> signalIdToIndex_;
    std::vector<signalforge::decoder::SignalMetadata> currentCatalog_;
    std::int64_t metadataRecordedAtNs_ = 0;
    std::chrono::steady_clock::time_point metadataRecordingStart_{};
    std::atomic<std::size_t> bytesWritten_{0};
    std::atomic<std::size_t> droppedEvents_{0};
    std::uint32_t totalRecords_ = 0;

    static constexpr std::size_t kQueueCapacity = 10000;
    static constexpr std::chrono::milliseconds kFlushInterval{1000};
    static constexpr std::chrono::milliseconds kNonDroppableBlockTimeout{10};
};

}  // namespace signalforge::session
