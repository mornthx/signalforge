// src/drivers/udp_driver.hpp
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

class UdpIoWorker;

/// UDP endpoint driver over `QUdpSocket`, running IO on a dedicated
/// `QThread`. `open()` validates the config per spec §4.4 (needs at
/// least bind-intent or send-intent) and binds the local endpoint on
/// the IO thread. Multicast group join happens after bind when
/// `UdpConfig::multicastGroup` is non-empty.
///
/// Unlike TCP, UDP preserves framing: each datagram delivered by
/// `readyRead` becomes one `RawFrame` with `payload = datagram`.
///
/// `write()` sends via `writeDatagram(payload, remoteHost, remotePort)`.
/// If `remoteHost` is empty, `write()` returns `ConfigInvalid` without
/// touching the socket.
class UdpDriver : public DriverInterface {
    Q_OBJECT

public:
    explicit UdpDriver(UdpConfig config, QObject* parent = nullptr);
    ~UdpDriver() override;

    DriverErrorCode open() override;
    void close() override;
    DriverErrorCode start() override;
    void stop() override;
    DriverErrorCode write(const QByteArray& payload) override;

    [[nodiscard]] DriverState state() const override;
    [[nodiscard]] DriverErrorCode health() const override;
    [[nodiscard]] signalforge::frame::DriverStatistics statistics() const override;

    [[nodiscard]] const UdpConfig& config() const noexcept;

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

    UdpConfig config_;
    std::unique_ptr<QThread> thread_;
    std::unique_ptr<UdpIoWorker> worker_;

    std::atomic<DriverState> state_{DriverState::Idle};
    std::atomic<DriverErrorCode> health_{DriverErrorCode::Success};

    std::atomic<std::uint64_t> rxFrames_{0};
    std::atomic<std::uint64_t> rxBytes_{0};
    std::atomic<std::uint64_t> rxErrors_{0};
    std::atomic<std::uint64_t> txFrames_{0};
    std::atomic<std::uint64_t> txBytes_{0};
    std::atomic<std::uint64_t> txFailures_{0};
};

}  // namespace signalforge::drivers
