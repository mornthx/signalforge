// src/drivers/io_worker_base.hpp
#pragma once

#include <QObject>
#include <QString>

namespace signalforge::drivers {

/// Common base for IO worker classes that live on a dedicated `QThread`.
///
/// Each concrete driver (`SerialDriver`, `TcpDriver`, `UdpDriver`,
/// `ReplayDriver`) defines a private IoWorker subclass in its `.cpp`. The
/// worker's instance is `moveToThread()`'d to a driver-owned `QThread` at
/// construction time. When the thread starts, the base's `onThreadStart()`
/// hook is invoked on the worker's thread (the IO thread), allowing the
/// subclass to set its thread name via
/// `signalforge::platform::setCurrentThreadName()` and initialize any
/// QObjects that require thread-affinity (QSerialPort, QTcpSocket, etc.).
///
/// Not part of the M2 DriverInterface freeze scope. Internal abstraction;
/// may evolve in M4+ without an ADR.
class IoWorkerBase : public QObject {
    Q_OBJECT

public:
    explicit IoWorkerBase(QString threadName, QObject* parent = nullptr);
    ~IoWorkerBase() override;

    /// Thread name (used for debugger / top / htop visibility).
    [[nodiscard]] const QString& threadName() const noexcept;

public slots:
    /// Connected by the owning driver to `QThread::started`. Default
    /// implementation sets the OS-level thread name via
    /// `platform::setCurrentThreadName`, then delegates to `onStarted()`
    /// for subclass-specific initialization.
    ///
    /// Thread context: runs on the worker's IO thread, not the caller's.
    void onThreadStart();

protected:
    /// Subclass hook for IO-thread initialization. Called once, on the IO
    /// thread, after the base class sets the OS thread name.
    virtual void onStarted() = 0;

private:
    QString threadName_;
};

}  // namespace signalforge::drivers
