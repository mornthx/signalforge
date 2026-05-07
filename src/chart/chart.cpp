// src/chart/chart.cpp
//
// S3: Chart lifecycle API + per-signal SG node map allocation.
// S4: 30 Hz tick driven from QTimer (Qt::PreciseTimer) — collects
//     per-signal data via `bufferFor(id)->queryRange(start, end,
//     target = width())`, populates the geometry vertex buffers,
//     renders the live time cursor with subpixel-alternate forcing
//     to defeat Qt RHI frame coalescing (per spec §4.5 / [Proto]
//     Anomaly §1), and updates FrameStats.

#include "chart/chart.hpp"

#include "buffer/signal_buffer.hpp"
#include "decode/decoder_interface.hpp"

#include <QSGFlatColorMaterial>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QSGNode>
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <variant>

namespace signalforge::chart {

namespace {

constexpr double kDropFrameMs = 50.0;  ///< Frame interval > this counts as a dropped frame.

/// Map M5 SignalType → spec §3.4 default display mode.
[[nodiscard]] SignalDisplayMode autoModeForType(signalforge::decoder::SignalType type) noexcept {
    using signalforge::decoder::SignalType;
    switch (type) {
    case SignalType::Bool:
        return SignalDisplayMode::Step;
    case SignalType::Int64:
    case SignalType::Double:
        return SignalDisplayMode::Line;
    case SignalType::String:
        return SignalDisplayMode::Point;
    }
    return SignalDisplayMode::Line;
}

/// Pleasant default-color palette. Auto-assigned when the user
/// adds a signal without specifying a color. Loops if more signals
/// than entries — fine for V1, theming is V1.5+.
[[nodiscard]] QColor defaultColorForIndex(int idx) noexcept {
    static const QColor kPalette[] = {
        QColor(122, 192, 255), QColor(255, 122, 122), QColor(122, 255, 158), QColor(255, 218, 122),
        QColor(196, 122, 255), QColor(122, 255, 244), QColor(255, 158, 122), QColor(180, 220, 122),
    };
    constexpr int n = static_cast<int>(sizeof(kPalette) / sizeof(kPalette[0]));
    return kPalette[((idx % n) + n) % n];
}

/// Drawing mode for a per-signal QSGGeometryNode given the M8
/// display mode. Step is rendered as a line strip at S3 (the spec
/// §4.4 step-rectangle pseudocode lands later if measurement says
/// it matters; for V1, line-strip with sample-snapping is correct).
[[nodiscard]] QSGGeometry::DrawingMode drawingModeFor(SignalDisplayMode mode) noexcept {
    switch (mode) {
    case SignalDisplayMode::Line:
    case SignalDisplayMode::Step:
        return QSGGeometry::DrawLineStrip;
    case SignalDisplayMode::Point:
        return QSGGeometry::DrawPoints;
    }
    return QSGGeometry::DrawLineStrip;
}

/// Extract a numeric Y from a SignalValue. Returns NaN for
/// QString variants so the caller can either skip them or render
/// them as point-with-text (V1.5+).
[[nodiscard]] double valueToDouble(const signalforge::decoder::SignalValue& v) noexcept {
    if (auto* d = std::get_if<double>(&v); d != nullptr) {
        return *d;
    }
    if (auto* i = std::get_if<std::int64_t>(&v); i != nullptr) {
        return static_cast<double>(*i);
    }
    if (auto* b = std::get_if<bool>(&v); b != nullptr) {
        return *b ? 1.0 : 0.0;
    }
    return std::numeric_limits<double>::quiet_NaN();
}

}  // namespace

struct Chart::Impl {
    /// One QSGGeometryNode per visible signal, keyed by signal id.
    /// Entries are created in `updatePaintNode` on first paint of a
    /// signal and destroyed when the signal is removed from config.
    /// Owned by the parent root node via the SG's tree parenting.
    std::unordered_map<QString, QSGGeometryNode*> signalNodes;

    /// Set of signal ids whose nodes need removal on the next paint.
    /// Populated by `removeSignal`; consumed by `updatePaintNode`.
    std::unordered_set<QString> pendingRemovals;

    /// Latest queryRange results stamped by `onTick`. Read on the
    /// render thread inside `updatePaintNode`. The map is rebuilt
    /// each tick — small enough that per-tick reallocation is fine.
    std::unordered_map<QString, std::vector<signalforge::buffer::SignalSample>> latestSamples;

    /// Auto-scale Y range computed from `latestSamples` each tick.
    double yMin = -1.0;
    double yMax = 1.0;

    /// Live cursor node + its current x-pixel position. Per spec
    /// §4.5, the cursor is what defeats Qt RHI frame coalescing —
    /// every frame, the cursor's vertex data must change by at
    /// least one rendered pixel from the previous frame. We use
    /// `cursorYAlternate` to flip the cursor's top-y between rows
    /// 0 and 1 so even at long visible_duration (where cursor x
    /// moves < 1 px/frame) the rendered output differs.
    QSGGeometryNode* cursorNode = nullptr;
    double lastCursorX = -1.0;
    bool cursorYAlternate = false;

    /// Last `onTick` wall time (steady_clock). Used to compute
    /// per-frame interval for the dropped-frame metric.
    std::chrono::steady_clock::time_point lastTickAt{};
};

Chart::Chart(signalforge::buffer::SignalBufferRegistry& registry, TimeAxisManager& timeAxis, ChartConfig config,
             QQuickItem* parent)
    : QQuickItem(parent), registry_(&registry), timeAxis_(&timeAxis), config_(std::move(config)),
      impl_(std::make_unique<Impl>()) {
    setFlag(ItemHasContents, true);
    redrawTimer_.setInterval(33);  // 30 Hz; precise timer per spec §9 note
    redrawTimer_.setTimerType(Qt::PreciseTimer);
    QObject::connect(&redrawTimer_, &QTimer::timeout, this, &Chart::onTick);
    redrawTimer_.start();

    // Auto-assign colors to any incoming pre-configured signals
    // that lack one.
    int idx = 0;
    for (auto& s : config_.signalConfigs) {
        if (!s.color.isValid()) {
            s.color = defaultColorForIndex(idx);
        }
        ++idx;
    }
}

Chart::~Chart() {
    redrawTimer_.stop();
}

void Chart::addSignal(const QString& signalId, std::optional<SignalDisplayMode> displayMode) {
    if (signalId.isEmpty()) {
        return;
    }
    auto it = std::find_if(config_.signalConfigs.begin(), config_.signalConfigs.end(),
                           [&](const ChartSignalConfig& s) { return s.signalId == signalId; });
    if (it != config_.signalConfigs.end()) {
        return;
    }

    ChartSignalConfig entry;
    entry.signalId = signalId;
    if (displayMode.has_value()) {
        entry.displayMode = *displayMode;
    } else if (auto* buf = registry_->bufferFor(signalId); buf != nullptr) {
        entry.displayMode = autoModeForType(buf->metadata().type);
    } else {
        entry.displayMode = SignalDisplayMode::Line;
    }
    entry.color = defaultColorForIndex(static_cast<int>(config_.signalConfigs.size()));
    entry.visible = true;
    config_.signalConfigs.push_back(std::move(entry));
    update();
    Q_EMIT signalAdded(signalId);
}

void Chart::removeSignal(const QString& signalId) {
    auto it = std::find_if(config_.signalConfigs.begin(), config_.signalConfigs.end(),
                           [&](const ChartSignalConfig& s) { return s.signalId == signalId; });
    if (it == config_.signalConfigs.end()) {
        return;
    }
    config_.signalConfigs.erase(it);
    impl_->pendingRemovals.insert(signalId);
    impl_->latestSamples.erase(signalId);
    update();
    Q_EMIT signalRemoved(signalId);
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

void Chart::setDisplayMode(const QString& signalId, SignalDisplayMode mode) {
    for (auto& s : config_.signalConfigs) {
        if (s.signalId == signalId) {
            if (s.displayMode != mode) {
                s.displayMode = mode;
                impl_->pendingRemovals.insert(signalId);
                update();
            }
            return;
        }
    }
}

void Chart::setSignalVisible(const QString& signalId, bool visible) {
    for (auto& s : config_.signalConfigs) {
        if (s.signalId == signalId) {
            if (s.visible != visible) {
                s.visible = visible;
                if (!visible) {
                    impl_->pendingRemovals.insert(signalId);
                    impl_->latestSamples.erase(signalId);
                }
                update();
            }
            return;
        }
    }
}

ChartConfig Chart::config() const {
    return config_;
}

void Chart::setConfig(ChartConfig config) {
    for (const auto& s : config_.signalConfigs) {
        impl_->pendingRemovals.insert(s.signalId);
    }
    impl_->latestSamples.clear();
    config_ = std::move(config);
    int idx = 0;
    for (auto& s : config_.signalConfigs) {
        if (!s.color.isValid()) {
            s.color = defaultColorForIndex(idx);
        }
        ++idx;
    }
    update();
}

Chart::FrameStats Chart::stats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_;
}

void Chart::onTick() {
    const auto tickStart = std::chrono::steady_clock::now();

    // Update FrameStats interval bookkeeping. The first tick has
    // no prior reference, so skip the drop-frame check on tick 1.
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        ++stats_.totalRedraws;
        if (impl_->lastTickAt.time_since_epoch().count() != 0) {
            const double dtMs = std::chrono::duration<double, std::milli>(tickStart - impl_->lastTickAt).count();
            if (dtMs > kDropFrameMs) {
                ++stats_.droppedFrames;
            }
        }
    }
    impl_->lastTickAt = tickStart;

    // Query each visible signal for the visible time range.
    // M6 LOD selection happens inside queryRange (target = pixel
    // width). The Y auto-scale is computed across all visible
    // signals' samples — V1 uses a single shared Y axis per chart.
    const auto axisStart = timeAxis_->visibleStart();
    const auto axisEnd = timeAxis_->visibleEnd();
    const auto pixelWidth = static_cast<std::size_t>(std::max(1, static_cast<int>(width())));

    impl_->latestSamples.clear();
    double yMin = std::numeric_limits<double>::infinity();
    double yMax = -std::numeric_limits<double>::infinity();
    for (const auto& s : config_.signalConfigs) {
        if (!s.visible) {
            continue;
        }
        auto* buf = registry_->bufferFor(s.signalId);
        if (buf == nullptr) {
            continue;
        }
        auto samples = buf->queryRange(axisStart, axisEnd, pixelWidth);
        for (const auto& sample : samples) {
            const double y = valueToDouble(sample.value);
            if (std::isnan(y)) {
                continue;
            }
            if (y < yMin) {
                yMin = y;
            }
            if (y > yMax) {
                yMax = y;
            }
        }
        impl_->latestSamples[s.signalId] = std::move(samples);
    }
    if (!std::isfinite(yMin) || !std::isfinite(yMax)) {
        // No data yet (or all NaN). Use a neutral default range.
        yMin = -1.0;
        yMax = 1.0;
    } else if (yMin == yMax) {
        // Constant signal: pad ±0.5 so the line is centered.
        yMin -= 0.5;
        yMax += 0.5;
    } else {
        // Pad 5% so peaks don't touch the chart edge.
        const double pad = 0.05 * (yMax - yMin);
        yMin -= pad;
        yMax += pad;
    }
    impl_->yMin = yMin;
    impl_->yMax = yMax;

    // Update render duration stat. peakRenderUs is the worst-case
    // observed (cumulative max).
    const auto tickEnd = std::chrono::steady_clock::now();
    const auto renderUs = std::chrono::duration_cast<std::chrono::microseconds>(tickEnd - tickStart);
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.lastRenderUs = renderUs;
        if (renderUs > stats_.peakRenderUs) {
            stats_.peakRenderUs = renderUs;
        }
    }

    update();
}

QSGNode* Chart::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* /*data*/) {
    auto* root = oldNode != nullptr ? oldNode : new QSGNode;

    // Tear down nodes for removed / hidden / mode-changed signals.
    for (const auto& id : impl_->pendingRemovals) {
        auto it = impl_->signalNodes.find(id);
        if (it != impl_->signalNodes.end()) {
            root->removeChildNode(it->second);
            delete it->second;
            impl_->signalNodes.erase(it);
        }
    }
    impl_->pendingRemovals.clear();

    const double w = static_cast<double>(width());
    const double h = static_cast<double>(height());
    const double yRangeSpan = (impl_->yMax - impl_->yMin > 0.0) ? (impl_->yMax - impl_->yMin) : 1.0;

    const auto axisStart = timeAxis_->visibleStart();
    const auto axisEnd = timeAxis_->visibleEnd();
    const auto axisDurationNs = std::chrono::duration_cast<std::chrono::nanoseconds>(axisEnd - axisStart).count();
    const double xScale = (axisDurationNs > 0) ? (w / static_cast<double>(axisDurationNs)) : 0.0;

    // Per-signal geometry update.
    for (const auto& s : config_.signalConfigs) {
        if (!s.visible) {
            continue;
        }
        auto it = impl_->signalNodes.find(s.signalId);
        QSGGeometryNode* node;
        if (it == impl_->signalNodes.end()) {
            node = new QSGGeometryNode;
            auto* geom = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0);
            geom->setLineWidth(1);
            geom->setDrawingMode(drawingModeFor(s.displayMode));
            node->setGeometry(geom);
            node->setFlag(QSGNode::OwnsGeometry);
            auto* mat = new QSGFlatColorMaterial;
            mat->setColor(s.color.isValid() ? s.color : QColor(200, 200, 200));
            node->setMaterial(mat);
            node->setFlag(QSGNode::OwnsMaterial);
            root->appendChildNode(node);
            impl_->signalNodes[s.signalId] = node;
        } else {
            node = it->second;
            auto* mat = static_cast<QSGFlatColorMaterial*>(node->material());
            if (s.color.isValid() && mat->color() != s.color) {
                mat->setColor(s.color);
                node->markDirty(QSGNode::DirtyMaterial);
            }
        }

        const auto sampIt = impl_->latestSamples.find(s.signalId);
        if (sampIt == impl_->latestSamples.end()) {
            // No samples this tick — drop the geometry to zero so
            // the existing node renders nothing without re-allocating.
            node->geometry()->allocate(0);
            node->markDirty(QSGNode::DirtyGeometry);
            continue;
        }
        const auto& samples = sampIt->second;
        node->geometry()->allocate(static_cast<int>(samples.size()));
        if (samples.empty()) {
            node->markDirty(QSGNode::DirtyGeometry);
            continue;
        }
        auto* v = node->geometry()->vertexDataAsPoint2D();
        for (std::size_t i = 0; i < samples.size(); ++i) {
            const auto& sample = samples[i];
            const double y = valueToDouble(sample.value);
            const auto offsetNs =
                std::chrono::duration_cast<std::chrono::nanoseconds>(sample.timestamp - axisStart).count();
            const float x = static_cast<float>(static_cast<double>(offsetNs) * xScale);
            const float yPx = static_cast<float>(h - (y - impl_->yMin) / yRangeSpan * h);
            v[i].set(x, yPx);
        }
        node->markDirty(QSGNode::DirtyGeometry);
    }

    // Live time cursor: vertical line at "now" position. The
    // alternation pattern (cursorYAlternate flipping y0 by 1 px)
    // is the PRIMARY mechanism that defeats RHI frame coalescing
    // (per S4 user note: at 1-hour visible / 1500 px width, cursor
    // moves <1 px/frame, so we can't rely on x-motion alone). The
    // x-position update is correct semantics; the alternation is
    // the visibility guarantee.
    if (impl_->cursorNode == nullptr) {
        impl_->cursorNode = new QSGGeometryNode;
        auto* geom = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 2);
        geom->setLineWidth(1);
        geom->setDrawingMode(QSGGeometry::DrawLineStrip);
        impl_->cursorNode->setGeometry(geom);
        impl_->cursorNode->setFlag(QSGNode::OwnsGeometry);
        auto* mat = new QSGFlatColorMaterial;
        mat->setColor(QColor(255, 255, 255, 180));
        impl_->cursorNode->setMaterial(mat);
        impl_->cursorNode->setFlag(QSGNode::OwnsMaterial);
        root->appendChildNode(impl_->cursorNode);
    }
    {
        const auto now = std::chrono::steady_clock::now();
        const auto offsetNs = std::chrono::duration_cast<std::chrono::nanoseconds>(now - axisStart).count();
        const double cursorX = (axisDurationNs > 0) ? std::clamp(static_cast<double>(offsetNs) * xScale, 0.0, w) : 0.0;
        impl_->lastCursorX = cursorX;
        const float xPx = static_cast<float>(cursorX);
        const float y0 = impl_->cursorYAlternate ? 1.0f : 0.0f;
        impl_->cursorYAlternate = !impl_->cursorYAlternate;
        auto* v = impl_->cursorNode->geometry()->vertexDataAsPoint2D();
        v[0].set(xPx, y0);
        v[1].set(xPx, static_cast<float>(h));
        impl_->cursorNode->markDirty(QSGNode::DirtyGeometry);
    }

    return root;
}

}  // namespace signalforge::chart
