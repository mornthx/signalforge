// src/video/color_panel.cpp
#include "video/color_panel.hpp"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>

namespace signalforge::video {

namespace {
// Sliders are integers scaled by 100 (e.g. value 100 ⇒ 1.0).
QSlider* makeSlider(int min, int max, int value) {
    auto* s = new QSlider(Qt::Horizontal);
    s->setRange(min, max);
    s->setValue(value);
    return s;
}
}  // namespace

ColorPanel::ColorPanel(QWidget* parent) : QWidget(parent) {
    rGain_ = makeSlider(0, 400, 100);
    gGain_ = makeSlider(0, 400, 100);
    bGain_ = makeSlider(0, 400, 100);
    brightness_ = makeSlider(-100, 100, 0);
    contrast_ = makeSlider(0, 200, 100);
    gamma_ = makeSlider(10, 500, 100);
    saturation_ = makeSlider(0, 200, 100);

    auto* grid = new QGridLayout(this);
    grid->setContentsMargins(8, 6, 8, 6);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(4);

    const struct {
        const char* label;
        QSlider* slider;
    } rows[] = {
        {QT_TR_NOOP("R gain"), rGain_},          {QT_TR_NOOP("G gain"), gGain_},      {QT_TR_NOOP("B gain"), bGain_},
        {QT_TR_NOOP("Brightness"), brightness_}, {QT_TR_NOOP("Contrast"), contrast_}, {QT_TR_NOOP("Gamma"), gamma_},
        {QT_TR_NOOP("Saturation"), saturation_},
    };
    int row = 0;
    for (const auto& r : rows) {
        grid->addWidget(new QLabel(tr(r.label), this), row, 0);
        grid->addWidget(r.slider, row, 1);
        connect(r.slider, &QSlider::valueChanged, this, &ColorPanel::onSliderChanged);
        ++row;
    }

    auto* buttons = new QHBoxLayout();
    auto* reset = new QPushButton(tr("Reset"), this);
    connect(reset, &QPushButton::clicked, this, [this] { setParams(ColorParams{}); });
    auto* save = new QPushButton(tr("Save…"), this);
    connect(save, &QPushButton::clicked, this, &ColorPanel::saveRequested);
    auto* load = new QPushButton(tr("Load…"), this);
    connect(load, &QPushButton::clicked, this, &ColorPanel::loadRequested);
    buttons->addWidget(reset);
    buttons->addWidget(save);
    buttons->addWidget(load);
    grid->addLayout(buttons, row, 0, 1, 2);
}

ColorParams ColorPanel::params() const {
    return readSliders();
}

ColorParams ColorPanel::readSliders() const {
    ColorParams p;
    p.rGain = rGain_->value() / 100.0;
    p.gGain = gGain_->value() / 100.0;
    p.bGain = bGain_->value() / 100.0;
    p.brightness = brightness_->value() / 100.0;
    p.contrast = contrast_->value() / 100.0;
    p.gamma = gamma_->value() / 100.0;
    p.saturation = saturation_->value() / 100.0;
    return p;
}

void ColorPanel::applyToSliders(const ColorParams& p) {
    const QSignalBlocker b1(rGain_);
    const QSignalBlocker b2(gGain_);
    const QSignalBlocker b3(bGain_);
    const QSignalBlocker b4(brightness_);
    const QSignalBlocker b5(contrast_);
    const QSignalBlocker b6(gamma_);
    const QSignalBlocker b7(saturation_);
    rGain_->setValue(static_cast<int>(p.rGain * 100.0));
    gGain_->setValue(static_cast<int>(p.gGain * 100.0));
    bGain_->setValue(static_cast<int>(p.bGain * 100.0));
    brightness_->setValue(static_cast<int>(p.brightness * 100.0));
    contrast_->setValue(static_cast<int>(p.contrast * 100.0));
    gamma_->setValue(static_cast<int>(p.gamma * 100.0));
    saturation_->setValue(static_cast<int>(p.saturation * 100.0));
}

void ColorPanel::setParams(const ColorParams& p) {
    applyToSliders(p);
    emit paramsChanged(readSliders());
}

void ColorPanel::onSliderChanged() {
    emit paramsChanged(readSliders());
}

}  // namespace signalforge::video
