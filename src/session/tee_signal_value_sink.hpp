// src/session/tee_signal_value_sink.hpp
#pragma once

#include "decode/decoder_interface.hpp"

#include <QString>
#include <chrono>
#include <mutex>
#include <vector>

namespace signalforge::session {

/// Fan-out `SignalValueSink` adapter. Forwards every incoming
/// callback (`onSignal` / `onSignalsRegistered` /
/// `onSignalsUnregistered`) to every registered downstream sink.
///
/// Use case: M10's MainWindow integration. The pipeline /
/// DecoderRegistrar (M5) accepts a single `SignalValueSink`. The
/// SignalBufferRegistry (M6) is the primary sink. To also feed
/// signals to a `SessionWriter` (M10) without modifying any
/// frozen interface, MainWindow constructs a `TeeSignalValueSink`,
/// pre-registers the buffer registry, and dynamically adds /
/// removes the writer when recording starts / stops.
///
/// Threading: the ownership pattern is the caller's
/// responsibility. `addSink` / `removeSink` and the sink-callback
/// methods are mutually mutex-protected, so the fan-out is safe
/// against concurrent membership changes.
///
/// Not part of the M10 freeze surface (per spec §6.2 only
/// SessionWriter, SessionMetadata, and the format spec are
/// frozen). The TeeSink is internal to the application's wiring.
class TeeSignalValueSink : public signalforge::decoder::SignalValueSink {
public:
    TeeSignalValueSink() = default;
    ~TeeSignalValueSink() override = default;

    TeeSignalValueSink(const TeeSignalValueSink&) = delete;
    TeeSignalValueSink& operator=(const TeeSignalValueSink&) = delete;

    /// Register a downstream sink. The pointer must outlive the
    /// TeeSignalValueSink, or be removed via `removeSink` first.
    void addSink(signalforge::decoder::SignalValueSink* sink);

    /// Remove a previously-registered downstream sink. Safe to
    /// call with a sink that was never added (no-op).
    void removeSink(signalforge::decoder::SignalValueSink* sink);

    /// How many downstream sinks are currently registered.
    [[nodiscard]] std::size_t size() const;

    // SignalValueSink overrides

    void onSignal(std::chrono::steady_clock::time_point timestamp, const QString& signalId,
                  const signalforge::decoder::SignalValue& value) override;

    void onSignalsRegistered(const QString& driverId,
                             const std::vector<signalforge::decoder::SignalMetadata>& signalsList) override;

    void onSignalsUnregistered(const QString& driverId) override;

private:
    mutable std::mutex mutex_;
    std::vector<signalforge::decoder::SignalValueSink*> sinks_;
};

}  // namespace signalforge::session
