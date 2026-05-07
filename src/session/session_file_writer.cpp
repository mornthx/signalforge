// src/session/session_file_writer.cpp
//
// S3 lands lifecycle: openFile / enqueue / processQueue exit on
// StopEvent. The file is opened (zero-byte placeholder) and
// closed cleanly; signal events are accepted into the queue but
// not yet encoded to disk. S4 replaces every method body below
// with the real SFREPLAY v1 encoder.
#include "session/session_file_writer.hpp"

#include "observability/logging.hpp"

#include <QDeadlineTimer>
#include <QFileInfo>
#include <QMutexLocker>
#include <QThread>

namespace signalforge::session {

SessionFileWriter::SessionFileWriter(QObject* parent) : QObject(parent) {}

SessionFileWriter::~SessionFileWriter() {
    if (file_.isOpen()) {
        file_.close();
    }
}

bool SessionFileWriter::openFile(const QString& filePath, const SessionMetadata& metadata) {
    if (file_.isOpen()) {
        SF_LOG_ERROR("SessionFileWriter::openFile called while a file is already open");
        return false;
    }
    file_.setFileName(filePath);
    if (!file_.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        SF_LOG_ERROR("SessionFileWriter::openFile failed to open: {} ({})", filePath.toStdString(),
                     file_.errorString().toStdString());
        return false;
    }

    // S3: open the file and seed the in-memory catalog index map
    // from the metadata snapshot. S4 writes the real SFREPLAY v1
    // header + signal catalog here. For now the file remains
    // empty; the lifecycle is what S3 exercises.
    currentCatalog_ = metadata.signalCatalog;
    signalIdToIndex_.clear();
    for (std::size_t i = 0; i < currentCatalog_.size(); ++i) {
        signalIdToIndex_.emplace(currentCatalog_[i].id, static_cast<std::uint32_t>(i));
    }

    bytesWritten_.store(0, std::memory_order_relaxed);
    droppedEvents_.store(0, std::memory_order_relaxed);
    return true;
}

bool SessionFileWriter::enqueue(SessionEvent event) {
    QMutexLocker lock(&queueMutex_);
    // S3 simple bounded enqueue. S5 implements the C3 4-point
    // policy (drop droppable / drop-oldest-droppable / 10 ms block
    // for non-droppable / Error transition).
    if (queue_.size() >= kQueueCapacity) {
        droppedEvents_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    queue_.enqueue(std::move(event));
    queueNotFull_.wakeAll();
    return true;
}

void SessionFileWriter::processQueue() {
    if (!file_.isOpen()) {
        SF_LOG_WARN("SessionFileWriter::processQueue invoked without an open file");
        return;
    }
    // S3 drain loop: pop events, ignore signal-event content,
    // exit on StopEvent. S4 replaces the inner dispatch with
    // real per-record encoding.
    while (true) {
        SessionEvent event;
        {
            QMutexLocker lock(&queueMutex_);
            while (queue_.isEmpty()) {
                // Worker stays in this loop until the queue has
                // an event or the thread is asked to quit.
                if (QThread::currentThread()->isInterruptionRequested()) {
                    file_.close();
                    return;
                }
                queueNotFull_.wait(&queueMutex_, kFlushInterval.count());
            }
            event = std::move(queue_.head());
            queue_.dequeue();
        }
        if (std::holds_alternative<StopEvent>(event)) {
            // S4 will write the SFREPLAY v1 footer here. For S3
            // the file is closed without a footer (which the
            // format spec §9 explicitly tolerates as "incomplete").
            file_.close();
            return;
        }
        // S4 will dispatch WriteSignalEvent + CatalogExtensionEvent
        // here. S3 silently consumes them.
    }
}

std::size_t SessionFileWriter::bytesWritten() const noexcept {
    return bytesWritten_.load(std::memory_order_relaxed);
}

std::size_t SessionFileWriter::droppedEvents() const noexcept {
    return droppedEvents_.load(std::memory_order_relaxed);
}

}  // namespace signalforge::session
