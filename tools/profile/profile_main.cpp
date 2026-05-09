// tools/profile/profile_main.cpp
//
// M12 S1 — profile harness driving the three required scenarios
// per spec §4.1. Designed to run **under** an external profiler:
//
//   perf record -g -F 999 -o profile-A.data tools/profile/profile_main --scenario A
//   perf report -i profile-A.data
//
// Or under valgrind callgrind (tier 2 fallback):
//
//   valgrind --tool=callgrind --callgrind-out-file=callgrind.A.out \
//       tools/profile/profile_main --scenario A
//
// Or with built-in QElapsedTimer instrumentation (tier 3 — last
// resort, coarse-grained per-region timers; always works):
//
//   tools/profile/profile_main --scenario A --tier3
//
// Scenario A — live-style: synthetic SignalValueSink dispatch
//   exercising M5 decoder + M6 buffer plumbing through a
//   60-signal × 1 kHz × 30 s workload.
//
// Scenario B — replay: SessionPlayer driving a 600 k-record
//   SFREPLAY v1 fixture at 1× speed.
//
// Scenario C — concurrent record: SessionWriter receiving
//   60 sig × 1 kHz × 30 s synthetic events while a chart-style
//   secondary sink also fans out.
//
// This binary is an opt-in tool (bench-style); CI does not run
// it. Operator runs it by hand for M12 S2 profile report.

#include "buffer/signal_buffer_registry.hpp"
#include "decode/decoder_interface.hpp"
#include "replay/playback_controller.hpp"
#include "session/session_writer.hpp"
#include "session/tee_signal_value_sink.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QString>
#include <QTemporaryDir>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace s = signalforge::session;
namespace b = signalforge::buffer;
namespace d = signalforge::decoder;
namespace r = signalforge::replay;

namespace {

constexpr int kNumSignals = 60;
constexpr int kFreqHz = 1000;

d::SignalMetadata makeMeta(const QString& id) {
    d::SignalMetadata m;
    m.id = id;
    m.name = id;
    m.unit = QStringLiteral("u");
    m.type = d::SignalType::Double;
    return m;
}

/// Scenario A — live-style: synthetic dispatch into M6
/// SignalBufferRegistry. Stand-in for the full M3+M4+M5+M6+M7+M8
/// path (a real live workload would need network drivers and
/// schema files; the synthetic path covers the dispatch + buffer
/// hot-spots that profile is most likely to flag).
int runScenarioA(int durationSeconds, bool tier3Instrumented) {
    b::SignalBufferRegistry registry;
    std::vector<d::SignalMetadata> metas;
    metas.reserve(kNumSignals);
    for (int i = 0; i < kNumSignals; ++i) {
        metas.push_back(makeMeta(QStringLiteral("sig_%1").arg(i)));
    }
    registry.onSignalsRegistered(QStringLiteral("d_live"), metas);

    QElapsedTimer total;
    total.start();
    qint64 dispatchedNs = 0;

    const auto t0 = std::chrono::steady_clock::now();
    for (int sec = 0; sec < durationSeconds; ++sec) {
        for (int tick = 0; tick < kFreqHz; ++tick) {
            const auto ts = t0 + std::chrono::microseconds(sec * 1'000'000LL + tick * 1'000LL);
            QElapsedTimer dispatch;
            if (tier3Instrumented) {
                dispatch.start();
            }
            for (int sig = 0; sig < kNumSignals; ++sig) {
                registry.onSignal(ts, QStringLiteral("sig_%1").arg(sig),
                                  d::SignalValue{static_cast<double>(sec * kFreqHz + tick)});
            }
            if (tier3Instrumented) {
                dispatchedNs += dispatch.nsecsElapsed();
            }
        }
    }

    const qint64 totalNs = total.nsecsElapsed();
    const std::int64_t totalEvents = static_cast<std::int64_t>(durationSeconds) * kFreqHz * kNumSignals;
    std::printf("{\"scenario\":\"A\",\"duration_s\":%d,\"events\":%lld,"
                "\"total_ms\":%lld,\"events_per_sec\":%.0f",
                durationSeconds, static_cast<long long>(totalEvents), static_cast<long long>(totalNs / 1'000'000),
                static_cast<double>(totalEvents) * 1e9 / static_cast<double>(totalNs));
    if (tier3Instrumented) {
        std::printf(",\"dispatch_ns_total\":%lld,\"dispatch_pct\":%.2f", static_cast<long long>(dispatchedNs),
                    100.0 * static_cast<double>(dispatchedNs) / static_cast<double>(totalNs));
    }
    std::printf("}\n");
    return 0;
}

/// Scenario B — replay: drive SessionPlayer at 1× through a
/// 600 k-record fixture. Reuses the M11 bench_replay fixture
/// pattern (60 sig × 1 kHz × 10 s) — short enough to land
/// in profile traces but representative.
int runScenarioB(int durationSeconds) {
    QTemporaryDir tmp;
    const QString path = tmp.filePath(QStringLiteral("profile_B.sfreplay"));

    {
        b::SignalBufferRegistry registry;
        s::SessionWriter writer(registry);
        std::vector<d::SignalMetadata> metas;
        metas.reserve(kNumSignals);
        for (int i = 0; i < kNumSignals; ++i) {
            metas.push_back(makeMeta(QStringLiteral("sig_%1").arg(i)));
        }
        writer.onSignalsRegistered(QStringLiteral("d1"), metas);
        if (!writer.start(path)) {
            std::fprintf(stderr, "profile_main(B): writer start failed\n");
            return 2;
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
    }

    b::SignalBufferRegistry registry;
    r::PlaybackController ctrl(registry);
    if (!ctrl.loadSession(path)) {
        std::fprintf(stderr, "profile_main(B): loadSession failed\n");
        return 2;
    }

    QEventLoop loop;
    QObject::connect(&ctrl, &r::PlaybackController::stateChanged, [&loop](r::PlaybackState st) {
        if (st == r::PlaybackState::Ended || st == r::PlaybackState::Error) {
            loop.quit();
        }
    });

    QElapsedTimer total;
    total.start();
    if (!ctrl.play()) {
        std::fprintf(stderr, "profile_main(B): play failed\n");
        return 2;
    }
    loop.exec();
    const qint64 totalNs = total.nsecsElapsed();

    std::printf("{\"scenario\":\"B\",\"records\":%zu,\"total_ms\":%lld}\n", ctrl.totalRecords(),
                static_cast<long long>(totalNs / 1'000'000));
    return 0;
}

/// Scenario C — concurrent record + chart: tee fans live signals
/// to both M6 SignalBufferRegistry (chart side) and M10
/// SessionWriter (record side). Profile sees both write paths.
int runScenarioC(int durationSeconds) {
    QTemporaryDir tmp;
    const QString path = tmp.filePath(QStringLiteral("profile_C.sfreplay"));

    b::SignalBufferRegistry registry;
    s::TeeSignalValueSink tee;
    tee.addSink(&registry);

    s::SessionWriter writer(registry);
    if (!writer.start(path)) {
        std::fprintf(stderr, "profile_main(C): writer start failed\n");
        return 2;
    }
    tee.addSink(&writer);

    std::vector<d::SignalMetadata> metas;
    metas.reserve(kNumSignals);
    for (int i = 0; i < kNumSignals; ++i) {
        metas.push_back(makeMeta(QStringLiteral("sig_%1").arg(i)));
    }
    tee.onSignalsRegistered(QStringLiteral("d_concurrent"), metas);

    QElapsedTimer total;
    total.start();
    const auto t0 = std::chrono::steady_clock::now();
    for (int sec = 0; sec < durationSeconds; ++sec) {
        for (int tick = 0; tick < kFreqHz; ++tick) {
            const auto ts = t0 + std::chrono::microseconds(sec * 1'000'000LL + tick * 1'000LL);
            for (int sig = 0; sig < kNumSignals; ++sig) {
                tee.onSignal(ts, QStringLiteral("sig_%1").arg(sig),
                             d::SignalValue{static_cast<double>(sec * kFreqHz + tick)});
            }
        }
    }
    const qint64 totalNs = total.nsecsElapsed();
    tee.removeSink(&writer);
    const std::size_t bytes = writer.stop();

    std::printf("{\"scenario\":\"C\",\"duration_s\":%d,\"events\":%lld,\"total_ms\":%lld,\"bytes\":%zu}\n",
                durationSeconds, static_cast<long long>(durationSeconds) * kFreqHz * kNumSignals,
                static_cast<long long>(totalNs / 1'000'000), bytes);
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    char scenario = 'A';
    int durationSeconds = 10;
    bool tier3Instrumented = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--scenario" && i + 1 < argc) {
            const std::string s = argv[++i];
            if (!s.empty()) {
                scenario = s[0];
            }
        } else if (arg == "--duration" && i + 1 < argc) {
            durationSeconds = std::atoi(argv[++i]);
        } else if (arg == "--tier3") {
            tier3Instrumented = true;
        }
    }

    switch (scenario) {
    case 'A':
    case 'a':
        return runScenarioA(durationSeconds, tier3Instrumented);
    case 'B':
    case 'b':
        return runScenarioB(durationSeconds);
    case 'C':
    case 'c':
        return runScenarioC(durationSeconds);
    default:
        std::fprintf(stderr, "Unknown scenario: %c (expected A, B, or C)\n", scenario);
        return 1;
    }
}
