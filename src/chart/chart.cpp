// src/chart/chart.cpp
//
// S1 ctor/dtor stubs. Full implementation lands in S3 (SG node
// management) and S4 (30 Hz tick + LOD + live cursor).

#include "chart/chart.hpp"

namespace signalforge::chart {

struct Chart::Impl {
    // S3 will hold per-signal SG nodes, the cursor node, render
    // state. Empty in S1.
};

Chart::Chart(signalforge::buffer::SignalBufferRegistry& registry, TimeAxisManager& timeAxis, ChartConfig config,
             QQuickItem* parent)
    : QQuickItem(parent), registry_(&registry), timeAxis_(&timeAxis), config_(std::move(config)),
      impl_(std::make_unique<Impl>()) {
    setFlag(ItemHasContents, true);
    redrawTimer_.setInterval(33);
    redrawTimer_.setTimerType(Qt::PreciseTimer);
    QObject::connect(&redrawTimer_, &QTimer::timeout, this, &Chart::onTick);
    // Timer started in S4.
}

Chart::~Chart() = default;

void Chart::addSignal(const QString& /*signalId*/, std::optional<SignalDisplayMode> /*displayMode*/) {
    // S3 implementation.
}

void Chart::removeSignal(const QString& /*signalId*/) {
    // S3 implementation.
}

QStringList Chart::visibleSignals() const {
    QStringList out;
    out.reserve(static_cast<int>(config_.signalConfigs.size()));
    for (const auto& s : config_.signalConfigs) {
        if (s.visible) {
            out.append(s.signalId);
        }
    }
    return out;
}

void Chart::setDisplayMode(const QString& /*signalId*/, SignalDisplayMode /*mode*/) {
    // S3 implementation.
}

void Chart::setSignalVisible(const QString& /*signalId*/, bool /*visible*/) {
    // S3 implementation.
}

ChartConfig Chart::config() const {
    return config_;
}

void Chart::setConfig(ChartConfig config) {
    config_ = std::move(config);
}

Chart::FrameStats Chart::stats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_;
}

QSGNode* Chart::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* /*data*/) {
    // S3 implementation. Returning the old node (or nullptr on the
    // first call) means the SG keeps an empty subtree for now.
    return oldNode;
}

void Chart::onTick() {
    // S4 implementation.
}

}  // namespace signalforge::chart
