// tests/benchmark/bench_session_writer.cpp
//
// M10 S10 bench harness for SessionWriter throughput + memory.
//
// Default mode: 60 signals × 1 kHz × 10 seconds = 600 000 events
// driven through SessionWriter::onSignal as fast as the main
// thread can issue them. Reports:
//
//   - sustained event rate (events/sec)
//   - dropped events (queue overflow)
//   - bytes written / second (disk throughput)
//   - per-call enqueue latency p99 (main-thread block proxy;
//     spec §5.1 caps this at 5 ms)
//
// Soak mode (--soak <seconds>): runs the same workload for the
// requested duration with VmRSS snapshots every
// `--memory-snapshot <seconds>` interval (mirrors M9 S5s
// pattern). Internal acceptance gate: VmRSS growth < 10 % vs
// baseline at sec ≥ 120 s.
//
// Bench is opt-in via -DSIGNALFORGE_BENCHMARKS=ON; never run by
// ctest / CI. Spec §5.1 + §8.2 + plan §S10.

#include "buffer/signal_buffer_registry.hpp"
#include "decode/decoder_interface.hpp"
#include "session/session_writer.hpp"

#include <QCoreApplication>
#include <QString>
#include <QTemporaryDir>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace s = signalforge::session;
namespace b = signalforge::buffer;
namespace d = signalforge::decoder;

namespace {

std::int64_t readVmRssKb() {
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

d::SignalMetadata makeMeta(const QString& id) {
    d::SignalMetadata m;
    m.id = id;
    m.name = id;
    m.unit = QStringLiteral("u");
    m.type = d::SignalType::Double;
    return m;
}

int runBench(int durationSeconds, int memorySnapshotSeconds) {
    constexpr int kNumSignals = 60;
    constexpr int kInjectIntervalMicros = 1000;  // 1 kHz per signal-batch
    constexpr int kBaselineSeconds = 120;
    constexpr double kGrowthGatePct = 10.0;

    QTemporaryDir tmp;
    if (!tmp.isValid()) {
        std::fprintf(stderr, "bench: could not create temp dir\n");
        return 2;
    }
    const QString outPath = tmp.filePath(QStringLiteral("bench.sfreplay"));

    b::SignalBufferRegistry registry;
    s::SessionWriter writer(registry);

    std::vector<d::SignalMetadata> sigs;
    sigs.reserve(kNumSignals);
    for (int i = 0; i < kNumSignals; ++i) {
        sigs.push_back(makeMeta(QStringLiteral("bench/s%1").arg(i)));
    }
    writer.onSignalsRegistered(QStringLiteral("bench"), sigs);

    if (!writer.start(outPath, QStringLiteral("bench harness"), QStringLiteral("schema"))) {
        std::fprintf(stderr, "bench: writer.start() failed\n");
        return 2;
    }

    const auto t0 = std::chrono::steady_clock::now();
    const auto t0Steady = writer.metadata().recordingStart;
    const auto deadline = t0 + std::chrono::seconds(durationSeconds);

    const std::int64_t vmRssInitialKb = readVmRssKb();
    std::int64_t vmRssBaselineKb = -1;

    // Tracking — rolling buffer to keep bench memory bounded
    // under long soak runs. M13 S4 found the unbounded
    // `reserve(durationSeconds * 60000)` at 60 k events/sec ×
    // 30 min × 8 bytes ≈ 864 MB of latency samples, which
    // dominated the soak's VmRSS growth and looked like an
    // M10 leak. The cap below keeps the p99 statistic
    // representative (last ~100 k samples ≈ 1.7 s window at
    // 60 k events/sec) without unbounded growth.
    constexpr std::size_t kEnqueueLatRingSize = 100000;
    std::vector<long long> enqueueLatNs;
    enqueueLatNs.reserve(kEnqueueLatRingSize);
    std::size_t enqueueLatHead = 0;

    long long inject = 0;
    auto nextSnapshot = t0 + std::chrono::seconds(memorySnapshotSeconds);

    // Steady-clock-deadline pacing: each batch round target is
    // 1 ms. sleep_until is more accurate than sleep_for but still
    // limited by Linux's ~50-100 μs scheduler granularity. To
    // ensure the bench reaches the spec's 60k events/sec target
    // when the writer can in fact sustain it, we use a tight
    // pacing loop that doesn't undershoot the deadline.
    auto nextRound = t0;
    while (std::chrono::steady_clock::now() < deadline) {
        const auto now = std::chrono::steady_clock::now();
        for (int i = 0; i < kNumSignals; ++i) {
            const auto enqStart = std::chrono::steady_clock::now();
            writer.onSignal(t0Steady + std::chrono::nanoseconds(inject * 1000), sigs[i].id,
                            d::SignalValue{static_cast<double>(inject)});
            const auto enqEnd = std::chrono::steady_clock::now();
            const long long lat = std::chrono::duration_cast<std::chrono::nanoseconds>(enqEnd - enqStart).count();
            // Rolling buffer: fill once, then overwrite oldest.
            if (enqueueLatNs.size() < kEnqueueLatRingSize) {
                enqueueLatNs.push_back(lat);
            } else {
                enqueueLatNs[enqueueLatHead] = lat;
                enqueueLatHead = (enqueueLatHead + 1) % kEnqueueLatRingSize;
            }
        }
        ++inject;
        if (now >= nextSnapshot) {
            const std::int64_t kb = readVmRssKb();
            const auto sec = std::chrono::duration_cast<std::chrono::seconds>(now - t0).count();
            if (vmRssBaselineKb < 0 && sec >= kBaselineSeconds) {
                vmRssBaselineKb = kb;
            }
            std::printf("{\"scenario\":\"session_soak\",\"sec\":%lld,\"vmrss_kb\":%lld,"
                        "\"events_recorded\":%llu,\"bytes_written\":%llu,\"dropped\":%llu}\n",
                        static_cast<long long>(sec), static_cast<long long>(kb),
                        static_cast<unsigned long long>(writer.eventsRecorded()),
                        static_cast<unsigned long long>(writer.bytesWritten()),
                        static_cast<unsigned long long>(writer.droppedEvents()));
            std::fflush(stdout);
            nextSnapshot = now + std::chrono::seconds(memorySnapshotSeconds);
        }
        // Pace to the next 1 ms boundary; sleep_until is more
        // precise than sleep_for and skips overshoot.
        nextRound += std::chrono::microseconds(kInjectIntervalMicros);
        std::this_thread::sleep_until(nextRound);
    }

    const std::size_t bytes = writer.stop();
    const auto t1 = std::chrono::steady_clock::now();
    const std::int64_t vmRssFinalKb = readVmRssKb();

    // Stats
    const auto totalSec = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count() / 1000.0;
    const std::size_t totalEvents = writer.eventsRecorded();
    const std::size_t dropped = writer.droppedEvents();
    const double rate = totalEvents / std::max(0.001, totalSec);
    const double bytesPerSec = bytes / std::max(0.001, totalSec);

    std::sort(enqueueLatNs.begin(), enqueueLatNs.end());
    const auto p99Idx = static_cast<std::size_t>(enqueueLatNs.size() * 0.99);
    const long long enqP99Ns = enqueueLatNs.empty() ? 0 : enqueueLatNs[std::min(p99Idx, enqueueLatNs.size() - 1)];
    const long long enqMaxNs = enqueueLatNs.empty() ? 0 : enqueueLatNs.back();

    const std::int64_t baselineForGate = (vmRssBaselineKb > 0) ? vmRssBaselineKb : vmRssInitialKb;
    const double growthPct =
        (baselineForGate > 0) ? (100.0 * (vmRssFinalKb - baselineForGate) / static_cast<double>(baselineForGate)) : 0.0;

    std::printf("{\"scenario\":\"session_summary\",\"duration_seconds\":%d,"
                "\"events_recorded\":%llu,\"events_per_sec\":%.1f,"
                "\"bytes_written\":%llu,\"bytes_per_sec\":%.1f,"
                "\"dropped_events\":%llu,\"enqueue_p99_ns\":%lld,\"enqueue_max_ns\":%lld,"
                "\"vmrss_initial_kb\":%lld,\"vmrss_baseline_kb\":%lld,\"vmrss_final_kb\":%lld,"
                "\"vmrss_growth_pct\":%.3f}\n",
                durationSeconds, static_cast<unsigned long long>(totalEvents), rate,
                static_cast<unsigned long long>(bytes), bytesPerSec, static_cast<unsigned long long>(dropped), enqP99Ns,
                enqMaxNs, static_cast<long long>(vmRssInitialKb), static_cast<long long>(baselineForGate),
                static_cast<long long>(vmRssFinalKb), growthPct);
    std::fflush(stdout);

    // Acceptance gates per spec §5.1 + §8.2 + plan §3 trigger #4
    bool ok = true;
    if (rate < 60000.0) {
        std::fprintf(stderr, "FAIL: rate %.1f events/sec < 60000 (spec §5.1)\n", rate);
        ok = false;
    }
    if (enqP99Ns > 5LL * 1000 * 1000) {
        std::fprintf(stderr, "FAIL: enqueue p99 %lld ns > 5 ms (spec §5.1 main-thread cap)\n", enqP99Ns);
        ok = false;
    }
    if (durationSeconds >= kBaselineSeconds + 30 && baselineForGate > 0 && growthPct >= kGrowthGatePct) {
        std::fprintf(stderr, "FAIL: VmRSS growth %.3f%% >= %.1f%% (spec §5.4)\n", growthPct, kGrowthGatePct);
        ok = false;
    }

    return ok ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
    int duration = 10;
    int memSnapshot = 60;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--soak") == 0 && i + 1 < argc) {
            duration = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--seconds") == 0 && i + 1 < argc) {
            duration = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--memory-snapshot") == 0 && i + 1 < argc) {
            memSnapshot = std::atoi(argv[++i]);
        }
    }
    QCoreApplication app(argc, argv);
    return runBench(duration, memSnapshot);
}
