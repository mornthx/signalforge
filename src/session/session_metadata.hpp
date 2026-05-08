// src/session/session_metadata.hpp
#pragma once

#include "decode/decoder_interface.hpp"

#include <QString>
#include <chrono>
#include <optional>
#include <vector>

namespace signalforge::session {

/// Recording state for `SessionWriter`.
///
/// The 3-state lifecycle: a writer starts in `Idle`, transitions to
/// `Recording` on a successful `start(filePath)`, and either returns
/// to `Idle` on `stop()` or transitions to `Error` on an
/// unrecoverable disk / queue / lifecycle fault. From `Error`, the
/// next successful `start()` re-enters `Recording`.
///
/// Frozen at M10 close.
enum class RecordingState : int {
    Idle = 0,
    Recording = 1,
    Error = 2,
};

/// In-memory representation of a recording session's metadata.
///
/// Mirrors the SFREPLAY v1 file header (per spec §4.1 + ADR-007).
/// The fields below are the V1 frozen contract; any extension lives
/// in V2 (additive, V1 readers ignore unknown trailing fields).
///
/// Per M10 spec §3.5, this struct deliberately omits connection
/// config (driver type, host, port, baud rate, etc.). Replay needs
/// signals + decoder-schema reference, not connection details.
/// `M10-concerns.md` notes the §2.1-4 wording about "source
/// connection info (driver type + display name)" is superseded by
/// §3.5; only the §3.5 fields are persisted.
///
/// Frozen at M10 close.
struct SessionMetadata {
    /// Wall-clock origin of the recording, written into the file
    /// header's `recordedAt` field. Used only for human-readable
    /// display; ordering and arithmetic between events use steady
    /// clock (per arch §"Time"), not this value.
    std::chrono::system_clock::time_point recordedAt{};

    /// Steady-clock origin paired with `recordedAt`. Per-record
    /// timestamps in the file are nanos-since-recording-start; this
    /// value lets a reader convert them back to absolute steady
    /// clock for live continuation scenarios.
    std::chrono::steady_clock::time_point recordingStart{};

    /// Steady-clock end timestamp, populated on `SessionWriter::stop()`.
    /// Not written to the file (the implicit last-record timestamp +
    /// footer presence give the same information). `std::nullopt`
    /// while a recording is in progress.
    std::optional<std::chrono::steady_clock::time_point> recordingEnd{};

    /// Optional user-supplied human-readable annotation. UTF-8.
    /// Persisted in the file header's variable-length `description`
    /// field. Empty string is valid (encoded as `descLen=0`).
    QString description;

    /// Optional decoder schema reference (M5 schema id). Persisted
    /// in the file header's variable-length `schemaId` field.
    /// Empty string is valid (encoded as `schemaIdLen=0`); a reader
    /// should treat that as "no schema known — values are
    /// already-decoded primitives".
    QString decoderSchemaId;

    /// Snapshot of signal catalog at recording start. New signals
    /// registered mid-stream are added via Catalog Extension records
    /// (file format type 2) and **also** appended to this vector by
    /// the writer so that `SessionMetadata` always reflects the live
    /// catalog when queried via `SessionWriter::metadata()`.
    std::vector<signalforge::decoder::SignalMetadata> signalCatalog;
};

}  // namespace signalforge::session
