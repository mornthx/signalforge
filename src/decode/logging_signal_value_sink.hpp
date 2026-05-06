// src/decode/logging_signal_value_sink.hpp
#pragma once

#include "decode/decoder_interface.hpp"

#include <QString>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace signalforge::decoder {

/// M5-only test / diagnostic sink. Logs each signal at INFO level and
/// keeps per-type atomic counters for test introspection. Not intended
/// for production use — M6's SignalBuffer-backed SignalValueSink is the
/// production target.
///
/// Not part of the M5 freeze surface (per M5 spec §6.2).
class LoggingSignalValueSink : public SignalValueSink {
public:
    LoggingSignalValueSink();

    void onSignal(std::chrono::steady_clock::time_point timestamp, const QString& signalId,
                  const SignalValue& value) override;

    void onSignalsRegistered(const QString& driverId, const std::vector<SignalMetadata>& signalsList) override;
    void onSignalsUnregistered(const QString& driverId) override;

    [[nodiscard]] std::uint64_t signalsReceived() const noexcept;
    [[nodiscard]] std::uint64_t signalsByType(SignalType t) const noexcept;
    [[nodiscard]] std::uint64_t registrationsReceived() const noexcept;
    [[nodiscard]] std::uint64_t unregistrationsReceived() const noexcept;

    /// Test helper: reset all counters.
    void resetCounters() noexcept;

private:
    std::atomic<std::uint64_t> signalsReceived_{0};
    std::atomic<std::uint64_t> registrationsReceived_{0};
    std::atomic<std::uint64_t> unregistrationsReceived_{0};
    // One counter per SignalType enum entry (Bool, Int64, Double, String).
    std::array<std::atomic<std::uint64_t>, 4> perType_;
};

}  // namespace signalforge::decoder
