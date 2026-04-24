// src/pipeline/frame_pipeline.hpp
#pragma once

#include "drivers/driver_interface.hpp"
#include "frame/raw_frame.hpp"
#include "pipeline/frame_sink.hpp"

#include <QObject>
#include <QString>
#include <QThread>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace signalforge::pipeline {

class PipelineWorker;  // forward-declared; defined in .cpp

/// Configuration for a FramePipeline.
struct PipelineConfig {
    /// Human-readable driver identifier, used in logs and metric names.
    /// Expected format: `<driver-type>-<unique-suffix>`, e.g. "serial-ttyUSB0"
    /// or "tcp-localhost-9000". Must be non-empty and unique per
    /// PipelineManager.
    QString driverId;

    /// Ingress queue capacity. Frames arriving when the queue is at
    /// capacity are dropped (and counted in pipeline_frames_dropped metric).
    /// Default is sized for ~1 second of backlog at 10kHz frame rate.
    std::uint32_t ingressCapacity = 10000;

    /// Watermark thresholds for backpressure observation.
    /// `highPct`: depth above this triggers "QueueFilling" signal.
    /// `recoverPct`: depth below this (after being above highPct) triggers
    /// "QueueRecovered" signal.
    std::uint32_t watermarkHighPct = 80;
    std::uint32_t watermarkRecoverPct = 60;
};

/// Routing layer between a driver and downstream sinks.
///
/// One FramePipeline per driver instance. Manages:
/// - A dedicated QThread for sink callback execution
/// - An ingress MPSC queue (driver → pipeline)
/// - A vector of registered FrameSinks (fanout on pipeline thread)
/// - Backpressure observation on the ingress queue
/// - Driver signal wiring (frameReceived / errorOccurred / stateChanged)
///
/// Thread affinity:
/// - Public API (`addSink`, `removeSink`, `sinkCount`, `stats`,
///   `peakWatermarkPct`, `resetBackpressureStats`) is safe to call from
///   any thread; internally locked.
/// - Sinks' callbacks execute on the pipeline's dedicated thread.
///
/// Lifetime:
/// - Pipeline is owned by PipelineManager, not directly by callers.
/// - Caller receives a FramePipeline* (non-owning) from PipelineManager::attach
///   for sink registration. Do not store this pointer beyond the scope of
///   the driver's active connection.
///
/// Freeze scope: the public class interface (methods below) is frozen at M4
/// close. Internal implementation (PipelineWorker, queue, etc.) may evolve.
class FramePipeline : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(FramePipeline)

    // Worker is an internal implementation detail defined in the .cpp; it
    // needs read access to the sink list and write access to the stats
    // counters. Kept as friend rather than exposing those as public API so
    // the class contract stays the one in §6.1 (freeze surface).
    friend class PipelineWorker;

public:
    /// Construct a pipeline for a driver. Pipeline is created in a "ready"
    /// state but not yet connected to the driver; call `attachDriver()` to
    /// wire up and start processing.
    explicit FramePipeline(PipelineConfig config, QObject* parent = nullptr);
    ~FramePipeline() override;

    /// Connect pipeline to driver. Establishes signal/slot connections
    /// between driver's `frameReceived` / `errorOccurred` / `stateChanged`
    /// and the pipeline worker. Pipeline begins processing frames
    /// immediately upon driver emission.
    ///
    /// Precondition: pipeline not already attached.
    /// Thread: any thread (QueuedConnection used internally).
    void attachDriver(signalforge::drivers::DriverInterface* driver);

    /// Register a sink for frame fanout. Sink lifetime must outlast the
    /// pipeline's (or be removed via `removeSink` before destruction).
    /// The pipeline holds the shared_ptr while the sink is registered, so
    /// a sink cannot be destroyed mid-callback.
    /// Idempotent: registering the same sink pointer twice logs a warning
    /// and has no additional effect.
    /// Thread: any thread (internally locked).
    void addSink(std::shared_ptr<FrameSink> sink);

    /// Deregister a sink. No-op if not registered. Any in-flight callback
    /// already dispatched on the pipeline thread completes; no new
    /// callbacks will be made to this sink after `removeSink` returns.
    /// Thread: any thread (internally locked).
    void removeSink(std::shared_ptr<FrameSink> sink);

    /// Number of registered sinks. Safe from any thread.
    [[nodiscard]] std::size_t sinkCount() const;

    /// Current backpressure watermark peak (percentage). Monotonic since
    /// construction or last `resetBackpressureStats()`. Safe from any thread.
    [[nodiscard]] std::uint32_t peakWatermarkPct() const;

    /// Current pipeline statistics snapshot. Safe from any thread.
    struct Stats {
        std::uint64_t framesReceived = 0;
        std::uint64_t framesDropped = 0;  ///< Dropped at ingress queue full
        std::uint64_t errorsForwarded = 0;
        std::uint32_t ingressDepthCurrent = 0;
        std::uint32_t ingressDepthPeak = 0;
        std::chrono::steady_clock::time_point snapshotAt{};
    };
    [[nodiscard]] Stats stats() const;

    /// Reset backpressure peak and drop counters. Does not affect
    /// framesReceived counter. Thread-safe; caller should quiesce sink
    /// registration if called concurrently with it.
    void resetBackpressureStats();

    /// Driver identifier from config. Stable across pipeline lifetime.
    [[nodiscard]] const QString& driverId() const noexcept;

private:
    PipelineConfig config_;
    std::unique_ptr<QThread> thread_;
    std::unique_ptr<PipelineWorker> worker_;

    // Sink registry. Guarded by sinkMutex_.
    mutable std::mutex sinkMutex_;
    std::vector<std::shared_ptr<FrameSink>> sinks_;

    // Stats counters. Worker updates; accessors read via stats().
    std::atomic<std::uint64_t> framesReceived_{0};
    std::atomic<std::uint64_t> framesDropped_{0};
    std::atomic<std::uint64_t> errorsForwarded_{0};
    std::atomic<std::uint32_t> ingressDepthPeak_{0};

    // Driver connection state
    signalforge::drivers::DriverInterface* driver_ = nullptr;
};

}  // namespace signalforge::pipeline
