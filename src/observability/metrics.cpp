// src/observability/metrics.cpp
#include "observability/metrics.hpp"

#include "observability/logging.hpp"

#include <mutex>
#include <string>
#include <unordered_map>

namespace signalforge::observability {

bool isValidMetricName(const QString& name) {
    if (name.isEmpty()) {
        return false;
    }
    for (const QChar c : name) {
        if (c.isSpace() || c.category() == QChar::Other_Control) {
            return false;
        }
        const ushort u = c.unicode();
        if (u == '"' || u == '\'' || u == '\\' || u == '<' || u == '>') {
            return false;
        }
    }
    return true;
}

namespace {

// std::unordered_map is used rather than QHash because QHash cannot store
// move-only types (std::unique_ptr). Key is QString's UTF-8 representation
// so lookup doesn't depend on QString's hash behavior.
using MetricMap = std::unordered_map<std::string, std::unique_ptr<Metric>>;

}  // namespace

struct MetricsRegistry::Impl {
    mutable std::mutex mutex;
    MetricMap metrics;
};

MetricsRegistry::MetricsRegistry() : impl_(std::make_unique<Impl>()) {}
MetricsRegistry::~MetricsRegistry() = default;

MetricsRegistry& MetricsRegistry::instance() {
    static MetricsRegistry registry;
    return registry;
}

Metric* MetricsRegistry::getOrCreate(const QString& name, MetricKind kind) {
    if (!isValidMetricName(name)) {
        SF_LOG_ERROR("MetricsRegistry: rejected invalid metric name '{}'; see ADR-003 for policy", name.toStdString());
        return nullptr;
    }
    const std::string key = name.toStdString();
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (auto it = impl_->metrics.find(key); it != impl_->metrics.end()) {
        return it->second.get();
    }
    auto metric = std::make_unique<Metric>(name, kind);
    Metric* raw = metric.get();
    impl_->metrics.emplace(key, std::move(metric));
    return raw;
}

std::vector<std::pair<QString, std::int64_t>> MetricsRegistry::snapshot() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    std::vector<std::pair<QString, std::int64_t>> result;
    result.reserve(impl_->metrics.size());
    for (const auto& [key, metric] : impl_->metrics) {
        result.emplace_back(metric->name(), metric->value());
    }
    return result;
}

std::vector<QString> MetricsRegistry::metricNames() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    std::vector<QString> result;
    result.reserve(impl_->metrics.size());
    for (const auto& [key, metric] : impl_->metrics) {
        result.emplace_back(metric->name());
    }
    return result;
}

void MetricsRegistry::clearForTesting() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->metrics.clear();
}

}  // namespace signalforge::observability
