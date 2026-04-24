// src/pipeline/frame_pipeline.cpp
#include "pipeline/frame_pipeline.hpp"

#include "drivers/io_worker_base.hpp"
#include "observability/logging.hpp"
#include "utils/mpsc_queue.hpp"

#include <QMetaObject>
#include <algorithm>

namespace signalforge::pipeline {

/// Internal IO worker that owns the ingress queue and runs sink callbacks
/// on the pipeline's dedicated QThread. S2 scope: lifecycle only — driver
/// signal wiring and frame fanout land in S3; backpressure + metrics in
/// S4.
class PipelineWorker : public signalforge::drivers::IoWorkerBase {
    Q_OBJECT

public:
    explicit PipelineWorker(PipelineConfig config)
        : signalforge::drivers::IoWorkerBase(QStringLiteral("PipelineWorker-") + config.driverId),
          config_(std::move(config)), ingress_(config_.ingressCapacity) {}

    /// Approximate ingress depth, safe from any thread.
    [[nodiscard]] std::uint32_t ingressDepthApprox() const noexcept {
        return static_cast<std::uint32_t>(ingress_.sizeApprox());
    }

protected:
    void onStarted() override {
        // S2: nothing to do. S3 wires driver signals; at that point this
        // becomes the entry-point that starts frame processing.
    }

private:
    PipelineConfig config_;
    signalforge::utils::MpscQueue<signalforge::frame::RawFrame> ingress_;
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

    worker_ = std::make_unique<PipelineWorker>(config_);
    worker_->moveToThread(thread_.get());

    connect(thread_.get(), &QThread::started, worker_.get(), &signalforge::drivers::IoWorkerBase::onThreadStart);

    thread_->start();
}

FramePipeline::~FramePipeline() {
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
    // S2 stub: signal wiring lands in S3.
    driver_ = driver;
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
    // S2 stub: returns 0 until S4 wires WatermarkTracker.
    return 0;
}

FramePipeline::Stats FramePipeline::stats() const {
    // S2: only ingressDepthCurrent is meaningful (read-only approx from
    // the worker's queue). framesReceived / framesDropped / errorsForwarded
    // land in S3 and S4.
    Stats s;
    s.ingressDepthCurrent = worker_ ? worker_->ingressDepthApprox() : 0;
    s.snapshotAt = std::chrono::steady_clock::now();
    return s;
}

void FramePipeline::resetBackpressureStats() {
    // S2 stub: S4 will reset the peak tracker + drop counter.
}

const QString& FramePipeline::driverId() const noexcept {
    return config_.driverId;
}

}  // namespace signalforge::pipeline

#include "frame_pipeline.moc"
