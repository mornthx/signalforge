// src/session/session_writer.cpp
//
// S3 lands the SessionWriter lifecycle: start() / stop() with a
// worker QThread and a SessionFileWriter that runs on it. Signal
// catalog tracking via the SignalValueSink overrides is also done
// here so the catalog written at recording start reflects every
// onSignalsRegistered seen since the writer was constructed.
//
// S4 (encoder) and S5 (queue / backpressure) fill in the body of
// the worker's processQueue loop; this file routes events into
// the file writer's enqueue() but does not yet do the disk I/O.
#include "session/session_writer.hpp"

#include "observability/logging.hpp"
#include "session/session_file_writer.hpp"

#include <QMetaObject>
#include <QMutexLocker>
#include <QThread>
#include <Qt>

namespace signalforge::session {

SessionWriter::SessionWriter(signalforge::buffer::SignalBufferRegistry& registry, QObject* parent)
    : QObject(parent), registry_(&registry) {
    (void)registry_;  // The pointer is retained for V1.5+ when the
                      // writer may consult the registry directly
                      // (e.g., to pull memory-usage info into a
                      // file annotation). V1 routes signals via
                      // the SignalValueSink overrides below.
}

SessionWriter::~SessionWriter() {
    // Defensive: if a recording is still in flight when the writer
    // is destroyed, drain it. Caller should normally call stop()
    // explicitly to receive the byte count.
    if (state_ == RecordingState::Recording) {
        (void)stop();
    }
}

bool SessionWriter::start(const QString& filePath, const QString& description, const QString& decoderSchemaId) {
    if (state_ == RecordingState::Recording) {
        SF_LOG_WARN("SessionWriter::start() called while already recording");
        return false;
    }

    // Snapshot the metadata for this recording. The signal catalog
    // is what the writer's SignalValueSink::onSignalsRegistered
    // overrides have observed up to this moment; the writer reuses
    // its in-memory catalog (reset to empty between recordings).
    metadata_.recordedAt = std::chrono::system_clock::now();
    metadata_.recordingStart = std::chrono::steady_clock::now();
    metadata_.recordingEnd.reset();
    metadata_.description = description;
    metadata_.decoderSchemaId = decoderSchemaId;
    // metadata_.signalCatalog is already populated from cached
    // onSignalsRegistered events.

    fileWriter_ = std::make_unique<SessionFileWriter>();
    workerThread_ = std::make_unique<QThread>();
    workerThread_->setObjectName(QStringLiteral("session-writer-worker"));
    fileWriter_->moveToThread(workerThread_.get());

    // Wire up worker → main thread signal forwarding before the
    // worker starts processing.
    // Worker error → flip state to Error AND emit the public
    // errorOccurred signal. Both run on the main thread via
    // Qt::QueuedConnection.
    connect(
        fileWriter_.get(), &SessionFileWriter::error, this,
        [this](const QString& message) {
            state_ = RecordingState::Error;
            emit errorOccurred(message);
        },
        Qt::QueuedConnection);
    connect(fileWriter_.get(), &SessionFileWriter::flushed, this, &SessionWriter::flushed, Qt::QueuedConnection);

    workerThread_->start();

    // Open the file synchronously on the worker thread. Blocking
    // here is acceptable per spec §5.3 (start() → recordingStarted
    // budget is < 100 ms; file open at recording start is the only
    // disk-blocking call on the main thread).
    bool opened = false;
    QMetaObject::invokeMethod(
        fileWriter_.get(), [this, filePath, &opened]() { opened = fileWriter_->openFile(filePath, metadata_); },
        Qt::BlockingQueuedConnection);

    if (!opened) {
        SF_LOG_ERROR("SessionWriter::start() failed to open file: {}", filePath.toStdString());
        workerThread_->quit();
        workerThread_->wait();
        fileWriter_.reset();
        workerThread_.reset();
        state_ = RecordingState::Error;
        emit errorOccurred(QStringLiteral("Failed to open recording file: %1").arg(filePath));
        return false;
    }

    // Kick off the worker's queue-processing loop.
    QMetaObject::invokeMethod(fileWriter_.get(), &SessionFileWriter::processQueue, Qt::QueuedConnection);

    currentFilePath_ = filePath;
    eventsRecorded_.store(0, std::memory_order_relaxed);
    bytesWritten_.store(0, std::memory_order_relaxed);
    droppedEvents_.store(0, std::memory_order_relaxed);
    state_ = RecordingState::Recording;

    SF_LOG_INFO("SessionWriter recording started: {}", filePath.toStdString());
    emit recordingStarted(filePath);
    return true;
}

std::size_t SessionWriter::stop() {
    if (state_ != RecordingState::Recording) {
        return 0;
    }

    // Enqueue the StopEvent sentinel; the worker drains, writes
    // the footer, and exits processQueue().
    fileWriter_->enqueue(StopEvent{});

    // Wait for the worker to finish the drain + write the footer.
    workerThread_->quit();
    workerThread_->wait();

    const std::size_t bytes = fileWriter_->bytesWritten();
    bytesWritten_.store(bytes, std::memory_order_relaxed);
    droppedEvents_.store(droppedEvents_.load(std::memory_order_relaxed) + fileWriter_->droppedEvents(),
                         std::memory_order_relaxed);

    metadata_.recordingEnd = std::chrono::steady_clock::now();
    state_ = RecordingState::Idle;

    const QString path = currentFilePath_;

    fileWriter_.reset();
    workerThread_.reset();

    SF_LOG_INFO("SessionWriter recording stopped: {} ({} bytes)", path.toStdString(), bytes);
    emit recordingStopped(path, bytes);
    return bytes;
}

bool SessionWriter::isRecording() const noexcept {
    return state_ == RecordingState::Recording;
}

RecordingState SessionWriter::state() const noexcept {
    return state_;
}

QString SessionWriter::currentFilePath() const {
    return currentFilePath_;
}

SessionMetadata SessionWriter::metadata() const {
    return metadata_;
}

std::size_t SessionWriter::eventsRecorded() const noexcept {
    return eventsRecorded_.load(std::memory_order_relaxed);
}

std::size_t SessionWriter::bytesWritten() const noexcept {
    return bytesWritten_.load(std::memory_order_relaxed);
}

std::size_t SessionWriter::droppedEvents() const noexcept {
    return droppedEvents_.load(std::memory_order_relaxed);
}

void SessionWriter::onSignal(std::chrono::steady_clock::time_point timestamp, const QString& signalId,
                             const signalforge::decoder::SignalValue& value) {
    if (state_ != RecordingState::Recording) {
        return;
    }
    const bool ok = fileWriter_->enqueue(WriteSignalEvent{timestamp, signalId, value});
    if (ok) {
        eventsRecorded_.fetch_add(1, std::memory_order_relaxed);
    } else {
        droppedEvents_.fetch_add(1, std::memory_order_relaxed);
    }
}

void SessionWriter::onSignalsRegistered(const QString& driverId,
                                        const std::vector<signalforge::decoder::SignalMetadata>& signalsList) {
    // Always cache new signals, even when not recording: the next
    // start() uses metadata_.signalCatalog as the file's initial
    // catalog.
    metadata_.signalCatalog.insert(metadata_.signalCatalog.end(), signalsList.begin(), signalsList.end());

    if (state_ != RecordingState::Recording) {
        return;
    }
    // Live recording: tell the worker to write a Catalog Extension
    // record so subsequent signalIdx values resolve correctly in
    // the file.
    fileWriter_->enqueue(CatalogExtensionEvent{driverId, signalsList});
}

void SessionWriter::onSignalsUnregistered(const QString& /*driverId*/) {
    // No-op per spec §3.4: V1 keeps the catalog growable;
    // unregistration does not remove signals from either the live
    // catalog or the file's catalog.
}

}  // namespace signalforge::session
