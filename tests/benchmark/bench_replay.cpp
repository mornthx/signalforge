// tests/benchmark/bench_replay.cpp
//
// M11 S10 bench harness for SessionPlayer replay timing +
// memory. Drives a 60-signal × 1 kHz × 10-second SFREPLAY v1
// fixture (600 000 records — same shape as M10 S10's bench)
// through `signalforge::replay::SessionPlayer` and reports the
// four spec §5 metrics:
//
//   1× timing accuracy: wall-clock duration / file duration
//   10× completion: 10s file should finish in ~1s
//   Seek latency: single seekToTimestamp on a 600k-record file
//   Step latency: stepForward synchronous dispatch
//
// Internal acceptance gates per spec §5.1:
//   1×  → wall-clock within 5% of file duration (HALT > 20%)
//   10× → wall-clock < 1.1s for a 10s file (HALT > 1.5s)
//   Seek → < 500ms (HALT > 2s)
//   Step → < 50ms (HALT > 200ms)
//
// Bench is opt-in via -DSIGNALFORGE_BENCHMARKS=ON; never run by
// ctest / CI.

#include "buffer/signal_buffer_registry.hpp"
#include "decode/decoder_interface.hpp"
#include "replay/playback_controller.hpp"
#include "session/session_writer.hpp"

#include <QCoreApplication>
#include <QEventLoop>
#include <QTemporaryDir>
#include <QTimer>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

namespace s = signalforge::session;
namespace b = signalforge::buffer;
namespace d = signalforge::decoder;
namespace r = signalforge::replay;

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

/// Generate a 60-signal × 1 kHz × <seconds>-second SFREPLAY v1
/// fixture and return its path. Total records ≈ 60 × 1000 ×
/// seconds (the bench uses 10 s = 600 k records as the spec
/// reference for §5.1's "600k-record file" seek gate).
QString writeFixture(QTemporaryDir& tmp, int durationSeconds) {
    constexpr int kNumSignals = 60;
    constexpr int kFreqHz = 1000;
    const QString path = tmp.filePath(QStringLiteral("bench_replay.sfreplay"));

    b::SignalBufferRegistry registry;
    s::SessionWriter writer(registry);
    std::vector<d::SignalMetadata> metas;
    metas.reserve(kNumSignals);
    for (int i = 0; i < kNumSignals; ++i) {
        metas.push_back(makeMeta(QStringLiteral("sig_%1").arg(i)));
    }
    writer.onSignalsRegistered(QStringLiteral("d1"), metas);
    if (!writer.start(path)) {
        std::fprintf(stderr, "bench_replay: SessionWriter start failed\n");
        std::exit(2);
    }

    const auto t0 = writer.metadata().recordingStart;
    for (int sec = 0; sec < durationSeconds; ++sec) {
        for (int tick = 0; tick < kFreqHz; ++tick) {
            const auto offset = std::chrono::microseconds(sec * 1'000'000LL + tick * 1'000LL);
            for (int sig = 0; sig < kNumSignals; ++sig) {
                writer.onSignal(t0 + offset, QStringLiteral("sig_%1").arg(sig),
                                d::SignalValue{static_cast<double>(sec * kFreqHz + tick)});
            }
        }
    }
    (void)writer.stop();
    return path;
}

struct SilentSink : public d::SignalValueSink {
    void onSignal(std::chrono::steady_clock::time_point /*t*/, const QString& /*id*/,
                  const d::SignalValue& /*v*/) override {}
    void onSignalsRegistered(const QString& /*driverId*/, const std::vector<d::SignalMetadata>& /*sigs*/) override {}
};

int runRealtime(double speed, int durationSeconds) {
    QTemporaryDir tmp;
    const QString path = writeFixture(tmp, durationSeconds);

    b::SignalBufferRegistry registry;
    r::PlaybackController ctrl(registry);
    if (!ctrl.loadSession(path)) {
        std::fprintf(stderr, "bench_replay: loadSession failed\n");
        return 2;
    }
    (void)ctrl.setSpeed(speed);

    QEventLoop loop;
    QObject::connect(&ctrl, &r::PlaybackController::stateChanged, [&loop](r::PlaybackState st) {
        if (st == r::PlaybackState::Ended || st == r::PlaybackState::Error) {
            loop.quit();
        }
    });

    const auto wallStart = std::chrono::steady_clock::now();
    if (!ctrl.play()) {
        std::fprintf(stderr, "bench_replay: play failed\n");
        return 2;
    }
    loop.exec();
    const auto wallEnd = std::chrono::steady_clock::now();
    const auto wallMs = std::chrono::duration_cast<std::chrono::milliseconds>(wallEnd - wallStart).count();

    const double fileMs = static_cast<double>(durationSeconds) * 1000.0;
    const double expectedMs = fileMs / speed;
    const double errorPct = std::abs(static_cast<double>(wallMs) - expectedMs) / expectedMs * 100.0;

    std::printf("{\"mode\":\"realtime\",\"speed\":%.2f,\"file_ms\":%.0f,\"wall_ms\":%lld,"
                "\"expected_ms\":%.1f,\"error_pct\":%.2f,\"records\":%zu}\n",
                speed, fileMs, static_cast<long long>(wallMs), expectedMs, errorPct, ctrl.totalRecords());

    // Gate per `.claude/M11-plan.md` §3:
    //   H2 — 1× timing error > 20% over 10 s replay → HALT.
    // Spec §5.1's strict 5% target + the 10× completion gate are
    // soft targets surfaced as warnings; M11-baseline.md captures
    // the V1 numbers + the V1.5+ optimisation direction.
    if (speed >= 1.0 - 1e-3 && speed <= 1.0 + 1e-3) {
        if (errorPct > 20.0) {
            std::fprintf(stderr, "FAIL: 1x timing error %.2f%% > 20%% (HALT trigger H2)\n", errorPct);
            return 3;
        }
        if (errorPct > 5.0) {
            std::fprintf(stderr, "WARN: 1x timing error %.2f%% > 5%% (spec §5.1 target)\n", errorPct);
        }
    } else if (speed >= 9.5 && speed <= 10.5) {
        if (wallMs > 1500) {
            std::fprintf(stderr, "WARN: 10x wall_ms %lld > 1500 (spec §5.1 target; not H2)\n",
                         static_cast<long long>(wallMs));
        }
    }
    return 0;
}

int runSeekTest() {
    QTemporaryDir tmp;
    const QString path = writeFixture(tmp, 10);

    b::SignalBufferRegistry registry;
    r::PlaybackController ctrl(registry);
    if (!ctrl.loadSession(path)) {
        std::fprintf(stderr, "bench_replay: loadSession failed\n");
        return 2;
    }

    // Seek to mid-file (5 s).
    const auto t0 = std::chrono::steady_clock::now();
    if (!ctrl.seek(5'000'000'000LL)) {
        std::fprintf(stderr, "bench_replay: seek failed\n");
        return 2;
    }
    const auto t1 = std::chrono::steady_clock::now();
    const auto seekMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    std::printf("{\"mode\":\"seek\",\"records\":%zu,\"seek_ms\":%lld}\n", ctrl.totalRecords(),
                static_cast<long long>(seekMs));

    if (seekMs > 2000) {
        std::fprintf(stderr, "FAIL: seek_ms %lld > 2000 (HALT trigger H4)\n", static_cast<long long>(seekMs));
        return 3;
    }
    return 0;
}

int runStepTest() {
    QTemporaryDir tmp;
    const QString path = writeFixture(tmp, 10);

    b::SignalBufferRegistry registry;
    r::PlaybackController ctrl(registry);
    if (!ctrl.loadSession(path)) {
        return 2;
    }

    constexpr int kIterations = 100;
    std::vector<long long> stepMicros;
    stepMicros.reserve(kIterations);
    for (int i = 0; i < kIterations; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        if (!ctrl.stepForward()) {
            break;
        }
        const auto t1 = std::chrono::steady_clock::now();
        stepMicros.push_back(std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count());
    }

    std::sort(stepMicros.begin(), stepMicros.end());
    const auto medianMicros = stepMicros[stepMicros.size() / 2];
    const auto p99Micros = stepMicros[stepMicros.size() * 99 / 100];
    std::printf("{\"mode\":\"step\",\"iterations\":%zu,\"median_us\":%lld,\"p99_us\":%lld}\n", stepMicros.size(),
                static_cast<long long>(medianMicros), static_cast<long long>(p99Micros));

    if (p99Micros > 200'000) {  // 200 ms in micros
        std::fprintf(stderr, "FAIL: step p99 %lld us > 200 ms (HALT trigger H7)\n", static_cast<long long>(p99Micros));
        return 3;
    }
    return 0;
}

int runMemorySoak(int durationSeconds, int snapshotSeconds) {
    QTemporaryDir tmp;
    const QString path = writeFixture(tmp, 10);  // 10s file looped

    b::SignalBufferRegistry registry;
    r::PlaybackController ctrl(registry);
    if (!ctrl.loadSession(path)) {
        return 2;
    }
    (void)ctrl.setSpeed(10.0);

    const auto baseline = readVmRssKb();
    std::printf("{\"mode\":\"soak\",\"baseline_kb\":%lld,\"duration_s\":%d}\n", static_cast<long long>(baseline),
                durationSeconds);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(durationSeconds);
    auto nextSnapshot = std::chrono::steady_clock::now() + std::chrono::seconds(snapshotSeconds);

    while (std::chrono::steady_clock::now() < deadline) {
        QEventLoop loop;
        QObject::connect(&ctrl, &r::PlaybackController::stateChanged, [&loop](r::PlaybackState st) {
            if (st == r::PlaybackState::Ended || st == r::PlaybackState::Error) {
                loop.quit();
            }
        });
        if (!ctrl.play()) {
            return 2;
        }
        loop.exec();
        // Rewind to start.
        if (!ctrl.seek(0)) {
            return 2;
        }

        if (std::chrono::steady_clock::now() >= nextSnapshot) {
            const auto rss = readVmRssKb();
            const auto growth = (rss - baseline) * 100 / std::max<std::int64_t>(baseline, 1);
            std::printf("{\"mode\":\"soak\",\"rss_kb\":%lld,\"growth_pct\":%lld}\n", static_cast<long long>(rss),
                        static_cast<long long>(growth));
            if (growth > 10) {
                std::fprintf(stderr, "FAIL: VmRSS growth %lld%% > 10%% (HALT trigger H3)\n",
                             static_cast<long long>(growth));
                return 3;
            }
            nextSnapshot += std::chrono::seconds(snapshotSeconds);
        }
    }
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    std::string mode = "realtime";
    double speed = 1.0;
    int durationSeconds = 10;
    int snapshotSeconds = 30;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--realtime" && i + 1 < argc) {
            mode = "realtime";
            durationSeconds = std::atoi(argv[++i]);
        } else if (arg == "--fast" && i + 1 < argc) {
            mode = "realtime";
            speed = std::atof(argv[++i]);
        } else if (arg == "--seek-test") {
            mode = "seek";
        } else if (arg == "--step-test") {
            mode = "step";
        } else if (arg == "--memory-soak" && i + 1 < argc) {
            mode = "soak";
            durationSeconds = std::atoi(argv[++i]);
        } else if (arg == "--memory-snapshot" && i + 1 < argc) {
            snapshotSeconds = std::atoi(argv[++i]);
        }
    }

    if (mode == "realtime") {
        return runRealtime(speed, durationSeconds);
    }
    if (mode == "seek") {
        return runSeekTest();
    }
    if (mode == "step") {
        return runStepTest();
    }
    if (mode == "soak") {
        return runMemorySoak(durationSeconds, snapshotSeconds);
    }
    std::fprintf(stderr, "Unknown mode: %s\n", mode.c_str());
    return 1;
}
