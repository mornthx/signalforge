// src/session/session_file_writer.cpp
//
// S4 lands the SFREPLAY v1 encoder per docs/format/sfreplay-v1.md.
// openFile writes the header (fixed prefix + variable section +
// initial signal catalog). processQueue dispatches signal /
// catalog-extension / stop events to the matching record types
// (1 / 2 / 4). Heartbeats (type 4) and periodic flushes are
// fired by worker-side QTimers. fsync is called on every flush
// and on close so the file is bound-to-disk for the data we've
// reported as written.
//
// S5 swaps the simple bounded enqueue() for the C3 4-point
// backpressure policy.
#include "session/session_file_writer.hpp"

#include "observability/logging.hpp"

#include <QByteArray>
#include <QDeadlineTimer>
#include <QFileInfo>
#include <QMutexLocker>
#include <QThread>
#include <QTimer>
#include <QtEndian>
#include <chrono>
#include <cstring>

namespace signalforge::session {

namespace {

constexpr const char* kSFReplayMagic = "SFREPLAY";
constexpr std::uint32_t kFormatVersion = 1;
constexpr const char* kFooterMagic = "REPLAYEO";  // 8 bytes; the human "REPLAYEOF" is 9 letters
constexpr std::uint32_t kRecordTypeSignalValue = 1;
constexpr std::uint32_t kRecordTypeCatalogExtension = 2;
constexpr std::uint32_t kRecordTypeMarker = 3;  // V1 reserved (writer doesn't emit; reader may ignore)
constexpr std::uint32_t kRecordTypeHeartbeat = 4;
constexpr std::chrono::seconds kHeartbeatInterval{10};

// LE encoding helpers. On x86_64 these are no-ops; on big-endian
// hosts they byte-swap.
template <typename T> void appendLe(QByteArray& out, T value) {
    static_assert(std::is_trivially_copyable_v<T>);
    if constexpr (std::is_same_v<T, double>) {
        std::uint64_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        const std::uint64_t le = qToLittleEndian(bits);
        out.append(reinterpret_cast<const char*>(&le), sizeof(le));
    } else if constexpr (std::is_unsigned_v<T>) {
        const T le = qToLittleEndian(value);
        out.append(reinterpret_cast<const char*>(&le), sizeof(le));
    } else {
        // signed integers via the equivalent unsigned encoding.
        using U = std::make_unsigned_t<T>;
        U u = 0;
        std::memcpy(&u, &value, sizeof(value));
        const U le = qToLittleEndian(u);
        out.append(reinterpret_cast<const char*>(&le), sizeof(le));
    }
}

void appendString(QByteArray& out, const QString& s) {
    const QByteArray utf8 = s.toUtf8();
    appendLe<std::uint32_t>(out, static_cast<std::uint32_t>(utf8.size()));
    out.append(utf8);
}

std::uint8_t typeTag(signalforge::decoder::SignalType t) {
    using T = signalforge::decoder::SignalType;
    switch (t) {
    case T::Bool:
        return 0;
    case T::Int64:
        return 1;
    case T::Double:
        return 2;
    case T::String:
        return 3;
    }
    return 0;
}

void appendSignalEntry(QByteArray& out, const signalforge::decoder::SignalMetadata& m) {
    appendString(out, m.id);
    appendString(out, m.name);
    appendString(out, m.unit);
    appendString(out, m.description.value_or(QString{}));
    out.append(static_cast<char>(typeTag(m.type)));
    out.append(static_cast<char>(m.scale.has_value() ? 1 : 0));
    if (m.scale.has_value()) {
        appendLe<double>(out, *m.scale);
    }
    out.append(static_cast<char>(m.offset.has_value() ? 1 : 0));
    if (m.offset.has_value()) {
        appendLe<double>(out, *m.offset);
    }
}

}  // namespace

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

    currentCatalog_ = metadata.signalCatalog;
    signalIdToIndex_.clear();
    for (std::size_t i = 0; i < currentCatalog_.size(); ++i) {
        signalIdToIndex_.emplace(currentCatalog_[i].id, static_cast<std::uint32_t>(i));
    }

    bytesWritten_.store(0, std::memory_order_relaxed);
    droppedEvents_.store(0, std::memory_order_relaxed);
    totalRecords_ = 0;

    // Build header per docs/format/sfreplay-v1.md §4.
    // Fixed prefix (16 bytes) is written with placeholder
    // headerLen=0; we patch it once the variable section + catalog
    // are appended.
    QByteArray header;
    header.reserve(64 + currentCatalog_.size() * 64);

    header.append(kSFReplayMagic, 8);
    appendLe<std::uint32_t>(header, kFormatVersion);
    appendLe<std::uint32_t>(header, 0);  // headerLen patched below

    // Variable section.
    const std::int64_t recordedAtNs =
        std::chrono::duration_cast<std::chrono::nanoseconds>(metadata.recordedAt.time_since_epoch()).count();
    appendLe<std::int64_t>(header, recordedAtNs);
    appendString(header, metadata.description);
    appendString(header, metadata.decoderSchemaId);
    appendLe<std::uint32_t>(header, static_cast<std::uint32_t>(currentCatalog_.size()));

    // Signal catalog inline.
    for (const auto& sig : currentCatalog_) {
        appendSignalEntry(header, sig);
    }

    // Patch headerLen at offset 12.
    const std::uint32_t headerLen = qToLittleEndian(static_cast<std::uint32_t>(header.size()));
    std::memcpy(header.data() + 12, &headerLen, sizeof(headerLen));

    if (file_.write(header) != header.size()) {
        SF_LOG_ERROR("SessionFileWriter::openFile failed to write header: {}", file_.errorString().toStdString());
        file_.close();
        return false;
    }
    bytesWritten_.fetch_add(static_cast<std::size_t>(header.size()), std::memory_order_relaxed);
    metadataRecordedAtNs_ = recordedAtNs;
    metadataRecordingStart_ = metadata.recordingStart;
    return true;
}

bool SessionFileWriter::enqueue(SessionEvent event) {
    // C3 4-point policy per ADR-007 / M10-concerns.md:
    //   1. Droppable + queue full → drop NEW; return false.
    //   2. Non-droppable + queue full → drop OLDEST droppable;
    //      enqueue NEW; return true.
    //   3. Queue full of non-droppable → block on enqueue with
    //      10 ms timeout.
    //   4. 10 ms timeout exceeded → log ERROR, emit error
    //      signal, return false (caller transitions to Error).
    QMutexLocker lock(&queueMutex_);

    const bool isDroppable = std::holds_alternative<WriteSignalEvent>(event);

    if (queue_.size() < kQueueCapacity) {
        queue_.enqueue(std::move(event));
        queueNotFull_.wakeAll();
        return true;
    }

    if (isDroppable) {
        // Policy 1: drop the new droppable event (FIFO retention
        // of recent writes). Caller increments its own counter
        // from `false` return; we increment ours for the metric.
        droppedEvents_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    // New event is non-droppable (CatalogExtensionEvent /
    // StopEvent). Search for the oldest droppable in queue and
    // evict it.
    for (qsizetype i = 0; i < queue_.size(); ++i) {
        if (std::holds_alternative<WriteSignalEvent>(queue_[i])) {
            queue_.removeAt(i);
            droppedEvents_.fetch_add(1, std::memory_order_relaxed);
            queue_.enqueue(std::move(event));
            queueNotFull_.wakeAll();
            return true;
        }
    }

    // Policy 3 / 4: queue is full of non-droppable events
    // (vanishingly rare — would require thousands of mid-stream
    // signal registrations queued ahead). Block briefly hoping
    // the worker drains.
    QDeadlineTimer deadline(static_cast<qint64>(kNonDroppableBlockTimeout.count()));
    while (queue_.size() >= kQueueCapacity) {
        if (deadline.hasExpired()) {
            SF_LOG_ERROR("SessionFileWriter::enqueue: queue full of non-droppable events; transitioning to Error");
            droppedEvents_.fetch_add(1, std::memory_order_relaxed);
            // Caller wires the error signal to SessionWriter::errorOccurred,
            // which flips state_ to Error.
            emit error(QStringLiteral("SessionFileWriter: queue full of non-droppable events"));
            return false;
        }
        queueNotFull_.wait(&queueMutex_, deadline.remainingTime());
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

    // Worker-thread timers for periodic flush + heartbeat.
    QTimer flushTimer;
    flushTimer.setInterval(static_cast<int>(kFlushInterval.count()));
    flushTimer.setTimerType(Qt::CoarseTimer);
    QObject::connect(&flushTimer, &QTimer::timeout, this, [this]() {
        if (file_.isOpen()) {
            file_.flush();
            const std::size_t bytes = bytesWritten_.load(std::memory_order_relaxed);
            emit flushed(bytes);
        }
    });
    flushTimer.start();

    QTimer heartbeatTimer;
    heartbeatTimer.setInterval(static_cast<int>(std::chrono::milliseconds(kHeartbeatInterval).count()));
    heartbeatTimer.setTimerType(Qt::CoarseTimer);
    QObject::connect(&heartbeatTimer, &QTimer::timeout, this, [this]() { writeHeartbeat(); });
    heartbeatTimer.start();

    while (true) {
        SessionEvent event;
        {
            QMutexLocker lock(&queueMutex_);
            while (queue_.isEmpty()) {
                if (QThread::currentThread()->isInterruptionRequested()) {
                    flushTimer.stop();
                    heartbeatTimer.stop();
                    file_.flush();
                    file_.close();
                    return;
                }
                queueNotFull_.wait(&queueMutex_, kFlushInterval.count());
            }
            event = std::move(queue_.head());
            queue_.dequeue();
        }
        if (std::holds_alternative<StopEvent>(event)) {
            flushTimer.stop();
            heartbeatTimer.stop();
            writeFooter();
            file_.flush();
            file_.close();
            return;
        }
        if (std::holds_alternative<WriteSignalEvent>(event)) {
            writeSignalRecord(std::get<WriteSignalEvent>(event));
        } else if (std::holds_alternative<CatalogExtensionEvent>(event)) {
            writeCatalogExtension(std::get<CatalogExtensionEvent>(event));
        }
    }
}

void SessionFileWriter::writeSignalRecord(const WriteSignalEvent& evt) {
    auto it = signalIdToIndex_.find(evt.signalId);
    if (it == signalIdToIndex_.end()) {
        SF_LOG_WARN("SessionFileWriter: signal '{}' not in catalog; record skipped", evt.signalId.toStdString());
        return;
    }
    const std::uint32_t signalIdx = it->second;
    const std::int64_t timestampNs =
        std::chrono::duration_cast<std::chrono::nanoseconds>(evt.timestamp - metadataRecordingStart_).count();

    QByteArray payload;
    payload.reserve(64);
    appendLe<std::uint32_t>(payload, signalIdx);
    appendLe<std::int64_t>(payload, timestampNs);
    std::visit(
        [&payload](auto&& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, bool>) {
                payload.append(v ? '\x01' : '\x00');
            } else if constexpr (std::is_same_v<T, std::int64_t>) {
                appendLe<std::int64_t>(payload, v);
            } else if constexpr (std::is_same_v<T, double>) {
                appendLe<double>(payload, v);
            } else if constexpr (std::is_same_v<T, QString>) {
                appendString(payload, v);
            }
        },
        evt.value);

    writeRecord(kRecordTypeSignalValue, payload);
}

void SessionFileWriter::writeCatalogExtension(const CatalogExtensionEvent& evt) {
    QByteArray payload;
    payload.reserve(64 + evt.newSignals.size() * 64);
    appendLe<std::uint32_t>(payload, static_cast<std::uint32_t>(evt.newSignals.size()));
    for (const auto& sig : evt.newSignals) {
        appendSignalEntry(payload, sig);
        signalIdToIndex_.emplace(sig.id, static_cast<std::uint32_t>(currentCatalog_.size()));
        currentCatalog_.push_back(sig);
    }
    writeRecord(kRecordTypeCatalogExtension, payload);
}

void SessionFileWriter::writeHeartbeat() {
    if (!file_.isOpen()) {
        return;
    }
    const std::int64_t timestampNs =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - metadataRecordingStart_)
            .count();
    QByteArray payload;
    payload.reserve(8);
    appendLe<std::int64_t>(payload, timestampNs);
    writeRecord(kRecordTypeHeartbeat, payload);
}

void SessionFileWriter::writeRecord(std::uint32_t recordType, const QByteArray& payload) {
    QByteArray header;
    header.reserve(8);
    appendLe<std::uint32_t>(header, recordType);
    appendLe<std::uint32_t>(header, static_cast<std::uint32_t>(payload.size()));
    if (file_.write(header) != header.size()) {
        emit error(QStringLiteral("SessionFileWriter: record header write failed: %1").arg(file_.errorString()));
        return;
    }
    if (!payload.isEmpty() && file_.write(payload) != payload.size()) {
        emit error(QStringLiteral("SessionFileWriter: record payload write failed: %1").arg(file_.errorString()));
        return;
    }
    bytesWritten_.fetch_add(static_cast<std::size_t>(header.size() + payload.size()), std::memory_order_relaxed);
    ++totalRecords_;
}

void SessionFileWriter::writeFooter() {
    QByteArray footer;
    footer.reserve(16);
    footer.append(kFooterMagic, 8);
    appendLe<std::uint32_t>(footer, totalRecords_);
    appendLe<std::uint32_t>(footer, 0);  // reserved
    if (file_.write(footer) != footer.size()) {
        emit error(QStringLiteral("SessionFileWriter: footer write failed: %1").arg(file_.errorString()));
        return;
    }
    bytesWritten_.fetch_add(static_cast<std::size_t>(footer.size()), std::memory_order_relaxed);
}

std::size_t SessionFileWriter::bytesWritten() const noexcept {
    return bytesWritten_.load(std::memory_order_relaxed);
}

std::size_t SessionFileWriter::droppedEvents() const noexcept {
    return droppedEvents_.load(std::memory_order_relaxed);
}

}  // namespace signalforge::session
