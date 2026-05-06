// src/expression/expression_engine.cpp
#include "expression/expression_engine.hpp"

namespace signalforge::expression {

ExpressionEngine::ExpressionEngine(signalforge::buffer::SignalBufferRegistry& registry, ExpressionEngineConfig config,
                                   QObject* parent)
    : QObject(parent), registry_(&registry), config_(std::move(config)) {
    tickTimer_.setInterval(config_.tickInterval);
    if (config_.useTimePrecision) {
        tickTimer_.setTimerType(Qt::PreciseTimer);
    }
    QObject::connect(&tickTimer_, &QTimer::timeout, this, &ExpressionEngine::onTick);
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
    // S4 wires registry registration + topological retention.
    expressions_ = std::move(expressions);
}

void ExpressionEngine::start() {
    // S4 wires the actual tick loop.
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
    // S4 wires the tick body. S1 emits the signal so wiring tests can
    // verify the timer connection.
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        ++stats_.ticksTotal;
    }
    Q_EMIT tickCompleted(stats_.ticksTotal);
}

}  // namespace signalforge::expression
