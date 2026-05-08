// src/session/session_reader.cpp
//
// SFREPLAY v1 reader. Parses files produced by SessionWriter
// (per docs/format/sfreplay-v1.md).
//
// Two delivery modes share a single record-walker:
//   - Bulk: replayAll(sink) — fire-and-forget. M10 entry point;
//     drives the HALT-trigger-#2 round-trip test.
//   - Streaming (M11 S2): readNextRecord(out) returns one signal
//     value at a time; seekToTimestamp repositions; catalog
//     extensions encountered are dispatched to a separately-bound
//     catalog sink.
//
// Tolerates truncated files (no footer or partial last record)
// per spec §9.
#include "session/session_reader.hpp"

#include "observability/logging.hpp"

#include <QByteArray>
#include <QtEndian>
#include <algorithm>
#include <cstring>

namespace signalforge::session {

namespace {

constexpr const char* kSFReplayMagic = "SFREPLAY";
constexpr std::uint32_t kFormatVersion = 1;
constexpr const char* kFooterMagic = "REPLAYEO";  // 8 bytes; format spec §7.1
constexpr std::uint32_t kRecordTypeSignalValue = 1;
constexpr std::uint32_t kRecordTypeCatalogExtension = 2;
constexpr std::uint32_t kRecordTypeMarker = 3;
constexpr std::uint32_t kRecordTypeHeartbeat = 4;
constexpr qint64 kFooterSize = 16;
constexpr qint64 kRecordHeaderSize = 8;  // recordType (u32 LE) + payloadLen (u32 LE)

// LE decoders. On x86_64 these are no-ops; on big-endian they
// byte-swap.
template <typename T> T readLe(const QByteArray& data, qsizetype offset) {
    static_assert(std::is_trivially_copyable_v<T>);
    if constexpr (std::is_same_v<T, double>) {
        std::uint64_t bits = 0;
        std::memcpy(&bits, data.constData() + offset, sizeof(bits));
        bits = qFromLittleEndian(bits);
        double out;
        std::memcpy(&out, &bits, sizeof(out));
        return out;
    } else if constexpr (std::is_unsigned_v<T>) {
        T value;
        std::memcpy(&value, data.constData() + offset, sizeof(value));
        return qFromLittleEndian(value);
    } else {
        using U = std::make_unsigned_t<T>;
        U u;
        std::memcpy(&u, data.constData() + offset, sizeof(u));
        u = qFromLittleEndian(u);
        T out;
        std::memcpy(&out, &u, sizeof(out));
        return out;
    }
}

template <typename T> bool readLeFromFile(QFile& file, T& out) {
    QByteArray bytes = file.read(sizeof(T));
    if (bytes.size() != static_cast<qsizetype>(sizeof(T))) {
        return false;
    }
    out = readLe<T>(bytes, 0);
    return true;
}

bool readUtf8FromFile(QFile& file, QString& out) {
    std::uint32_t len = 0;
    if (!readLeFromFile(file, len)) {
        return false;
    }
    if (len == 0) {
        out.clear();
        return true;
    }
    QByteArray bytes = file.read(static_cast<qint64>(len));
    if (bytes.size() != static_cast<qsizetype>(len)) {
        return false;
    }
    out = QString::fromUtf8(bytes);
    return true;
}

signalforge::decoder::SignalType decodeTypeTag(std::uint8_t tag) {
    using T = signalforge::decoder::SignalType;
    switch (tag) {
    case 0:
        return T::Bool;
    case 1:
        return T::Int64;
    case 2:
        return T::Double;
    case 3:
        return T::String;
    default:
        // Unknown type tag from V2+; preserve the file's intent
        // by mapping to Double as a degraded fallback. V1 readers
        // that hit this case will already have rejected the
        // record at the higher level.
        return T::Double;
    }
}

}  // namespace

SessionReader::SessionReader() = default;

SessionReader::~SessionReader() {
    close();
}

bool SessionReader::open(const QString& filePath) {
    if (file_.isOpen()) {
        SF_LOG_ERROR("SessionReader::open called while another file is already open");
        return false;
    }
    file_.setFileName(filePath);
    if (!file_.open(QIODevice::ReadOnly)) {
        SF_LOG_ERROR("SessionReader::open failed to open: {} ({})", filePath.toStdString(),
                     file_.errorString().toStdString());
        return false;
    }
    fileSize_ = file_.size();
    if (!readHeader()) {
        file_.close();
        return false;
    }
    replayOriginSteady_ = std::chrono::steady_clock::now();
    metadata_.recordingStart = replayOriginSteady_;

    // Establish the record region boundary: everything between
    // the header end and the footer start (or EOF if footerless).
    recordsEnd_ = (fileSize_ >= headerLen_ + kFooterSize) ? fileSize_ - kFooterSize : fileSize_;

    // Snapshot the header catalog so backward seek can restore it.
    headerCatalog_ = runningCatalog_;

    // Streaming-mode counters reset.
    recordsRead_ = 0;
    currentRecordIdx_ = 0;
    currentTimestampNs_ = 0;
    fileComplete_ = false;
    atEnd_ = false;

    // Best-effort footer pre-scan to populate `footerRecordCount_`
    // and `lastTimestampNs_`. Failures are non-fatal.
    preScanFooter();

    return true;
}

void SessionReader::close() {
    if (file_.isOpen()) {
        file_.close();
    }
    headerCatalog_.clear();
    runningCatalog_.clear();
    catalogSink_ = nullptr;
    headerLen_ = 0;
    fileSize_ = 0;
    recordsEnd_ = 0;
    recordsRead_ = 0;
    currentRecordIdx_ = 0;
    currentTimestampNs_ = 0;
    footerRecordCount_ = 0;
    lastTimestampNs_ = 0;
    fileComplete_ = false;
    atEnd_ = false;
}

bool SessionReader::isOpen() const noexcept {
    return file_.isOpen();
}

SessionMetadata SessionReader::metadata() const {
    return metadata_;
}

void SessionReader::bindCatalogSink(signalforge::decoder::SignalValueSink* sink) {
    catalogSink_ = sink;
}

std::size_t SessionReader::currentRecordIndex() const noexcept {
    return currentRecordIdx_;
}

std::int64_t SessionReader::currentTimestampNs() const noexcept {
    return currentTimestampNs_;
}

bool SessionReader::atEnd() const noexcept {
    return atEnd_;
}

std::size_t SessionReader::footerRecordCount() const noexcept {
    return footerRecordCount_;
}

std::int64_t SessionReader::lastTimestampNs() const noexcept {
    return lastTimestampNs_;
}

bool SessionReader::readHeader() {
    // Fixed prefix
    QByteArray fixed = file_.read(16);
    if (fixed.size() != 16) {
        SF_LOG_ERROR("SessionReader: file too short for fixed header (size={})", fixed.size());
        return false;
    }
    if (std::memcmp(fixed.constData(), kSFReplayMagic, 8) != 0) {
        SF_LOG_ERROR("SessionReader: SFREPLAY magic mismatch");
        return false;
    }
    const std::uint32_t formatVersion = readLe<std::uint32_t>(fixed, 8);
    if (formatVersion != kFormatVersion) {
        SF_LOG_ERROR("SessionReader: unsupported formatVersion={}", formatVersion);
        return false;
    }
    headerLen_ = static_cast<qint64>(readLe<std::uint32_t>(fixed, 12));
    if (headerLen_ < 16 || headerLen_ > fileSize_) {
        SF_LOG_ERROR("SessionReader: invalid headerLen={} (fileSize={})", headerLen_, fileSize_);
        return false;
    }

    // Variable section
    std::int64_t recordedAtNs = 0;
    if (!readLeFromFile(file_, recordedAtNs)) {
        return false;
    }
    metadata_.recordedAt = std::chrono::system_clock::time_point{std::chrono::nanoseconds{recordedAtNs}};
    if (!readUtf8FromFile(file_, metadata_.description)) {
        return false;
    }
    if (!readUtf8FromFile(file_, metadata_.decoderSchemaId)) {
        return false;
    }
    std::uint32_t signalCount = 0;
    if (!readLeFromFile(file_, signalCount)) {
        return false;
    }

    runningCatalog_.clear();
    runningCatalog_.reserve(signalCount);
    metadata_.signalCatalog.clear();
    metadata_.signalCatalog.reserve(signalCount);
    for (std::uint32_t i = 0; i < signalCount; ++i) {
        signalforge::decoder::SignalMetadata m;
        if (!readSignalEntry(m)) {
            SF_LOG_ERROR("SessionReader: signal catalog entry {} truncated", i);
            return false;
        }
        runningCatalog_.push_back(m);
        metadata_.signalCatalog.push_back(m);
    }

    // Sanity: the file pointer should now be at headerLen_.
    if (file_.pos() != headerLen_) {
        SF_LOG_WARN("SessionReader: header parse advanced to {} but headerLen={} — V2 trailer fields skipped",
                    file_.pos(), headerLen_);
        if (!file_.seek(headerLen_)) {
            return false;
        }
    }
    return true;
}

bool SessionReader::readSignalEntry(signalforge::decoder::SignalMetadata& out) {
    QString idStr;
    if (!readUtf8FromFile(file_, idStr)) {
        return false;
    }
    out.id = idStr;
    if (!readUtf8FromFile(file_, out.name)) {
        return false;
    }
    if (!readUtf8FromFile(file_, out.unit)) {
        return false;
    }
    QString desc;
    if (!readUtf8FromFile(file_, desc)) {
        return false;
    }
    if (!desc.isEmpty()) {
        out.description = desc;
    }
    QByteArray typeByte = file_.read(1);
    if (typeByte.size() != 1) {
        return false;
    }
    out.type = decodeTypeTag(static_cast<std::uint8_t>(typeByte[0]));
    QByteArray hasScale = file_.read(1);
    if (hasScale.size() != 1) {
        return false;
    }
    if (hasScale[0] != 0) {
        double s = 0;
        if (!readLeFromFile(file_, s)) {
            return false;
        }
        out.scale = s;
    }
    QByteArray hasOffset = file_.read(1);
    if (hasOffset.size() != 1) {
        return false;
    }
    if (hasOffset[0] != 0) {
        double o = 0;
        if (!readLeFromFile(file_, o)) {
            return false;
        }
        out.offset = o;
    }
    return true;
}

bool SessionReader::readRecordHeader(std::uint32_t& recordType, std::uint32_t& payloadLen) {
    if (file_.pos() + kRecordHeaderSize > recordsEnd_) {
        return false;
    }
    if (!readLeFromFile(file_, recordType)) {
        return false;
    }
    if (!readLeFromFile(file_, payloadLen)) {
        return false;
    }
    return true;
}

bool SessionReader::readNextRecord(ReplayRecord& out) {
    if (!file_.isOpen()) {
        return false;
    }
    while (file_.pos() < recordsEnd_) {
        std::uint32_t recordType = 0;
        std::uint32_t payloadLen = 0;
        if (!readRecordHeader(recordType, payloadLen)) {
            atEnd_ = true;
            return false;
        }
        if (file_.pos() + static_cast<qint64>(payloadLen) > recordsEnd_) {
            // Truncated last record. Treat as EOF.
            SF_LOG_WARN("SessionReader: truncated record at offset {}; stopping stream",
                        file_.pos() - kRecordHeaderSize);
            atEnd_ = true;
            return false;
        }
        switch (recordType) {
        case kRecordTypeSignalValue:
            if (!decodeSignalValueRecord(payloadLen, out)) {
                atEnd_ = true;
                return false;
            }
            ++recordsRead_;
            return true;
        case kRecordTypeCatalogExtension:
            if (!decodeCatalogExtension(payloadLen)) {
                atEnd_ = true;
                return false;
            }
            // Catalog extensions count toward `recordsRead_` so it
            // matches the writer's footer `totalRecords` (which
            // counts every record including extensions). The
            // streaming caller observes only signal-value records;
            // `currentRecordIndex()` reports those.
            ++recordsRead_;
            break;
        case kRecordTypeMarker:
        case kRecordTypeHeartbeat:
        default:
            if (!skipPayload(payloadLen)) {
                atEnd_ = true;
                return false;
            }
            ++recordsRead_;
            break;
        }
    }
    atEnd_ = true;
    return false;
}

bool SessionReader::decodeSignalValueRecord(std::uint32_t payloadLen, ReplayRecord& out) {
    if (payloadLen < 12) {
        SF_LOG_ERROR("SessionReader: signal record payload too short ({})", payloadLen);
        return false;
    }
    std::uint32_t signalIdx = 0;
    std::int64_t timestampNs = 0;
    if (!readLeFromFile(file_, signalIdx)) {
        return false;
    }
    if (!readLeFromFile(file_, timestampNs)) {
        return false;
    }
    if (signalIdx >= runningCatalog_.size()) {
        SF_LOG_ERROR("SessionReader: signalIdx={} exceeds catalog size {}", signalIdx, runningCatalog_.size());
        return false;
    }
    const auto& meta = runningCatalog_[signalIdx];
    const std::uint32_t valueLen = payloadLen - 12;
    (void)valueLen;

    using T = signalforge::decoder::SignalType;
    switch (meta.type) {
    case T::Bool: {
        QByteArray b = file_.read(1);
        if (b.size() != 1) {
            return false;
        }
        out.value = (b[0] != 0);
        break;
    }
    case T::Int64: {
        std::int64_t n = 0;
        if (!readLeFromFile(file_, n)) {
            return false;
        }
        out.value = n;
        break;
    }
    case T::Double: {
        double d = 0;
        if (!readLeFromFile(file_, d)) {
            return false;
        }
        out.value = d;
        break;
    }
    case T::String: {
        QString s;
        if (!readUtf8FromFile(file_, s)) {
            return false;
        }
        out.value = s;
        break;
    }
    }

    out.timestampNs = timestampNs;
    out.signalId = meta.id;
    currentTimestampNs_ = timestampNs;
    ++currentRecordIdx_;
    return true;
}

bool SessionReader::decodeCatalogExtension(std::uint32_t payloadLen) {
    std::uint32_t addedCount = 0;
    if (!readLeFromFile(file_, addedCount)) {
        return false;
    }
    (void)payloadLen;
    std::vector<signalforge::decoder::SignalMetadata> added;
    added.reserve(addedCount);
    for (std::uint32_t i = 0; i < addedCount; ++i) {
        signalforge::decoder::SignalMetadata m;
        if (!readSignalEntry(m)) {
            return false;
        }
        added.push_back(m);
        runningCatalog_.push_back(m);
    }
    if (catalogSink_ != nullptr) {
        catalogSink_->onSignalsRegistered(QStringLiteral("session-replay"), added);
    }
    return true;
}

bool SessionReader::skipPayload(std::uint32_t payloadLen) {
    return file_.seek(file_.pos() + static_cast<qint64>(payloadLen));
}

bool SessionReader::seekToTimestamp(std::int64_t targetNs) {
    if (!file_.isOpen()) {
        return false;
    }

    // Clamp; out-of-range targets snap to the file's bounds.
    if (lastTimestampNs_ > 0 && targetNs > lastTimestampNs_) {
        SF_LOG_WARN("SessionReader::seek: target {} > last {}; clamping", targetNs, lastTimestampNs_);
        targetNs = lastTimestampNs_;
    }
    if (targetNs < 0) {
        targetNs = 0;
    }

    // Backward seek: re-position to header end and reset catalog
    // state to the header catalog.
    if (targetNs <= currentTimestampNs_) {
        if (!file_.seek(headerLen_)) {
            SF_LOG_ERROR("SessionReader::seek: failed to seek to headerLen={}", headerLen_);
            return false;
        }
        runningCatalog_ = headerCatalog_;
        currentRecordIdx_ = 0;
        currentTimestampNs_ = 0;
        atEnd_ = false;
        recordsRead_ = 0;

        // The header catalog needs to be re-announced so the bound
        // catalog sink observes a fresh starting state. (M11
        // ReplayModeManager will additionally clear its sink-side
        // state on backward seek, but the catalog sink here is
        // for any extensions encountered during the scan; the
        // header catalog itself is the open-time invariant.)
    }

    // Forward scan: peek each record header; for signal-value
    // records, stop when next record's timestamp >= targetNs;
    // dispatch catalog extensions to bound sink along the way.
    while (file_.pos() < recordsEnd_) {
        const qint64 recordStart = file_.pos();
        std::uint32_t recordType = 0;
        std::uint32_t payloadLen = 0;
        if (!readRecordHeader(recordType, payloadLen)) {
            atEnd_ = true;
            break;
        }
        if (file_.pos() + static_cast<qint64>(payloadLen) > recordsEnd_) {
            atEnd_ = true;
            break;
        }
        if (recordType == kRecordTypeSignalValue) {
            // Peek timestamp without consuming the record.
            if (payloadLen < 12) {
                SF_LOG_ERROR("SessionReader::seek: signal record payload too short ({})", payloadLen);
                return false;
            }
            std::uint32_t signalIdx = 0;
            std::int64_t recordTs = 0;
            if (!readLeFromFile(file_, signalIdx)) {
                return false;
            }
            if (!readLeFromFile(file_, recordTs)) {
                return false;
            }
            (void)signalIdx;
            if (recordTs >= targetNs) {
                // Rewind to record start so the next
                // readNextRecord re-parses this record fully.
                if (!file_.seek(recordStart)) {
                    return false;
                }
                return true;
            }
            // Not far enough yet — skip the rest of the value
            // payload (we already consumed signalIdx + ts = 12).
            const std::uint32_t remaining = payloadLen - 12;
            if (!skipPayload(remaining)) {
                return false;
            }
            currentTimestampNs_ = recordTs;
            ++currentRecordIdx_;
            ++recordsRead_;
        } else if (recordType == kRecordTypeCatalogExtension) {
            if (!decodeCatalogExtension(payloadLen)) {
                return false;
            }
        } else {
            // Markers / Heartbeats / unknown V2+: skip.
            if (!skipPayload(payloadLen)) {
                return false;
            }
        }
    }
    atEnd_ = true;
    return true;
}

void SessionReader::preScanFooter() {
    footerRecordCount_ = 0;
    lastTimestampNs_ = 0;

    // Footer presence check.
    if (fileSize_ < headerLen_ + kFooterSize) {
        return;
    }
    const qint64 footerOffset = fileSize_ - kFooterSize;
    if (!file_.seek(footerOffset)) {
        return;
    }
    QByteArray footer = file_.read(kFooterSize);
    if (footer.size() != kFooterSize) {
        return;
    }
    if (std::memcmp(footer.constData(), kFooterMagic, 8) != 0) {
        if (!file_.seek(headerLen_)) {
            // Unable to recover; surface via SF_LOG.
            SF_LOG_WARN("SessionReader::preScanFooter: seek back to header end failed");
        }
        return;
    }
    footerRecordCount_ = static_cast<std::size_t>(readLe<std::uint32_t>(footer, 8));

    // Walk records to find the last record's timestamp. We do
    // this once at open time so `seekToTimestamp` can clamp
    // without scanning twice.
    if (!file_.seek(headerLen_)) {
        return;
    }
    std::vector<signalforge::decoder::SignalMetadata> tempCatalog = runningCatalog_;
    std::int64_t lastTs = 0;
    while (file_.pos() < recordsEnd_) {
        std::uint32_t recordType = 0;
        std::uint32_t payloadLen = 0;
        if (file_.pos() + kRecordHeaderSize > recordsEnd_) {
            break;
        }
        if (!readLeFromFile(file_, recordType)) {
            break;
        }
        if (!readLeFromFile(file_, payloadLen)) {
            break;
        }
        if (file_.pos() + static_cast<qint64>(payloadLen) > recordsEnd_) {
            break;
        }
        if (recordType == kRecordTypeSignalValue) {
            if (payloadLen < 12) {
                break;
            }
            std::uint32_t signalIdx = 0;
            std::int64_t ts = 0;
            if (!readLeFromFile(file_, signalIdx)) {
                break;
            }
            if (!readLeFromFile(file_, ts)) {
                break;
            }
            (void)signalIdx;
            const std::uint32_t remaining = payloadLen - 12;
            if (!file_.seek(file_.pos() + static_cast<qint64>(remaining))) {
                break;
            }
            lastTs = ts;
        } else {
            // Catalog extensions / markers / heartbeats are
            // skipped; they don't carry signal-value timestamps.
            if (!file_.seek(file_.pos() + static_cast<qint64>(payloadLen))) {
                break;
            }
        }
    }
    lastTimestampNs_ = lastTs;

    // Restore catalog mutated nothing here, but reset position
    // to header end for the streaming caller.
    runningCatalog_ = tempCatalog;
    if (!file_.seek(headerLen_)) {
        SF_LOG_WARN("SessionReader::preScanFooter: failed to reset position to headerLen={}", headerLen_);
    }
}

bool SessionReader::replayAll(signalforge::decoder::SignalValueSink& sink) {
    if (!file_.isOpen()) {
        SF_LOG_ERROR("SessionReader::replayAll called without an open file");
        return false;
    }

    // Reset stream state — the caller may have used streaming
    // APIs before invoking replayAll.
    if (!file_.seek(headerLen_)) {
        return false;
    }
    runningCatalog_ = headerCatalog_;
    recordsRead_ = 0;
    currentRecordIdx_ = 0;
    currentTimestampNs_ = 0;
    fileComplete_ = false;
    atEnd_ = false;

    // Initial catalog announcement.
    sink.onSignalsRegistered(QStringLiteral("session-replay"), runningCatalog_);

    // Streaming mode dispatches catalog extensions through
    // `catalogSink_`. For replayAll we want the same sink to
    // receive both per-record signals AND extension-time catalog
    // updates, so we bind it for the duration of the call.
    auto* prevCatalogSink = catalogSink_;
    catalogSink_ = &sink;

    ReplayRecord rec;
    while (readNextRecord(rec)) {
        const auto t = replayOriginSteady_ + std::chrono::nanoseconds{rec.timestampNs};
        sink.onSignal(t, rec.signalId, rec.value);
    }

    catalogSink_ = prevCatalogSink;

    // Footer parsing — non-fatal if missing. Mirrors the M10
    // semantics so the round-trip test sees fileComplete() == true
    // for complete files.
    if (fileSize_ - file_.pos() >= kFooterSize) {
        QByteArray footer = file_.read(kFooterSize);
        if (footer.size() == kFooterSize && std::memcmp(footer.constData(), kFooterMagic, 8) == 0) {
            const std::uint32_t totalRecords = readLe<std::uint32_t>(footer, 8);
            fileComplete_ = (totalRecords == static_cast<std::uint32_t>(recordsRead_));
            if (!fileComplete_) {
                SF_LOG_WARN("SessionReader: footer.totalRecords={} but actually read {}", totalRecords, recordsRead_);
            }
        } else {
            SF_LOG_WARN("SessionReader: trailing 16 bytes are not a valid footer");
        }
    } else {
        SF_LOG_INFO("SessionReader: file is incomplete (no footer)");
    }
    return fileComplete_;
}

std::size_t SessionReader::recordsRead() const noexcept {
    return recordsRead_;
}

bool SessionReader::fileComplete() const noexcept {
    return fileComplete_;
}

}  // namespace signalforge::session
