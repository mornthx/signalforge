// src/expression/expression_engine.hpp
#pragma once

#include "buffer/signal_buffer_registry.hpp"
#include "decode/decoder_interface.hpp"
#include "expression/expression.hpp"

#include <QObject>
#include <QString>
#include <QTimer>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>

namespace signalforge::expression {

/// Configuration for `ExpressionEngine`.
struct ExpressionEngineConfig {
    /// Tick interval. Default 33 ms (≈ 30 Hz).
    std::chrono::milliseconds tickInterval{33};

    /// If true, the QTimer uses `Qt::PreciseTimer`. Default true.
    bool useTimePrecision = true;

    /// Driver ID used when registering derived signals with the
    /// `SignalBufferRegistry`. Default `"expression-engine"`.
    QString virtualDriverId = QStringLiteral("expression-engine");
};

/// 30 Hz batch-evaluating expression engine.
///
/// Reads source-signal values from a `SignalBufferRegistry`, evaluates
/// each registered expression in topologically sorted order, and writes
/// results back to the same registry as derived signals (sharing the
/// same `SignalValueSink::onSignal` interface as M5 decoders).
///
/// Lifecycle:
///   Constructed → setExpressions() → Ready
///   Ready → start() → Running (tick fires every `tickInterval`)
///   Running → stop() → Ready (registrations retained)
///   Running → destruction → Stopped + unregistered
///
/// Thread affinity: `QObject` living on the main thread (or whichever
/// thread the QTimer is to fire on). All `SignalValueSink` writes
/// happen from this thread.
///
/// Freeze scope: this header is frozen at M7 close. See spec §6.1.
class ExpressionEngine : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ExpressionEngine)

public:
    explicit ExpressionEngine(signalforge::buffer::SignalBufferRegistry& registry, ExpressionEngineConfig config = {},
                              QObject* parent = nullptr);
    ~ExpressionEngine() override;

    /// Register an already-validated `ExpressionSet` with the engine.
    /// Pre-condition: validator has done cycle detection + type checks.
    /// Post-condition: derived signals registered with the registry;
    /// engine ready for `start()`. Idempotent if called with the same
    /// set; replaces previous set otherwise.
    void setExpressions(ExpressionSet expressions);

    /// Start tick evaluation.
    void start();

    /// Stop tick evaluation. Registrations retained.
    void stop();

    /// Whether the engine is currently ticking.
    [[nodiscard]] bool isRunning() const noexcept;

    /// Number of registered expressions.
    [[nodiscard]] std::size_t expressionCount() const noexcept;

    /// Most-recent tick wall time in microseconds.
    [[nodiscard]] std::chrono::microseconds lastTickDurationUs() const noexcept;

    /// Tick statistics for diagnostics.
    struct TickStats {
        std::uint64_t ticksTotal = 0;
        std::uint64_t evaluationsTotal = 0;
        std::uint64_t evaluationErrors = 0;
        std::chrono::microseconds lastTickDurationUs{0};
        std::chrono::microseconds peakTickDurationUs{0};
    };
    [[nodiscard]] TickStats stats() const;

signals:
    void started();
    void stopped();
    void tickCompleted(std::uint64_t tickIndex);

private slots:
    void onTick();

private:
    signalforge::buffer::SignalBufferRegistry* registry_ = nullptr;
    ExpressionEngineConfig config_;
    QTimer tickTimer_;
    ExpressionSet expressions_;
    bool registered_ = false;

    mutable std::mutex statsMutex_;
    TickStats stats_;
};

}  // namespace signalforge::expression
