// src/session/session_reader.hpp
#pragma once

#include "decode/decoder_interface.hpp"
#include "session/session_metadata.hpp"

#include <QFile>
#include <QString>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace signalforge::session {

/// One signal-value record returned by `SessionReader::readNextRecord`.
/// Catalog-extension / heartbeat / marker records are consumed
/// transparently inside the reader and never surface here.
///
/// Added in M11 S2 (ADR-007 / `M11-concerns.md` C1.α — additive
/// extension to the M10 non-frozen reader surface).
struct ReplayRecord {
    /// Raw timestamp from the file — nanoseconds elapsed since
    /// the recording's start. Identical to the value the writer
    /// stored.
    std::int64_t timestampNs;

    /// Signal id resolved against the running catalog
    /// (header catalog + any catalog extensions seen so far).
    QString signalId;

    /// Decoded value, dispatched per the catalog's declared type.
    signalforge::decoder::SignalValue value;
};

/// Reads SFREPLAY v1 files (the format M10 SessionWriter produces).
/// Two delivery modes:
///
///   - Bulk: `replayAll(sink)` — fire-and-forget. M10 entry point.
///   - Streaming (M11): `readNextRecord(out)` returns one record
///     at a time; `seekToTimestamp` repositions; `currentRecordIndex`
///     and `currentTimestampNs` expose progress.
///
/// Per ADR-007, this class is the round-trip target for M10
/// integration tests (HALT trigger #2 gate) and the input path
/// for the M11 Replay UX.
///
/// Tolerates truncated files (no footer or partial last record)
/// per spec §9 — replays everything up to the truncation and
/// returns false from `replayAll` (or stops streaming) to indicate
/// incomplete state.
///
/// **Not part of the M10 freeze surface** (per M10 spec §6.2 only
/// the writer-side plus the format spec are frozen). M11 S2 extends
/// the public API additively per `.claude/M11-concerns.md` C1.α.
class SessionReader {
public:
    SessionReader();
    ~SessionReader();

    SessionReader(const SessionReader&) = delete;
    SessionReader& operator=(const SessionReader&) = delete;

    /// Open `filePath` for reading. Validates the SFREPLAY magic +
    /// `formatVersion=1` and parses the header (variable section +
    /// signal catalog). Returns false if the file is missing,
    /// unreadable, has wrong magic, declares an incompatible
    /// `formatVersion`, or the header runs past EOF.
    [[nodiscard]] bool open(const QString& filePath);

    /// Close the file. Idempotent.
    void close();

    /// True after a successful `open()` and before `close()`.
    [[nodiscard]] bool isOpen() const noexcept;

    /// Metadata parsed from the file header. `recordingStart` is
    /// reconstructed from `recordedAt` (the wall-clock origin in
    /// the file) so `replayAll` can deliver per-record timestamps
    /// in steady-clock space rooted at this instance's open time.
    [[nodiscard]] SessionMetadata metadata() const;

    /// Replay every record in order, pushing signal events to
    /// `sink`. The sink first receives an `onSignalsRegistered`
    /// call for the initial catalog (driverId =
    /// `"session-replay"`); each Catalog Extension produces a
    /// further `onSignalsRegistered` call (with the same driverId
    /// — the catalog is intentionally flat in V1).
    ///
    /// Per-event `onSignal` timestamps use a `steady_clock`
    /// origin captured at `open()`; per-record `timestampNs` from
    /// the file is added on top so the sink sees monotonically
    /// increasing timestamps that match the writer's relative
    /// ordering. (The absolute steady-clock value is not
    /// preserved across writer / reader processes; only relative
    /// ordering is.)
    ///
    /// Returns true if the file ended with a valid footer
    /// (complete recording); false if truncated (incomplete
    /// recording — replay still delivers everything up to the
    /// truncation).
    ///
    /// **M11 note**: implemented as a thin loop over
    /// `readNextRecord` so the M10 round-trip semantics are
    /// preserved exactly.
    [[nodiscard]] bool replayAll(signalforge::decoder::SignalValueSink& sink);

    // ── Streaming API (M11 S2) ──────────────────────────────

    /// Read the next signal-value record into `out`. Catalog
    /// extension records encountered during the scan are dispatched
    /// to the catalog sink set via `bindCatalogSink` (if any) and
    /// the running catalog is updated; markers / heartbeats are
    /// silently consumed.
    ///
    /// Returns false at end-of-file, on the start of the file
    /// footer, or on parse error. Sets `atEnd()` to true when the
    /// file's record region is exhausted (regardless of whether a
    /// footer is present).
    [[nodiscard]] bool readNextRecord(ReplayRecord& out);

    /// Reposition the reader so the next `readNextRecord` returns
    /// the first record whose `timestampNs >= targetNs`. Out-of-
    /// range targets are clamped to the file's record range.
    /// Backward seek re-opens the file at the post-header offset
    /// and walks forward (sequential scan; O(file)).
    ///
    /// Catalog extensions encountered during the scan are
    /// dispatched to the bound catalog sink (if any) so the sink
    /// observes the catalog state at the seek target. Returns
    /// false on I/O failure.
    [[nodiscard]] bool seekToTimestamp(std::int64_t targetNs);

    /// Ordinal of the most recently read signal-value record
    /// (0-based). Equals `recordsRead()` after a full streaming
    /// pass.
    [[nodiscard]] std::size_t currentRecordIndex() const noexcept;

    /// Timestamp (ns) of the most recently read signal-value
    /// record. Returns 0 before the first read.
    [[nodiscard]] std::int64_t currentTimestampNs() const noexcept;

    /// True after the file's record region has been exhausted
    /// (or a fatal parse error stopped the stream). Reset by
    /// `open()` and `seekToTimestamp` on backward seek.
    [[nodiscard]] bool atEnd() const noexcept;

    /// Set the sink that receives `onSignalsRegistered` callbacks
    /// for catalog-extension records seen during streaming reads.
    /// Pass `nullptr` to detach. The bound sink is independent of
    /// any sink passed to `replayAll`. Lifetime is the caller's
    /// responsibility.
    void bindCatalogSink(signalforge::decoder::SignalValueSink* sink);

    /// Total records dispatched in the most recent `replayAll`,
    /// or the number of signal-value records the streaming API
    /// has produced since the most recent `open` / backward seek.
    [[nodiscard]] std::size_t recordsRead() const noexcept;

    /// True iff the most recent `replayAll` saw a complete footer
    /// matching `totalRecords`.
    [[nodiscard]] bool fileComplete() const noexcept;

    /// Total signal-value records reported by the file's footer,
    /// or 0 if no footer is present. Available after `open()`.
    [[nodiscard]] std::size_t footerRecordCount() const noexcept;

    /// Last record's timestamp in ns, computed from the footer
    /// scan at `open()`. Returns 0 if the file has no footer.
    [[nodiscard]] std::int64_t lastTimestampNs() const noexcept;

private:
    bool readHeader();
    bool readSignalEntry(signalforge::decoder::SignalMetadata& out);

    /// Parse one record header (8 bytes). Returns false at EOF or
    /// when only the footer remains.
    bool readRecordHeader(std::uint32_t& recordType, std::uint32_t& payloadLen);

    /// Decode a signal-value record body (payloadLen bytes
    /// already accounted) into `out`. Updates `currentRecordIdx_`
    /// and `currentTimestampNs_` on success.
    bool decodeSignalValueRecord(std::uint32_t payloadLen, ReplayRecord& out);

    /// Decode a catalog-extension body. Updates `runningCatalog_`;
    /// dispatches `onSignalsRegistered` to `catalogSink_` if bound.
    bool decodeCatalogExtension(std::uint32_t payloadLen);

    bool skipPayload(std::uint32_t payloadLen);

    /// Pre-walk the file once at open() to capture footer record
    /// count + last record timestamp (so `durationNs` and `seek`
    /// have something to clamp against without scanning twice
    /// later). Always non-fatal — failures fall back to "no footer
    /// known".
    void preScanFooter();

    QFile file_;
    SessionMetadata metadata_;

    /// Initial catalog parsed from the header (immutable after
    /// open). Used as the reset target on backward seek.
    std::vector<signalforge::decoder::SignalMetadata> headerCatalog_;

    /// Live catalog: header catalog + any catalog extensions seen
    /// so far during the current scan. Truncated to `headerCatalog_`
    /// size on backward seek.
    std::vector<signalforge::decoder::SignalMetadata> runningCatalog_;

    /// Bound catalog sink for streaming-mode catalog extensions.
    signalforge::decoder::SignalValueSink* catalogSink_ = nullptr;

    std::chrono::steady_clock::time_point replayOriginSteady_{};
    std::size_t recordsRead_ = 0;
    std::size_t currentRecordIdx_ = 0;
    std::int64_t currentTimestampNs_ = 0;
    std::size_t footerRecordCount_ = 0;
    std::int64_t lastTimestampNs_ = 0;
    bool fileComplete_ = false;
    bool atEnd_ = false;
    qint64 headerLen_ = 0;
    qint64 fileSize_ = 0;
    qint64 recordsEnd_ = 0;  ///< File offset at which the record
                             ///< region stops (footer start, or EOF
                             ///< for footerless files).
};

}  // namespace signalforge::session
