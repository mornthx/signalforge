// src/drivers/serial_driver.hpp
#pragma once

#include "drivers/driver_configs.hpp"
#include "drivers/driver_interface.hpp"

#include <QObject>
#include <QString>
#include <QThread>
#include <atomic>
#include <cstdint>
#include <memory>

namespace signalforge::drivers {

class SerialIoWorker;

/// Serial port driver over `QSerialPort`, running IO on a dedicated
/// `QThread` via a private `SerialIoWorker`.
///
/// Typical usage:
///   SerialConfig cfg;
///   cfg.device = "/dev/ttyUSB0";
///   cfg.baudRate = 115200;
///   SerialDriver d{cfg};
///   connect(&d, &DriverInterface::frameReceived, ..., Qt::QueuedConnection);
///   d.open();   // async; wait for stateChanged(Open)
///   d.start();  // frames flow
///   d.write(payload);
///   d.stop();
///   d.close();  // blocks < 500 ms for IO thread join
///
/// Thread affinity matches spec §4.1: the driver lives on the caller's
/// thread (typically main); its `SerialIoWorker` is moved to a dedicated
/// `QThread`. Public signals are emitted from the driver's own thread
/// after translating worker signals via queued slots. Consumers on other
/// threads connect with `Qt::QueuedConnection`.
class SerialDriver : public DriverInterface {
    Q_OBJECT

public:
    explicit SerialDriver(SerialConfig config, QObject* parent = nullptr);
    ~SerialDriver() override;

    DriverErrorCode open() override;
    void close() override;
    DriverErrorCode start() override;
    void stop() override;
    DriverErrorCode write(const QByteArray& payload) override;

    [[nodiscard]] DriverState state() const override;
    [[nodiscard]] DriverErrorCode health() const override;
    [[nodiscard]] signalforge::frame::DriverStatistics statistics() const override;

    /// Current configuration. Returned by reference; callers must not
    /// retain the reference across the driver's destruction.
    [[nodiscard]] const SerialConfig& config() const noexcept;

private slots:
    void onWorkerOpened();
    void onWorkerClosed();
    void onWorkerStarted();
    void onWorkerStopped();
    void onWorkerErrorOccurred(const signalforge::drivers::DriverError& err);
    void onWorkerFrameReceived(const signalforge::frame::RawFrame& frame);
    void onWorkerTxComplete(std::uint64_t bytesWritten);
    void onWorkerTxFailure();

private:
    void transitionTo(DriverState newState);

    SerialConfig config_;
    std::unique_ptr<QThread> thread_;
    std::unique_ptr<SerialIoWorker> worker_;

    std::atomic<DriverState> state_{DriverState::Idle};
    std::atomic<DriverErrorCode> health_{DriverErrorCode::Success};

    // Stats: per-field atomics per M2 §4.2 clarification 3.
    std::atomic<std::uint64_t> rxFrames_{0};
    std::atomic<std::uint64_t> rxBytes_{0};
    std::atomic<std::uint64_t> rxErrors_{0};
    std::atomic<std::uint64_t> txFrames_{0};
    std::atomic<std::uint64_t> txBytes_{0};
    std::atomic<std::uint64_t> txFailures_{0};
};

}  // namespace signalforge::drivers
