// src/dashboard/state_panel.cpp

#include "dashboard/state_panel.hpp"

#include "buffer/signal_buffer.hpp"
#include "buffer/signal_buffer_registry.hpp"
#include "dashboard/value_format.hpp"
#include "decode/decoder_interface.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <variant>

namespace signalforge::dashboard {

namespace {

QString fieldName(const QString& signalId) {
    const int slash = signalId.indexOf(QLatin1Char('/'));
    return slash >= 0 ? signalId.mid(slash + 1) : signalId;
}

/// Decide the "active" flag for the indicator: bool true, or a
/// non-empty string that isn't "false"/"0"/"off", or a non-zero number.
bool isActiveValue(const signalforge::decoder::SignalValue& v) {
    if (const auto* b = std::get_if<bool>(&v); b != nullptr) {
        return *b;
    }
    if (const auto* s = std::get_if<QString>(&v); s != nullptr) {
        const QString t = s->trimmed().toLower();
        return !(t.isEmpty() || t == QStringLiteral("false") || t == QStringLiteral("0") || t == QStringLiteral("off"));
    }
    const double d = valueToDouble(v);
    return d != 0.0;
}

}  // namespace

StatePanel::StatePanel(PanelConfig config, signalforge::buffer::SignalBufferRegistry& registry, QWidget* parent)
    : Panel(std::move(config), parent), registry_(&registry) {
    auto* body = new QWidget(this);
    auto* layout = new QHBoxLayout(body);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(8);

    indicatorLabel_ = new QLabel(QStringLiteral("○"), body);
    indicatorLabel_->setObjectName(QStringLiteral("stateIndicator"));
    indicatorLabel_->setProperty("class", QLatin1String("display"));
    layout->addWidget(indicatorLabel_);

    stateLabel_ = new QLabel(QStringLiteral("—"), body);
    stateLabel_->setObjectName(QStringLiteral("stateLabel"));
    stateLabel_->setProperty("class", QLatin1String("heading"));
    layout->addWidget(stateLabel_);
    layout->addStretch(1);

    setBody(body);
    refresh();
}

StatePanel::~StatePanel() = default;

QString StatePanel::boundSignalId() const {
    return config_.signalIds.isEmpty() ? QString() : config_.signalIds.first();
}

void StatePanel::refresh() {
    const QString signalId = boundSignalId();
    if (signalId.isEmpty() || registry_ == nullptr) {
        return;
    }
    auto* buf = registry_->bufferFor(signalId);
    if (config_.title.isEmpty()) {
        const QString name =
            (buf != nullptr && !buf->metadata().name.isEmpty()) ? buf->metadata().name : fieldName(signalId);
        setHeaderTitle(name);
    }
    if (buf == nullptr) {
        return;
    }
    const auto latest = buf->queryLatestOne();
    if (!latest.has_value()) {
        active_ = false;
        indicatorLabel_->setText(QStringLiteral("○"));
        stateLabel_->setText(QStringLiteral("—"));
        return;
    }
    active_ = isActiveValue(latest->value);
    indicatorLabel_->setText(active_ ? QStringLiteral("●") : QStringLiteral("○"));
    stateLabel_->setText(formatValue(latest->value, config_.decimals));
}

QString StatePanel::stateText() const {
    return stateLabel_ != nullptr ? stateLabel_->text() : QString();
}

}  // namespace signalforge::dashboard
