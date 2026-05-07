// src/chart/chart.cpp
//
// S3 — Chart lifecycle API (addSignal / removeSignal / setDisplayMode
// / setSignalVisible / config / setConfig) and the SG-node map
// allocation in updatePaintNode. Data population (queryRange + LOD)
// and the live time cursor land in S4.

#include "chart/chart.hpp"

#include "buffer/signal_buffer.hpp"
#include "decode/decoder_interface.hpp"

#include <QSGFlatColorMaterial>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QSGNode>
#include <algorithm>
#include <unordered_set>

namespace signalforge::chart {

namespace {

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
        QColor(122, 192, 255),  // soft blue
        QColor(255, 122, 122),  // soft red
        QColor(122, 255, 158),  // soft green
        QColor(255, 218, 122),  // soft amber
        QColor(196, 122, 255),  // soft purple
        QColor(122, 255, 244),  // teal
        QColor(255, 158, 122),  // peach
        QColor(180, 220, 122),  // olive
    };
    constexpr int n = static_cast<int>(sizeof(kPalette) / sizeof(kPalette[0]));
    return kPalette[((idx % n) + n) % n];
}

/// Drawing mode for a per-signal QSGGeometryNode given the M8
/// display mode. Step is rendered as a line strip at S3 (the spec
/// §4.4 step-rectangle pseudocode lands in S4 once we have data).
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

    /// Cursor node (live-time line) — created in S4.
    QSGNode* cursorNode = nullptr;
};

Chart::Chart(signalforge::buffer::SignalBufferRegistry& registry, TimeAxisManager& timeAxis, ChartConfig config,
             QQuickItem* parent)
    : QQuickItem(parent), registry_(&registry), timeAxis_(&timeAxis), config_(std::move(config)),
      impl_(std::make_unique<Impl>()) {
    setFlag(ItemHasContents, true);
    redrawTimer_.setInterval(33);
    redrawTimer_.setTimerType(Qt::PreciseTimer);
    QObject::connect(&redrawTimer_, &QTimer::timeout, this, &Chart::onTick);
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

Chart::~Chart() = default;

void Chart::addSignal(const QString& signalId, std::optional<SignalDisplayMode> displayMode) {
    if (signalId.isEmpty()) {
        return;
    }
    // No-op if the signal id is already present.
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
                // Force node recreation: the geometry's drawing
                // mode is bound at allocation time.
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
    // Mark every previous signal for removal so updatePaintNode
    // tears down their nodes; new entries will be allocated in
    // the next paint.
    for (const auto& s : config_.signalConfigs) {
        impl_->pendingRemovals.insert(s.signalId);
    }
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

    // Ensure each visible signal has a node. Vertex data is left
    // empty in S3 (S4 populates from queryRange).
    for (const auto& s : config_.signalConfigs) {
        if (!s.visible) {
            continue;
        }
        auto it = impl_->signalNodes.find(s.signalId);
        if (it == impl_->signalNodes.end()) {
            auto* node = new QSGGeometryNode;
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
            // Color may have been updated via setConfig.
            auto* mat = static_cast<QSGFlatColorMaterial*>(it->second->material());
            if (mat->color() != s.color && s.color.isValid()) {
                mat->setColor(s.color);
                it->second->markDirty(QSGNode::DirtyMaterial);
            }
        }
    }

    return root;
}

void Chart::onTick() {
    // S4 implementation.
}

}  // namespace signalforge::chart
