// src/video/color_panel.hpp
#pragma once

#include "video/color_correction.hpp"

#include <QWidget>

class QSlider;

namespace signalforge::video {

/// Slider panel that edits a `ColorParams` (white-balance R/G/B gain, brightness,
/// contrast, gamma, saturation) and emits `paramsChanged` live. A Reset button
/// returns to the identity transform.
class ColorPanel : public QWidget {
    Q_OBJECT

public:
    explicit ColorPanel(QWidget* parent = nullptr);

    [[nodiscard]] ColorParams params() const;

    /// Load `p` into the sliders and emit `paramsChanged(p)` once.
    void setParams(const ColorParams& p);

signals:
    void paramsChanged(const signalforge::video::ColorParams& params);

private slots:
    void onSliderChanged();

private:
    [[nodiscard]] ColorParams readSliders() const;
    void applyToSliders(const ColorParams& p);

    QSlider* rGain_ = nullptr;
    QSlider* gGain_ = nullptr;
    QSlider* bGain_ = nullptr;
    QSlider* brightness_ = nullptr;
    QSlider* contrast_ = nullptr;
    QSlider* gamma_ = nullptr;
    QSlider* saturation_ = nullptr;
};

}  // namespace signalforge::video
