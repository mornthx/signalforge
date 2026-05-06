// src/buffer/signal_buffer_registry.cpp
#include "buffer/signal_buffer_registry.hpp"

namespace signalforge::buffer {

SignalBufferRegistry::SignalBufferRegistry(RegistryConfig config) : config_(std::move(config)) {
    // S7 wires budget tracking + metric registration.
}

SignalBufferRegistry::~SignalBufferRegistry() = default;

void SignalBufferRegistry::onSignal(std::chrono::steady_clock::time_point /*timestamp*/, const QString& /*signalId*/,
                                    const signalforge::decoder::SignalValue& /*value*/) {
    // S7 wires lookup + dispatch to the matching SignalBuffer.
}

void SignalBufferRegistry::onSignalsRegistered(
    const QString& /*driverId*/, const std::vector<signalforge::decoder::SignalMetadata>& /*signalsList*/) {
    // S7 wires budget estimation + buffer allocation.
}

void SignalBufferRegistry::onSignalsUnregistered(const QString& /*driverId*/) {
    // S7 wires buffer release + budget reclaim.
}

SignalBuffer* SignalBufferRegistry::bufferFor(const QString& signalId) const {
    std::lock_guard<std::mutex> lock(registryMutex_);
    auto it = buffersBySignalId_.find(signalId);
    return it != buffersBySignalId_.end() ? it->second.get() : nullptr;
}

QStringList SignalBufferRegistry::signalIds() const {
    std::lock_guard<std::mutex> lock(registryMutex_);
    QStringList out;
    out.reserve(static_cast<int>(buffersBySignalId_.size()));
    for (const auto& [id, _] : buffersBySignalId_) {
        out.append(id);
    }
    return out;
}

QStringList SignalBufferRegistry::signalIdsForDriver(const QString& driverId) const {
    std::lock_guard<std::mutex> lock(registryMutex_);
    auto it = signalsByDriverId_.find(driverId);
    return it != signalsByDriverId_.end() ? it->second : QStringList{};
}

void SignalBufferRegistry::setDriverConfigOverrides(const QString& driverId, const SignalConfigOverrides& overrides) {
    std::lock_guard<std::mutex> lock(registryMutex_);
    driverOverrides_[driverId] = overrides;
}

std::size_t SignalBufferRegistry::totalMemoryBytes() const {
    return totalBytes_.load(std::memory_order_relaxed);
}

std::size_t SignalBufferRegistry::totalBudgetBytes() const {
    return config_.totalBudgetBytes;
}

std::size_t SignalBufferRegistry::signalCount() const {
    std::lock_guard<std::mutex> lock(registryMutex_);
    return buffersBySignalId_.size();
}

SignalBufferRegistry::UsageReport SignalBufferRegistry::memoryUsage() const {
    UsageReport report;
    report.budgetBytes = config_.totalBudgetBytes;
    std::lock_guard<std::mutex> lock(registryMutex_);
    report.totalBytes = totalBytes_.load(std::memory_order_relaxed);
    report.drivers.reserve(signalsByDriverId_.size());
    for (const auto& [driverId, signalIdList] : signalsByDriverId_) {
        UsageReport::PerDriver entry;
        entry.driverId = driverId;
        entry.signalCount = signalIdList.size();
        entry.bytes = 0;
        for (const auto& signalId : signalIdList) {
            auto it = buffersBySignalId_.find(signalId);
            if (it != buffersBySignalId_.end() && it->second) {
                entry.bytes += it->second->memoryBytes();
            }
        }
        report.drivers.push_back(std::move(entry));
    }
    return report;
}

}  // namespace signalforge::buffer
