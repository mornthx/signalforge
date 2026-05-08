// src/replay/session_player.hpp
#pragma once

#include "decode/decoder_interface.hpp"
#include "session/session_reader.hpp"

#include <QObject>
#include <QString>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

class QThread;

namespace signalforge::replay {

/// Wraps M10 `signalforge::session::SessionReader` with timing
/// control + a worker thread for dispatch.
///
/// One session at a time per V1. The worker thread owns the
/// reader; the main thread issues control commands via atomics.
/// Records are dispatched to the configured `SignalValueSink`
/// **on the main thread** via `Qt::QueuedConnection` so charts and
/// any other consumers receive events on the same thread that
/// produces them in live mode.
///
/// **API gap with M10**: M10 SessionReader as shipped exposes only
/// `replayAll(sink)` — no streaming, seek, or per-record accessor.
/// M11 extends `SessionReader` additively at S2 (per `.claude/M11-
/// concerns.md` C1 resolution α) so this class can drive it
/// record-by-record. SessionReader is **not** part of M10's
/// freeze surface (M10-done.md §Freezes), so this extension does
/// not require ADR-008.
///
/// Threading: caller (main thread) controls. Worker `QThread`
/// reads file + sleeps for timing. `setSpeed`, `pause`, `play`,
/// `stepForward`, and `stepBackward` are safe to call from the
/// main thread while the worker is active (atomic flags + queued
/// shutdown).
///
/// Freeze scope: this class (its public API + signals) is frozen
/// at M11 close. Modifications post-freeze require a new ADR.
class SessionPlayer : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SessionPlayer)

public:
    /// `sink` outlives this player. Receives both per-record
    /// `onSignal` callbacks and the `onSignalsRegistered` event for
    /// the file's catalog (driverId is `"session-replay"` —
    /// inherited from M10 `SessionReader::replayAll`).
    explicit SessionPlayer(signalforge::decoder::SignalValueSink& sink, QObject* parent = nullptr);
    ~SessionPlayer() override;

    // ── File management ─────────────────────────────────────

    /// Open `filePath` for replay. On success, parses the SFREPLAY
    /// v1 header + signal catalog, captures duration + record
    /// count, and emits `onSignalsRegistered` to the sink. Returns
    /// `false` on file open / parse error.
    [[nodiscard]] bool openFile(const QString& filePath);

    /// Close the file. Stops the worker if active. Idempotent.
    void closeFile();

    [[nodiscard]] bool isOpen() const noexcept;

    // ── Playback controls ───────────────────────────────────

    /// Begin (or resume) timed dispatch on the worker thread.
    void play();

    /// Pause; the worker exits its dispatch loop on the next
    /// iteration. Position is preserved.
    void pause();

    /// Synchronously dispatch the next record without timing
    /// delay. Returns `false` at end of file. Caller must not be
    /// `play()`-ing concurrently (state machine in
    /// `PlaybackController` enforces this).
    [[nodiscard]] bool stepForward();

    /// Move one record back. Re-reads from the nearest earlier
    /// point; O(file) per V1.
    [[nodiscard]] bool stepBackward();

    /// Seek to a timestamp (ns from `recordedAt`). Pauses if
    /// playing, repositions, resumes if it was playing. Returns
    /// `false` only on reader I/O failure; out-of-range targets
    /// are clamped to the file's range.
    [[nodiscard]] bool seek(std::int64_t timestampNs);

    /// Set speed multiplier; valid range [0.5, 10.0]. Out-of-range
    /// values are clamped + logged WARN. Atomic — takes effect on
    /// the worker's next iteration.
    void setSpeed(double factor);

    // ── State queries ───────────────────────────────────────

    [[nodiscard]] bool isPlaying() const noexcept;
    [[nodiscard]] double currentSpeed() const noexcept;
    [[nodiscard]] std::int64_t currentPositionNs() const noexcept;
    [[nodiscard]] std::size_t currentRecordIndex() const noexcept;
    [[nodiscard]] std::int64_t durationNs() const noexcept;
    [[nodiscard]] std::size_t totalRecords() const noexcept;
    [[nodiscard]] bool atEnd() const noexcept;

signals:
    /// Emitted at most every 33 ms (≈ 30 Hz) per spec §2.1-10.
    /// The last skipped position is captured before the next emit
    /// boundary so the UI never falls behind by more than one frame.
    void positionUpdated(std::int64_t timestampNs, std::size_t recordIndex);

    void endReached();
    void error(const QString& errorMessage);

private:
    signalforge::decoder::SignalValueSink* sink_;
    std::unique_ptr<signalforge::session::SessionReader> reader_;
    std::unique_ptr<QThread> workerThread_;
    QString currentFilePath_;

    // Steady-clock origin captured at openFile time. Each
    // dispatched record's timestamp is `openTimeSteady_ +
    // nanoseconds{rec.timestampNs}` so the sink sees monotonic
    // chrono timestamps consistent with live mode.
    std::chrono::steady_clock::time_point openTimeSteady_{};

    // Lock-free state shared with the worker thread.
    std::atomic<bool> playing_{false};
    std::atomic<double> speedFactor_{1.0};
    std::atomic<std::int64_t> currentPosNs_{0};
    std::atomic<std::size_t> currentRecordIdx_{0};
    std::atomic<std::int64_t> durationNs_{0};
    std::atomic<std::size_t> totalRecords_{0};
    std::atomic<bool> atEnd_{false};

    /// Record read from the reader but not yet dispatched (because
    /// pause() interrupted the inter-record sleep). Drained on
    /// resume so no record is lost across pause/play. Worker-only
    /// state — accessed without locking because pause() blocks
    /// until the worker has exited the loop.
    std::optional<signalforge::session::ReplayRecord> pendingRecord_;

    /// Worker-thread entry point — runs the timed-dispatch loop.
    /// Connected to the worker QThread's `started()` signal in
    /// `play()`. Exits when `playing_` flips to false, the file
    /// runs out, or the thread is interrupted.
    void dispatchLoop();
};

}  // namespace signalforge::replay
