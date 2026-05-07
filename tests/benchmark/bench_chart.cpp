// tests/benchmark/bench_chart.cpp
//
// M8 chart benchmark: reproduces the M8-prototype's 4 scenarios as
// production benchmarks against the real Chart / ChartManager / M6
// SignalBufferRegistry stack.
//
// Measurement design: drives `Chart::onTick` directly via
// `QMetaObject::invokeMethod`. This isolates the per-tick cost
// (Scene Graph sync + queryRange + auto-scale + LOD selection)
// from vsync wait. Maps to spec §5.1's "render-loop p99 < 1 ms"
// gate. The prototype's vsync-bound frame-interval measurements
// remain authoritative for UX cadence.
//
// One JSON line per scenario plus a final summary, matching the
// existing M3/M5/M6/M7 bench output format. M8-baseline.md is
// authored from these numbers.

#include "buffer/signal_buffer.hpp"
#include "buffer/signal_buffer_registry.hpp"
#include "chart/chart.hpp"
#include "chart/time_axis_manager.hpp"
#include "decode/decoder_interface.hpp"

#include <QGuiApplication>
#include <QMetaObject>
#include <QString>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using signalforge::buffer::SignalBufferRegistry;
using signalforge::chart::Chart;
using signalforge::chart::TimeAxisManager;
using signalforge::decoder::SignalMetadata;
using signalforge::decoder::SignalType;
using signalforge::decoder::SignalValue;

namespace {

double percentile(std::vector<double> sortedValues, double p) {
    if (sortedValues.empty()) {
        return 0.0;
    }
    const double pos = p * static_cast<double>(sortedValues.size() - 1);
    const std::size_t lo = static_cast<std::size_t>(pos);
    const std::size_t hi = std::min(lo + 1, sortedValues.size() - 1);
    const double frac = pos - static_cast<double>(lo);
    return sortedValues[lo] * (1.0 - frac) + sortedValues[hi] * frac;
}

SignalMetadata makeMeta(QString id, SignalType type) {
    SignalMetadata m;
    m.id = std::move(id);
    m.name = QStringLiteral("");
    m.unit = QStringLiteral("");
    m.type = type;
    return m;
}

void runOneTick(Chart& chart) {
    QMetaObject::invokeMethod(&chart, "onTick", Qt::DirectConnection);
}

void printResult(const char* scenario, int charts, int signalCount, int ticks, const std::vector<double>& tickMs) {
    std::vector<double> sorted = tickMs;
    std::sort(sorted.begin(), sorted.end());
    const double p50 = percentile(sorted, 0.50);
    const double p95 = percentile(sorted, 0.95);
    const double p99 = percentile(sorted, 0.99);
    const double pMax = sorted.back();
    double sum = 0.0;
    for (double v : sorted) {
        sum += v;
    }
    const double mean = sum / static_cast<double>(sorted.size());
    std::printf("{\"scenario\":\"%s\",\"charts\":%d,\"signals\":%d,\"ticks\":%d,"
                "\"p50_ms\":%.4f,\"p95_ms\":%.4f,\"p99_ms\":%.4f,\"max_ms\":%.4f,"
                "\"mean_ms\":%.4f}\n",
                scenario, charts, signalCount, ticks, p50, p95, p99, pMax, mean);
    std::fflush(stdout);
}

// ---- Scenario 1: 60 charts × 1000 static samples, ticks ----

void runScenario1(SignalBufferRegistry& registry) {
    constexpr int kCharts = 60;
    constexpr int kSamples = 1000;
    constexpr int kTicks = 1000;

    std::vector<SignalMetadata> metas;
    metas.reserve(kCharts);
    for (int i = 0; i < kCharts; ++i) {
        metas.push_back(makeMeta(QStringLiteral("d/s%1").arg(i), SignalType::Double));
    }
    registry.onSignalsRegistered(QStringLiteral("d"), metas);

    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kSamples; ++i) {
        for (int c = 0; c < kCharts; ++c) {
            registry.onSignal(t0 + std::chrono::microseconds(i * 1000), metas[c].id,
                              SignalValue{std::sin(i * 0.01 + c * 0.1)});
        }
    }

    TimeAxisManager axis;
    Chart chart(registry, axis);
    chart.setWidth(1920);
    chart.setHeight(1080);
    for (const auto& m : metas) {
        chart.addSignal(m.id);
    }

    std::vector<double> tickMs;
    tickMs.reserve(kTicks);
    for (int t = 0; t < kTicks; ++t) {
        const auto a = std::chrono::steady_clock::now();
        runOneTick(chart);
        const auto b = std::chrono::steady_clock::now();
        tickMs.push_back(std::chrono::duration<double, std::milli>(b - a).count());
    }
    printResult("scenario_1_pure_render", 1, kCharts, kTicks, tickMs);
}

// ---- Scenario 2: 60 signals × 1 kHz updates × 30 Hz render ----

void runScenario2(SignalBufferRegistry& registry) {
    constexpr int kCharts = 60;
    constexpr int kInitialSamples = 1000;
    constexpr int kFrames = 900;  // 30 sec × 30 Hz

    std::vector<SignalMetadata> metas;
    metas.reserve(kCharts);
    for (int i = 0; i < kCharts; ++i) {
        metas.push_back(makeMeta(QStringLiteral("u/s%1").arg(i), SignalType::Double));
    }
    registry.onSignalsRegistered(QStringLiteral("u"), metas);

    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kInitialSamples; ++i) {
        for (int c = 0; c < kCharts; ++c) {
            registry.onSignal(t0 + std::chrono::microseconds(i * 1000), metas[c].id,
                              SignalValue{std::sin(i * 0.01 + c * 0.1)});
        }
    }

    TimeAxisManager axis;
    Chart chart(registry, axis);
    chart.setWidth(1920);
    chart.setHeight(1080);
    for (const auto& m : metas) {
        chart.addSignal(m.id);
    }

    std::vector<double> tickMs;
    tickMs.reserve(kFrames);
    std::int64_t sampleIdx = kInitialSamples;
    for (int f = 0; f < kFrames; ++f) {
        // Inject ~33 samples per chart per frame (30 fps × 1 kHz =
        // 33 samples between frames).
        for (int j = 0; j < 33; ++j) {
            for (int c = 0; c < kCharts; ++c) {
                registry.onSignal(t0 + std::chrono::microseconds(sampleIdx * 1000), metas[c].id,
                                  SignalValue{std::sin(sampleIdx * 0.01 + c * 0.1)});
            }
            ++sampleIdx;
        }
        const auto a = std::chrono::steady_clock::now();
        runOneTick(chart);
        const auto b = std::chrono::steady_clock::now();
        tickMs.push_back(std::chrono::duration<double, std::milli>(b - a).count());
    }
    printResult("scenario_2_update_render", 1, kCharts, kFrames, tickMs);
}

// ---- Scenario 3: 3 charts × 20 signals + pan ----

void runScenario3(SignalBufferRegistry& registry) {
    constexpr int kCharts = 3;
    constexpr int kSignalsPerChart = 20;
    constexpr int kSamples = 2000;
    constexpr int kTicks = 1000;
    constexpr int kTotalSignals = kCharts * kSignalsPerChart;

    std::vector<SignalMetadata> metas;
    metas.reserve(kTotalSignals);
    for (int i = 0; i < kTotalSignals; ++i) {
        metas.push_back(makeMeta(QStringLiteral("p/s%1").arg(i), SignalType::Double));
    }
    registry.onSignalsRegistered(QStringLiteral("p"), metas);

    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kSamples; ++i) {
        for (int c = 0; c < kTotalSignals; ++c) {
            registry.onSignal(t0 + std::chrono::microseconds(i * 1000), metas[c].id,
                              SignalValue{std::sin(i * 0.005 + c * 0.07)});
        }
    }

    TimeAxisManager axis;
    std::vector<std::unique_ptr<Chart>> charts;
    for (int c = 0; c < kCharts; ++c) {
        auto chart = std::make_unique<Chart>(registry, axis);
        chart->setWidth(1920);
        chart->setHeight(360);
        for (int s = 0; s < kSignalsPerChart; ++s) {
            chart->addSignal(metas[c * kSignalsPerChart + s].id);
        }
        charts.push_back(std::move(chart));
    }

    std::vector<double> tickMs;
    tickMs.reserve(kTicks);
    for (int t = 0; t < kTicks; ++t) {
        // Pan 25 units / frame (matches M8 prototype).
        axis.pan(std::chrono::microseconds(25 * 1000));
        const auto a = std::chrono::steady_clock::now();
        for (auto& c : charts) {
            runOneTick(*c);
        }
        const auto b = std::chrono::steady_clock::now();
        tickMs.push_back(std::chrono::duration<double, std::milli>(b - a).count());
    }
    printResult("scenario_3_pan", kCharts, kTotalSignals, kTicks, tickMs);
}

// ---- Scenario 4: LOD pyramid integration ----

void runScenario4(SignalBufferRegistry& registry) {
    // 600 k samples (matches the M8 prototype). LOD pyramid spans
    // 4 levels at this scale per M6 §4.5.
    constexpr int kSamples = 600'000;
    constexpr int kTicks = 1000;
    constexpr double kSampleHz = 1000.0;

    SignalMetadata meta = makeMeta(QStringLiteral("lod/sig"), SignalType::Double);
    registry.onSignalsRegistered(QStringLiteral("lod"), {meta});

    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kSamples; ++i) {
        const double y = std::sin(i * 0.01);
        registry.onSignal(t0 + std::chrono::microseconds(static_cast<std::int64_t>(i * 1e6 / kSampleHz)), meta.id,
                          SignalValue{y});
    }

    TimeAxisManager axis;
    Chart chart(registry, axis);
    chart.setWidth(1920);
    chart.setHeight(1080);
    chart.addSignal(meta.id);

    std::vector<double> tickMs;
    tickMs.reserve(kTicks);
    for (int t = 0; t < kTicks; ++t) {
        // Cycle through 4 zoom levels every ~30 ticks.
        const int zoomIdx = (t / 30) % 4;
        const auto end = t0 + std::chrono::microseconds(static_cast<std::int64_t>(kSamples * 1e6 / kSampleHz));
        std::chrono::nanoseconds duration{};
        switch (zoomIdx) {
        case 0:
            duration = std::chrono::seconds(1);
            break;
        case 1:
            duration = std::chrono::seconds(10);
            break;
        case 2:
            duration = std::chrono::seconds(60);
            break;
        case 3:
            duration = std::chrono::seconds(600);
            break;
        }
        axis.setRange(end - duration, end);
        const auto a = std::chrono::steady_clock::now();
        runOneTick(chart);
        const auto b = std::chrono::steady_clock::now();
        tickMs.push_back(std::chrono::duration<double, std::milli>(b - a).count());
    }
    printResult("scenario_4_lod", 1, 1, kTicks, tickMs);
}

}  // namespace

int main(int argc, char** argv) {
    // Each scenario uses its own registry to avoid cross-scenario
    // metric counter / signal-id collisions.
    QGuiApplication app(argc, argv);
    {
        SignalBufferRegistry r1;
        runScenario1(r1);
    }
    {
        SignalBufferRegistry r2;
        runScenario2(r2);
    }
    {
        SignalBufferRegistry r3;
        runScenario3(r3);
    }
    {
        SignalBufferRegistry r4;
        runScenario4(r4);
    }
    std::puts("{\"scenario\":\"summary\",\"status\":\"complete\"}");
    return 0;
}
