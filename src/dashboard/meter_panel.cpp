// src/dashboard/meter_panel.cpp

#include "dashboard/meter_panel.hpp"

#include "buffer/signal_buffer.hpp"
#include "buffer/signal_buffer_registry.hpp"
#include "dashboard/meter_view.hpp"
#include "dashboard/value_format.hpp"
#include "decode/decoder_interface.hpp"

#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <utility>

namespace signalforge::dashboard {

namespace {
QString fieldName(const QString& signalId) {
    const int slash = signalId.indexOf(QLatin1Char('/'));
    return slash >= 0 ? signalId.mid(slash + 1) : signalId;
}
}  // namespace

MeterPanel::MeterPanel(PanelConfig config, signalforge::buffer::SignalBufferRegistry& registry, QWidget* parent)
    : Panel(std::move(config), parent), registry_(&registry) {
    const auto style = (config_.type == PanelType::Gauge) ? MeterView::Style::Gauge : MeterView::Style::Bar;
    auto* body = new QWidget(this);
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(8, 8, 8, 8);
    meter_ = new MeterView(style, body);
    layout->addWidget(meter_);
    setBody(body);
    refresh();
}

MeterPanel::~MeterPanel() = default;

QString MeterPanel::boundSignalId() const {
    return config_.signalIds.isEmpty() ? QString() : config_.signalIds.first();
}

void MeterPanel::refresh() {
    const QString signalId = boundSignalId();
    auto* buf = (registry_ != nullptr && !signalId.isEmpty()) ? registry_->bufferFor(signalId) : nullptr;

    if (config_.title.isEmpty()) {
        const QString name =
            (buf != nullptr && !buf->metadata().name.isEmpty()) ? buf->metadata().name : fieldName(signalId);
        setHeaderTitle(name);
    }
    if (meter_ != nullptr) {
        meter_->setUnit(!config_.unitOverride.isEmpty() ? config_.unitOverride
                                                        : (buf != nullptr ? buf->metadata().unit : QString()));
    }

    hasData_ = false;
    if (buf != nullptr) {
        if (const auto latest = buf->queryLatestOne(); latest.has_value()) {
            const double v = valueToDouble(latest->value);
            if (std::isfinite(v)) {
                value_ = v;
                hasData_ = true;
                obsMin_ = obsMin_.has_value() ? std::min(*obsMin_, v) : v;
                obsMax_ = obsMax_.has_value() ? std::max(*obsMax_, v) : v;
            }
        }
    }

    // Range: explicit config wins, else observed (fall back to the current
    // value, then to [0,1] when nothing is known).
    lo_ = config_.rangeMin.value_or(obsMin_.value_or(hasData_ ? value_ : 0.0));
    hi_ = config_.rangeMax.value_or(obsMax_.value_or(hasData_ ? value_ : 1.0));
    if (lo_ >= hi_) {
        hi_ = lo_ + 1.0;
    }

    if (meter_ != nullptr) {
        meter_->setRange(lo_, hi_);
        meter_->setValue(value_, hasData_);
        if (colorProvider_) {
            meter_->setColor(colorProvider_(signalId));
        }
    }
}

void MeterPanel::setSignalColorProvider(SignalColorProvider provider) {
    colorProvider_ = std::move(provider);
    if (colorProvider_ && meter_ != nullptr) {
        meter_->setColor(colorProvider_(boundSignalId()));
    }
}

}  // namespace signalforge::dashboard
