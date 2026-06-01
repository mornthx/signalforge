// tests/unit/video/color_correction_test.cpp
#include "video/color_correction.hpp"

#include <QColor>
#include <QImage>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using signalforge::video::ColorCorrector;
using signalforge::video::ColorParams;
using signalforge::video::colorParamsFromJson;
using signalforge::video::colorParamsToJson;

namespace {
QImage solid(int w, int h, QColor c) {
    QImage img(w, h, QImage::Format_RGB888);
    img.fill(c);
    return img;
}
}  // namespace

TEST_CASE("ColorParams: defaults are identity", "[video][color]") {
    CHECK(ColorParams{}.isIdentity());
    ColorParams p;
    p.brightness = 0.1;
    CHECK_FALSE(p.isIdentity());
}

TEST_CASE("ColorCorrector: identity returns the source unchanged", "[video][color]") {
    ColorCorrector cc;  // default params
    const QImage src = solid(4, 4, QColor(40, 80, 120));
    const QImage out = cc.apply(src);
    REQUIRE(out.size() == src.size());
    CHECK(out.pixelColor(0, 0) == QColor(40, 80, 120));
}

TEST_CASE("ColorCorrector: white-balance gain scales a channel", "[video][color]") {
    ColorParams p;
    p.rGain = 2.0;  // red doubled
    ColorCorrector cc;
    cc.setParams(p);
    const QImage out = cc.apply(solid(2, 2, QColor(100, 100, 100)));
    const QColor c = out.pixelColor(0, 0);
    CHECK(c.red() == 200);    // 100*2
    CHECK(c.green() == 100);  // unchanged
    CHECK(c.blue() == 100);
}

TEST_CASE("ColorCorrector: gain saturates at full scale", "[video][color]") {
    ColorParams p;
    p.rGain = 4.0;
    ColorCorrector cc;
    cc.setParams(p);
    const QImage out = cc.apply(solid(2, 2, QColor(200, 10, 10)));
    CHECK(out.pixelColor(0, 0).red() == 255);  // clamped
}

TEST_CASE("ColorCorrector: brightness lifts all channels", "[video][color]") {
    ColorParams p;
    p.brightness = 0.2;  // +0.2*255 ≈ +51
    ColorCorrector cc;
    cc.setParams(p);
    const QColor c = cc.apply(solid(2, 2, QColor(50, 50, 50))).pixelColor(0, 0);
    CHECK(c.red() > 90);
    CHECK(c.red() < 110);
}

TEST_CASE("ColorCorrector: saturation=0 yields greyscale", "[video][color]") {
    ColorParams p;
    p.saturation = 0.0;
    ColorCorrector cc;
    cc.setParams(p);
    const QColor c = cc.apply(solid(2, 2, QColor(200, 50, 10))).pixelColor(0, 0);
    // All channels equal the luma → r==g==b.
    CHECK(c.red() == c.green());
    CHECK(c.green() == c.blue());
}

TEST_CASE("ColorCorrector: gamma > 1 brightens mid-tones", "[video][color]") {
    ColorParams p;
    p.gamma = 2.0;
    ColorCorrector cc;
    cc.setParams(p);
    const int mid = cc.apply(solid(2, 2, QColor(128, 128, 128))).pixelColor(0, 0).red();
    CHECK(mid > 128);  // gamma 2.0 lifts mid-grey
}

TEST_CASE("color preset JSON round-trips all fields", "[video][color]") {
    ColorParams p;
    p.rGain = 1.3;
    p.gGain = 0.8;
    p.bGain = 1.1;
    p.brightness = 0.05;
    p.contrast = 1.2;
    p.gamma = 0.9;
    p.saturation = 1.4;
    const auto back = colorParamsFromJson(colorParamsToJson(p));
    REQUIRE(back.has_value());
    CHECK(back->rGain == Catch::Approx(1.3));
    CHECK(back->gGain == Catch::Approx(0.8));
    CHECK(back->bGain == Catch::Approx(1.1));
    CHECK(back->brightness == Catch::Approx(0.05));
    CHECK(back->contrast == Catch::Approx(1.2));
    CHECK(back->gamma == Catch::Approx(0.9));
    CHECK(back->saturation == Catch::Approx(1.4));
}

TEST_CASE("color preset rejects malformed JSON", "[video][color]") {
    CHECK_FALSE(colorParamsFromJson("not json").has_value());
    CHECK_FALSE(colorParamsFromJson("[1,2,3]").has_value());  // non-object root
}

TEST_CASE("color preset missing fields keep identity defaults", "[video][color]") {
    const auto p = colorParamsFromJson(R"({"rGain":2.0})");
    REQUIRE(p.has_value());
    CHECK(p->rGain == Catch::Approx(2.0));
    CHECK(p->gGain == Catch::Approx(1.0));       // default
    CHECK(p->saturation == Catch::Approx(1.0));  // default
}
