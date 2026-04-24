// src/pipeline/pipeline_manager.hpp
#pragma once

#include "drivers/driver_interface.hpp"
#include "pipeline/frame_pipeline.hpp"

#include <QObject>
#include <QString>
#include <QStringList>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace signalforge::pipeline {

/// Central registry for FramePipeline instances.
///
/// Callers (typically the Connection Manager) attach drivers here to get
/// a FramePipeline managing their frame flow. Sink registrars (M5 Decoder,
/// future components) look up pipelines by driver ID to register sinks.
///
/// Thread safety: all methods safe from any thread.
///
/// Lifetime:
/// - PipelineManager owns all FramePipeline instances.
/// - Detaching a driver destroys its pipeline (joining its thread, releasing
///   its sinks).
/// - Manager destruction detaches all pipelines.
///
/// Freeze scope: the public class interface (methods + signals below) is
/// frozen at M4 close. Modifications post-merge require a new ADR.
class PipelineManager : public QObject {
    Q_OBJECT

public:
    explicit PipelineManager(QObject* parent = nullptr);
    ~PipelineManager() override;

    /// Attach a driver to the manager, creating a pipeline. Returns a
    /// non-owning pointer to the pipeline for sink registration.
    ///
    /// `config.driverId` must be non-empty and unique across currently-
    /// attached pipelines. If a pipeline with the same ID already exists,
    /// returns nullptr and logs ERROR.
    ///
    /// Pipeline is created with the provided config, attached to the driver,
    /// and ready to receive frames. The pipelineAttached signal fires after
    /// `attachDriver` succeeds but before the function returns (same-thread
    /// observers may receive it before the pointer is returned — caller
    /// should not rely on that ordering for safety).
    [[nodiscard]] FramePipeline* attach(signalforge::drivers::DriverInterface* driver, PipelineConfig config);

    /// Detach the pipeline for `driverId`. Stops and destroys the pipeline.
    /// Sinks registered with that pipeline are released; their destructors
    /// run on the caller's thread unless the sinks have their own lifetime
    /// management.
    ///
    /// Safe to call even if `driverId` is not attached (no-op).
    void detach(const QString& driverId);

    /// Look up a pipeline by driver ID. Returns nullptr if not attached.
    /// Returned pointer is valid until `detach` is called for this ID.
    [[nodiscard]] FramePipeline* pipelineFor(const QString& driverId) const;

    /// Current number of attached pipelines.
    [[nodiscard]] std::size_t pipelineCount() const;

    /// Enumerate active driver IDs. Useful for sink registrars that want
    /// to attach to all current pipelines.
    [[nodiscard]] QStringList driverIds() const;

signals:
    /// Emitted when a pipeline is attached. Sink registrars connect to this
    /// to auto-register sinks on newly-attached pipelines.
    void pipelineAttached(const QString& driverId, signalforge::pipeline::FramePipeline* pipeline);

    /// Emitted when a pipeline is detached (after sink release).
    void pipelineDetached(const QString& driverId);

private:
    // std::unordered_map<std::string, ...> keyed on UTF-8 form of driverId
    // so we do not depend on QString's std::hash specialisation.
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::unique_ptr<FramePipeline>> pipelines_;
};

}  // namespace signalforge::pipeline
