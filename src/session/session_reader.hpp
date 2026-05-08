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

/// Reads SFREPLAY v1 files (the format M10 SessionWriter produces)
/// and replays the recorded signal values to a `SignalValueSink`.
///
/// Per ADR-007, this class is the round-trip target for M10
/// integration tests (HALT trigger #2 gate). It is also the input
/// path the future M11 Replay UX will use.
///
/// V1 scope:
///   - Synchronous `replayAll(sink)` reads the entire file in order
///     and dispatches events.
///   - No streaming / pause / scrubbing / playbackSpeed (M11
///     adds those when it needs them).
///
/// Tolerates truncated files (no footer or partial last record)
/// per spec §9 — replays everything up to the truncation and
/// returns false to indicate the file was incomplete.
///
/// Not part of the M10 freeze surface (per spec §6.2 only the
/// writer-side plus the format spec are frozen). M11 may extend
/// the public API additively.
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
    [[nodiscard]] bool replayAll(signalforge::decoder::SignalValueSink& sink);

    /// Total records dispatched in the most recent `replayAll`.
    [[nodiscard]] std::size_t recordsRead() const noexcept;

    /// True iff the most recent `replayAll` saw a complete footer
    /// matching `totalRecords`.
    [[nodiscard]] bool fileComplete() const noexcept;

private:
    bool readHeader();
    bool readSignalEntry(signalforge::decoder::SignalMetadata& out);
    bool readSignalValueRecord(signalforge::decoder::SignalValueSink& sink, std::uint32_t payloadLen);
    bool readCatalogExtensionRecord(signalforge::decoder::SignalValueSink& sink, std::uint32_t payloadLen);
    bool skipPayload(std::uint32_t payloadLen);

    QFile file_;
    SessionMetadata metadata_;
    std::vector<signalforge::decoder::SignalMetadata> runningCatalog_;
    std::chrono::steady_clock::time_point replayOriginSteady_{};
    std::size_t recordsRead_ = 0;
    bool fileComplete_ = false;
    qint64 headerLen_ = 0;
    qint64 fileSize_ = 0;
};

}  // namespace signalforge::session
