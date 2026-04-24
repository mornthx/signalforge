// tests/unit/observability/metrics_test.cpp
#include "observability/metrics.hpp"

#include <algorithm>
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <vector>

using namespace signalforge::observability;

namespace {

MetricsRegistry& freshRegistry() {
    auto& r = MetricsRegistry::instance();
    r.clearForTesting();
    return r;
}

}  // namespace

TEST_CASE("Metric: counter add accumulates", "[observability][metrics]") {
    Metric m(QStringLiteral("test_counter"), MetricKind::Counter);
    REQUIRE(m.value() == 0);
    m.add(5);
    m.add(3);
    REQUIRE(m.value() == 8);
}

TEST_CASE("Metric: gauge set replaces", "[observability][metrics]") {
    Metric m(QStringLiteral("test_gauge"), MetricKind::Gauge);
    m.set(42);
    REQUIRE(m.value() == 42);
    m.set(17);
    REQUIRE(m.value() == 17);
    m.set(-5);
    REQUIRE(m.value() == -5);
}

TEST_CASE("Metric: name and kind are immutable", "[observability][metrics]") {
    Metric m(QStringLiteral("foo"), MetricKind::Counter);
    REQUIRE(m.name() == "foo");
    REQUIRE(m.kind() == MetricKind::Counter);
    m.add(1);
    REQUIRE(m.name() == "foo");
    REQUIRE(m.kind() == MetricKind::Counter);
}

TEST_CASE("MetricsRegistry: getOrCreate returns same pointer on repeat calls", "[observability][metrics]") {
    auto& r = freshRegistry();
    auto* a = r.getOrCreate(QStringLiteral("m1"), MetricKind::Counter);
    auto* b = r.getOrCreate(QStringLiteral("m1"), MetricKind::Counter);
    REQUIRE(a == b);
}

TEST_CASE("MetricsRegistry: different names produce different metrics", "[observability][metrics]") {
    auto& r = freshRegistry();
    auto* a = r.getOrCreate(QStringLiteral("m1"), MetricKind::Counter);
    auto* b = r.getOrCreate(QStringLiteral("m2"), MetricKind::Gauge);
    REQUIRE(a != b);
    REQUIRE(a->kind() == MetricKind::Counter);
    REQUIRE(b->kind() == MetricKind::Gauge);
}

TEST_CASE("MetricsRegistry: existing-kind wins on mismatched second registration", "[observability][metrics]") {
    auto& r = freshRegistry();
    auto* a = r.getOrCreate(QStringLiteral("m1"), MetricKind::Counter);
    auto* b = r.getOrCreate(QStringLiteral("m1"), MetricKind::Gauge);
    REQUIRE(a == b);
    REQUIRE(a->kind() == MetricKind::Counter);
}

TEST_CASE("MetricsRegistry: snapshot captures current values", "[observability][metrics]") {
    auto& r = freshRegistry();
    auto* a = r.getOrCreate(QStringLiteral("c1"), MetricKind::Counter);
    auto* b = r.getOrCreate(QStringLiteral("g1"), MetricKind::Gauge);
    a->add(10);
    b->set(55);

    auto snap = r.snapshot();
    REQUIRE(snap.size() == 2);

    auto findValue = [&](const QString& name) -> std::int64_t {
        auto it = std::find_if(snap.begin(), snap.end(), [&](const auto& pair) { return pair.first == name; });
        REQUIRE(it != snap.end());
        return it->second;
    };
    REQUIRE(findValue(QStringLiteral("c1")) == 10);
    REQUIRE(findValue(QStringLiteral("g1")) == 55);
}

TEST_CASE("MetricsRegistry: metricNames returns all registered", "[observability][metrics]") {
    auto& r = freshRegistry();
    r.getOrCreate(QStringLiteral("a"), MetricKind::Counter);
    r.getOrCreate(QStringLiteral("b"), MetricKind::Counter);
    r.getOrCreate(QStringLiteral("c"), MetricKind::Gauge);

    auto names = r.metricNames();
    REQUIRE(names.size() == 3);
    std::sort(names.begin(), names.end());
    REQUIRE(names[0] == "a");
    REQUIRE(names[1] == "b");
    REQUIRE(names[2] == "c");
}

TEST_CASE("MetricsRegistry: 8-thread × 1M counter increment stress", "[observability][metrics][stress]") {
    auto& r = freshRegistry();
    auto* counter = r.getOrCreate(QStringLiteral("stress_counter"), MetricKind::Counter);

    constexpr int kThreads = 8;
    constexpr int kPerThread = 1'000'000;
    std::vector<std::thread> workers;
    workers.reserve(kThreads);
    for (int i = 0; i < kThreads; ++i) {
        workers.emplace_back([&] {
            for (int j = 0; j < kPerThread; ++j) {
                counter->add(1);
            }
        });
    }
    for (auto& t : workers) {
        t.join();
    }

    REQUIRE(counter->value() == static_cast<std::int64_t>(kThreads) * kPerThread);
}

TEST_CASE("MetricsRegistry: getOrCreate and snapshot concurrent are safe", "[observability][metrics][stress]") {
    auto& r = freshRegistry();

    std::atomic<bool> stop{false};
    std::thread reader([&] {
        while (!stop.load(std::memory_order_relaxed)) {
            auto snap = r.snapshot();
            (void)snap;
        }
    });

    // Producers register distinct metric names concurrently.
    std::vector<std::thread> producers;
    for (int i = 0; i < 4; ++i) {
        producers.emplace_back([&, i] {
            for (int j = 0; j < 1000; ++j) {
                auto name = QString("p%1_m%2").arg(i).arg(j);
                auto* m = r.getOrCreate(name, MetricKind::Counter);
                m->add(1);
            }
        });
    }
    for (auto& t : producers) {
        t.join();
    }
    stop.store(true, std::memory_order_relaxed);
    reader.join();

    auto names = r.metricNames();
    REQUIRE(names.size() == 4 * 1000);
}

TEST_CASE("isValidMetricName: accepts permissive set per ADR-003", "[observability][metrics]") {
    using signalforge::observability::isValidMetricName;
    REQUIRE(isValidMetricName(QStringLiteral("snake_case")));
    REQUIRE(isValidMetricName(QStringLiteral("pipeline_frames_received_serial:/tmp/ttyV0")));
    REQUIRE(isValidMetricName(QStringLiteral("tcp_rx_bytes_localhost:9000")));
    REQUIRE(isValidMetricName(QStringLiteral("metric.with.dots")));
    REQUIRE(isValidMetricName(QStringLiteral("metric-with-dashes")));
    REQUIRE(isValidMetricName(QStringLiteral("metric@host")));
    REQUIRE(isValidMetricName(QStringLiteral("metric[index]")));
    REQUIRE(isValidMetricName(QStringLiteral("队列_水位")));
}

TEST_CASE("isValidMetricName: rejects tooling-hostile characters per ADR-003", "[observability][metrics]") {
    using signalforge::observability::isValidMetricName;
    REQUIRE_FALSE(isValidMetricName(QString()));
    REQUIRE_FALSE(isValidMetricName(QStringLiteral("")));
    REQUIRE_FALSE(isValidMetricName(QStringLiteral("metric with space")));
    REQUIRE_FALSE(isValidMetricName(QStringLiteral("metric\twith\ttab")));
    REQUIRE_FALSE(isValidMetricName(QStringLiteral("metric\nwith\nnewline")));
    REQUIRE_FALSE(isValidMetricName(QStringLiteral("metric\"with\"quote")));
    REQUIRE_FALSE(isValidMetricName(QStringLiteral("metric'with'apostrophe")));
    REQUIRE_FALSE(isValidMetricName(QStringLiteral("metric\\with\\backslash")));
    REQUIRE_FALSE(isValidMetricName(QStringLiteral("metric<with>angle")));
}

TEST_CASE("MetricsRegistry: getOrCreate returns nullptr on invalid name", "[observability][metrics]") {
    auto& r = freshRegistry();
    REQUIRE(r.getOrCreate(QString(), MetricKind::Counter) == nullptr);
    REQUIRE(r.getOrCreate(QStringLiteral("metric with space"), MetricKind::Counter) == nullptr);
    REQUIRE(r.getOrCreate(QStringLiteral("metric<with>angle"), MetricKind::Gauge) == nullptr);
    REQUIRE(r.metricNames().empty());  // No side effect on rejected names
}

TEST_CASE("MetricsRegistry: getOrCreate accepts separator-rich driver IDs per ADR-003", "[observability][metrics]") {
    auto& r = freshRegistry();
    const QString name = QStringLiteral("pipeline_frames_received_tcp:127.0.0.1:9000");
    auto* a = r.getOrCreate(name, MetricKind::Counter);
    REQUIRE(a != nullptr);
    REQUIRE(a->name() == name);
    auto* b = r.getOrCreate(name, MetricKind::Counter);
    REQUIRE(b == a);
}

TEST_CASE("MetricsRegistry: clearForTesting empties the registry", "[observability][metrics]") {
    auto& r = MetricsRegistry::instance();
    r.clearForTesting();
    REQUIRE(r.metricNames().empty());
    r.getOrCreate(QStringLiteral("x"), MetricKind::Counter);
    REQUIRE(r.metricNames().size() == 1);
    r.clearForTesting();
    REQUIRE(r.metricNames().empty());
}
