// src/pipeline/pipeline_manager.cpp
#include "pipeline/pipeline_manager.hpp"

#include "observability/logging.hpp"

#include <utility>

namespace signalforge::pipeline {

PipelineManager::PipelineManager(QObject* parent) : QObject(parent) {}

PipelineManager::~PipelineManager() {
    // Swap the map out under lock, then destroy pipelines outside the lock
    // so each unique_ptr's destructor can run without the manager's mutex
    // held (pipeline destructors join a thread which may take ≤500 ms).
    std::unordered_map<std::string, std::unique_ptr<FramePipeline>> doomed;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        doomed.swap(pipelines_);
    }
    for (auto& [key, pipeline] : doomed) {
        const QString driverId = pipeline->driverId();
        pipeline.reset();
        emit pipelineDetached(driverId);
    }
}

FramePipeline* PipelineManager::attach(signalforge::drivers::DriverInterface* driver, PipelineConfig config) {
    if (!driver) {
        SF_LOG_ERROR("PipelineManager::attach: driver is nullptr; refusing");
        return nullptr;
    }
    if (config.driverId.isEmpty()) {
        SF_LOG_ERROR("PipelineManager::attach: driverId is empty; refusing");
        return nullptr;
    }

    const std::string key = config.driverId.toStdString();
    const QString driverId = config.driverId;

    auto pipeline = std::make_unique<FramePipeline>(std::move(config));
    pipeline->attachDriver(driver);
    FramePipeline* raw = pipeline.get();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto [it, inserted] = pipelines_.emplace(key, std::move(pipeline));
        if (!inserted) {
            // A pipeline with this id already exists — release the one we
            // just built (it will destroy its thread on the way out) and
            // return nullptr.
            SF_LOG_ERROR("PipelineManager::attach: driverId '{}' already attached; refusing", driverId.toStdString());
            return nullptr;
        }
    }

    emit pipelineAttached(driverId, raw);
    return raw;
}

void PipelineManager::detach(const QString& driverId) {
    const std::string key = driverId.toStdString();
    std::unique_ptr<FramePipeline> doomed;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (auto it = pipelines_.find(key); it != pipelines_.end()) {
            doomed = std::move(it->second);
            pipelines_.erase(it);
        }
    }
    if (doomed) {
        // Destroy outside the lock so the pipeline's thread-join (≤500 ms
        // budget) does not block attach/lookup from other threads.
        doomed.reset();
        emit pipelineDetached(driverId);
    }
}

FramePipeline* PipelineManager::pipelineFor(const QString& driverId) const {
    const std::string key = driverId.toStdString();
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = pipelines_.find(key);
    return it != pipelines_.end() ? it->second.get() : nullptr;
}

std::size_t PipelineManager::pipelineCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pipelines_.size();
}

QStringList PipelineManager::driverIds() const {
    std::lock_guard<std::mutex> lock(mutex_);
    QStringList result;
    result.reserve(static_cast<qsizetype>(pipelines_.size()));
    for (const auto& [key, pipeline] : pipelines_) {
        result.append(pipeline->driverId());
    }
    return result;
}

}  // namespace signalforge::pipeline
