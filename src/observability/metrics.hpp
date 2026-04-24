// src/observability/metrics.hpp
#pragma once

#include <QString>
#include <atomic>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace signalforge::observability {

/// Classification of a metric. Drives how the performance panel formats
/// the row — counters show rates, gauges show raw values.
enum class MetricKind : int {
    Counter,  ///< Monotonic increasing (e.g., frames_received)
    Gauge,    ///< Can go up or down (e.g., queue_watermark_pct)
};

/// Single named metric. Owned by `MetricsRegistry`; consumers hold a raw
/// pointer returned by `MetricsRegistry::getOrCreate()`. Valid for the
/// lifetime of the process.
///
/// Thread safety: `add()`, `set()`, and `value()` are lock-free atomic ops.
/// `name()` and `kind()` return immutable data set at construction.
class Metric {
public:
    Metric(QString name, MetricKind kind) : name_(std::move(name)), kind_(kind) {}

    Metric(const Metric&) = delete;
    Metric& operator=(const Metric&) = delete;

    /// Counter use. Adds `delta` to the current value. Thread-safe.
    void add(std::int64_t delta) noexcept {
        value_.fetch_add(delta, std::memory_order_relaxed);
    }

    /// Gauge use. Sets the current value. Thread-safe.
    void set(std::int64_t newValue) noexcept {
        value_.store(newValue, std::memory_order_relaxed);
    }

    /// Current value. Thread-safe.
    [[nodiscard]] std::int64_t value() const noexcept {
        return value_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] const QString& name() const noexcept {
        return name_;
    }
    [[nodiscard]] MetricKind kind() const noexcept {
        return kind_;
    }

private:
    QString name_;
    MetricKind kind_;
    std::atomic<std::int64_t> value_{0};
};

/// Returns true when `name` is acceptable as a metric key.
///
/// Rejected:
/// - empty string
/// - any whitespace (QChar::isSpace) or Other_Control category
/// - quote / backslash / angle brackets (`"`, `'`, `\`, `<`, `>`) —
///   these characters break log parsing, JSON / YAML serialization, and
///   HTML display in downstream consumers
///
/// Accepted: everything else, including `:`, `/`, `.`, `-`, `@`, `[`,
/// `]`, Unicode letters/digits. Metric names are not query-language
/// identifiers in this project — they appear only in the internal
/// snapshot API, log output, and (future) UI display, so the validation
/// is intentionally permissive.
///
/// See ADR-003 for the rationale (considered and rejected: Prometheus-
/// style `[a-zA-Z_][a-zA-Z0-9_]*`; sanitization with information loss).
[[nodiscard]] bool isValidMetricName(const QString& name);

/// Process-wide registry of named metrics. Producers call `getOrCreate()`
/// to obtain a `Metric*` and then use its atomic accessors directly. The
/// M5 performance panel will consume `snapshot()` at ~30Hz.
///
/// Thread safety: `getOrCreate()`, `snapshot()`, and `metricNames()` are
/// safe from any thread.
///
/// Not part of the M2 freeze surface (spec §6.2) — expected to evolve in
/// M5 when the performance panel is built.
class MetricsRegistry {
public:
    /// Singleton accessor. Thread-safe initialization (Meyers singleton).
    static MetricsRegistry& instance();

    MetricsRegistry(const MetricsRegistry&) = delete;
    MetricsRegistry& operator=(const MetricsRegistry&) = delete;

    ~MetricsRegistry();

    /// Fetch the existing metric with `name`, or create a new one with
    /// `kind` if absent. If a metric with the given name exists with a
    /// different `kind`, the existing metric is returned unchanged — the
    /// first registrant wins. The returned pointer is owned by the
    /// registry and valid for the process lifetime.
    ///
    /// Returns `nullptr` if `name` fails `isValidMetricName` (logs ERROR
    /// once). Callers should treat a nullptr return as a misconfigured
    /// name and skip tracking that metric — `add()` / `set()` must not
    /// be invoked on a nullptr.
    Metric* getOrCreate(const QString& name, MetricKind kind);

    /// Snapshot all metrics as (name, value) pairs. Each pair is a
    /// point-in-time capture; cross-metric consistency is not guaranteed
    /// (callers tolerate ~33ms skew per architecture §14.2).
    [[nodiscard]] std::vector<std::pair<QString, std::int64_t>> snapshot() const;

    /// All registered metric names. Useful for the panel's row list.
    [[nodiscard]] std::vector<QString> metricNames() const;

    /// Test-only: clears the registry. Do NOT call in production code —
    /// existing `Metric*` pointers are invalidated. Intended for test
    /// setup to guarantee a clean per-test state.
    void clearForTesting();

private:
    MetricsRegistry();
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace signalforge::observability
