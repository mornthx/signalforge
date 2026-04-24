// src/drivers/tcp_driver.hpp
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

class TcpIoWorker;

/// TCP client driver over `QTcpSocket`, running IO on a dedicated
/// `QThread`. `open()` validates the config and initiates a connect on
/// the IO thread using `QTcpSocket::connectToHost` + `connected` /
/// `errorOccurred` signals and a `connectTimeout` via `QTimer`, per
/// spec §4.3 and understanding §3.2 default.
///
/// TCP is a byte stream: every `readyRead` emits one `RawFrame` with
/// `payload = socket->readAll()`. Re-framing is M4's (decoder's) job.
///
/// Mid-run disconnect (peer closes, RST, etc.) surfaces as
/// `errorOccurred(ResourceLost)` and a transition to `Error`. M3 does
/// not auto-reconnect.
class TcpDriver : public DriverInterface {
    Q_OBJECT

public:
    explicit TcpDriver(TcpConfig config, QObject* parent = nullptr);
    ~TcpDriver() override;

    DriverErrorCode open() override;
    void close() override;
    DriverErrorCode start() override;
    void stop() override;
    DriverErrorCode write(const QByteArray& payload) override;

    [[nodiscard]] DriverState state() const override;
    [[nodiscard]] DriverErrorCode health() const override;
    [[nodiscard]] signalforge::frame::DriverStatistics statistics() const override;

    [[nodiscard]] const TcpConfig& config() const noexcept;

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

    TcpConfig config_;
    std::unique_ptr<QThread> thread_;
    std::unique_ptr<TcpIoWorker> worker_;

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
