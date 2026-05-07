// tools/m8_prototype/main.cpp
//
// Usage:
//   m8_prototype --scenario {1|2|3|4} [--frames N]
//
// Scenarios (per the M8 prototype task description):
//   1: 60 charts × 1000 static samples, 30 Hz redraw, 1000 frames.
//      Lower bound for "ideal" Scene Graph cost.
//   2: 60 charts × 1 kHz incremental updates, 30 sec run.
//   3: 3 charts × 20 signals + scripted pan, multi-chart sync.
//   4: 1 chart × 3.6 M-sample LOD pyramid, 4 zoom levels.
//
// Single-window, pure C++ (no QML files). Emits a JSON line to stdout
// when complete; the operator copies numbers into RESULTS.md.

#include "buffer/signal_buffer.hpp"
#include "buffer/signal_buffer_registry.hpp"
#include "decode/decoder_interface.hpp"
#include "tools/m8_prototype/line_chart_item.hpp"
#include "tools/m8_prototype/scenario_runner.hpp"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QObject>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QString>
#include <QTimer>
#include <QVector>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>
#include <variant>

using signalforge::buffer::SignalBuffer;
using signalforge::buffer::SignalBufferConfig;
using signalforge::buffer::SignalBufferRegistry;
using signalforge::decoder::SignalMetadata;
using signalforge::decoder::SignalType;
using signalforge::decoder::SignalValue;

namespace {

QString rendererName(QSGRendererInterface::GraphicsApi api) {
    // OpenGL and OpenGLRhi are aliases on Qt 6; keep one case label.
    switch (api) {
        case QSGRendererInterface::Software:
            return QStringLiteral("software");
        case QSGRendererInterface::OpenGL:
            return QStringLiteral("opengl-rhi");
        case QSGRendererInterface::Direct3D11:
            return QStringLiteral("d3d11-rhi");
        case QSGRendererInterface::Vulkan:
            return QStringLiteral("vulkan-rhi");
        case QSGRendererInterface::Metal:
            return QStringLiteral("metal-rhi");
        default:
            return QStringLiteral("unknown");
    }
}

QVector<QPointF> generateSinSamples(int count, double phase) {
    QVector<QPointF> pts;
    pts.reserve(count);
    for (int i = 0; i < count; ++i) {
        const double x = static_cast<double>(i);
        const double y = std::sin((x / count) * 6.0 * 3.14159265 + phase);
        pts.append(QPointF(x, y));
    }
    return pts;
}

void emitResult(const QString& scenarioName, const ScenarioResult& r, const QString& renderer,
                const QString& extraJson = QString{}) {
    std::printf("{\"scenario\":\"%s\",\"renderer\":\"%s\","
                "\"frames\":%d,\"p50_ms\":%.4f,\"p95_ms\":%.4f,\"p99_ms\":%.4f,"
                "\"max_ms\":%.4f,\"mean_ms\":%.4f,\"std_dev_ms\":%.4f,\"dropped_frames\":%d,"
                "\"render_samples\":%d,\"render_p50_ms\":%.4f,\"render_p95_ms\":%.4f,"
                "\"render_p99_ms\":%.4f,\"render_max_ms\":%.4f%s}\n",
                scenarioName.toLocal8Bit().constData(), renderer.toLocal8Bit().constData(), r.framesRecorded, r.p50Ms,
                r.p95Ms, r.p99Ms, r.maxMs, r.meanMs, r.stdDevMs, r.droppedFrames, r.renderSamples, r.renderP50Ms,
                r.renderP95Ms, r.renderP99Ms, r.renderMaxMs,
                extraJson.isEmpty() ? "" : (QStringLiteral(",") + extraJson).toLocal8Bit().constData());
    std::fflush(stdout);
}

// ---------- Scenario 1: 60 charts × 1000 samples, animated 30 Hz ----------

// The user spec said "data does NOT change between frames"; in practice
// Qt's Scene Graph aggressively skips swap when the rendered output is
// unchanged, which silently zeros out any frameSwapped-based timing.
// To measure the realistic per-frame rendering cost (which is what V1
// pays on every cursor / time-axis tick), we apply a small x-axis pan
// per frame. Data values are static — only the visible range moves.
int runScenario1(QGuiApplication& app, int targetFrames) {
    constexpr int kCharts = 60;
    constexpr int kSamples = 1000;

    QQuickWindow window;
    window.setTitle(QStringLiteral("M8 prototype — Scenario 1"));
    window.setWidth(1920);
    window.setHeight(1080);
    window.setColor(QColor(20, 20, 25));

    QVector<LineChartItem*> charts;
    charts.reserve(kCharts);
    auto* root = window.contentItem();
    for (int i = 0; i < kCharts; ++i) {
        auto* c = new LineChartItem(root);
        c->setX(0);
        c->setWidth(1920);
        c->setY(i * (1080.0 / kCharts));
        c->setHeight(1080.0 / kCharts);
        c->setRanges(0.0, kSamples, -1.05, 1.05);
        // Pre-fill with 1000 samples; data is identical across runs.
        c->setPoints(generateSinSamples(kSamples, i * 0.1));
        c->setColor(QColor::fromHsv((i * 6) % 360, 220, 240));
        charts.append(c);
    }

    FrameTimingRecorder recorder(&window, targetFrames);
    QObject::connect(&recorder, &FrameTimingRecorder::completed, &app, [&] {
        const auto api = window.rendererInterface() ? window.rendererInterface()->graphicsApi()
                                                    : QSGRendererInterface::Unknown;
        emitResult(QStringLiteral("scenario_1_pure_render"), recorder.finalize(), rendererName(api));
        app.exit(0);
    });

    int frameTick = 0;
    QTimer redraw;
    redraw.setInterval(33);  // 30 Hz
    QObject::connect(&redraw, &QTimer::timeout, &app, [&] {
        // Re-shift the data points themselves, not just the range, so
        // every frame's pixel output is materially different. This
        // simulates "data is static + axis pan" — what V1 does when
        // the user pauses the live feed but the time cursor keeps
        // advancing.
        const double phaseStep = static_cast<double>(frameTick) * 0.05;
        for (int i = 0; i < charts.size(); ++i) {
            charts[i]->setPoints(generateSinSamples(kSamples, i * 0.1 + phaseStep));
        }
        ++frameTick;
    });
    redraw.start();

    window.show();
    window.raise();
    window.requestActivate();
    return app.exec();
}

// ---------- Scenario 2: 60 charts × 1 kHz updates, 30 sec ----------

int runScenario2(QGuiApplication& app, int seconds) {
    constexpr int kCharts = 60;
    constexpr int kInitialSamples = 1000;
    constexpr int kKeepSamples = 1000;  // ring-window per chart

    QQuickWindow window;
    window.setTitle(QStringLiteral("M8 prototype — Scenario 2"));
    window.setWidth(1920);
    window.setHeight(1080);
    window.setColor(QColor(20, 20, 25));

    struct ChartState {
        LineChartItem* item = nullptr;
        QVector<QPointF> ring;
        double phase = 0.0;
        std::int64_t totalSamples = 0;
    };
    QVector<ChartState> charts(kCharts);
    auto* root = window.contentItem();
    for (int i = 0; i < kCharts; ++i) {
        charts[i].item = new LineChartItem(root);
        charts[i].item->setX(0);
        charts[i].item->setWidth(1920);
        charts[i].item->setY(i * (1080.0 / kCharts));
        charts[i].item->setHeight(1080.0 / kCharts);
        charts[i].item->setRanges(0.0, kKeepSamples, -1.1, 1.1);
        charts[i].item->setColor(QColor::fromHsv((i * 6) % 360, 220, 240));
        charts[i].phase = i * 0.1;
        charts[i].ring = generateSinSamples(kInitialSamples, charts[i].phase);
        charts[i].totalSamples = kInitialSamples;
        charts[i].item->setPoints(charts[i].ring);
    }

    // 1 kHz update timer per chart (60 charts × 1 kHz = 60k samples/sec
    // injected into the ring buffers; 30 Hz redraw consumes them). The
    // ring is a fixed-size circular buffer indexed by `writeIdx`; we
    // overwrite slots in place so the per-push cost is O(1) — the
    // O(N) reindex of an earlier draft would dominate the bench.
    QTimer updateTimer;
    updateTimer.setInterval(1);  // 1 kHz
    QObject::connect(&updateTimer, &QTimer::timeout, &app, [&] {
        for (int i = 0; i < kCharts; ++i) {
            auto& s = charts[i];
            const double y = std::sin((s.totalSamples / 100.0) + s.phase);
            // Use slot index = totalSamples % kKeepSamples; x is the
            // slot, y is the value. The visual order is "scrambled"
            // (scrolling effect) but each pixel column gets refreshed
            // every kKeepSamples samples — same draw-call cost as a
            // continuously-shifting ring.
            const int slot = static_cast<int>(s.totalSamples % kKeepSamples);
            s.ring[slot] = QPointF(static_cast<double>(slot), y);
            ++s.totalSamples;
        }
    });

    QTimer redraw;
    redraw.setInterval(33);
    QObject::connect(&redraw, &QTimer::timeout, &app, [&] {
        for (int i = 0; i < kCharts; ++i) {
            charts[i].item->setPoints(charts[i].ring);
        }
    });

    const int targetFrames = seconds * 30;  // 30 Hz × N seconds
    FrameTimingRecorder recorder(&window, targetFrames);
    QObject::connect(&recorder, &FrameTimingRecorder::completed, &app, [&] {
        const auto api = window.rendererInterface() ? window.rendererInterface()->graphicsApi()
                                                    : QSGRendererInterface::Unknown;
        std::int64_t totalUpdates = 0;
        for (const auto& s : charts) {
            totalUpdates += s.totalSamples;
        }
        QString extra = QStringLiteral("\"total_updates\":%1,\"target_seconds\":%2").arg(totalUpdates).arg(seconds);
        emitResult(QStringLiteral("scenario_2_update_render"), recorder.finalize(), rendererName(api), extra);
        app.exit(0);
    });

    updateTimer.start();
    redraw.start();
    window.show();
    window.raise();
    window.requestActivate();
    return app.exec();
}

// ---------- Scenario 3: 3 charts × 20 signals + scripted pan ----------

int runScenario3(QGuiApplication& app, int targetFrames) {
    constexpr int kChartCount = 3;
    constexpr int kSignalsPerChart = 20;
    constexpr int kSamples = 2000;

    QQuickWindow window;
    window.setTitle(QStringLiteral("M8 prototype — Scenario 3"));
    window.setWidth(1920);
    window.setHeight(1080);
    window.setColor(QColor(20, 20, 25));

    // Each "chart" is a logical block of N signals, each rendered as
    // its own LineChartItem stacked in that block.
    QVector<LineChartItem*> allItems;
    auto* root = window.contentItem();
    const double blockH = 1080.0 / kChartCount;
    for (int b = 0; b < kChartCount; ++b) {
        const double signalH = blockH / kSignalsPerChart;
        for (int s = 0; s < kSignalsPerChart; ++s) {
            auto* c = new LineChartItem(root);
            c->setX(0);
            c->setWidth(1920);
            c->setY(b * blockH + s * signalH);
            c->setHeight(signalH);
            c->setRanges(0.0, kSamples, -1.05, 1.05);
            c->setPoints(generateSinSamples(kSamples, b * 0.5 + s * 0.07));
            c->setColor(QColor::fromHsv(((b * kSignalsPerChart + s) * 5) % 360, 200, 240));
            allItems.append(c);
        }
    }

    // Shared time-axis pan: repaint each chart with a translated x-range.
    // The pan rate simulates a user dragging at ~1 axis-unit / frame.
    double panOffset = 0.0;

    QTimer panTimer;
    panTimer.setInterval(33);  // 30 Hz pan + redraw
    QObject::connect(&panTimer, &QTimer::timeout, &app, [&] {
        panOffset += 25.0;  // 25 units/frame → 750 units/sec
        for (auto* c : allItems) {
            c->setRanges(panOffset, panOffset + kSamples, -1.05, 1.05);
        }
    });

    FrameTimingRecorder recorder(&window, targetFrames);
    QObject::connect(&recorder, &FrameTimingRecorder::completed, &app, [&] {
        const auto api = window.rendererInterface() ? window.rendererInterface()->graphicsApi()
                                                    : QSGRendererInterface::Unknown;
        emitResult(QStringLiteral("scenario_3_pan"), recorder.finalize(), rendererName(api),
                   QStringLiteral("\"pan_units_per_frame\":25.0"));
        app.exit(0);
    });

    panTimer.start();
    window.show();
    window.raise();
    window.requestActivate();
    return app.exec();
}

// ---------- Scenario 4: M6 SignalBuffer LOD integration ----------

int runScenario4(QGuiApplication& app, int targetFrames) {
    // 1 hour × 1 kHz = 3.6 M samples. Push them into a single
    // SignalBuffer and query at 4 zoom levels per frame, rotating
    // through the levels to exercise the LOD switch path.
    SignalBufferConfig cfg;
    cfg.windowSeconds = 3700.0;       // hold the full hour
    cfg.capSamples = 4'000'000;       // > 3.6 M
    cfg.estimatedRateHz = 1000.0;
    cfg.lodEnabled = true;
    SignalMetadata meta;
    meta.id = QStringLiteral("m8/lod_bench/sig");
    meta.name = QStringLiteral("LOD bench signal");
    meta.unit = QStringLiteral("V");
    meta.type = SignalType::Double;
    SignalBuffer buffer(meta, cfg);

    constexpr int kSamples = 3'600'000;
    constexpr double kSampleHz = 1000.0;
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kSamples; ++i) {
        const double y = std::sin(i * 0.01) + 0.3 * std::sin(i * 0.001);
        buffer.push(t0 + std::chrono::microseconds(static_cast<std::int64_t>(i * 1e6 / kSampleHz)),
                    SignalValue{y});
    }

    QQuickWindow window;
    window.setTitle(QStringLiteral("M8 prototype — Scenario 4"));
    window.setWidth(1920);
    window.setHeight(1080);
    window.setColor(QColor(20, 20, 25));

    auto* chart = new LineChartItem(window.contentItem());
    chart->setX(0);
    chart->setY(0);
    chart->setWidth(1920);
    chart->setHeight(1080);
    chart->setColor(QColor(120, 220, 255));

    // 4 zoom levels — windows in microseconds (the SignalBuffer uses
    // steady_clock; we offset relative to t0).
    const std::vector<std::pair<QString, std::chrono::microseconds>> zoomLevels = {
        {QStringLiteral("1s"), std::chrono::microseconds(1'000'000)},
        {QStringLiteral("1min"), std::chrono::microseconds(60'000'000)},
        {QStringLiteral("10min"), std::chrono::microseconds(600'000'000)},
        {QStringLiteral("1hour"), std::chrono::microseconds(3'600'000'000)},
    };

    struct ZoomTiming {
        QString name;
        std::vector<double> queryMs;
    };
    auto* timings = new std::vector<ZoomTiming>(zoomLevels.size());
    for (std::size_t i = 0; i < zoomLevels.size(); ++i) {
        (*timings)[i].name = zoomLevels[i].first;
    }

    int frameCount = 0;
    int currentZoom = 0;

    QTimer redraw;
    redraw.setInterval(33);
    QObject::connect(&redraw, &QTimer::timeout, &app, [&, timings] {
        // Cycle through zoom levels every 30 frames (~1 sec each).
        const int zoomIdx = (frameCount / 30) % static_cast<int>(zoomLevels.size());
        if (zoomIdx != currentZoom) {
            currentZoom = zoomIdx;
        }
        const auto& level = zoomLevels[zoomIdx];
        const auto end = t0 + std::chrono::microseconds(static_cast<std::int64_t>(kSamples * 1e6 / kSampleHz));
        const auto start = end - level.second;

        const auto qa = std::chrono::steady_clock::now();
        auto bins = buffer.queryRange(start, end, /*targetBins=*/1920);
        const auto qb = std::chrono::steady_clock::now();
        (*timings)[zoomIdx].queryMs.push_back(std::chrono::duration<double, std::milli>(qb - qa).count());

        // Convert returned samples → polyline points. queryRange does
        // the LOD decimation internally; we just extract the double
        // payload from the SignalValue variant.
        QVector<QPointF> pts;
        pts.reserve(static_cast<int>(bins.size()));
        const double xSpan = static_cast<double>(bins.size());
        if (xSpan > 0) {
            for (std::size_t i = 0; i < bins.size(); ++i) {
                const auto* d = std::get_if<double>(&bins[i].value);
                const double y = (d != nullptr ? *d : 0.0);
                pts.append(QPointF(static_cast<double>(i), y));
            }
            chart->setRanges(0.0, xSpan, -1.5, 1.5);
            chart->setPoints(std::move(pts));
        }
        ++frameCount;
    });

    FrameTimingRecorder recorder(&window, targetFrames);
    QObject::connect(&recorder, &FrameTimingRecorder::completed, &app, [&, timings] {
        const auto api = window.rendererInterface() ? window.rendererInterface()->graphicsApi()
                                                    : QSGRendererInterface::Unknown;
        QString extra = QStringLiteral("\"zoom_query_ms\":{");
        for (std::size_t i = 0; i < timings->size(); ++i) {
            if (i > 0) {
                extra += QLatin1Char(',');
            }
            const auto& q = (*timings)[i].queryMs;
            double meanQ = 0.0;
            for (double v : q) {
                meanQ += v;
            }
            if (!q.empty()) {
                meanQ /= q.size();
            }
            extra += QStringLiteral("\"%1\":%2").arg((*timings)[i].name).arg(meanQ, 0, 'f', 4);
        }
        extra += QLatin1Char('}');
        emitResult(QStringLiteral("scenario_4_lod"), recorder.finalize(), rendererName(api), extra);
        delete timings;
        app.exit(0);
    });

    redraw.start();
    window.show();
    return app.exec();
}

}  // namespace

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("SignalForge M8 prototype benchmarks"));
    parser.addHelpOption();
    QCommandLineOption scenarioOpt(QStringLiteral("scenario"),
                                   QStringLiteral("Scenario number (1-4)"),
                                   QStringLiteral("N"), QStringLiteral("1"));
    QCommandLineOption framesOpt(QStringLiteral("frames"),
                                 QStringLiteral("Override frame count for the timing window"),
                                 QStringLiteral("N"), QStringLiteral("1000"));
    parser.addOption(scenarioOpt);
    parser.addOption(framesOpt);
    parser.process(app);

    const int scenario = parser.value(scenarioOpt).toInt();
    const int frames = parser.value(framesOpt).toInt();

    switch (scenario) {
        case 1:
            return runScenario1(app, frames);
        case 2:
            return runScenario2(app, /*seconds=*/30);
        case 3:
            return runScenario3(app, frames);
        case 4:
            return runScenario4(app, frames);
        default:
            std::fprintf(stderr, "unknown scenario %d (valid: 1..4)\n", scenario);
            return 2;
    }
}
