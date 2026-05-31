// tests/unit/dashboard/meter_panel_test.cpp
//
// M24 S1 — MeterPanel (Bar/Gauge): value, observed vs explicit range,
// no-data state, headless render.

#include "buffer/signal_buffer_registry.hpp"
#include "dashboard/meter_panel.hpp"
#include "dashboard/panel_types.hpp"
#include "decode/decoder_interface.hpp"

#include <QApplication>
#include <QImage>
#include <catch2/catch_test_macros.hpp>
#include <chrono>

namespace dash = signalforge::dashboard;
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

dec::SignalMetadata makeMeta(QString id, QString unit) {
    dec::SignalMetadata meta;
    meta.id = std::move(id);
    meta.unit = std::move(unit);
    meta.type = dec::SignalType::Double;
    return meta;
}

void pushN(buf::SignalBufferRegistry& reg, const QString& id, double v) {
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) {
        reg.onSignal(t0 + std::chrono::microseconds(i), id, dec::SignalValue{v});
    }
}

dash::PanelConfig cfg(dash::PanelType type, const QString& id) {
    dash::PanelConfig c;
    c.id = QStringLiteral("panel-meter-1");
    c.type = type;
    c.signalIds << id;
    return c;
}

}  // namespace

TEST_CASE("M24 S1: Bar meter shows value and observed range", "[dashboard][m24][meter]") {
    app();
    buf::SignalBufferRegistry reg;
    const QString id = QStringLiteral("rig/level");
    reg.onSignalsRegistered(QStringLiteral("rig"), {makeMeta(id, QStringLiteral("%"))});

    dash::MeterPanel panel(cfg(dash::PanelType::Bar, id), reg);
    CHECK_FALSE(panel.hasData());

    pushN(reg, id, 40.0);
    panel.refresh();
    CHECK(panel.hasData());
    CHECK(panel.displayValue() == 40.0);
    // Single observed value → range collapses; refresh widens to keep span>0.
    CHECK(panel.rangeLo() <= 40.0);
    CHECK(panel.rangeHi() > panel.rangeLo());
}

TEST_CASE("M24 S1: explicit range pins the scale", "[dashboard][m24][meter]") {
    app();
    buf::SignalBufferRegistry reg;
    const QString id = QStringLiteral("rig/level");
    reg.onSignalsRegistered(QStringLiteral("rig"), {makeMeta(id, QStringLiteral("%"))});

    auto c = cfg(dash::PanelType::Gauge, id);
    c.rangeMin = 0.0;
    c.rangeMax = 100.0;
    dash::MeterPanel panel(c, reg);
    pushN(reg, id, 40.0);
    panel.refresh();
    CHECK(panel.rangeLo() == 0.0);
    CHECK(panel.rangeHi() == 100.0);
    CHECK(panel.displayValue() == 40.0);
}

TEST_CASE("M24 S1: meter renders non-empty headlessly", "[dashboard][m24][meter]") {
    app();
    buf::SignalBufferRegistry reg;
    const QString id = QStringLiteral("rig/level");
    reg.onSignalsRegistered(QStringLiteral("rig"), {makeMeta(id, QStringLiteral("%"))});

    auto c = cfg(dash::PanelType::Gauge, id);
    c.rangeMin = 0.0;
    c.rangeMax = 100.0;
    dash::MeterPanel panel(c, reg);
    panel.resize(240, 180);
    pushN(reg, id, 70.0);
    panel.refresh();

    const QImage img = panel.grab().toImage();
    REQUIRE_FALSE(img.isNull());
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
