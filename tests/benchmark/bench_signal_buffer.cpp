// tests/benchmark/bench_signal_buffer.cpp
//
// M6 buffer benchmark: three scenarios per spec §5.4 plus the
// ADR-005 large-buffer scaling check (added with the chunked-storage
// patch on `fix/m6-publish-cadence`).
//
//   1. Writer per type (Bool / Int64 / Double / QString) — tight push
//      loop on a single SignalBuffer at a steady-state cap of 10 000
//      samples (so the deque does not grow without bound), reporting
//      samples/sec.
//   2. Reader queryRange — preload 60 s × 1 kHz of double samples,
//      loop queryRange(target=2000) for a fixed iteration count,
//      queries/sec.
//   3. End-to-end pipeline overhead — feeds frames through the M5
//      SchemaDecoder with the M6 SignalBufferRegistry as sink, vs the
//      same decoder with a counting-only sink. The ratio is reported.
//   4. Large-buffer scaling (ADR-005) — push N samples (10 k / 100 k /
//      1 M) into a single Double buffer with retention >= N (no
//      eviction), reporting samples/sec at each N. The HALT-trigger #5
//      gate is "Double @ 1 M ≥ 500 k samples/sec".
//
// JSON lines are emitted to stdout, one per scenario plus a final
// summary. `tests/benchmark/results/M6-baseline.md` is authored from
// these numbers.

#include "buffer/signal_buffer.hpp"
#include "buffer/signal_buffer_registry.hpp"
#include "decode/decoder_interface.hpp"
#include "decode/schema.hpp"
#include "decode/schema_decoder.hpp"
#include "frame/raw_frame.hpp"

#include <QByteArray>
#include <QString>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

using namespace signalforge::buffer;
using namespace signalforge::decoder;
using signalforge::frame::RawFrame;

namespace {

template <typename Fn> double measureSeconds(Fn&& fn) {
    const auto t0 = std::chrono::steady_clock::now();
    fn();
    const auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(t1 - t0).count();
}

SignalMetadata makeMeta(QString id, SignalType type) {
    SignalMetadata meta;
    meta.id = std::move(id);
    meta.name = QStringLiteral("Bench");
    meta.unit = QStringLiteral("");
    meta.type = type;
    return meta;
}

// ---- Scenario 1: per-type writer throughput ----

void runWriterBench(const QString& type, std::uint64_t samples, SignalType st,
                    std::function<SignalValue(std::uint64_t)> makeValue) {
    SignalBufferConfig cfg;
    cfg.windowSeconds = 1e9;       // unbounded by time
    cfg.capSamples = 10'000;       // realistic steady-state cap
    cfg.estimatedRateHz = 1000.0;  // hint for budget sizing
    SignalBuffer buf(makeMeta(QStringLiteral("bench/writer/") + type, st), cfg);

    const auto t0 = std::chrono::steady_clock::now();
    const double sec = measureSeconds([&] {
        for (std::uint64_t i = 0; i < samples; ++i) {
            buf.push(t0 + std::chrono::microseconds(i), makeValue(i));
        }
    });
    const double rate = static_cast<double>(samples) / sec;
    std::printf("{\"scenario\":\"writer\",\"type\":\"%s\",\"samples\":%llu,\"seconds\":%.4f,"
                "\"samples_per_sec\":%.1f}\n",
                type.toLocal8Bit().constData(), static_cast<unsigned long long>(samples), sec, rate);
    std::fflush(stdout);
}

void runWriterBenches() {
    runWriterBench(QStringLiteral("bool"), 2'000'000, SignalType::Bool,
                   [](std::uint64_t i) { return SignalValue{(i % 2) == 0}; });
    runWriterBench(QStringLiteral("int64"), 2'000'000, SignalType::Int64,
                   [](std::uint64_t i) { return SignalValue{static_cast<std::int64_t>(i)}; });
    runWriterBench(QStringLiteral("double"), 2'000'000, SignalType::Double,
                   [](std::uint64_t i) { return SignalValue{static_cast<double>(i)}; });
    const QString fixed = QStringLiteral("v");
    runWriterBench(QStringLiteral("string"), 500'000, SignalType::String,
                   [fixed](std::uint64_t /*i*/) { return SignalValue{fixed}; });
}

// ---- Scenario 2: reader throughput ----

void runReaderBench() {
    SignalBufferConfig cfg;
    cfg.windowSeconds = 1e9;
    cfg.capSamples = 100'000;
    cfg.estimatedRateHz = 1000.0;
    SignalBuffer buf(makeMeta(QStringLiteral("bench/reader"), SignalType::Double), cfg);

    // Preload 60 s × 1 kHz = 60 000 samples (1 ms apart) — simulates the
    // M8 chart's typical "60-second view" workload.
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 60'000; ++i) {
        buf.push(t0 + std::chrono::milliseconds(i), SignalValue{static_cast<double>(i)});
    }

    constexpr int kIterations = 20'000;
    const double sec = measureSeconds([&] {
        for (int i = 0; i < kIterations; ++i) {
            volatile auto out = buf.queryRange(t0, t0 + std::chrono::seconds(60), 2000);
            (void)out;
        }
    });
    std::printf("{\"scenario\":\"reader\",\"iterations\":%d,\"seconds\":%.4f,"
                "\"queries_per_sec\":%.1f,\"window\":\"60s_1kHz\",\"target\":2000}\n",
                kIterations, sec, kIterations / sec);
    std::fflush(stdout);
}

// ---- Scenario 3: end-to-end pipeline overhead ----

class CounterSink : public SignalValueSink {
public:
    void onSignal(std::chrono::steady_clock::time_point, const QString&, const SignalValue&) override {
        signalsReceived_.fetch_add(1, std::memory_order_relaxed);
    }
    std::atomic<std::uint64_t> signalsReceived_{0};
};

[[nodiscard]] Schema buildSimpleSchema() {
    Schema schema;
    schema.schemaVersion = 1;
    schema.id = QStringLiteral("bench:m6");
    Layout layout;
    layout.name = QStringLiteral("simple");
    layout.endianness = Endianness::Little;
    layout.match.offset = 0;
    layout.match.bytes = {0xAA};
    layout.minPayloadBytes = 12;

    auto add = [&](QString name, int offset, FieldEncoding enc, int sz) {
        FieldDef f;
        f.name = std::move(name);
        f.offset = offset;
        f.encoding = enc;
        f.sizeBytes = sz;
        layout.fields.push_back(std::move(f));
    };
    add(QStringLiteral("magic"), 0, FieldEncoding::Uint8, 1);
    add(QStringLiteral("counter"), 1, FieldEncoding::Uint32, 4);
    add(QStringLiteral("a"), 5, FieldEncoding::Int16, 2);
    add(QStringLiteral("b"), 7, FieldEncoding::Uint16, 2);
    add(QStringLiteral("c"), 9, FieldEncoding::Uint16, 2);

    schema.layouts.push_back(std::move(layout));
    return schema;
}

[[nodiscard]] RawFrame buildFrame() {
    RawFrame frame;
    frame.sourceId = QStringLiteral("bench/decoder");
    frame.recvAt = std::chrono::steady_clock::now();
    QByteArray payload(12, '\0');
    payload[0] = static_cast<char>(0xAA);  // magic byte
    frame.payload = std::move(payload);
    return frame;
}

void runEndToEndBench() {
    constexpr int kFrames = 50'000;

    // Baseline: SchemaDecoder + counting sink only.
    double counterSec = 0.0;
    std::uint64_t counterSignals = 0;
    {
        SchemaDecoder decoder(buildSimpleSchema(), QStringLiteral("bench/decoder/baseline"));
        auto sink = std::make_shared<CounterSink>();
        decoder.setSignalSink(sink);
        const auto frame = buildFrame();
        const double sec = measureSeconds([&] {
            for (int f = 0; f < kFrames; ++f) {
                decoder.onFrame(frame);
            }
        });
        counterSec = sec;
        counterSignals = sink->signalsReceived_.load(std::memory_order_relaxed);
    }

    // M6 path: SchemaDecoder + SignalBufferRegistry as sink.
    double registrySec = 0.0;
    {
        RegistryConfig regCfg;
        regCfg.signalDefaults.windowSeconds = 1e9;
        regCfg.signalDefaults.capSamples = 10'000;  // bounded steady-state
        regCfg.signalDefaults.estimatedRateHz = 1000.0;
        regCfg.totalBudgetBytes = 1ULL * 1024 * 1024 * 1024;
        SignalBufferRegistry registry(regCfg);

        SchemaDecoder decoder(buildSimpleSchema(), QStringLiteral("bench/decoder/m6"));
        auto sink = std::shared_ptr<SignalValueSink>(&registry, [](SignalValueSink*) {});
        decoder.setSignalSink(sink);
        const auto frame = buildFrame();
        const double sec = measureSeconds([&] {
            for (int f = 0; f < kFrames; ++f) {
                decoder.onFrame(frame);
            }
        });
        registrySec = sec;
    }

    const double overheadPct = ((registrySec - counterSec) / counterSec) * 100.0;
    const double counterFps = kFrames / counterSec;
    const double registryFps = kFrames / registrySec;
    std::printf("{\"scenario\":\"end_to_end\",\"frames\":%d,\"counter_seconds\":%.4f,"
                "\"counter_fps\":%.1f,\"counter_signals\":%llu,\"registry_seconds\":%.4f,"
                "\"registry_fps\":%.1f,\"overhead_pct\":%.2f}\n",
                kFrames, counterSec, counterFps, static_cast<unsigned long long>(counterSignals), registrySec,
                registryFps, overheadPct);
    std::fflush(stdout);
}

// ---- Scenario 4: large-buffer scaling (ADR-005) ----

void runLargeBufferBench(std::uint64_t totalSamples) {
    SignalBufferConfig cfg;
    cfg.windowSeconds = 1e9;                        // unbounded by time
    cfg.capSamples = totalSamples + 1;              // no cap eviction
    cfg.estimatedRateHz = 1e6;                      // sizing hint only
    cfg.lodEnabled = true;                          // realistic chart config
    SignalBuffer buf(makeMeta(QStringLiteral("bench/large/double"), SignalType::Double), cfg);

    const auto t0 = std::chrono::steady_clock::now();
    const double sec = measureSeconds([&] {
        for (std::uint64_t i = 0; i < totalSamples; ++i) {
            buf.push(t0 + std::chrono::microseconds(static_cast<std::int64_t>(i)),
                     SignalValue{static_cast<double>(i) * 0.001});
        }
    });
    const double rate = static_cast<double>(totalSamples) / sec;
    std::printf("{\"scenario\":\"large_buffer_scaling\",\"type\":\"double\","
                "\"samples\":%llu,\"seconds\":%.4f,\"samples_per_sec\":%.1f}\n",
                static_cast<unsigned long long>(totalSamples), sec, rate);
    std::fflush(stdout);
}

void runLargeBufferBenches() {
    runLargeBufferBench(10'000);
    runLargeBufferBench(100'000);
    runLargeBufferBench(1'000'000);
}

}  // namespace

int main() {
    runWriterBenches();
    runReaderBench();
    runEndToEndBench();
    runLargeBufferBenches();
    std::puts("{\"scenario\":\"summary\",\"status\":\"complete\"}");
    return 0;
}
