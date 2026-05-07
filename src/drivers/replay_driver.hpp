// src/drivers/replay_driver.hpp
#pragma once

#include "drivers/driver_configs.hpp"
#include "drivers/driver_interface.hpp"
#include "frame/raw_frame.hpp"

#include <QObject>
#include <QString>
#include <QThread>
#include <atomic>
#include <memory>

namespace signalforge::drivers {

class ReplayIoWorker;

/// Session file replay driver.
///
/// **M3 status: SKELETON.** Lifecycle methods are complete and correct per
/// the `DriverInterface` contract: `open()` verifies the session file
/// exists and has a non-zero 16-byte header, `close()` releases, `start()`
/// transitions to `Running` but emits no frames, `stop()` is idempotent,
/// and `write()` always returns `NotConfigured` (read-only driver).
///
/// Actual session file parsing and frame emission is M9 territory. The
/// specific insertion points are marked with `// TODO(M9):` comments at
/// line-precision in `replay_driver.cpp` and `ReplayIoWorker::onStarted()`.
///
/// Thread affinity: the driver lives on its constructing thread (typically
/// main). A private `ReplayIoWorker` is `moveToThread`'d to a dedicated
/// `QThread` owned by the driver. Signals are emitted from the driver's
/// own thread via internal slots translating worker signals.
class ReplayDriver : public DriverInterface {
    Q_OBJECT

public:
    explicit ReplayDriver(ReplayConfig config, QObject* parent = nullptr);
    ~ReplayDriver() override;

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
    [[nodiscard]] const ReplayConfig& config() const noexcept;

private slots:
    // Translators for worker signals. Each runs on the driver's own thread
    // via Qt::QueuedConnection from the worker.
    void onWorkerOpened();
    void onWorkerClosed();
    void onWorkerStarted();
    void onWorkerStopped();
    void onWorkerErrorOccurred(const signalforge::drivers::DriverError& err);
    void onWorkerFrame(signalforge::frame::RawFrame frame);

private:
    void transitionTo(DriverState newState);

    ReplayConfig config_;
    std::unique_ptr<QThread> thread_;
    std::unique_ptr<ReplayIoWorker> worker_;

    std::atomic<DriverState> state_{DriverState::Idle};
    std::atomic<DriverErrorCode> health_{DriverErrorCode::Success};
};

}  // namespace signalforge::drivers
