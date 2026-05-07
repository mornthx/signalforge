// src/chart/chart.hpp
#pragma once

#include "buffer/signal_buffer_registry.hpp"
#include "chart/time_axis_manager.hpp"
#include "decode/decoder_interface.hpp"

#include <QColor>
#include <QQuickItem>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace signalforge::chart {

/// Display mode for a signal within a chart. Defaults are auto-
/// selected from `SignalMetadata::type` (decision M8.4):
/// `Bool` → `Step`, `Int64` / `Double` → `Line`, `QString` → `Point`.
/// Users may override per signal in chart context menu (V1) or via
/// the `charts.yaml` config (V1).
enum class SignalDisplayMode {
    Line,   ///< Default for Double / Int64 — connected line strip.
    Step,   ///< Default for Bool — step-function 0/1 rectangles.
    Point,  ///< Default for QString — scatter point with annotation.
};

/// Per-signal config within a chart.
struct ChartSignalConfig {
    QString signalId;
    SignalDisplayMode displayMode = SignalDisplayMode::Line;
    QColor color;         ///< Optional; auto-assigned if invalid.
    bool visible = true;  ///< Toggle without removing from chart.
};

/// Chart configuration; saved to / loaded from yaml.
///
/// Note: the C++ field for the signal list is `signalConfigs`
/// rather than the spec's literal `signals` to avoid a collision
/// with Qt's `signals` macro (AUTOMOC parses the macro even in
/// non-Q_OBJECT contexts). The yaml key remains `signals` —
/// see `.claude/M8-concerns.md` C1.
struct ChartConfig {
    QString id;                                    ///< Stable identifier.
    QString title;                                 ///< Human-readable.
    std::vector<ChartSignalConfig> signalConfigs;  ///< Visible signals (yaml key: `signals`).
    std::optional<QString> timeAxisId;             ///< If set, joins a specific axis (V1.5+).
};

/// Single chart rendering N signals via Qt Quick Scene Graph.
///
/// Threading: lives on the main thread (QQuickItem). Scene Graph
/// node manipulation happens on the render thread (Qt manages this);
/// data updates from M6 buffer happen on the main thread via
/// `update()` calls.
///
/// Lifecycle:
/// - Constructed with a `SignalBufferRegistry` reference, a shared
///   `TimeAxisManager` reference, and an initial `ChartConfig`.
/// - Self-driven 30 Hz QTimer (`Qt::PreciseTimer`) drives `update()`.
/// - `addSignal` / `removeSignal` / `setDisplayMode` mutate state;
///   the next paint reflects the new config.
///
/// Freeze scope: this class is frozen at M8 close. See spec §6.1.
class Chart : public QQuickItem {
    Q_OBJECT

public:
    explicit Chart(signalforge::buffer::SignalBufferRegistry& registry, TimeAxisManager& timeAxis,
                   ChartConfig config = {}, QQuickItem* parent = nullptr);
    ~Chart() override;

    Chart(const Chart&) = delete;
    Chart& operator=(const Chart&) = delete;
    Chart(Chart&&) = delete;
    Chart& operator=(Chart&&) = delete;

    /// Add a signal to the chart. If `displayMode` is unset,
    /// auto-select per spec §3.4 from the registry's metadata.
    /// No-op if the signal id is already present.
    void addSignal(const QString& signalId, std::optional<SignalDisplayMode> displayMode = std::nullopt);

    /// Remove a signal. No-op if not present.
    void removeSignal(const QString& signalId);

    /// List currently-visible signal ids (in insertion order).
    [[nodiscard]] QStringList visibleSignals() const;

    /// Override display mode for a specific signal.
    void setDisplayMode(const QString& signalId, SignalDisplayMode mode);

    /// Toggle signal visibility without removing from the chart.
    void setSignalVisible(const QString& signalId, bool visible);

    /// Current chart config (snapshot, for persistence).
    [[nodiscard]] ChartConfig config() const;

    /// Replace the current state with `config`.
    void setConfig(ChartConfig config);

    /// Per-tick stats for diagnostics.
    struct FrameStats {
        std::uint64_t totalRedraws = 0;
        std::uint64_t droppedFrames = 0;  ///< Frame interval > 50 ms.
        std::chrono::microseconds lastRenderUs{0};
        std::chrono::microseconds peakRenderUs{0};
        int currentLodLevel = 0;
    };
    [[nodiscard]] FrameStats stats() const;

    // QQuickItem overrides
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;

Q_SIGNALS:
    void signalAdded(const QString& signalId);
    void signalRemoved(const QString& signalId);
    void redrawCompleted(std::uint64_t redrawIndex);

private Q_SLOTS:
    void onTick();  ///< 30 Hz timer slot.

private:
    signalforge::buffer::SignalBufferRegistry* registry_;
    TimeAxisManager* timeAxis_;
    ChartConfig config_;
    QTimer redrawTimer_;

    struct Impl;
    std::unique_ptr<Impl> impl_;

    mutable std::mutex statsMutex_;
    FrameStats stats_;
};

}  // namespace signalforge::chart
