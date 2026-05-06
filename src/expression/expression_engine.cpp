// src/expression/expression_engine.cpp
#include "expression/expression_engine.hpp"

#include "buffer/signal_buffer.hpp"
#include "observability/logging.hpp"
#include "observability/metrics.hpp"

#include <QString>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <limits>
#include <utility>
#include <vector>

namespace signalforge::expression {

namespace {

using signalforge::observability::Metric;
using signalforge::observability::MetricKind;
using signalforge::observability::MetricsRegistry;

constexpr auto kMetricTicksTotal = QLatin1String("expression_engine_ticks_total");
constexpr auto kMetricTickUs = QLatin1String("expression_engine_tick_us");
constexpr auto kMetricEvaluationsTotal = QLatin1String("expression_engine_evaluations_total");
constexpr auto kMetricEvalUsPrefix = QLatin1String("expression_evaluation_us_");
constexpr auto kMetricEvalErrorsPrefix = QLatin1String("expression_evaluation_errors_");

}  // namespace

struct ExpressionEngine::ExpressionEnginePrivateOpaque {
    Metric* ticksTotal = nullptr;
    Metric* tickUs = nullptr;
    Metric* evaluationsTotal = nullptr;

    /// Per-expression metrics indexed by expression position in
    /// `expressions_.expressions`.
    std::vector<Metric*> evalUsMetrics;
    std::vector<Metric*> evalErrorMetrics;

    /// Per-expression last-warn timestamp (steady) for the
    /// once-per-second rate-limit on evaluation-error logs.
    std::vector<std::chrono::steady_clock::time_point> lastWarnAt;
};

ExpressionEngine::ExpressionEngine(signalforge::buffer::SignalBufferRegistry& registry, ExpressionEngineConfig config,
                                   QObject* parent)
    : QObject(parent), registry_(&registry), config_(std::move(config)),
      privateState_(std::make_unique<ExpressionEngine::ExpressionEnginePrivateOpaque>()) {
    tickTimer_.setInterval(config_.tickInterval);
    if (config_.useTimePrecision) {
        tickTimer_.setTimerType(Qt::PreciseTimer);
    }
    QObject::connect(&tickTimer_, &QTimer::timeout, this, &ExpressionEngine::onTick);

    auto& reg = MetricsRegistry::instance();
    privateState_->ticksTotal = reg.getOrCreate(kMetricTicksTotal, MetricKind::Counter);
    privateState_->tickUs = reg.getOrCreate(kMetricTickUs, MetricKind::Gauge);
    privateState_->evaluationsTotal = reg.getOrCreate(kMetricEvaluationsTotal, MetricKind::Counter);
}

ExpressionEngine::~ExpressionEngine() {
    if (tickTimer_.isActive()) {
        tickTimer_.stop();
    }
    if (registered_ && registry_ != nullptr) {
        registry_->onSignalsUnregistered(config_.virtualDriverId);
    }
}

void ExpressionEngine::setExpressions(ExpressionSet expressions) {
    if (registered_ && registry_ != nullptr) {
        registry_->onSignalsUnregistered(config_.virtualDriverId);
        registered_ = false;
    }

    expressions_ = std::move(expressions);

    // Build derived-signal metadata catalog.
    std::vector<signalforge::decoder::SignalMetadata> derivedMetas;
    derivedMetas.reserve(expressions_.expressions.size());
    for (const auto& expr : expressions_.expressions) {
        derivedMetas.push_back(expr.derivedSignalMetadata());
    }

    if (registry_ != nullptr && !derivedMetas.empty()) {
        registry_->onSignalsRegistered(config_.virtualDriverId, derivedMetas);
        registered_ = true;
    }

    // Register per-expression metrics.
    auto& reg = MetricsRegistry::instance();
    privateState_->evalUsMetrics.clear();
    privateState_->evalErrorMetrics.clear();
    privateState_->lastWarnAt.assign(expressions_.expressions.size(), std::chrono::steady_clock::time_point{});
    for (const auto& expr : expressions_.expressions) {
        privateState_->evalUsMetrics.push_back(reg.getOrCreate(kMetricEvalUsPrefix + expr.id(), MetricKind::Gauge));
        privateState_->evalErrorMetrics.push_back(
            reg.getOrCreate(kMetricEvalErrorsPrefix + expr.id(), MetricKind::Counter));
    }
}

void ExpressionEngine::start() {
    if (tickTimer_.isActive()) {
        return;
    }
    tickTimer_.start();
    Q_EMIT started();
}

void ExpressionEngine::stop() {
    if (tickTimer_.isActive()) {
        tickTimer_.stop();
        Q_EMIT stopped();
    }
}

bool ExpressionEngine::isRunning() const noexcept {
    return tickTimer_.isActive();
}

std::size_t ExpressionEngine::expressionCount() const noexcept {
    return expressions_.expressions.size();
}

std::chrono::microseconds ExpressionEngine::lastTickDurationUs() const noexcept {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_.lastTickDurationUs;
}

ExpressionEngine::TickStats ExpressionEngine::stats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_;
}

void ExpressionEngine::onTick() {
    if (registry_ == nullptr) {
        return;
    }

    const auto t0 = std::chrono::steady_clock::now();
    std::uint64_t evaluationsThisTick = 0;
    std::uint64_t errorsThisTick = 0;

    // Source-value buffer reused across expressions to avoid per-tick
    // allocations on the hot path.
    std::vector<std::pair<QString, signalforge::decoder::SignalValue>> sources;

    for (std::size_t i = 0; i < expressions_.expressions.size(); ++i) {
        const auto& expr = expressions_.expressions[i];
        const auto evalStart = std::chrono::steady_clock::now();

        // Collect source values.
        sources.clear();
        sources.reserve(expr.sourceSignalIds().size());
        for (const auto& srcId : expr.sourceSignalIds()) {
            auto* buf = registry_->bufferFor(srcId);
            if (buf == nullptr) {
                sources.emplace_back(srcId,
                                     signalforge::decoder::SignalValue{std::numeric_limits<double>::quiet_NaN()});
                continue;
            }
            auto latest = buf->queryLatestOne();
            if (!latest.has_value()) {
                sources.emplace_back(srcId,
                                     signalforge::decoder::SignalValue{std::numeric_limits<double>::quiet_NaN()});
            } else {
                sources.emplace_back(srcId, latest->value);
            }
        }

        // Evaluate.
        signalforge::decoder::SignalValue result{std::numeric_limits<double>::quiet_NaN()};
        bool errored = false;
        try {
            result = expr.evaluate(sources);
        } catch (const std::exception& e) {
            errored = true;
            ++errorsThisTick;
            if (i < privateState_->evalErrorMetrics.size() && privateState_->evalErrorMetrics[i] != nullptr) {
                privateState_->evalErrorMetrics[i]->add(1);
            }
            // Rate-limit warn logs to once per second per expression.
            const auto now = std::chrono::steady_clock::now();
            if (i < privateState_->lastWarnAt.size()) {
                if ((now - privateState_->lastWarnAt[i]) >= std::chrono::seconds(1)) {
                    SF_LOG_WARN("expression: evaluation of '{}' threw: {}", expr.id().toStdString(), e.what());
                    privateState_->lastWarnAt[i] = now;
                }
            }
        }

        const auto evalEnd = std::chrono::steady_clock::now();
        const auto evalUs = std::chrono::duration_cast<std::chrono::microseconds>(evalEnd - evalStart).count();
        if (i < privateState_->evalUsMetrics.size() && privateState_->evalUsMetrics[i] != nullptr) {
            privateState_->evalUsMetrics[i]->set(static_cast<std::int64_t>(evalUs));
        }

        ++evaluationsThisTick;

        // Push result regardless of error (NaN result on error;
        // M8 chart renders gap; spec §4.7).
        (void)errored;  // status reflected in metric
        registry_->onSignal(t0, expr.id(), result);
    }

    const auto t1 = std::chrono::steady_clock::now();
    const auto tickDuration = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0);

    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        ++stats_.ticksTotal;
        stats_.evaluationsTotal += evaluationsThisTick;
        stats_.evaluationErrors += errorsThisTick;
        stats_.lastTickDurationUs = tickDuration;
        if (tickDuration > stats_.peakTickDurationUs) {
            stats_.peakTickDurationUs = tickDuration;
        }
    }

    if (privateState_->ticksTotal != nullptr) {
        privateState_->ticksTotal->add(1);
    }
    if (privateState_->evaluationsTotal != nullptr) {
        privateState_->evaluationsTotal->add(static_cast<std::int64_t>(evaluationsThisTick));
    }
    if (privateState_->tickUs != nullptr) {
        privateState_->tickUs->set(static_cast<std::int64_t>(tickDuration.count()));
    }

    Q_EMIT tickCompleted(stats_.ticksTotal);
}

}  // namespace signalforge::expression
