// src/video/color_correction.hpp
#pragma once

#include <QImage>
#include <array>
#include <cstdint>

namespace signalforge::video {

/// Standard-set software colour-correction parameters.
///
/// Applied to RGB24 frames host-side to compensate the camera's no-AWB,
/// slightly-off-colour/dark RAW. Defaults are the identity transform.
struct ColorParams {
    double rGain = 1.0;       ///< White-balance gain, red   (≈0..4).
    double gGain = 1.0;       ///< White-balance gain, green.
    double bGain = 1.0;       ///< White-balance gain, blue.
    double brightness = 0.0;  ///< Additive, fraction of full-scale (−1..1).
    double contrast = 1.0;    ///< Multiplicative about mid-grey (0..2).
    double gamma = 1.0;       ///< Gamma (>0; 1 = none).
    double saturation = 1.0;  ///< 0 = greyscale, 1 = unchanged, >1 = boosted.

    /// True when every field is its identity value (transform is a no-op).
    [[nodiscard]] bool isIdentity() const noexcept;
};

/// Applies `ColorParams` to RGB24 frames using per-channel lookup tables (for
/// white-balance/contrast/brightness/gamma) plus an optional saturation pass.
///
/// `setParams` rebuilds the LUTs once; `apply` is then a tight per-pixel loop.
/// When the params are identity, `apply` returns the source unchanged.
class ColorCorrector {
public:
    ColorCorrector();

    /// Set the parameters and rebuild the lookup tables.
    void setParams(const ColorParams& params);
    [[nodiscard]] const ColorParams& params() const noexcept;

    /// Return a corrected copy of `src` (converted to RGB888 if needed). When
    /// the current params are identity, returns `src` unchanged.
    [[nodiscard]] QImage apply(const QImage& src) const;

private:
    void rebuild();

    ColorParams params_;
    bool identity_ = true;
    bool saturationActive_ = false;
    std::array<std::uint8_t, 256> lutR_{};
    std::array<std::uint8_t, 256> lutG_{};
    std::array<std::uint8_t, 256> lutB_{};
};

}  // namespace signalforge::video
