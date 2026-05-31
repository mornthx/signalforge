// src/dashboard/numeric_panel.cpp

#include "dashboard/numeric_panel.hpp"

#include "buffer/signal_buffer.hpp"
#include "buffer/signal_buffer_registry.hpp"
#include "dashboard/value_format.hpp"
#include "decode/decoder_interface.hpp"

#include <QLabel>
#include <QVBoxLayout>
#include <cmath>

namespace signalforge::dashboard {

namespace {

/// Field portion of a "<driverId>/<field>" signal id (everything
/// after the first '/'); the whole id if there is no '/'.
QString fieldName(const QString& signalId) {
    const int slash = signalId.indexOf(QLatin1Char('/'));
    return slash >= 0 ? signalId.mid(slash + 1) : signalId;
}

}  // namespace

NumericPanel::NumericPanel(PanelConfig config, signalforge::buffer::SignalBufferRegistry& registry, QWidget* parent)
    : Panel(std::move(config), parent), registry_(&registry) {
    auto* body = new QWidget(this);
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(2);

    valueLabel_ = new QLabel(QStringLiteral("—"), body);
    valueLabel_->setObjectName(QStringLiteral("numericValue"));
    valueLabel_->setProperty("class", QLatin1String("display"));
    layout->addWidget(valueLabel_);

    unitLabel_ = new QLabel(body);
    unitLabel_->setProperty("class", QLatin1String("caption"));
    layout->addWidget(unitLabel_);

    rangeLabel_ = new QLabel(body);
    rangeLabel_->setProperty("class", QLatin1String("caption"));
    layout->addWidget(rangeLabel_);
    layout->addStretch(1);

    setBody(body);
    refresh();
}

NumericPanel::~NumericPanel() = default;

QString NumericPanel::boundSignalId() const {
    return config_.signalIds.isEmpty() ? QString() : config_.signalIds.first();
}

void NumericPanel::refresh() {
    const QString signalId = boundSignalId();
    if (signalId.isEmpty() || registry_ == nullptr) {
        valueLabel_->setText(QStringLiteral("—"));
        return;
    }
    auto* buf = registry_->bufferFor(signalId);

    // Title + unit are derived lazily: metadata may not exist until the
    // first sample arrives, so we re-derive each refresh until set.
    const QString unit =
        !config_.unitOverride.isEmpty() ? config_.unitOverride : (buf != nullptr ? buf->metadata().unit : QString());
    unitLabel_->setText(unit);
    if (config_.title.isEmpty()) {
        const QString name =
            (buf != nullptr && !buf->metadata().name.isEmpty()) ? buf->metadata().name : fieldName(signalId);
        setHeaderTitle(name);
    }

    if (buf == nullptr) {
        valueLabel_->setText(QStringLiteral("—"));
        return;
    }
    const auto latest = buf->queryLatestOne();
    if (!latest.has_value()) {
        valueLabel_->setText(QStringLiteral("—"));
        return;
    }

    valueLabel_->setText(formatValue(latest->value, config_.decimals));

    const double d = valueToDouble(latest->value);
    if (std::isfinite(d)) {
        const double lo = observedMin_.has_value() ? std::min(*observedMin_, d) : d;
        const double hi = observedMax_.has_value() ? std::max(*observedMax_, d) : d;
        observedMin_ = lo;
        observedMax_ = hi;
        rangeLabel_->setText(tr("min %1  max %2")
                                 .arg(QString::number(lo, 'f', config_.decimals))
                                 .arg(QString::number(hi, 'f', config_.decimals)));
    } else {
        rangeLabel_->clear();
    }
}

QString NumericPanel::valueText() const {
    return valueLabel_ != nullptr ? valueLabel_->text() : QString();
}

QString NumericPanel::unitText() const {
    return unitLabel_ != nullptr ? unitLabel_->text() : QString();
}

}  // namespace signalforge::dashboard
