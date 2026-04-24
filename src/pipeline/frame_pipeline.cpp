// src/pipeline/frame_pipeline.cpp
#include "pipeline/frame_pipeline.hpp"

#include "drivers/io_worker_base.hpp"
#include "observability/logging.hpp"
#include "utils/mpsc_queue.hpp"

#include <QMetaObject>
#include <algorithm>
#include <exception>

namespace signalforge::pipeline {

/// Internal IO worker. Lives on the pipeline's dedicated QThread and
/// handles all sink callback execution.
///
/// Receives three kinds of events from the driver (via Qt::QueuedConnection
/// signal hops established in `FramePipeline::attachDriver`):
///
/// - `enqueueFrame(RawFrame)`: pushes into the ingress MPSC, then drains
///   inline. Running both on the worker thread means the push is
///   single-producer in practice (the driver's own thread enqueues; the
///   worker thread processes). MPSC is chosen for future flexibility
///   (multi-driver aggregation, test-harness producers) without changing
///   this code.
/// - `forwardError(DriverError)`: direct sink fanout, not queued through
///   the MPSC. Errors are rare; serializing them with frames would delay
///   error propagation behind a queue backlog.
/// - `forwardState(DriverState)`: same rationale as errors.
///
/// All sink callbacks are wrapped in try/catch so a misbehaving sink
/// cannot crash the pipeline (spec §3.1, §7.4 HALT trigger).
class PipelineWorker : public signalforge::drivers::IoWorkerBase {
    Q_OBJECT

public:
    PipelineWorker(PipelineConfig config, FramePipeline* owner)
        : signalforge::drivers::IoWorkerBase(QStringLiteral("PipelineWorker-") + config.driverId),
          config_(std::move(config)), ingress_(config_.ingressCapacity), owner_(owner) {}

    /// Approximate ingress depth, safe from any thread.
    [[nodiscard]] std::uint32_t ingressDepthApprox() const noexcept {
        return static_cast<std::uint32_t>(ingress_.sizeApprox());
    }

public slots:
    /// Driver → pipeline frame hop. Push to MPSC; drain inline.
    void enqueueFrame(signalforge::frame::RawFrame frame) {
        if (!ingress_.push(std::move(frame))) {
            owner_->framesDropped_.fetch_add(1, std::memory_order_relaxed);
            SF_LOG_WARN("FramePipeline[{}]: ingress queue full; frame dropped", config_.driverId.toStdString());
            return;
        }
        drain();
    }

    /// Direct error fanout. Not queued through MPSC.
    void forwardError(signalforge::drivers::DriverError error) {
        owner_->errorsForwarded_.fetch_add(1, std::memory_order_relaxed);
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
    FramePipeline* owner_;
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
    // S2/S3 stub: WatermarkTracker wired in S4.
    return 0;
}

FramePipeline::Stats FramePipeline::stats() const {
    Stats s;
    s.framesReceived = framesReceived_.load(std::memory_order_relaxed);
    s.framesDropped = framesDropped_.load(std::memory_order_relaxed);
    s.errorsForwarded = errorsForwarded_.load(std::memory_order_relaxed);
    s.ingressDepthCurrent = worker_ ? worker_->ingressDepthApprox() : 0;
    s.snapshotAt = std::chrono::steady_clock::now();
    return s;
}

void FramePipeline::resetBackpressureStats() {
    // S4 will also reset the peak watermark tracker. For now, reset the
    // drop counter so long-running tests can re-baseline.
    framesDropped_.store(0, std::memory_order_relaxed);
}

const QString& FramePipeline::driverId() const noexcept {
    return config_.driverId;
}

}  // namespace signalforge::pipeline

#include "frame_pipeline.moc"
