// src/drivers/driver_interface.hpp
#pragma once

#include "frame/raw_frame.hpp"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <chrono>

namespace signalforge::drivers {

/// Lifecycle state of a driver. Monotonic where possible, but
/// `Error → Idle` transition is allowed via `close()` after handling.
/// Numeric values are explicit for log stability and metatype
/// round-trip safety across shared-memory or IPC use cases.
enum class DriverState : int {
    Idle = 0,      ///< Not yet configured or after close()
    Opening = 1,   ///< open() in progress
    Open = 2,      ///< Resource acquired; not yet reading
    Running = 3,   ///< start() succeeded; data flowing
    Stopping = 4,  ///< stop() in progress
    Closing = 5,   ///< close() in progress
    Error = 6,     ///< Terminal for the current lifecycle; recoverable via close() → open()
};

/// Error categories for driver failures. String detail lives in
/// `DriverError::message`. Numeric values are explicit.
enum class DriverErrorCode : int {
    Success = 0,
    NotConfigured = 1,
    ConfigInvalid = 2,
    ResourceUnavailable = 3,  ///< Port in use, device not found, etc.
    ResourceLost = 4,         ///< Was open; became unavailable mid-run
    PermissionDenied = 5,
    IoFailure = 6,
    ProtocolFailure = 7,
    BackpressureDrop = 8,  ///< Data dropped due to downstream backpressure
    Timeout = 9,
    Internal = 10,  ///< Driver bug; always a defect
};

/// Detail about a driver error. Carries the code, a human-readable
/// message, and the steady_clock time at which the error was observed.
struct DriverError {
    DriverErrorCode code = DriverErrorCode::Success;
    QString message;
    std::chrono::steady_clock::time_point at{};
};

/// Abstract driver base. One instance per logical device.
///
/// Thread affinity:
///   - Construction: any thread (typically main).
///   - open() / close() / start() / stop() / write(): called from the
///     thread that owns the driver (typically main or a dedicated
///     coordinator). Concrete drivers may move themselves or a worker
///     QObject to a dedicated IO QThread at open() time; consult each
///     concrete driver's documentation.
///   - Signals are emitted from the driver's IO thread. Consumers that
///     connect slots on other threads MUST use `Qt::QueuedConnection`.
///
/// Ownership:
///   - Subclasses own all IO resources. open() acquires; close()
///     releases. The precondition for destruction is `state() == Idle`.
///   - RawFrame is passed by value in `frameReceived`; the `QByteArray`
///     inside shares implicitly for O(1) delivery cost.
///
/// Const accessors (`state()`, `health()`, `statistics()`) are all
/// safe to call from any thread. Concrete drivers back them with
/// per-field `std::atomic` state (see spec §4.2 atomicity note).
///
/// Freeze scope: this class, its signals, its enums, and `DriverError`
/// are frozen at M2 close. Modifications post-freeze require a new ADR
/// per spec §6.3.
class DriverInterface : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(DriverInterface)

public:
    explicit DriverInterface(QObject* parent = nullptr);
    ~DriverInterface() override;

    /// Request IO resource acquisition. Non-blocking.
    ///
    /// Return semantics:
    ///   - Returns `Success` if the request is accepted for async
    ///     processing (not if the resource is actually open). The
    ///     driver transitions Idle → Opening synchronously on acceptance.
    ///   - Returns a non-Success code (`NotConfigured`, `ConfigInvalid`)
    ///     if the request itself is malformed. No state transition in
    ///     this case.
    ///
    /// Actual open completion is reported asynchronously via signals:
    ///   - `stateChanged(Open)` on success (Opening → Open)
    ///   - `stateChanged(Error)` + `errorOccurred(...)` on failure
    ///     (Opening → Error)
    ///
    /// The IO thread executes the actual acquisition. Consumers must
    /// connect `stateChanged` and `errorOccurred` with
    /// `Qt::QueuedConnection` if on another thread.
    ///
    /// Precondition: `state() == Idle`.
    virtual DriverErrorCode open() = 0;

    /// Release the IO resource. Idempotent: no-op if already Idle. May
    /// block briefly for graceful shutdown (recommended < 500 ms).
    virtual void close() = 0;

    /// Begin data flow. Frames emitted via `frameReceived` until stop().
    /// Precondition: `state() == Open`.
    virtual DriverErrorCode start() = 0;

    /// Suspend data flow. Idempotent.
    /// Postcondition: no more `frameReceived` after this returns,
    /// barring one in-flight frame already posted to the target thread.
    virtual void stop() = 0;

    /// Attempt to transmit `payload`. Returns accepted/rejected.
    /// Concrete drivers define whether this is synchronous (Serial) or
    /// queued (TCP). Precondition: `state() == Running`.
    virtual DriverErrorCode write(const QByteArray& payload) = 0;

    /// Current lifecycle state. Safe to call from any thread.
    [[nodiscard]] virtual DriverState state() const = 0;

    /// Health probe. Returns `Success` if the driver is in a nominal
    /// state, otherwise the primary error code observed. Not a status
    /// snapshot — `statistics()` provides that. Safe to call from any
    /// thread.
    [[nodiscard]] virtual DriverErrorCode health() const = 0;

    /// Snapshot of driver statistics. Safe to call from any thread.
    /// Per-field atomicity; cross-field consistency is not guaranteed
    /// (spec §4.2 clarification 3).
    [[nodiscard]] virtual signalforge::frame::DriverStatistics statistics() const = 0;

signals:
    /// A frame was received. Emitted from the driver's IO thread.
    /// Consumers connect with `Qt::QueuedConnection`.
    void frameReceived(signalforge::frame::RawFrame frame);

    /// An error occurred. Emitted from the driver's IO thread. The
    /// consumer decides whether to `close()` or continue.
    void errorOccurred(signalforge::drivers::DriverError error);

    /// Lifecycle state changed. Emitted on every transition.
    void stateChanged(signalforge::drivers::DriverState newState);
};

/// Register `DriverState` and `DriverError` as Qt metatypes for use in
/// queued signal connections. Idempotent. Must be called once per
/// process before any `DriverInterface` signal is connected, usually in
/// program `main()` or (for tests) in a Catch2 listener.
void registerMetatypes();

}  // namespace signalforge::drivers

Q_DECLARE_METATYPE(signalforge::drivers::DriverState)
Q_DECLARE_METATYPE(signalforge::drivers::DriverError)
