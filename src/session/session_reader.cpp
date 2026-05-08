// src/session/session_reader.cpp
//
// SFREPLAY v1 reader. Parses files produced by SessionWriter
// (per docs/format/sfreplay-v1.md) and dispatches signal events
// to a SignalValueSink. The HALT-trigger-#2 gate test
// (M10 S7 round-trip) drives this class.
//
// Tolerates truncated files (no footer or partial last record)
// per spec §9 — replays everything up to the truncation and
// returns false from replayAll() to indicate incomplete state.
#include "session/session_reader.hpp"

#include "observability/logging.hpp"

#include <QByteArray>
#include <QtEndian>
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
    return true;
}

void SessionReader::close() {
    if (file_.isOpen()) {
        file_.close();
    }
    runningCatalog_.clear();
    headerLen_ = 0;
    fileSize_ = 0;
}

bool SessionReader::isOpen() const noexcept {
    return file_.isOpen();
}

SessionMetadata SessionReader::metadata() const {
    return metadata_;
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

bool SessionReader::replayAll(signalforge::decoder::SignalValueSink& sink) {
    recordsRead_ = 0;
    fileComplete_ = false;

    if (!file_.isOpen()) {
        SF_LOG_ERROR("SessionReader::replayAll called without an open file");
        return false;
    }

    // Initial catalog announcement.
    sink.onSignalsRegistered(QStringLiteral("session-replay"), runningCatalog_);

    // Walk records up to the start of the footer (or EOF for
    // truncated files).
    const qint64 recordsEnd = (fileSize_ >= headerLen_ + kFooterSize) ? fileSize_ - kFooterSize : fileSize_;

    while (file_.pos() + 8 <= recordsEnd) {
        std::uint32_t recordType = 0;
        std::uint32_t payloadLen = 0;
        if (!readLeFromFile(file_, recordType)) {
            break;
        }
        if (!readLeFromFile(file_, payloadLen)) {
            break;
        }
        if (file_.pos() + static_cast<qint64>(payloadLen) > recordsEnd) {
            // Partial / truncated record at end of file. Stop
            // replaying; report incomplete.
            SF_LOG_WARN("SessionReader: truncated record at offset {}; stopping replay", file_.pos() - 8);
            return false;
        }
        switch (recordType) {
        case kRecordTypeSignalValue:
            if (!readSignalValueRecord(sink, payloadLen)) {
                return false;
            }
            break;
        case kRecordTypeCatalogExtension:
            if (!readCatalogExtensionRecord(sink, payloadLen)) {
                return false;
            }
            break;
        case kRecordTypeMarker:
        case kRecordTypeHeartbeat:
        default:
            // V1 readers ignore Markers + Heartbeats per spec
            // §6.4 / §6.5; unknown V2+ record types are skipped
            // by payloadLen per §8 forward-compat.
            if (!skipPayload(payloadLen)) {
                return false;
            }
            break;
        }
        ++recordsRead_;
    }

    // Footer parsing — non-fatal if missing.
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

bool SessionReader::readSignalValueRecord(signalforge::decoder::SignalValueSink& sink, std::uint32_t payloadLen) {
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

    signalforge::decoder::SignalValue value;
    using T = signalforge::decoder::SignalType;
    switch (meta.type) {
    case T::Bool: {
        QByteArray b = file_.read(1);
        if (b.size() != 1) {
            return false;
        }
        (void)valueLen;
        value = (b[0] != 0);
        break;
    }
    case T::Int64: {
        std::int64_t n = 0;
        if (!readLeFromFile(file_, n)) {
            return false;
        }
        value = n;
        break;
    }
    case T::Double: {
        double d = 0;
        if (!readLeFromFile(file_, d)) {
            return false;
        }
        value = d;
        break;
    }
    case T::String: {
        QString s;
        if (!readUtf8FromFile(file_, s)) {
            return false;
        }
        value = s;
        break;
    }
    }

    const auto t = replayOriginSteady_ + std::chrono::nanoseconds{timestampNs};
    sink.onSignal(t, meta.id, value);
    return true;
}

bool SessionReader::readCatalogExtensionRecord(signalforge::decoder::SignalValueSink& sink, std::uint32_t payloadLen) {
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
    sink.onSignalsRegistered(QStringLiteral("session-replay"), added);
    return true;
}

bool SessionReader::skipPayload(std::uint32_t payloadLen) {
    return file_.seek(file_.pos() + static_cast<qint64>(payloadLen));
}

std::size_t SessionReader::recordsRead() const noexcept {
    return recordsRead_;
}

bool SessionReader::fileComplete() const noexcept {
    return fileComplete_;
}

}  // namespace signalforge::session
