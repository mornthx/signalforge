// src/buffer/signal_buffer_registry.hpp
#pragma once

#include "buffer/signal_buffer.hpp"
#include "decode/decoder_interface.hpp"  // For SignalValueSink

#include <QString>
#include <QStringList>
#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace signalforge::buffer {

/// Registry-level configuration.
struct RegistryConfig {
    /// Total memory budget in bytes. Default 256 MB.
    /// Registrations that would exceed this are rejected.
    std::size_t totalBudgetBytes = 256ULL * 1024 * 1024;

    /// Per-signal default config. May be overridden per signal at registration.
    SignalBufferConfig signalDefaults;

    /// If true, attempts to register a signal that would exceed budget log
    /// ERROR and return failure. If false, log ERROR but allow registration.
    /// Default: true (strict).
    bool rejectOnBudgetExceeded = true;
};

/// Per-driver per-signal config override map. Keyed by signalId.
/// Used at registration to override registry defaults for specific signals.
using SignalConfigOverrides = std::unordered_map<QString, SignalBufferConfig>;

/// Registry of all `SignalBuffer` instances, implementing `SignalValueSink`.
///
/// One registry per app process; instantiated at app startup, passed to
/// `DecoderRegistrar` as the production sink (M5's `LoggingSignalValueSink`
/// is replaced).
///
/// Threading: thread-safe across all public methods.
///
/// Lifetime: outlives all decoders; destruction releases all signal buffers.
///
/// Freeze scope: this header is frozen at M6 close. See spec §6.1.
class SignalBufferRegistry : public signalforge::decoder::SignalValueSink {
public:
    explicit SignalBufferRegistry(RegistryConfig config = {});
    ~SignalBufferRegistry() override;

    SignalBufferRegistry(const SignalBufferRegistry&) = delete;
    SignalBufferRegistry& operator=(const SignalBufferRegistry&) = delete;

    // SignalValueSink overrides

    void onSignal(std::chrono::steady_clock::time_point timestamp, const QString& signalId,
                  const signalforge::decoder::SignalValue& value) override;

    void onSignalsRegistered(const QString& driverId,
                             const std::vector<signalforge::decoder::SignalMetadata>& signalsList) override;

    void onSignalsUnregistered(const QString& driverId) override;

    // Query API

    /// Look up a buffer. Returns nullptr if signalId not registered.
    /// Returned pointer remains valid until the signal is unregistered.
    [[nodiscard]] SignalBuffer* bufferFor(const QString& signalId) const;

    /// All currently-registered signal IDs.
    [[nodiscard]] QStringList signalIds() const;

    /// All signal IDs from a specific driver.
    [[nodiscard]] QStringList signalIdsForDriver(const QString& driverId) const;

    /// Per-driver registration override. Set BEFORE the driver's signals are
    /// registered. Subsequent `onSignalsRegistered` calls for this driver use
    /// these overrides.
    void setDriverConfigOverrides(const QString& driverId, const SignalConfigOverrides& overrides);

    // Status

    [[nodiscard]] std::size_t totalMemoryBytes() const;
    [[nodiscard]] std::size_t totalBudgetBytes() const;
    [[nodiscard]] std::size_t signalCount() const;

    /// Returns a struct with per-driver and per-signal memory usage.
    /// Useful for diagnostic UI and the M8 performance panel.
    struct UsageReport {
        std::size_t totalBytes;
        std::size_t budgetBytes;
        struct PerDriver {
            QString driverId;
            std::size_t bytes;
            int signalCount;
        };
        std::vector<PerDriver> drivers;
    };
    [[nodiscard]] UsageReport memoryUsage() const;

private:
    /// Internal bookkeeping (metrics + soft-warn one-shot flag). Defined in
    /// the .cpp so the registry .hpp does not pull in observability headers.
    struct Bookkeeping;

    RegistryConfig config_;
    mutable std::mutex registryMutex_;
    std::unordered_map<QString, std::unique_ptr<SignalBuffer>> buffersBySignalId_;
    std::unordered_map<QString, QStringList> signalsByDriverId_;  // driverId -> [signalId, ...]
    std::unordered_map<QString, SignalConfigOverrides> driverOverrides_;
    std::unordered_map<QString, std::size_t> signalEstimates_;  // estimated bytes per signal at registration
    std::atomic<std::size_t> totalBytes_{0};
    std::unique_ptr<Bookkeeping> bookkeeping_;
};

}  // namespace signalforge::buffer
