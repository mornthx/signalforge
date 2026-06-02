// src/video/color_correction.cpp
#include "video/color_correction.hpp"

#include "observability/logging.hpp"

#include <QtGlobal>
#include <algorithm>
#include <cmath>
#include <exception>
#include <nlohmann/json.hpp>

namespace signalforge::video {

namespace {

std::uint8_t buildEntry(int v, double gain, double contrast, double brightness, double invGamma) {
    double x = (static_cast<double>(v) / 255.0) * gain;  // white balance
    x = (x - 0.5) * contrast + 0.5;                      // contrast about mid-grey
    x += brightness;                                     // brightness
    x = std::clamp(x, 0.0, 1.0);
    x = std::pow(x, invGamma);  // gamma
    const int out = static_cast<int>(std::lround(std::clamp(x, 0.0, 1.0) * 255.0));
    return static_cast<std::uint8_t>(std::clamp(out, 0, 255));
}

}  // namespace

bool ColorParams::isIdentity() const noexcept {
    return qFuzzyCompare(rGain, 1.0) && qFuzzyCompare(gGain, 1.0) && qFuzzyCompare(bGain, 1.0) &&
           qFuzzyIsNull(brightness) && qFuzzyCompare(contrast, 1.0) && qFuzzyCompare(gamma, 1.0) &&
           qFuzzyCompare(saturation, 1.0);
}

ColorCorrector::ColorCorrector() {
    rebuild();
}

void ColorCorrector::setParams(const ColorParams& params) {
    params_ = params;
    rebuild();
}

const ColorParams& ColorCorrector::params() const noexcept {
    return params_;
}

void ColorCorrector::rebuild() {
    identity_ = params_.isIdentity();
    saturationActive_ = !qFuzzyCompare(params_.saturation, 1.0);
    const double invGamma = params_.gamma > 0.0 ? 1.0 / params_.gamma : 1.0;
    for (int v = 0; v < 256; ++v) {
        lutR_[static_cast<std::size_t>(v)] =
            buildEntry(v, params_.rGain, params_.contrast, params_.brightness, invGamma);
        lutG_[static_cast<std::size_t>(v)] =
            buildEntry(v, params_.gGain, params_.contrast, params_.brightness, invGamma);
        lutB_[static_cast<std::size_t>(v)] =
            buildEntry(v, params_.bGain, params_.contrast, params_.brightness, invGamma);
    }
}

std::string colorParamsToJson(const ColorParams& p) {
    const nlohmann::json j = {
        {"version", 1},     {"rGain", p.rGain},           {"gGain", p.gGain},
        {"bGain", p.bGain}, {"brightness", p.brightness}, {"contrast", p.contrast},
        {"gamma", p.gamma}, {"saturation", p.saturation},
    };
    return j.dump(2);
}

std::optional<ColorParams> colorParamsFromJson(const std::string& text) {
    try {
        const auto j = nlohmann::json::parse(text);
        if (!j.is_object()) {
            SF_LOG_WARN("video: color preset JSON root is not an object");
            return std::nullopt;
        }
        ColorParams p;
        p.rGain = j.value("rGain", p.rGain);
        p.gGain = j.value("gGain", p.gGain);
        p.bGain = j.value("bGain", p.bGain);
        p.brightness = j.value("brightness", p.brightness);
        p.contrast = j.value("contrast", p.contrast);
        p.gamma = j.value("gamma", p.gamma);
        p.saturation = j.value("saturation", p.saturation);
        return p;
    } catch (const std::exception& e) {
        SF_LOG_WARN("video: color preset JSON parse failed: {}", e.what());
        return std::nullopt;
    }
}

QImage ColorCorrector::apply(const QImage& src) const {
    if (identity_ || src.isNull()) {
        return src;
    }
    const QImage in = src.format() == QImage::Format_RGB888 ? src : src.convertToFormat(QImage::Format_RGB888);
    QImage out(in.width(), in.height(), QImage::Format_RGB888);

    const double sat = params_.saturation;
    for (int y = 0; y < in.height(); ++y) {
        const auto* sp = in.constScanLine(y);
        auto* dp = out.scanLine(y);
        for (int x = 0; x < in.width(); ++x) {
            const std::size_t i = static_cast<std::size_t>(x) * 3;
            int r = lutR_[sp[i + 0]];
            int g = lutG_[sp[i + 1]];
            int b = lutB_[sp[i + 2]];
            if (saturationActive_) {
                const double luma = 0.299 * r + 0.587 * g + 0.114 * b;
                r = std::clamp(static_cast<int>(std::lround(luma + (r - luma) * sat)), 0, 255);
                g = std::clamp(static_cast<int>(std::lround(luma + (g - luma) * sat)), 0, 255);
                b = std::clamp(static_cast<int>(std::lround(luma + (b - luma) * sat)), 0, 255);
            }
            dp[i + 0] = static_cast<uchar>(r);
            dp[i + 1] = static_cast<uchar>(g);
            dp[i + 2] = static_cast<uchar>(b);
        }
    }
    return out;
}

}  // namespace signalforge::video
