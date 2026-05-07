// tests/benchmark/bench_expression_engine.cpp
//
// M7 expression-engine benchmark (spec §5.4 / plan §S10).
//
// Scenario: 500 base signals + 100 expressions, each referencing 5
// random sources from the base set. Drive 1000 ticks. Each tick the
// engine evaluates all 100 expressions and pushes results to the
// SignalBufferRegistry. Capture p50 / p95 / p99 of tick wall time.
//
// Targets (spec §5.4):
//   p50 < 5 ms, p95 < 8 ms, p99 < 10 ms.
// HALT trigger #3: p99 > 15 ms after one optimization pass.
//
// JSON output to stdout (one summary line). M7-baseline.md is
// authored from the numbers reported here.

#include "buffer/signal_buffer.hpp"
#include "buffer/signal_buffer_registry.hpp"
#include "decode/decoder_interface.hpp"
#include "expression/expression_engine.hpp"
#include "expression/expression_validator.hpp"

#include <QFile>
#include <QMetaObject>
#include <QString>
#include <QTemporaryDir>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace signalforge::buffer;
using namespace signalforge::decoder;
using namespace signalforge::expression;

namespace {

constexpr int kBaseSignals = 500;
constexpr int kExpressions = 100;
constexpr int kSourcesPerExpression = 5;
constexpr int kTicks = 1000;

SignalMetadata makeMeta(QString id, SignalType type) {
    SignalMetadata m;
    m.id = std::move(id);
    m.name = QStringLiteral("Bench");
    m.unit = QStringLiteral("");
    m.type = type;
    return m;
}

void invokeTick(ExpressionEngine& engine) {
    QMetaObject::invokeMethod(&engine, "onTick", Qt::DirectConnection);
}

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

}  // namespace

int main() {
    // 1) Register 500 base signals (all double). The default 256 MB
    //    registry budget is sized for production schema decoders; this
    //    bench fits 500 large doubles + 100 derived buffers in 2 GB
    //    (debug spec says 600 base signals × ~1 MB each = ~600 MB, so
    //    we bump the budget up here).
    RegistryConfig regCfg;
    regCfg.totalBudgetBytes = 2ULL * 1024 * 1024 * 1024;  // 2 GB
    SignalBufferRegistry registry(regCfg);
    std::vector<SignalMetadata> baseSignals;
    baseSignals.reserve(kBaseSignals);
    std::vector<QString> baseIds;
    baseIds.reserve(kBaseSignals);
    for (int i = 0; i < kBaseSignals; ++i) {
        const QString id = QStringLiteral("b%1").arg(i);
        baseIds.push_back(id);
        baseSignals.push_back(makeMeta(id, SignalType::Double));
    }
    registry.onSignalsRegistered(QStringLiteral("bench-driver"), baseSignals);

    // 2) Build 100 expressions, each picking 5 random base signals and
    //    summing them with two arithmetic ops. We assemble a yaml
    //    document and run it through the validator so the engine sees
    //    a topologically-sorted ExpressionSet (matches production
    //    flow).
    std::mt19937 rng(0xC0FFEE);
    std::uniform_int_distribution<int> pick(0, kBaseSignals - 1);

    std::ostringstream yaml;
    yaml << "schema_version: 1\n";
    yaml << "expressions:\n";
    for (int e = 0; e < kExpressions; ++e) {
        std::vector<int> sources;
        sources.reserve(kSourcesPerExpression);
        while (static_cast<int>(sources.size()) < kSourcesPerExpression) {
            const int idx = pick(rng);
            if (std::find(sources.begin(), sources.end(), idx) == sources.end()) {
                sources.push_back(idx);
            }
        }
        // Formula: b_i + b_j * b_k - b_l + b_m
        yaml << "  - id: e" << e << "\n";
        yaml << "    formula: \"b" << sources[0] << " + b" << sources[1] << " * b" << sources[2] << " - b" << sources[3]
             << " + b" << sources[4] << "\"\n";
    }

    auto result =
        ExpressionValidator::validateString(QString::fromStdString(yaml.str()), QStringLiteral("<bench>"), baseSignals);
    if (!result.has_value()) {
        std::fprintf(stderr, "bench setup failed: validator returned %zu errors\n", result.error().size());
        for (const auto& err : result.error()) {
            std::fprintf(stderr, "  %s\n", err.message.toLocal8Bit().constData());
        }
        return 1;
    }

    ExpressionEngine engine(registry);
    engine.setExpressions(std::move(*result));

    // 3) Pre-populate base signals with values so queryLatestOne
    //    resolves at every tick (cross publish cadence first).
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) {
        for (int b = 0; b < kBaseSignals; ++b) {
            registry.onSignal(t0 + std::chrono::microseconds(i), baseIds[b], SignalValue{static_cast<double>(b) + 0.5});
        }
    }

    // 4) Drive `kTicks` ticks; capture per-tick wall time.
    std::vector<double> tickMs;
    tickMs.reserve(kTicks);
    for (int t = 0; t < kTicks; ++t) {
        // Refresh one base signal per tick to simulate a live
        // pipeline — keeps queryLatestOne pointing at fresh data.
        registry.onSignal(t0 + std::chrono::microseconds(100 + t), baseIds[t % kBaseSignals],
                          SignalValue{static_cast<double>(t) * 0.01});

        const auto a = std::chrono::steady_clock::now();
        invokeTick(engine);
        const auto b = std::chrono::steady_clock::now();
        tickMs.push_back(std::chrono::duration<double, std::milli>(b - a).count());
    }

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

    const auto& stats = engine.stats();
    std::printf("{\"scenario\":\"expression_engine_tick\","
                "\"base_signals\":%d,\"expressions\":%d,\"sources_per_expression\":%d,\"ticks\":%d,"
                "\"p50_ms\":%.4f,\"p95_ms\":%.4f,\"p99_ms\":%.4f,\"max_ms\":%.4f,\"mean_ms\":%.4f,"
                "\"evaluations_total\":%llu,\"errors_total\":%llu}\n",
                kBaseSignals, kExpressions, kSourcesPerExpression, kTicks, p50, p95, p99, pMax, mean,
                static_cast<unsigned long long>(stats.evaluationsTotal),
                static_cast<unsigned long long>(stats.evaluationErrors));

    // 5) Acceptance verdict on stderr.
    const bool p50ok = p50 < 5.0;
    const bool p95ok = p95 < 8.0;
    const bool p99ok = p99 < 10.0;
    const bool haltLimit = p99 > 15.0;
    std::fprintf(stderr, "p50=%.3f ms (target<5)  %s\n", p50, p50ok ? "PASS" : "FAIL");
    std::fprintf(stderr, "p95=%.3f ms (target<8)  %s\n", p95, p95ok ? "PASS" : "FAIL");
    std::fprintf(stderr, "p99=%.3f ms (target<10) %s\n", p99, p99ok ? "PASS" : "FAIL");
    if (haltLimit) {
        std::fprintf(stderr, "HALT trigger #3: p99=%.3f > 15 ms (after one optimization pass)\n", p99);
        return 2;
    }
    if (!p50ok || !p95ok || !p99ok) {
        std::fprintf(stderr, "PERF WARNING: at least one target missed but no HALT (one opt pass remaining)\n");
        return 3;
    }
    return 0;
}
