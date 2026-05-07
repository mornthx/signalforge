// tools/sfreplay_inspect/main.cpp
//
// Usage:
//   sfreplay_inspect <file.sfreplay> [--json]
//
// Read-only inspector for SFREPLAY v1 files. Prints the header,
// signal catalog, record histogram, and footer status. Mirrors
// the M5 schema_lint and M7 expr_lint pattern.
//
// Exit codes:
//   0 — file parses cleanly (footer may be absent for incomplete files)
//   1 — file is malformed (bad magic / unsupported version / truncated record)
//   2 — usage error (missing path / unknown flag / unreadable file)
//
// Independent of the M10 SessionReader: this tool does its own
// byte-level parse so it doubles as a third-party reference
// implementation of the format spec at docs/format/sfreplay-v1.md.

#include <QByteArray>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QTimeZone>
#include <QtEndian>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

constexpr const char* kSFReplayMagic = "SFREPLAY";
constexpr std::uint32_t kFormatVersion = 1;
constexpr const char* kFooterMagic = "REPLAYEO";  // 8 bytes per spec §7.1
constexpr qint64 kFooterSize = 16;
constexpr qint64 kFixedHeaderSize = 16;

void printUsage(std::ostream& out) {
    out << "Usage: sfreplay_inspect <file.sfreplay> [--json]\n"
        << "\n"
        << "Read-only inspector for SFREPLAY v1 files (M10 format).\n"
        << "\n"
        << "Options:\n"
        << "  --json    Emit machine-readable JSON instead of human-readable output.\n"
        << "\n"
        << "Exit codes:\n"
        << "  0  file parses cleanly (footer may be absent for incomplete files)\n"
        << "  1  file malformed (bad magic / version / truncated record)\n"
        << "  2  usage error (missing path / unknown flag / unreadable file)\n";
}

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

template <typename T> bool readLeFromFile(QFile& f, T& out) {
    QByteArray b = f.read(sizeof(T));
    if (b.size() != static_cast<qsizetype>(sizeof(T))) {
        return false;
    }
    out = readLe<T>(b, 0);
    return true;
}

bool readUtf8(QFile& f, QString& out) {
    std::uint32_t len = 0;
    if (!readLeFromFile(f, len)) {
        return false;
    }
    if (len == 0) {
        out.clear();
        return true;
    }
    QByteArray b = f.read(static_cast<qint64>(len));
    if (b.size() != static_cast<qsizetype>(len)) {
        return false;
    }
    out = QString::fromUtf8(b);
    return true;
}

const char* typeName(std::uint8_t tag) {
    switch (tag) {
    case 0:
        return "Bool";
    case 1:
        return "Int64";
    case 2:
        return "Double";
    case 3:
        return "String";
    default:
        return "Unknown";
    }
}

struct SignalEntry {
    QString id;
    QString name;
    QString unit;
    QString description;
    std::uint8_t typeTag = 0;
    bool hasScale = false;
    double scale = 0;
    bool hasOffset = false;
    double offset = 0;
};

bool parseSignalEntry(QFile& f, SignalEntry& out) {
    if (!readUtf8(f, out.id))
        return false;
    if (!readUtf8(f, out.name))
        return false;
    if (!readUtf8(f, out.unit))
        return false;
    if (!readUtf8(f, out.description))
        return false;
    QByteArray t = f.read(1);
    if (t.size() != 1)
        return false;
    out.typeTag = static_cast<std::uint8_t>(t[0]);
    QByteArray hs = f.read(1);
    if (hs.size() != 1)
        return false;
    out.hasScale = (hs[0] != 0);
    if (out.hasScale) {
        if (!readLeFromFile(f, out.scale))
            return false;
    }
    QByteArray ho = f.read(1);
    if (ho.size() != 1)
        return false;
    out.hasOffset = (ho[0] != 0);
    if (out.hasOffset) {
        if (!readLeFromFile(f, out.offset))
            return false;
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    bool jsonMode = false;
    QString path;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--json") {
            jsonMode = true;
        } else if (arg == "-h" || arg == "--help") {
            printUsage(std::cout);
            return 0;
        } else if (path.isEmpty()) {
            path = QString::fromStdString(arg);
        } else {
            std::fprintf(stderr, "Unknown argument: %s\n", arg.c_str());
            printUsage(std::cerr);
            return 2;
        }
    }
    if (path.isEmpty()) {
        printUsage(std::cerr);
        return 2;
    }

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        std::fprintf(stderr, "Cannot open: %s (%s)\n", path.toStdString().c_str(),
                     f.errorString().toStdString().c_str());
        return 2;
    }
    const qint64 fileSize = f.size();

    // Fixed prefix
    QByteArray fixed = f.read(kFixedHeaderSize);
    if (fixed.size() != kFixedHeaderSize) {
        std::fprintf(stderr, "File too short for fixed header.\n");
        return 1;
    }
    if (std::memcmp(fixed.constData(), kSFReplayMagic, 8) != 0) {
        std::fprintf(stderr, "Bad magic: not an SFREPLAY file.\n");
        return 1;
    }
    const std::uint32_t formatVersion = readLe<std::uint32_t>(fixed, 8);
    if (formatVersion != kFormatVersion) {
        std::fprintf(stderr, "Unsupported formatVersion=%u (expected 1).\n", formatVersion);
        return 1;
    }
    const qint64 headerLen = static_cast<qint64>(readLe<std::uint32_t>(fixed, 12));
    if (headerLen < kFixedHeaderSize || headerLen > fileSize) {
        std::fprintf(stderr, "Invalid headerLen=%lld (fileSize=%lld).\n", static_cast<long long>(headerLen),
                     static_cast<long long>(fileSize));
        return 1;
    }

    // Variable section
    std::int64_t recordedAtNs = 0;
    if (!readLeFromFile(f, recordedAtNs)) {
        return 1;
    }
    QString description, schemaId;
    if (!readUtf8(f, description) || !readUtf8(f, schemaId)) {
        return 1;
    }
    std::uint32_t signalCount = 0;
    if (!readLeFromFile(f, signalCount)) {
        return 1;
    }
    std::vector<SignalEntry> catalog;
    catalog.reserve(signalCount);
    for (std::uint32_t i = 0; i < signalCount; ++i) {
        SignalEntry e;
        if (!parseSignalEntry(f, e)) {
            std::fprintf(stderr, "Truncated signal catalog entry %u.\n", i);
            return 1;
        }
        catalog.push_back(std::move(e));
    }
    if (f.pos() != headerLen) {
        // Skip any V2 trailer fields we don't understand.
        f.seek(headerLen);
    }

    // Pre-check whether the last 16 bytes look like the footer.
    // Footerless files (incomplete recordings, per spec §9) must
    // have records walked all the way to EOF; the loop below
    // can't tell records from a missing footer otherwise.
    bool tailLooksLikeFooter = false;
    if (fileSize >= headerLen + kFooterSize) {
        const qint64 saved = f.pos();
        if (f.seek(fileSize - kFooterSize)) {
            QByteArray maybeFooter = f.read(8);
            if (maybeFooter.size() == 8 && std::memcmp(maybeFooter.constData(), kFooterMagic, 8) == 0) {
                tailLooksLikeFooter = true;
            }
        }
        f.seek(saved);
    }

    // Records — stop before the footer when one is present;
    // walk to EOF when it isn't.
    const qint64 recordsEnd = tailLooksLikeFooter ? fileSize - kFooterSize : fileSize;
    std::unordered_map<std::uint32_t, std::uint64_t> typeHistogram;
    std::uint32_t catalogExtensionAdded = 0;
    std::int64_t firstTimestampNs = -1;
    std::int64_t lastTimestampNs = -1;
    std::uint64_t totalRecordsParsed = 0;
    bool truncatedRecord = false;

    while (f.pos() + 8 <= recordsEnd) {
        std::uint32_t recordType = 0;
        std::uint32_t payloadLen = 0;
        if (!readLeFromFile(f, recordType) || !readLeFromFile(f, payloadLen)) {
            truncatedRecord = true;
            break;
        }
        if (f.pos() + static_cast<qint64>(payloadLen) > recordsEnd) {
            truncatedRecord = true;
            break;
        }
        // Peek timestamp for the time-bracketing summary.
        if (recordType == 1 && payloadLen >= 12) {
            const auto rec = f.read(static_cast<qint64>(payloadLen));
            if (rec.size() < 12) {
                truncatedRecord = true;
                break;
            }
            const std::int64_t ts = readLe<std::int64_t>(rec, 4);
            if (firstTimestampNs < 0)
                firstTimestampNs = ts;
            lastTimestampNs = ts;
        } else if (recordType == 2 && payloadLen >= 4) {
            const auto rec = f.read(static_cast<qint64>(payloadLen));
            if (rec.size() < 4) {
                truncatedRecord = true;
                break;
            }
            catalogExtensionAdded += readLe<std::uint32_t>(rec, 0);
        } else if (recordType == 4 && payloadLen >= 8) {
            const auto rec = f.read(static_cast<qint64>(payloadLen));
            if (rec.size() < 8) {
                truncatedRecord = true;
                break;
            }
            const std::int64_t ts = readLe<std::int64_t>(rec, 0);
            if (firstTimestampNs < 0)
                firstTimestampNs = ts;
            lastTimestampNs = ts;
        } else {
            if (!f.seek(f.pos() + static_cast<qint64>(payloadLen))) {
                truncatedRecord = true;
                break;
            }
        }
        ++typeHistogram[recordType];
        ++totalRecordsParsed;
    }

    // Footer parsing — only attempt if the up-front tail check
    // already said the trailing bytes are a footer (otherwise the
    // file is incomplete and the records loop already walked to
    // EOF).
    bool footerPresent = false;
    std::uint32_t footerTotalRecords = 0;
    if (!truncatedRecord && tailLooksLikeFooter) {
        if (f.seek(fileSize - kFooterSize)) {
            QByteArray footer = f.read(kFooterSize);
            if (footer.size() == kFooterSize && std::memcmp(footer.constData(), kFooterMagic, 8) == 0) {
                footerPresent = true;
                footerTotalRecords = readLe<std::uint32_t>(footer, 8);
            }
        }
    }
    const bool fileComplete = footerPresent && (footerTotalRecords == totalRecordsParsed);

    // Output
    if (jsonMode) {
        nlohmann::json j;
        j["path"] = path.toStdString();
        j["size"] = fileSize;
        j["format_version"] = formatVersion;
        j["header_length"] = headerLen;
        j["recorded_at_ns"] = recordedAtNs;
        j["description"] = description.toStdString();
        j["schema_id"] = schemaId.toStdString();
        j["initial_signal_count"] = signalCount;
        j["catalog_extension_added"] = catalogExtensionAdded;
        nlohmann::json sigs = nlohmann::json::array();
        for (const auto& e : catalog) {
            nlohmann::json s;
            s["id"] = e.id.toStdString();
            s["name"] = e.name.toStdString();
            s["unit"] = e.unit.toStdString();
            s["description"] = e.description.toStdString();
            s["type"] = typeName(e.typeTag);
            if (e.hasScale)
                s["scale"] = e.scale;
            if (e.hasOffset)
                s["offset"] = e.offset;
            sigs.push_back(s);
        }
        j["signals"] = sigs;
        nlohmann::json hist;
        hist["1_signal_value"] = typeHistogram[1];
        hist["2_catalog_extension"] = typeHistogram[2];
        hist["3_marker"] = typeHistogram[3];
        hist["4_heartbeat"] = typeHistogram[4];
        j["record_histogram"] = hist;
        j["records_parsed"] = totalRecordsParsed;
        j["footer_present"] = footerPresent;
        j["footer_total_records"] = footerTotalRecords;
        j["file_complete"] = fileComplete;
        if (firstTimestampNs >= 0)
            j["first_timestamp_ns"] = firstTimestampNs;
        if (lastTimestampNs >= 0)
            j["last_timestamp_ns"] = lastTimestampNs;
        std::cout << j.dump(2) << "\n";
        return truncatedRecord ? 1 : 0;
    }

    std::printf("File: %s (%lld bytes)\n", path.toStdString().c_str(), static_cast<long long>(fileSize));
    std::printf("Format: SFREPLAY v%u\n", formatVersion);
    if (recordedAtNs > 0) {
        const auto millis = recordedAtNs / 1000000;
        std::printf("Recorded:  %s UTC (ns since epoch: %lld)\n",
                    QDateTime::fromMSecsSinceEpoch(millis, QTimeZone::utc())
                        .toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"))
                        .toStdString()
                        .c_str(),
                    static_cast<long long>(recordedAtNs));
    }
    std::printf("Description: \"%s\"\n", description.toStdString().c_str());
    std::printf("Schema ID:   \"%s\"\n", schemaId.toStdString().c_str());
    std::printf("Header length: %lld bytes\n\n", static_cast<long long>(headerLen));

    std::printf("Signal catalog (initial: %u; extensions added: %u):\n", signalCount, catalogExtensionAdded);
    for (const auto& e : catalog) {
        std::printf("  %-30s  %-12s  unit=\"%s\"  type=%s", e.id.toStdString().c_str(), e.name.toStdString().c_str(),
                    e.unit.toStdString().c_str(), typeName(e.typeTag));
        if (e.hasScale)
            std::printf("  scale=%g", e.scale);
        if (e.hasOffset)
            std::printf("  offset=%g", e.offset);
        std::printf("\n");
    }
    if (signalCount == 0) {
        std::printf("  (empty)\n");
    }
    std::printf("\n");

    std::printf("Records: %llu total\n", static_cast<unsigned long long>(totalRecordsParsed));
    std::printf("  Type 1 (Signal Value):       %llu\n", static_cast<unsigned long long>(typeHistogram[1]));
    std::printf("  Type 2 (Catalog Extension):  %llu\n", static_cast<unsigned long long>(typeHistogram[2]));
    std::printf("  Type 3 (Marker):             %llu\n", static_cast<unsigned long long>(typeHistogram[3]));
    std::printf("  Type 4 (Heartbeat):          %llu\n", static_cast<unsigned long long>(typeHistogram[4]));

    if (firstTimestampNs >= 0 && lastTimestampNs >= 0) {
        const std::int64_t span = lastTimestampNs - firstTimestampNs;
        std::printf("Time span: %lld ns (%.3f s)\n", static_cast<long long>(span), span / 1e9);
    }

    if (footerPresent) {
        std::printf("Footer: present (totalRecords=%u; recordsParsed=%llu)\n", footerTotalRecords,
                    static_cast<unsigned long long>(totalRecordsParsed));
        std::printf("File status: %s\n", fileComplete ? "complete" : "footer-totalRecords-mismatch");
    } else {
        std::printf("Footer: ABSENT (file is incomplete — recording was interrupted)\n");
    }

    if (truncatedRecord) {
        std::printf("WARNING: a record at offset ~%lld was truncated; file is malformed or interrupted.\n",
                    static_cast<long long>(f.pos()));
    }

    return truncatedRecord ? 1 : 0;
}
