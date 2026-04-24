// src/pipeline/frame_pipeline.cpp
#include "pipeline/frame_pipeline.hpp"

#include "drivers/io_worker_base.hpp"
#include "frame/backpressure.hpp"
#include "observability/logging.hpp"
#include "observability/metrics.hpp"
#include "utils/mpsc_queue.hpp"

#include <QMetaObject>
#include <algorithm>
#include <exception>
#include <utility>

namespace signalforge::pipeline {

namespace {

/// Build a metric-name string for a per-pipeline metric. ADR-003 accepts
/// driver IDs with `:`, `/`, `.` verbatim, so no sanitization is needed.
QString metricName(const QString& base, const QString& driverId) {
    return base + driverId;
}

}  // namespace

/// Internal IO worker. Lives on the pipeline's dedicated QThread and
/// handles all sink callback execution + backpressure observation.
///
/// Receives three kinds of events from the driver (via Qt::QueuedConnection
/// signal hops established in `FramePipeline::attachDriver`):
///
/// - `enqueueFrame(RawFrame)`: pushes into the ingress MPSC, observes the
///   watermark tracker, then drains inline.
/// - `forwardError(DriverError)`: direct sink fanout.
/// - `forwardState(DriverState)`: direct sink fanout.
///
/// All sink callbacks are wrapped in try/catch so a misbehaving sink
/// cannot crash the pipeline (spec §3.1, §7.4 HALT trigger).
class PipelineWorker : public signalforge::drivers::IoWorkerBase {
    Q_OBJECT

public:
    PipelineWorker(PipelineConfig config, FramePipeline* owner)
        : signalforge::drivers::IoWorkerBase(QStringLiteral("PipelineWorker-") + config.driverId),
          config_(std::move(config)), ingress_(config_.ingressCapacity),
          tracker_(config_.ingressCapacity, config_.watermarkHighPct, config_.watermarkRecoverPct), owner_(owner) {
        auto& reg = signalforge::observability::MetricsRegistry::instance();
        using signalforge::observability::MetricKind;
        framesReceivedMetric_ = reg.getOrCreate(
            metricName(QStringLiteral("pipeline_frames_received_"), config_.driverId), MetricKind::Counter);
        framesDroppedMetric_ = reg.getOrCreate(metricName(QStringLiteral("pipeline_frames_dropped_"), config_.driverId),
                                               MetricKind::Counter);
        errorsForwardedMetric_ = reg.getOrCreate(
            metricName(QStringLiteral("pipeline_errors_forwarded_"), config_.driverId), MetricKind::Counter);
        watermarkGauge_ = reg.getOrCreate(metricName(QStringLiteral("pipeline_ingress_watermark_"), config_.driverId),
                                          MetricKind::Gauge);
        depthPeakGauge_ = reg.getOrCreate(metricName(QStringLiteral("pipeline_ingress_depth_peak_"), config_.driverId),
                                          MetricKind::Gauge);
    }

    /// Approximate ingress depth, safe from any thread.
    [[nodiscard]] std::uint32_t ingressDepthApprox() const noexcept {
        return static_cast<std::uint32_t>(ingress_.sizeApprox());
    }

    /// Current peak watermark percentage. Safe from any thread.
    [[nodiscard]] std::uint32_t peakWatermarkPct() const noexcept {
        return tracker_.peakPct();
    }

public slots:
    /// Driver → pipeline frame hop. Push to MPSC; observe watermark;
    /// drain inline.
    ///
    /// The underlying `MpscQueue` wraps moodycamel's unbounded queue;
    /// `ingressCapacity` is therefore a pipeline-level soft cap that we
    /// enforce ourselves: if `sizeApprox()` is already at capacity, drop
    /// the incoming frame. `sizeApprox` is approximate (low-contention
    /// bias), so under heavy concurrent pressure we may exceed `capacity`
    /// by 1–2 frames — an acceptable slop for backpressure purposes.
    void enqueueFrame(signalforge::frame::RawFrame frame) {
        const auto depthBefore = ingress_.sizeApprox();
        const bool atCap = depthBefore >= static_cast<std::size_t>(config_.ingressCapacity);
        const bool pushed = !atCap && ingress_.push(std::move(frame));
        if (!pushed) {
            owner_->framesDropped_.fetch_add(1, std::memory_order_relaxed);
            if (framesDroppedMetric_) {
                framesDroppedMetric_->add(1);
            }
            SF_LOG_WARN("FramePipeline[{}]: ingress queue full (depth {}, cap {}); frame dropped",
                        config_.driverId.toStdString(), depthBefore, config_.ingressCapacity);
            return;
        }

        const auto depth = static_cast<std::uint32_t>(ingress_.sizeApprox());
        // Monotonic peak tracking.
        auto expected = owner_->ingressDepthPeak_.load(std::memory_order_relaxed);
        while (depth > expected &&
               !owner_->ingressDepthPeak_.compare_exchange_weak(expected, depth, std::memory_order_relaxed)) {
            // `expected` is refreshed by compare_exchange_weak on failure.
        }
        if (depthPeakGauge_) {
            depthPeakGauge_->set(static_cast<std::int64_t>(owner_->ingressDepthPeak_.load(std::memory_order_relaxed)));
        }

        // Observe may emit a backpressure signal per M2 contract.
        if (auto signal = tracker_.observe(depth, config_.driverId)) {
            using signalforge::frame::BackpressureReason;
            switch (signal->reason) {
            case BackpressureReason::QueueFilling:
                SF_LOG_WARN("FramePipeline[{}]: ingress watermark QueueFilling at {}% (depth {}/{})",
                            config_.driverId.toStdString(), signal->watermarkPct, signal->currentDepth,
                            signal->capacity);
                break;
            case BackpressureReason::QueueRecovered:
                SF_LOG_INFO("FramePipeline[{}]: ingress watermark QueueRecovered at {}% (depth {}/{})",
                            config_.driverId.toStdString(), signal->watermarkPct, signal->currentDepth,
                            signal->capacity);
                break;
            case BackpressureReason::QueueFull:
                SF_LOG_WARN("FramePipeline[{}]: ingress queue reported QueueFull at {}% (depth {}/{})",
                            config_.driverId.toStdString(), signal->watermarkPct, signal->currentDepth,
                            signal->capacity);
                break;
            }
            if (watermarkGauge_) {
                watermarkGauge_->set(static_cast<std::int64_t>(signal->watermarkPct));
            }
        }

        drain();
    }

    /// Direct error fanout. Not queued through MPSC.
    void forwardError(signalforge::drivers::DriverError error) {
        owner_->errorsForwarded_.fetch_add(1, std::memory_order_relaxed);
        if (errorsForwardedMetric_) {
            errorsForwardedMetric_->add(1);
        }
        const auto snapshot = snapshotSinks();
        for (const auto& sink : snapshot) {
            try {
                sink->onError(error);
            } catch (const std::exception& e) {
                SF_LOG_ERROR("FramePipeline[{}]: sink '{}' threw from onError: {}", config_.driverId.toStdString(),
                             sink->sinkName().toStdString(), e.what());
            } catch (...) {
                SF_LOG_ERROR("FramePipeline[{}]: sink '{}' threw non-std exception from onError",
                             config_.driverId.toStdString(), sink->sinkName().toStdString());
            }
        }
    }

    /// Direct lifecycle fanout. Not queued through MPSC.
    void forwardState(signalforge::drivers::DriverState newState) {
        const auto snapshot = snapshotSinks();
        for (const auto& sink : snapshot) {
            try {
                sink->onLifecycle(newState);
            } catch (const std::exception& e) {
                SF_LOG_ERROR("FramePipeline[{}]: sink '{}' threw from onLifecycle: {}", config_.driverId.toStdString(),
                             sink->sinkName().toStdString(), e.what());
            } catch (...) {
                SF_LOG_ERROR("FramePipeline[{}]: sink '{}' threw non-std exception from onLifecycle",
                             config_.driverId.toStdString(), sink->sinkName().toStdString());
            }
        }
    }

    /// Reset internal backpressure state. Invoked on the worker thread via
    /// Qt::QueuedConnection so `WatermarkTracker::reset` (which is not
    /// thread-safe with concurrent observe()) runs without racing with
    /// enqueueFrame.
    void resetInternal() {
        tracker_.reset();
        if (watermarkGauge_) {
            watermarkGauge_->set(0);
        }
    }

protected:
    void onStarted() override {
        // Nothing to do. Driver signals are wired before start() runs;
        // incoming frames will begin arriving via enqueueFrame as the
        // driver emits them.
    }

private:
    /// Take a shared_ptr snapshot of the sink list. Releasing the lock
    /// before iteration means `removeSink` called from another thread
    /// during a fanout does not block, and the strong refs in the
    /// snapshot keep the sinks alive for the duration of this batch.
    [[nodiscard]] std::vector<std::shared_ptr<FrameSink>> snapshotSinks() const {
        std::lock_guard<std::mutex> lock(owner_->sinkMutex_);
        return owner_->sinks_;
    }

    /// Pop all frames currently queued and fan out each to every sink.
    void drain() {
        while (auto maybe = ingress_.pop()) {
            const auto frame = std::move(*maybe);
            owner_->framesReceived_.fetch_add(1, std::memory_order_relaxed);
            if (framesReceivedMetric_) {
                framesReceivedMetric_->add(1);
            }
            const auto snapshot = snapshotSinks();
            for (const auto& sink : snapshot) {
                try {
                    sink->onFrame(frame);
                } catch (const std::exception& e) {
                    SF_LOG_ERROR("FramePipeline[{}]: sink '{}' threw from onFrame: {}", config_.driverId.toStdString(),
                                 sink->sinkName().toStdString(), e.what());
                } catch (...) {
                    SF_LOG_ERROR("FramePipeline[{}]: sink '{}' threw non-std exception from onFrame",
                                 config_.driverId.toStdString(), sink->sinkName().toStdString());
                }
            }
        }
    }

    PipelineConfig config_;
    signalforge::utils::MpscQueue<signalforge::frame::RawFrame> ingress_;
    signalforge::frame::WatermarkTracker tracker_;
    FramePipeline* owner_;

    signalforge::observability::Metric* framesReceivedMetric_ = nullptr;
    signalforge::observability::Metric* framesDroppedMetric_ = nullptr;
    signalforge::observability::Metric* errorsForwardedMetric_ = nullptr;
    signalforge::observability::Metric* watermarkGauge_ = nullptr;
    signalforge::observability::Metric* depthPeakGauge_ = nullptr;
};

// =======================================================================
// FramePipeline
// =======================================================================

FramePipeline::FramePipeline(PipelineConfig config, QObject* parent) : QObject(parent), config_(std::move(config)) {
    if (config_.driverId.isEmpty()) {
        SF_LOG_ERROR("FramePipeline: constructed with empty driverId; metrics and thread naming will be degraded");
    }

    thread_ = std::make_unique<QThread>();
    thread_->setObjectName(QStringLiteral("PipelineWorker-") + config_.driverId);

    worker_ = std::make_unique<PipelineWorker>(config_, this);
    worker_->moveToThread(thread_.get());

    connect(thread_.get(), &QThread::started, worker_.get(), &signalforge::drivers::IoWorkerBase::onThreadStart);

    thread_->start();
}

FramePipeline::~FramePipeline() {
    if (driver_) {
        // Disconnect before thread teardown so no late signals fire into a
        // half-destroyed worker.
        driver_->disconnect(worker_.get());
    }
    if (thread_ && thread_->isRunning()) {
        thread_->quit();
        if (!thread_->wait(500)) {
            SF_LOG_ERROR("FramePipeline[{}]: IO thread did not exit within 500ms; forcing terminate",
                         config_.driverId.toStdString());
            thread_->terminate();
            thread_->wait();
        }
    }
    // sinks_ is released here; their strong references drop. Any in-flight
    // callback has already completed because the worker's thread has
    // joined above.
}

void FramePipeline::attachDriver(signalforge::drivers::DriverInterface* driver) {
    if (!driver) {
        SF_LOG_ERROR("FramePipeline[{}]: attachDriver(nullptr) ignored", config_.driverId.toStdString());
        return;
    }
    if (driver_) {
        SF_LOG_ERROR("FramePipeline[{}]: attachDriver called a second time; ignored", config_.driverId.toStdString());
        return;
    }
    driver_ = driver;
    connect(driver, &signalforge::drivers::DriverInterface::frameReceived, worker_.get(), &PipelineWorker::enqueueFrame,
            Qt::QueuedConnection);
    connect(driver, &signalforge::drivers::DriverInterface::errorOccurred, worker_.get(), &PipelineWorker::forwardError,
            Qt::QueuedConnection);
    connect(driver, &signalforge::drivers::DriverInterface::stateChanged, worker_.get(), &PipelineWorker::forwardState,
            Qt::QueuedConnection);
}

void FramePipeline::addSink(std::shared_ptr<FrameSink> sink) {
    if (!sink) {
        SF_LOG_ERROR("FramePipeline[{}]: addSink(nullptr) ignored", config_.driverId.toStdString());
        return;
    }
    std::lock_guard<std::mutex> lock(sinkMutex_);
    const auto it = std::find(sinks_.begin(), sinks_.end(), sink);
    if (it != sinks_.end()) {
        SF_LOG_WARN("FramePipeline[{}]: addSink({}) is a duplicate; ignored", config_.driverId.toStdString(),
                    sink->sinkName().toStdString());
        return;
    }
    sinks_.push_back(std::move(sink));
}

void FramePipeline::removeSink(std::shared_ptr<FrameSink> sink) {
    if (!sink) {
        return;
    }
    std::lock_guard<std::mutex> lock(sinkMutex_);
    const auto it = std::find(sinks_.begin(), sinks_.end(), sink);
    if (it != sinks_.end()) {
        sinks_.erase(it);
    }
}

std::size_t FramePipeline::sinkCount() const {
    std::lock_guard<std::mutex> lock(sinkMutex_);
    return sinks_.size();
}

std::uint32_t FramePipeline::peakWatermarkPct() const {
    return worker_ ? worker_->peakWatermarkPct() : 0;
}

FramePipeline::Stats FramePipeline::stats() const {
    Stats s;
    s.framesReceived = framesReceived_.load(std::memory_order_relaxed);
    s.framesDropped = framesDropped_.load(std::memory_order_relaxed);
    s.errorsForwarded = errorsForwarded_.load(std::memory_order_relaxed);
    s.ingressDepthCurrent = worker_ ? worker_->ingressDepthApprox() : 0;
    s.ingressDepthPeak = ingressDepthPeak_.load(std::memory_order_relaxed);
    s.snapshotAt = std::chrono::steady_clock::now();
    return s;
}

void FramePipeline::resetBackpressureStats() {
    framesDropped_.store(0, std::memory_order_relaxed);
    ingressDepthPeak_.store(0, std::memory_order_relaxed);
    if (worker_) {
        // WatermarkTracker::reset is not thread-safe with concurrent
        // observe(); dispatch to the worker thread so they serialize.
        QMetaObject::invokeMethod(worker_.get(), "resetInternal", Qt::QueuedConnection);
    }
}

const QString& FramePipeline::driverId() const noexcept {
    return config_.driverId;
}

}  // namespace signalforge::pipeline

#include "frame_pipeline.moc"
