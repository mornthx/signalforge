// tests/benchmark/bench_decoder_throughput.cpp
//
// Measures the throughput of `SchemaDecoder::onFrame` for two
// scenarios:
//
//   1. "simple"  — 4 numeric fields (uint32 + int16 + uint16 + uint32).
//                  Target: >= 100 000 frames/sec (spec §7.4).
//   2. "complex" — bit fields + fixed_string + scale + offset_transform.
//                  Target: >= 50 000 frames/sec.
//
// Both scenarios are run inline (no QThread / pipeline hop) so the
// number reflects pure decode cost. The sink is a cheap atomic
// counter that performs no logging.
//
// JSON lines are emitted to stdout, one per scenario + one summary
// object at the end. `run_baselines.sh` collates them into
// `tests/benchmark/results/M5-baseline.md`.
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

using namespace std::chrono_literals;
using namespace signalforge::decoder;
using signalforge::frame::RawFrame;

namespace {

constexpr int kFramesPerCycle = 1000;
constexpr int kDurationSec = 5;

class CounterSink : public SignalValueSink {
public:
    void onSignal(std::chrono::steady_clock::time_point, const QString&, const SignalValue&) override {
        signals_.fetch_add(1, std::memory_order_relaxed);
    }
    [[nodiscard]] std::uint64_t value() const noexcept {
        return signals_.load(std::memory_order_relaxed);
    }

private:
    std::atomic<std::uint64_t> signals_{0};
};

Schema makeSimpleSchema() {
    Schema s;
    s.schemaVersion = 1;
    s.id = QStringLiteral("bench:simple");
    Layout l;
    l.name = QStringLiteral("simple");
    l.endianness = Endianness::Little;
    l.match.offset = 0;
    l.match.bytes = {0xAA};
    l.minPayloadBytes = 12;

    auto add = [&](QString name, int offset, FieldEncoding enc, int sz) {
        FieldDef f;
        f.name = std::move(name);
        f.offset = offset;
        f.encoding = enc;
        f.sizeBytes = sz;
        l.fields.push_back(std::move(f));
    };
    add(QStringLiteral("magic"), 0, FieldEncoding::Uint8, 1);
    add(QStringLiteral("counter"), 1, FieldEncoding::Uint32, 4);
    add(QStringLiteral("value_a"), 5, FieldEncoding::Int16, 2);
    add(QStringLiteral("value_b"), 7, FieldEncoding::Uint16, 2);
    add(QStringLiteral("padding"), 9, FieldEncoding::Uint16, 2);

    s.layouts.push_back(std::move(l));
    return s;
}

Schema makeComplexSchema() {
    Schema s;
    s.schemaVersion = 1;
    s.id = QStringLiteral("bench:complex");
    Layout l;
    l.name = QStringLiteral("complex");
    l.endianness = Endianness::Little;
    l.match.offset = 0;
    l.match.bytes = {0xAA};
    l.minPayloadBytes = 24;

    {
        FieldDef f;
        f.name = QStringLiteral("magic");
        f.offset = 0;
        f.encoding = FieldEncoding::Uint8;
        f.sizeBytes = 1;
        l.fields.push_back(std::move(f));
    }
    {
        FieldDef f;
        f.name = QStringLiteral("temperature");
        f.offset = 1;
        f.encoding = FieldEncoding::Int16;
        f.sizeBytes = 2;
        f.scale = 0.01;
        f.offsetTransform = -10.0;
        l.fields.push_back(std::move(f));
    }
    {
        FieldDef f;
        f.name = QStringLiteral("pressure");
        f.offset = 3;
        f.encoding = FieldEncoding::Uint16;
        f.sizeBytes = 2;
        f.scale = 0.1;
        l.fields.push_back(std::move(f));
    }
    {
        FieldDef f;
        f.name = QStringLiteral("flags");
        f.offset = 5;
        f.encoding = FieldEncoding::BitField;
        f.sizeBytes = 2;
        f.bitFields.push_back({QStringLiteral("alarm"), 0, 1, std::nullopt});
        f.bitFields.push_back({QStringLiteral("mode"), 1, 3, std::nullopt});
        f.bitFields.push_back({QStringLiteral("device_id"), 4, 8, std::nullopt});
        f.bitFields.push_back({QStringLiteral("priority"), 12, 4, std::nullopt});
        l.fields.push_back(std::move(f));
    }
    {
        FieldDef f;
        f.name = QStringLiteral("voltage");
        f.offset = 7;
        f.encoding = FieldEncoding::Float32;
        f.sizeBytes = 4;
        l.fields.push_back(std::move(f));
    }
    {
        FieldDef f;
        f.name = QStringLiteral("tag");
        f.offset = 11;
        f.encoding = FieldEncoding::FixedString;
        f.sizeBytes = 8;
        l.fields.push_back(std::move(f));
    }
    {
        FieldDef f;
        f.name = QStringLiteral("padding");
        f.offset = 19;
        f.encoding = FieldEncoding::Uint32;
        f.sizeBytes = 4;
        l.fields.push_back(std::move(f));
    }

    s.layouts.push_back(std::move(l));
    return s;
}

std::vector<RawFrame> buildSimpleFrames() {
    std::vector<RawFrame> frames;
    frames.reserve(kFramesPerCycle);
    for (int i = 0; i < kFramesPerCycle; ++i) {
        QByteArray p(12, '\0');
        p[0] = static_cast<char>(0xAA);
        const std::uint32_t counter = static_cast<std::uint32_t>(i);
        std::memcpy(p.data() + 1, &counter, 4);
        const std::int16_t value_a = static_cast<std::int16_t>(i % 32768);
        std::memcpy(p.data() + 5, &value_a, 2);
        const std::uint16_t value_b = static_cast<std::uint16_t>(i % 65536);
        std::memcpy(p.data() + 7, &value_b, 2);
        const std::uint16_t pad = 0xFEED;
        std::memcpy(p.data() + 9, &pad, 2);
        RawFrame f;
        f.payload = std::move(p);
        f.recvAt = std::chrono::steady_clock::now();
        frames.push_back(std::move(f));
    }
    return frames;
}

std::vector<RawFrame> buildComplexFrames() {
    std::vector<RawFrame> frames;
    frames.reserve(kFramesPerCycle);
    for (int i = 0; i < kFramesPerCycle; ++i) {
        QByteArray p(24, '\0');
        p[0] = static_cast<char>(0xAA);
        const std::int16_t temp = static_cast<std::int16_t>(2000 + (i % 100));
        std::memcpy(p.data() + 1, &temp, 2);
        const std::uint16_t pres = static_cast<std::uint16_t>(1000 + (i % 50));
        std::memcpy(p.data() + 3, &pres, 2);
        const std::uint16_t flags = static_cast<std::uint16_t>(0x9A5C);
        std::memcpy(p.data() + 5, &flags, 2);
        const float voltage = 3.3f + 0.001f * (i % 100);
        std::memcpy(p.data() + 7, &voltage, 4);
        const char tag[] = "fw-1.23";
        std::memcpy(p.data() + 11, tag, sizeof(tag));  // includes null terminator
        const std::uint32_t pad = 0xDEADBEEF;
        std::memcpy(p.data() + 19, &pad, 4);
        RawFrame f;
        f.payload = std::move(p);
        f.recvAt = std::chrono::steady_clock::now();
        frames.push_back(std::move(f));
    }
    return frames;
}

struct Result {
    QString scenario;
    std::uint64_t framesTotal = 0;
    std::uint64_t signalsTotal = 0;
    double elapsedSec = 0.0;
    double framesPerSec = 0.0;
    double signalsPerSec = 0.0;
    double targetFramesPerSec = 0.0;
    bool meetsTarget = false;
};

Result runScenario(const QString& name, Schema schema, const std::vector<RawFrame>& frames, double targetFramesPerSec) {
    auto sink = std::make_shared<CounterSink>();
    SchemaDecoder decoder(std::move(schema), QStringLiteral("bench:") + name);
    decoder.setSignalSink(sink);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(kDurationSec);
    std::uint64_t framesSent = 0;
    const auto t0 = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() < deadline) {
        for (const RawFrame& f : frames) {
            decoder.onFrame(f);
            ++framesSent;
        }
    }
    const auto t1 = std::chrono::steady_clock::now();
    const double elapsed = std::chrono::duration<double>(t1 - t0).count();

    Result r;
    r.scenario = name;
    r.framesTotal = framesSent;
    r.signalsTotal = sink->value();
    r.elapsedSec = elapsed;
    r.framesPerSec = static_cast<double>(framesSent) / elapsed;
    r.signalsPerSec = static_cast<double>(sink->value()) / elapsed;
    r.targetFramesPerSec = targetFramesPerSec;
    r.meetsTarget = r.framesPerSec >= targetFramesPerSec;
    return r;
}

void emitJson(const Result& r) {
    std::printf(
        "{\"scenario\":\"%s\",\"frames_total\":%llu,\"signals_total\":%llu,\"elapsed_sec\":%.3f,"
        "\"frames_per_sec\":%.1f,\"signals_per_sec\":%.1f,\"target_frames_per_sec\":%.1f,\"meets_target\":%s}\n",
        r.scenario.toStdString().c_str(), static_cast<unsigned long long>(r.framesTotal),
        static_cast<unsigned long long>(r.signalsTotal), r.elapsedSec, r.framesPerSec, r.signalsPerSec,
        r.targetFramesPerSec, r.meetsTarget ? "true" : "false");
}

}  // namespace

int main() {
    const Result simple = runScenario(QStringLiteral("simple"), makeSimpleSchema(), buildSimpleFrames(), 100000.0);
    emitJson(simple);
    const Result complex = runScenario(QStringLiteral("complex"), makeComplexSchema(), buildComplexFrames(), 50000.0);
    emitJson(complex);

    const bool allPass = simple.meetsTarget && complex.meetsTarget;
    std::printf("{\"scenario\":\"summary\",\"simple_frames_per_sec\":%.1f,\"complex_frames_per_sec\":%.1f,"
                "\"simple_meets_target\":%s,\"complex_meets_target\":%s,\"verdict\":\"%s\"}\n",
                simple.framesPerSec, complex.framesPerSec, simple.meetsTarget ? "true" : "false",
                complex.meetsTarget ? "true" : "false", allPass ? "within_threshold" : "below_threshold");

    return allPass ? 0 : 1;
}
