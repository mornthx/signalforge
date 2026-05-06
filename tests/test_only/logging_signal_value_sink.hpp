// tests/test_only/logging_signal_value_sink.hpp
#pragma once

#include "decode/decoder_interface.hpp"

#include <QString>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace signalforge::decoder {

/// Test/diagnostic sink that logs each signal at INFO level and keeps
/// per-type atomic counters for test introspection. Originally lived in
/// `src/decode/` during M5 as the placeholder production sink. Relocated
/// to `tests/test_only/` in M6 (see plan §S8) once the production sink —
/// `signalforge::buffer::SignalBufferRegistry` — landed. Retained in the
/// `signalforge::decoder` namespace so M5-era tests need only an include
/// path update.
///
/// Production code paths must not depend on this header.
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
