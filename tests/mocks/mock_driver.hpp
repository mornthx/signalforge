// tests/mocks/mock_driver.hpp
#pragma once

#include "drivers/driver_interface.hpp"

#include <QByteArray>
#include <QString>
#include <atomic>

namespace signalforge::test {

/// Simple in-process mock of `DriverInterface`. Does not spawn a
/// separate thread by default; lifecycle transitions are synchronous.
/// Used to verify the interface contract: state transitions, signal
/// emissions, write-in-wrong-state errors, statistics snapshots.
///
/// For thread-affinity testing (S11 integration test), callers can
/// `moveToThread()` this object onto a dedicated `QThread`; then
/// signal emission happens from that thread.
class MockDriver : public signalforge::drivers::DriverInterface {
    Q_OBJECT

public:
    explicit MockDriver(QObject* parent = nullptr);
    ~MockDriver() override;

    signalforge::drivers::DriverErrorCode open() override;
    void close() override;
    signalforge::drivers::DriverErrorCode start() override;
    void stop() override;
    signalforge::drivers::DriverErrorCode write(const QByteArray& payload) override;

    [[nodiscard]] signalforge::drivers::DriverState state() const override;
    [[nodiscard]] signalforge::drivers::DriverErrorCode health() const override;
    [[nodiscard]] signalforge::frame::DriverStatistics statistics() const override;

    /// Test helper: emit one fabricated frame from the current thread.
    /// Intended for tests that want deterministic frame arrivals.
    void emitFrame(const QByteArray& payload);

    /// Test helper: force a given error state (e.g., for recovery tests).
    void injectError(signalforge::drivers::DriverErrorCode code, const QString& msg);

    /// Test helper: force `open()` to synchronously fail with the given
    /// code. Useful for verifying that a `NotConfigured`/`ConfigInvalid`
    /// return does not transition state.
    void setPreOpenFailure(signalforge::drivers::DriverErrorCode code);

private:
    void transitionTo(signalforge::drivers::DriverState newState);

    std::atomic<signalforge::drivers::DriverState> state_{signalforge::drivers::DriverState::Idle};
    std::atomic<signalforge::drivers::DriverErrorCode> health_{signalforge::drivers::DriverErrorCode::Success};

    std::atomic<std::uint64_t> rxFramesTotal_{0};
    std::atomic<std::uint64_t> rxBytesTotal_{0};
    std::atomic<std::uint64_t> txFramesTotal_{0};
    std::atomic<std::uint64_t> txBytesTotal_{0};

    signalforge::drivers::DriverErrorCode preOpenFailure_ = signalforge::drivers::DriverErrorCode::Success;
};

}  // namespace signalforge::test
