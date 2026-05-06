// src/buffer/signal_buffer_registry.cpp
#include "buffer/signal_buffer_registry.hpp"

#include "observability/logging.hpp"
#include "observability/metrics.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <utility>

namespace signalforge::buffer {

namespace {

using signalforge::observability::Metric;
using signalforge::observability::MetricKind;
using signalforge::observability::MetricsRegistry;

constexpr auto kMetricTotalMemoryBytes = QLatin1String("signal_buffer_total_memory_bytes");
constexpr auto kMetricBudgetWarned = QLatin1String("signal_buffer_budget_warned");
constexpr auto kMetricBudgetRejected = QLatin1String("signal_buffer_budget_rejected");

constexpr double kSoftWarnThreshold = 0.80;
constexpr std::size_t kTimestampBytes = sizeof(std::chrono::steady_clock::time_point);

/// Bytes per stored value (excluding the timestamp). Bool is bit-packed
/// (0.125 bytes); QString is the handle size (~24) plus a heuristic
/// allowance for small-string backing stores. Per spec §9 the estimate
/// is intentionally rough.
[[nodiscard]] double bytesPerValue(signalforge::decoder::SignalType type) noexcept {
    switch (type) {
    case signalforge::decoder::SignalType::Bool:
        return 0.125;
    case signalforge::decoder::SignalType::Int64:
        return 8.0;
    case signalforge::decoder::SignalType::Double:
        return 8.0;
    case signalforge::decoder::SignalType::String:
        return 32.0;
    }
    return 8.0;
}

/// LOD overhead factor (1.11 for numeric LOD-enabled signals; 1.0 for
/// Bool/QString/LOD-disabled).
[[nodiscard]] double lodOverheadFactor(signalforge::decoder::SignalType type, bool lodEnabledByConfig) noexcept {
    const bool numeric =
        (type == signalforge::decoder::SignalType::Int64 || type == signalforge::decoder::SignalType::Double);
    if (numeric && lodEnabledByConfig) {
        return 1.11;
    }
    return 1.0;
}

[[nodiscard]] std::size_t estimateSignalBytes(const signalforge::decoder::SignalMetadata& meta,
                                              const SignalBufferConfig& cfg) noexcept {
    const double rate = cfg.estimatedRateHz.value_or(1000.0);
    const double rawSamples = std::max(0.0, rate * cfg.windowSeconds);
    const double samples = std::min(rawSamples, static_cast<double>(cfg.capSamples));
    const bool lodEnabled = cfg.lodEnabled.value_or(true);
    const double bytes = samples * (bytesPerValue(meta.type) + static_cast<double>(kTimestampBytes)) *
                         lodOverheadFactor(meta.type, lodEnabled);
    return static_cast<std::size_t>(bytes);
}

}  // namespace

struct SignalBufferRegistry::Bookkeeping {
    Metric* totalMemoryMetric = nullptr;
    Metric* budgetWarnedMetric = nullptr;
    Metric* budgetRejectedMetric = nullptr;
    bool warnedAtThreshold = false;
};

SignalBufferRegistry::SignalBufferRegistry(RegistryConfig config)
    : config_(std::move(config)), bookkeeping_(std::make_unique<Bookkeeping>()) {
    auto& reg = MetricsRegistry::instance();
    bookkeeping_->totalMemoryMetric = reg.getOrCreate(kMetricTotalMemoryBytes, MetricKind::Gauge);
    bookkeeping_->budgetWarnedMetric = reg.getOrCreate(kMetricBudgetWarned, MetricKind::Counter);
    bookkeeping_->budgetRejectedMetric = reg.getOrCreate(kMetricBudgetRejected, MetricKind::Counter);
}

SignalBufferRegistry::~SignalBufferRegistry() = default;

void SignalBufferRegistry::onSignal(std::chrono::steady_clock::time_point timestamp, const QString& signalId,
                                    const signalforge::decoder::SignalValue& value) {
    SignalBuffer* buf = nullptr;
    {
        std::lock_guard<std::mutex> lock(registryMutex_);
        auto it = buffersBySignalId_.find(signalId);
        if (it == buffersBySignalId_.end()) {
            return;  // unregistered signal — silent miss
        }
        buf = it->second.get();
    }
    if (buf != nullptr) {
        buf->push(timestamp, value);
    }
}

void SignalBufferRegistry::onSignalsRegistered(const QString& driverId,
                                               const std::vector<signalforge::decoder::SignalMetadata>& signalsList) {
    if (signalsList.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(registryMutex_);

    // Resolve per-signal config (registry default + optional override).
    auto overrideIt = driverOverrides_.find(driverId);
    const SignalConfigOverrides* overrides = (overrideIt != driverOverrides_.end()) ? &overrideIt->second : nullptr;

    std::vector<std::pair<signalforge::decoder::SignalMetadata, SignalBufferConfig>> resolved;
    resolved.reserve(signalsList.size());
    std::size_t newBytes = 0;
    for (const auto& meta : signalsList) {
        SignalBufferConfig cfg = config_.signalDefaults;
        if (overrides != nullptr) {
            auto ovIt = overrides->find(meta.id);
            if (ovIt != overrides->end()) {
                cfg = ovIt->second;
            }
        }
        const std::size_t est = estimateSignalBytes(meta, cfg);
        newBytes += est;
        resolved.emplace_back(meta, cfg);
    }

    const std::size_t currentBytes = totalBytes_.load(std::memory_order_relaxed);
    const std::size_t afterBytes = currentBytes + newBytes;
    const std::size_t budget = config_.totalBudgetBytes;

    // Hard reject when registration would exceed 100% of the budget.
    if (afterBytes > budget && config_.rejectOnBudgetExceeded) {
        SF_LOG_ERROR("signal_buffer registration rejected: would exceed budget. "
                     "current={} bytes, requested={} bytes, budget={} bytes, driver={}",
                     currentBytes, newBytes, budget, driverId.toStdString());
        if (bookkeeping_->budgetRejectedMetric != nullptr) {
            bookkeeping_->budgetRejectedMetric->add(1);
        }
        return;
    }

    // Soft warn at 80% threshold (one-shot per crossing).
    const bool wasBelowThreshold = currentBytes < static_cast<std::size_t>(kSoftWarnThreshold * budget);
    const bool nowAtOrAbove = afterBytes >= static_cast<std::size_t>(kSoftWarnThreshold * budget);
    if (wasBelowThreshold && nowAtOrAbove) {
        const double percent = (static_cast<double>(afterBytes) / static_cast<double>(budget)) * 100.0;
        SF_LOG_WARN("signal_buffer budget {:.0f}% utilized after registration of driver {}", percent,
                    driverId.toStdString());
        if (bookkeeping_->budgetWarnedMetric != nullptr) {
            bookkeeping_->budgetWarnedMetric->add(1);
        }
    }

    // Allocate buffers and update bookkeeping.
    QStringList& driverIds = signalsByDriverId_[driverId];
    for (const auto& [meta, cfg] : resolved) {
        auto buf = std::make_unique<SignalBuffer>(meta, cfg);
        buffersBySignalId_[meta.id] = std::move(buf);
        driverIds.append(meta.id);
        const std::size_t est = estimateSignalBytes(meta, cfg);
        signalEstimates_[meta.id] = est;
    }

    totalBytes_.fetch_add(newBytes, std::memory_order_relaxed);
    if (bookkeeping_->totalMemoryMetric != nullptr) {
        bookkeeping_->totalMemoryMetric->set(static_cast<std::int64_t>(totalBytes_.load(std::memory_order_relaxed)));
    }
}

void SignalBufferRegistry::onSignalsUnregistered(const QString& driverId) {
    std::lock_guard<std::mutex> lock(registryMutex_);
    auto it = signalsByDriverId_.find(driverId);
    if (it == signalsByDriverId_.end()) {
        return;
    }
    std::size_t releasedBytes = 0;
    for (const auto& signalId : it->second) {
        auto estIt = signalEstimates_.find(signalId);
        if (estIt != signalEstimates_.end()) {
            releasedBytes += estIt->second;
            signalEstimates_.erase(estIt);
        }
        buffersBySignalId_.erase(signalId);
    }
    signalsByDriverId_.erase(it);

    if (releasedBytes > 0) {
        const std::size_t before = totalBytes_.load(std::memory_order_relaxed);
        const std::size_t after = (releasedBytes <= before) ? (before - releasedBytes) : 0;
        totalBytes_.store(after, std::memory_order_relaxed);
    }
    if (bookkeeping_->totalMemoryMetric != nullptr) {
        bookkeeping_->totalMemoryMetric->set(static_cast<std::int64_t>(totalBytes_.load(std::memory_order_relaxed)));
    }
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
