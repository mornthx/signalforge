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

#include <QFile>
#include <QGuiApplication>
#include <QMetaObject>
#include <QString>
#include <QTextStream>
#include <QTimer>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
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

// ---- Soak mode (M9 inherited M8 spec §5.6 + plan §S5s) ----
//
// Drives 60 charts × 1 kHz inject × 30 Hz redraw via real QTimers
// for `--soak <seconds>`. Captures /proc/self/status VmRSS every
// `--memory-snapshot <seconds>` and reports growth + dropped
// frames at the end. ASan/LSan safety is the CI debug-asan
// gate's responsibility (host ld.so.preload blocks local ASan).

std::int64_t readVmRssKb() {
    // Linux-only. /proc/self/status has a "VmRSS:\t<n> kB" line.
    std::ifstream f("/proc/self/status");
    if (!f.is_open()) {
        return -1;
    }
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("VmRSS:", 0) == 0) {
            const char* p = line.c_str() + 6;
            while (*p == ' ' || *p == '\t') {
                ++p;
            }
            return std::strtoll(p, nullptr, 10);
        }
    }
    return -1;
}

int runSoak(int soakSeconds, int memSnapshotSeconds) {
    constexpr int kCharts = 60;
    constexpr int kInjectIntervalMs = 1;   // 1 kHz
    constexpr int kRenderIntervalMs = 33;  // ~30 Hz
    constexpr int kFrameOverrunBudgetMs = 50;
    // M6 default windowSeconds is 60. Take the leak baseline at
    // 2× window so transient buffer fill is excluded from the
    // growth gate. Final / baseline must satisfy < 10% per
    // M8 spec §5.6 (steady-state interpretation).
    constexpr int kBaselineSeconds = 120;

    SignalBufferRegistry registry;
    std::vector<SignalMetadata> metas;
    metas.reserve(kCharts);
    for (int i = 0; i < kCharts; ++i) {
        metas.push_back(makeMeta(QStringLiteral("soak/s%1").arg(i), SignalType::Double));
    }
    registry.onSignalsRegistered(QStringLiteral("soak"), metas);

    TimeAxisManager axis;
    Chart chart(registry, axis);
    chart.setWidth(1920);
    chart.setHeight(1080);
    for (const auto& m : metas) {
        chart.addSignal(m.id);
    }

    const auto t0 = std::chrono::steady_clock::now();
    std::int64_t injectIdx = 0;

    QTimer injectTimer;
    injectTimer.setTimerType(Qt::PreciseTimer);
    injectTimer.setInterval(kInjectIntervalMs);
    QObject::connect(&injectTimer, &QTimer::timeout, [&]() {
        for (int c = 0; c < kCharts; ++c) {
            registry.onSignal(t0 + std::chrono::microseconds(injectIdx * 1000), metas[c].id,
                              SignalValue{std::sin(injectIdx * 0.01 + c * 0.1)});
        }
        ++injectIdx;
    });

    QTimer renderTimer;
    renderTimer.setTimerType(Qt::PreciseTimer);
    renderTimer.setInterval(kRenderIntervalMs);
    auto lastFrame = std::chrono::steady_clock::now();
    long long totalFrames = 0;
    long long droppedFrames = 0;
    QObject::connect(&renderTimer, &QTimer::timeout, [&]() {
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFrame).count();
        if (totalFrames > 0 && elapsed > kFrameOverrunBudgetMs) {
            ++droppedFrames;
        }
        lastFrame = now;
        ++totalFrames;
        runOneTick(chart);
    });

    const std::int64_t vmRssInitialKb = readVmRssKb();
    std::int64_t vmRssBaselineKb = -1;  // captured at t ≈ kBaselineSeconds

    QTimer memTimer;
    memTimer.setInterval(memSnapshotSeconds * 1000);
    QObject::connect(&memTimer, &QTimer::timeout, [&]() {
        const std::int64_t kb = readVmRssKb();
        const auto now = std::chrono::steady_clock::now();
        const auto sec = std::chrono::duration_cast<std::chrono::seconds>(now - t0).count();
        if (vmRssBaselineKb < 0 && sec >= kBaselineSeconds) {
            vmRssBaselineKb = kb;
        }
        std::printf("{\"scenario\":\"soak_snapshot\",\"sec\":%lld,\"vmrss_kb\":%lld,\"total_frames\":%lld,"
                    "\"dropped_frames\":%lld,\"inject_idx\":%lld}\n",
                    static_cast<long long>(sec), static_cast<long long>(kb), totalFrames, droppedFrames,
                    static_cast<long long>(injectIdx));
        std::fflush(stdout);
    });

    QTimer stopTimer;
    stopTimer.setSingleShot(true);
    stopTimer.setInterval(soakSeconds * 1000);
    QObject::connect(&stopTimer, &QTimer::timeout, []() { QCoreApplication::quit(); });

    injectTimer.start();
    renderTimer.start();
    memTimer.start();
    stopTimer.start();

    QCoreApplication::exec();

    const std::int64_t vmRssFinalKb = readVmRssKb();
    // The growth gate compares final vs the post-warmup baseline
    // (steady-state interpretation; transient buffer fill before
    // M6 windowSeconds eviction kicks in is not a leak).
    const std::int64_t vmRssBaselineForGate = (vmRssBaselineKb > 0) ? vmRssBaselineKb : vmRssInitialKb;
    const double growthPct =
        (vmRssBaselineForGate > 0)
            ? (100.0 * (vmRssFinalKb - vmRssBaselineForGate) / static_cast<double>(vmRssBaselineForGate))
            : 0.0;
    std::printf("{\"scenario\":\"soak_summary\",\"soak_seconds\":%d,\"snapshot_interval_seconds\":%d,"
                "\"vmrss_initial_kb\":%lld,\"vmrss_baseline_kb\":%lld,\"vmrss_final_kb\":%lld,"
                "\"vmrss_growth_pct\":%.3f,\"total_frames\":%lld,\"dropped_frames\":%lld,"
                "\"inject_idx\":%lld}\n",
                soakSeconds, memSnapshotSeconds, static_cast<long long>(vmRssInitialKb),
                static_cast<long long>(vmRssBaselineForGate), static_cast<long long>(vmRssFinalKb), growthPct,
                totalFrames, droppedFrames, static_cast<long long>(injectIdx));
    std::fflush(stdout);

    // Acceptance gates per M8 spec §7 trigger #6 + plan §S5s
    // (steady-state interpretation: baseline at 2× windowSeconds).
    if (soakSeconds < kBaselineSeconds + 30) {
        std::fprintf(stderr, "soak: --soak too short (need >= %d sec for steady-state baseline)\n",
                     kBaselineSeconds + 30);
        return 0;  // smoke run; no gate enforcement
    }
    const bool growthOk = vmRssBaselineForGate <= 0 || growthPct < 10.0;
    const bool droppedOk = droppedFrames < 50;
    if (!growthOk || !droppedOk) {
        std::fprintf(stderr, "soak FAILED: growth=%.3f%% (gate <10), dropped=%lld (gate <50)\n", growthPct,
                     droppedFrames);
        return 1;
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    int soakSeconds = 0;
    int memSnapshotSeconds = 60;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--soak") == 0 && i + 1 < argc) {
            soakSeconds = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--memory-snapshot") == 0 && i + 1 < argc) {
            memSnapshotSeconds = std::atoi(argv[++i]);
        }
    }

    QGuiApplication app(argc, argv);

    if (soakSeconds > 0) {
        return runSoak(soakSeconds, memSnapshotSeconds);
    }

    // Default: each scenario uses its own registry to avoid
    // cross-scenario metric counter / signal-id collisions.
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
