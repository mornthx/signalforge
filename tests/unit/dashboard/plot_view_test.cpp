// tests/unit/dashboard/plot_view_test.cpp
//
// M23 S1 — PlotView: signal management, distinct colors, Y-range
// (observed + explicit), and a non-empty headless render.

#include "buffer/signal_buffer_registry.hpp"
#include "chart/time_axis_manager.hpp"
#include "dashboard/plot_view.hpp"
#include "decode/decoder_interface.hpp"

#include <QApplication>
#include <QImage>
#include <catch2/catch_test_macros.hpp>
#include <chrono>

namespace dash = signalforge::dashboard;
namespace ch = signalforge::chart;
namespace buf = signalforge::buffer;
namespace dec = signalforge::decoder;

namespace {

QApplication& app() {
    static int argc = 1;
    static char arg0[] = "test";
    static char* argv[] = {arg0, nullptr};
    static QApplication instance(argc, argv);
    return instance;
}

dec::SignalMetadata makeMeta(QString id, dec::SignalType type, QString unit) {
    dec::SignalMetadata meta;
    meta.id = std::move(id);
    meta.unit = std::move(unit);
    meta.type = type;
    return meta;
}

// Spread n samples over the last ~`spanMs` ending at now, so queryRange's
// LOD/time selection over the live window returns them (and >100 samples
// crosses the publish cadence). Real signals are time-spread, not clustered.
void pushN(buf::SignalBufferRegistry& reg, const QString& id, double v, int n = 120, int spanMs = 2400) {
    const auto now = std::chrono::steady_clock::now();
    for (int i = 0; i < n; ++i) {
        const auto ts = now - std::chrono::milliseconds((spanMs * (n - 1 - i)) / n);
        reg.onSignal(ts, id, dec::SignalValue{v});
    }
}

}  // namespace

TEST_CASE("M23 S1: PlotView manages signals with distinct colors", "[dashboard][m23][plotview]") {
    app();
    buf::SignalBufferRegistry reg;
    reg.onSignalsRegistered(QStringLiteral("rig"),
                            {makeMeta(QStringLiteral("rig/a"), dec::SignalType::Double, QStringLiteral("V")),
                             makeMeta(QStringLiteral("rig/b"), dec::SignalType::Double, QStringLiteral("V"))});
    ch::TimeAxisManager axis;
    dash::PlotView view(reg, axis);

    view.addSignal(QStringLiteral("rig/a"));
    view.addSignal(QStringLiteral("rig/b"));
    view.addSignal(QStringLiteral("rig/a"));  // idempotent
    CHECK(view.signalIds() == QStringList{QStringLiteral("rig/a"), QStringLiteral("rig/b")});
    CHECK(view.colorFor(QStringLiteral("rig/a")) != view.colorFor(QStringLiteral("rig/b")));

    view.removeSignal(QStringLiteral("rig/a"));
    CHECK(view.signalIds() == QStringList{QStringLiteral("rig/b")});
    CHECK_FALSE(view.hasSignal(QStringLiteral("rig/a")));
}

TEST_CASE("M23 S1: PlotView Y range reflects data and explicit override", "[dashboard][m23][plotview]") {
    app();
    buf::SignalBufferRegistry reg;
    reg.onSignalsRegistered(QStringLiteral("rig"),
                            {makeMeta(QStringLiteral("rig/v"), dec::SignalType::Double, QStringLiteral("V"))});
    ch::TimeAxisManager axis;  // default live, 10 s window
    dash::PlotView view(reg, axis);
    view.resize(400, 300);
    view.addSignal(QStringLiteral("rig/v"));

    CHECK_FALSE(view.hasData());
    pushN(reg, QStringLiteral("rig/v"), 24.7);
    view.refresh();
    CHECK(view.hasData());
    const auto [lo, hi] = view.computedYRange();
    CHECK(lo < 24.7);
    CHECK(hi > 24.7);

    view.setYRange(0.0, 100.0);
    const auto [lo2, hi2] = view.computedYRange();
    CHECK(lo2 == 0.0);
    CHECK(hi2 == 100.0);
}

TEST_CASE("M27: plot trace stays within the plot rect (no Y-axis spill)", "[dashboard][m27][plotview]") {
    app();
    buf::SignalBufferRegistry reg;
    reg.onSignalsRegistered(QStringLiteral("rig"),
                            {makeMeta(QStringLiteral("rig/v"), dec::SignalType::Double, QStringLiteral("V"))});
    ch::TimeAxisManager axis;
    dash::PlotView view(reg, axis);
    view.resize(400, 300);
    view.addSignal(QStringLiteral("rig/v"));
    pushN(reg, QStringLiteral("rig/v"), 24.7);
    view.refresh();

    const QImage img = view.grab().toImage();
    REQUIRE_FALSE(img.isNull());
    // A drifted trace shows up as a horizontal RUN of series-coloured pixels
    // in the left margin (x < 52) within the plot body — distinct from the
    // sparse anti-aliasing of the Y-axis tick labels. Flag any row with a run
    // of >= 6 consecutive series-coloured pixels there.
    const QColor series = view.colorFor(QStringLiteral("rig/v"));
    auto isSeries = [&](const QColor& px) {
        return std::abs(px.red() - series.red()) < 20 && std::abs(px.green() - series.green()) < 20 &&
               std::abs(px.blue() - series.blue()) < 20;
    };
    int maxRun = 0;
    for (int y = 30; y < img.height() - 30; ++y) {
        int run = 0;
        for (int x = 2; x < 52 && x < img.width(); ++x) {
            run = isSeries(img.pixelColor(x, y)) ? run + 1 : 0;
            maxRun = std::max(maxRun, run);
        }
    }
    CHECK(maxRun < 6);
}

TEST_CASE("M23 S1: PlotView renders a non-empty image headlessly", "[dashboard][m23][plotview]") {
    app();
    buf::SignalBufferRegistry reg;
    reg.onSignalsRegistered(QStringLiteral("rig"),
                            {makeMeta(QStringLiteral("rig/v"), dec::SignalType::Double, QStringLiteral("V"))});
    ch::TimeAxisManager axis;
    dash::PlotView view(reg, axis);
    view.resize(400, 300);
    view.addSignal(QStringLiteral("rig/v"));
    pushN(reg, QStringLiteral("rig/v"), 24.7);
    view.refresh();

    const QImage img = view.grab().toImage();
    REQUIRE_FALSE(img.isNull());
    CHECK(img.width() == 400);
    // The image must not be a single uniform colour (axes/grid/line drawn).
    bool varied = false;
    const QRgb first = img.pixel(0, 0);
    for (int y = 0; y < img.height() && !varied; y += 7) {
        for (int x = 0; x < img.width(); x += 7) {
            if (img.pixel(x, y) != first) {
                varied = true;
                break;
            }
        }
    }
    CHECK(varied);
}
