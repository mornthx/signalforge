// src/pipeline/frame_sink.hpp
#pragma once

#include "drivers/driver_interface.hpp"
#include "frame/raw_frame.hpp"

#include <QString>

namespace signalforge::pipeline {

/// Downstream consumer of driver frames.
///
/// A sink is registered with a `FramePipeline` and receives every frame,
/// error, and state change from the pipeline's attached driver.
///
/// Thread affinity:
/// - All three callbacks (`onFrame`, `onError`, `onLifecycle`) are invoked
///   on the pipeline's dedicated thread. Sinks do not need internal locking
///   for data accessed only within these callbacks.
/// - If a sink wants to propagate data to another thread (e.g., a UI
///   thread), the sink implements that dispatch itself (Qt signals with
///   QueuedConnection, or post to a queue).
///
/// Lifetime:
/// - Sinks must outlive the pipeline they are registered with, OR the
///   caller keeps a `std::shared_ptr<FrameSink>` strong reference alive
///   for the duration of registration. `FramePipeline` holds the shared
///   pointer internally so a sink in the middle of an `onFrame` call
///   cannot be destroyed underneath the pipeline.
/// - Sinks may deregister via `FramePipeline::removeSink(this)` before
///   destruction. If not explicitly removed, the pipeline's destructor
///   releases its sink references without calling them.
///
/// Error handling:
/// - Exceptions thrown from `onFrame` / `onError` / `onLifecycle` are
///   caught by the pipeline, logged at ERROR level, and the pipeline
///   continues with the next sink. A faulty sink does not crash the
///   pipeline.
///
/// Freeze scope: this class is frozen at M4 close. Modifications
/// post-freeze require a new ADR.
class FrameSink {
public:
    virtual ~FrameSink() = default;

    /// Called for every frame received from the attached driver.
    /// `frame` is valid only for the duration of this call; if the sink
    /// needs to retain the frame, it must copy `frame.payload` (implicitly
    /// shared, O(1)) or the fields it needs.
    virtual void onFrame(const signalforge::frame::RawFrame& frame) = 0;

    /// Called when the driver reports an error. Default: no-op.
    /// Override if the sink needs to react (e.g., record error in session,
    /// suspend processing).
    virtual void onError(const signalforge::drivers::DriverError& error) {
        (void)error;
    }

    /// Called when the driver's state changes. Default: no-op.
    /// Override if the sink needs lifecycle awareness (e.g., flush on
    /// Stopping, reset state on Open).
    virtual void onLifecycle(signalforge::drivers::DriverState newState) {
        (void)newState;
    }

    /// Human-readable sink name, used in logs and metrics.
    /// Should be stable across invocations (pipeline may log it
    /// repeatedly).
    [[nodiscard]] virtual QString sinkName() const = 0;
};

}  // namespace signalforge::pipeline
