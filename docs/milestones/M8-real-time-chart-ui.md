# M8 — Real-time Chart UI

| Field | Value |
|---|---|
| Milestone ID | M8 |
| Sprint | 8 |
| Estimated effort | 10-14 person-days |
| Prerequisites | M6 closed + ADR-005 patch merged (main at 7e1837a or later); M7 closed (PR #10 merged) |
| Next milestone | M9 (Connection Manager full features) |
| Hard-stop type | **Interface freeze** (`Chart` API + `ChartManager` API + signal selection contract) + **Performance certification** (validated against M8 prototype RESULTS + 20% margin) |
| Soft-HALT allowed | **No** |
| Branch | `milestone/M8` |

**Cross-reference notation**:

- `[EM §N]` — Execution Manual, section N
- `[Arch §N]` — Architecture document, section N
- `[MR]` — Milestone Roadmap
- `[CM §X]` — CLAUDE.md, section X
- `[ADR-N]` — Architecture Decision Record N
- `[M<n> §N]` — M<n> spec
- `[Proto]` — `tools/m8_prototype/RESULTS.md`

---

## 1. Goal

Real-time visualization of base and derived signals via QQuickWidget Scene Graph charts, sustaining 30 Hz redraw with up to 60-100 visible signals across 3-6 concurrent charts, with global time axis synchronization and signal-type-appropriate display modes.

This is **the first user-facing UI milestone in V1**. Users open SignalForge, connect a device (M3), see frames flow (M4), see decoded signal values updating in charts (M5/M6/M7) — all of this real-time visualization happens in M8.

Spec authoring is grounded in measurement: the M8 prototype on `prototype/m8-perf` (May 2026) validated the architecture under realistic load. **All performance gates in §5 are derived from prototype measurements + 20% margin**, not estimation. This explicitly supersedes the M6 spec authoring approach where unmeasured aspirational thresholds caused the ADR-004 revision.

The 4 prototype scenarios established:
- Pure render: 60 charts × 1000 samples sustained 30Hz; render-loop p99 = 0.60ms
- Update + render: 60 charts × 1kHz updates × 30Hz render sustained; 1.65M total updates over 30s, 0 dropped frames
- Multi-chart with pan: 3 charts × 20 signals + global pan sustained 30Hz; pan smoothness variance 1.81 ms²
- LOD integration: 600k samples in M6 buffer + 4 zoom levels; LOD switch < 130µs

These results (RESULTS.md) inform every threshold in §5.

---

## 2. Scope

### 2.1 Must deliver

1. **`Chart`** at `src/chart/chart.{hpp,cpp}` (QQuickItem subclass):
   - Single chart instance rendering N signals
   - Scene Graph node management (line strips per signal)
   - 30 Hz self-driven redraw timer (per decision M8.1)
   - Per-signal display style selection (line / step / point per decision M8.4)
   - Time axis (shared via TimeAxisManager, per decision M8.3)
   - Live time cursor (functional, not cosmetic — ensures Qt RHI frame coalescing doesn't drop redraws; see §4.5)
   - Pan / zoom via mouse interaction
   - Memory bounded (chart maintains buffered LOD-decimated data; raw data stays in M6 SignalBuffer)

2. **`ChartManager`** at `src/chart/chart_manager.{hpp,cpp}`:
   - Owns N `Chart` instances
   - Owns the global `TimeAxisManager`
   - Coordinates pan/zoom across charts
   - Provides API to add/remove charts

3. **`TimeAxisManager`** at `src/chart/time_axis_manager.{hpp,cpp}`:
   - Single global time axis state (per decision M8.3)
   - Visible range, anchor (live mode vs paused mode)
   - Notifies registered charts via Qt signals on range change
   - Pan/zoom delegated here; charts react to changes

4. **`SignalSelector`** at `src/chart/signal_selector.{hpp,cpp}`:
   - QWidget-based signal tree panel (per decision M8.2)
   - Hierarchical: top-level grouping by driver ID (with "Derived" group for `expression-engine` virtual driver per M7)
   - Checkbox per signal; check toggles signal in active chart
   - Filter / search by signal ID
   - V1.5+ adds drag-and-drop (deferred per decision M8.2)

5. **`ChartRenderer`** (internal to chart.cpp; not in public API):
   - Scene Graph node creation per signal
   - Vertex buffer updates (incremental on data change)
   - Reuses nodes when signal set unchanged (avoids per-frame node allocation)
   - LOD level selection based on `samples_per_pixel` (per M6 §4.5)

6. **`SignalDisplayMode`** enum and per-type defaults:
   - `Line`: continuous line plot (default for double, int64)
   - `Step`: step plot 0/1 rectangles (default for bool)
   - `Point`: scatter plot (default for QString — single point per timestamp with text annotation)

7. **MainWindow integration**:
   - One central `ChartManager` widget hosting charts
   - Side panel `SignalSelector`
   - Top toolbar with time-axis live/paused toggle, time range presets (1s / 10s / 1min / 10min / 1hr)
   - Status bar shows current frame rate + dropped frame count

8. **Window activation handling** (per [Proto] Anomaly §2 — Mutter throttling):
   - On `show()`, call `window->raise()` + `window->requestActivate()`
   - Detect throttled state by measuring frame interval; if > 50ms sustained, log WARN
   - Documented limitation: when chart window is unfocused under X11/Mutter, redraw rate may degrade

9. **Live time cursor** (per [Proto] Anomaly §1 — Qt 6 RHI frame coalescing):
   - Vertical line at "current time" position, visible on all charts
   - Updates every frame (always changes y-coordinate or position by ≥ 1 pixel)
   - Drives per-frame visible change so RHI swap is not coalesced
   - Cursor position tied to `TimeAxisManager::currentTime()` updated at 30Hz

10. **Pan/zoom interaction**:
    - Click+drag: pan time axis
    - Mouse wheel: zoom time axis (centered on cursor)
    - Right-click: context menu (pause/resume live, snap to recent)
    - Touchpad gesture: not in V1 (V1.5+)

11. **Chart configuration persistence** (minimal V1):
    - Per-chart: visible signals (list of signal IDs), display mode overrides
    - Saved as yaml in `~/.config/signalforge/charts.yaml` (or platform equivalent)
    - Loaded on app start; if file missing, start with empty chart manager

12. **Integration tests** at `tests/integration/`:
    - `test_chart_basic_rendering.cpp` — instantiate Chart, attach 5 signals, push data, verify Scene Graph has expected nodes
    - `test_chart_30hz_sustained.cpp` — 60 signals × 30Hz × 5 second run; verify no dropped frames
    - `test_chart_global_time_axis.cpp` — 3 charts, pan one, all sync
    - `test_chart_lod_selection.cpp` — query at 4 zoom levels, verify correct LOD chosen
    - `test_chart_signal_type_display.cpp` — verify bool→step, double→line, QString→point automatic selection
    - `test_chart_window_activation.cpp` — verify `requestActivate()` called; warn on throttle
    - `test_signal_selector_tree_population.cpp` — registry signals appear in tree with correct grouping

13. **Unit tests** ≥ 80% coverage on chart modules (lower than M5-M7's 85% because UI tests are inherently harder to cover in headless CI; some interactive paths require manual verification)

14. **Benchmark** at `tests/benchmark/bench_chart.cpp`:
    - Reproduces prototype's 4 scenarios as production benchmarks
    - Targets per §5 (prototype + 20% margin)
    - Results to `tests/benchmark/results/M8-baseline.md`
    - Run-to-run variance tracking (3 runs minimum)

15. **Doxygen** on all public declarations

16. **`.claude/M8-done.md`** with standard completion report + freeze record

### 2.2 Must not do

1. **No modifications to M2/M3/M4/M5/M6/M7 frozen `.hpp`**. If freeze-scope change seems needed, HALT.
2. **No drag-and-drop signal selection**. V1 uses checkbox tree (decision M8.2 V1 scope). V1.5+ adds drag.
3. **No multi-time-axis mode**. V1 uses single global axis (decision M8.3). V1.5+ adds per-chart override.
4. **No signal grouping by user-custom categories**. V1 groups by driver ID + Derived. V1.5+ adds custom groups.
5. **No 3D charts, surface plots, FFT view, spectrogram**. V2 territory.
6. **No annotation tools** (markers, measurement cursors, regions). V1.5+ if needed.
7. **No keyboard-driven navigation**. V1 mouse-only. V1.5+ adds vim-style keys / configurable shortcuts.
8. **No print / export to image / PDF**. V1.5+ via M13 packaging.
9. **No QML scene customization API**. V1 charts have fixed visual style. V1.5+ adds theming.
10. **No dependency on M9 Connection Manager UI**. M8 charts use whatever signals are in `SignalBufferRegistry`; how signals get there (driver registration) is M3/M4/M5 territory.
11. **No new top-level dependencies**. Use existing Qt + M6 + M7.
12. **No QML scenes**. V1 charts are pure C++ QQuickItem subclasses + minimal QML wrapper for window setup. Avoids QML compilation surface for performance gating.

---

## 3. Design Decisions (locked by this spec)

Decisions confirmed in pre-M8 planning. M8 implements them as written; does not re-evaluate.

### 3.1 Chart self-driven 30Hz timer (decision M8.1 Option X)

Each `Chart` instance has its own QTimer at 33.33ms (`Qt::PreciseTimer`) that triggers `update()`. The timer is independent of `ExpressionEngine` (M7).

**Rationale**:
- ExpressionEngine is optional (no derived signals → engine not started). Chart redraw must work regardless.
- Decoupled architecture: M8 doesn't depend on M7's lifecycle.
- Chart has its own tick budget; visualization performance isolated from expression evaluation.

**Phase relationship with M7 ExpressionEngine**: Both run at 30Hz but unsynchronized phase. Chart at frame N may show derived signal values from ExpressionEngine tick N-1 (at most 33ms old). Acceptable per M7 spec §3.2 documented latency.

### 3.2 Signal tree + checkbox UI (decision M8.2 Option P)

`SignalSelector` shows all registered signals in a tree:

```
○ Driver: serial-driver-1
  ☑ voltage  (V)
  ☐ current  (A)
  ☑ temperature  (°C)
○ Driver: tcp-driver
  ☐ rpm
○ Derived
  ☑ power  (W)
  ☐ over_temperature
```

Each checkbox toggles the signal in the **active chart** (charted last clicked). Multi-select with Ctrl+click. Drag-and-drop **deferred to V1.5+**.

**Rationale**:
- Familiar pattern for engineering tools (oscilloscopes, MATLAB).
- No drag-drop reduces V1 complexity.
- "Active chart" model allows multiple charts via checkbox without per-chart selector.

### 3.3 Global time axis (decision M8.3 Option T)

`TimeAxisManager` owns single time-range state shared by all charts. User pan/zoom on any chart applies globally.

**Rationale**:
- Engineering scenario: comparing multiple signals at the same moment is core use case. Independent axes break this.
- Reduces UI complexity (one time scrubber, not N).
- V1.5+ may add per-chart "detach axis" toggle if needed.

**State**:
- `visible_start`, `visible_end` (timestamps)
- `live_mode`: bool — if true, `visible_end` advances with current time
- `paused_at`: optional timestamp — if live_mode is false, this anchors `visible_end`

### 3.4 Auto signal display by type (decision M8.4 Option W)

Display mode selected automatically based on `SignalMetadata::type`:

| Type | Default display mode |
|---|---|
| `Bool` | Step (0/1 rectangles) |
| `Int64` | Line |
| `Double` | Line |
| `QString` | Point (scatter with text annotation, height-binned for legibility) |

User may override per-signal in chart context menu (V1) or via config yaml.

**Rationale**:
- Bool as line plot creates 0→1 sloped artifacts; step is clear.
- QString as line is meaningless; point with text is intuitive.
- Defaults reduce user configuration burden.

### 3.5 Performance certification via M8 prototype baseline (decision M8.5 Option PT)

Spec performance thresholds derived from prototype measurements (RESULTS.md Scenarios 1-4) plus 20% margin to account for:
- Production code adds full Signal Selector + MainWindow integration not in prototype
- Optimization opportunities the prototype skipped
- Run-to-run variance on different hardware

**No HALT for "spec gate trip" if measurement is within margin.** ADR amendment only if a performance gap is fundamentally architectural (e.g., > 50% miss).

### 3.6 No soft-HALT (inherits M2-M7)

### 3.7 Metric naming

Per `<module>_<metric>_<scope>` convention:

- `chart_redraws_total` (counter, per-chart)
- `chart_frame_us_<chartId>` (gauge, per-chart)
- `chart_dropped_frames_<chartId>` (counter, per-chart)
- `chart_visible_signals_<chartId>` (gauge, per-chart)
- `chart_manager_active_charts` (gauge, manager-level)
- `chart_lod_level_<chartId>` (gauge, current LOD level: 0/1/2/3)
- `chart_window_throttled` (counter, manager-level: increments when frame interval > 50ms sustained)

---

## 4. Key Implementation Details

### 4.1 `Chart` class

Place at `src/chart/chart.hpp`.

```cpp
// src/chart/chart.hpp
#pragma once

#include "buffer/signal_buffer_registry.hpp"
#include "chart/time_axis_manager.hpp"
#include "decoder/decoder_interface.hpp"

#include <QQuickItem>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <chrono>
#include <memory>

namespace signalforge::chart {

/// Display mode for a signal.
enum class SignalDisplayMode {
    Line,    ///< Default for Double/Int64 — connected line strip
    Step,    ///< Default for Bool — step function (0/1 rectangles)
    Point,   ///< Default for QString — scatter point with annotation
};

/// Per-signal config within a chart.
struct ChartSignalConfig {
    QString signalId;
    SignalDisplayMode displayMode;     ///< Override default; explicit value
    QColor color;                       ///< Optional; auto-assigned if unset
    bool visible = true;                ///< Toggle without removing from chart
};

/// Chart configuration; saved to / loaded from yaml.
struct ChartConfig {
    QString id;                                      ///< Stable identifier
    QString title;                                   ///< Human-readable
    std::vector<ChartSignalConfig> signals;          ///< Visible signals
    std::optional<QString> timeAxisId;              ///< If set, joins a specific axis (V1.5)
};

/// Single chart rendering N signals via Scene Graph.
///
/// Threading: lives on the main thread (QQuickItem). All Scene Graph
/// node manipulation happens on the render thread (Qt manages this);
/// data updates from M6 buffer happen on the main thread via
/// `update()` calls.
///
/// Lifecycle:
/// - Constructed with a SignalBufferRegistry reference + TimeAxisManager
///   reference + initial config
/// - Self-driven 30Hz QTimer triggers update()
/// - On addSignal/removeSignal/setDisplayMode, internal state updates;
///   next update() re-renders
///
/// Freeze scope: this class is frozen at M8 close.
class Chart : public QQuickItem {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Chart)

public:
    explicit Chart(
        signalforge::buffer::SignalBufferRegistry& registry,
        TimeAxisManager& timeAxis,
        ChartConfig config = {},
        QQuickItem* parent = nullptr);
    ~Chart() override;

    /// Add a signal to the chart. Display mode auto-selected by type if not specified.
    void addSignal(const QString& signalId,
                   std::optional<SignalDisplayMode> displayMode = std::nullopt);

    /// Remove a signal.
    void removeSignal(const QString& signalId);

    /// List currently-visible signals.
    [[nodiscard]] QStringList visibleSignals() const;

    /// Set display mode for a specific signal.
    void setDisplayMode(const QString& signalId, SignalDisplayMode mode);

    /// Toggle signal visibility without removing from chart.
    void setSignalVisible(const QString& signalId, bool visible);

    /// Current chart config (for persistence).
    [[nodiscard]] ChartConfig config() const;

    /// Load config (replaces current state).
    void setConfig(ChartConfig config);

    // QQuickItem overrides
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;

    /// Per-tick stats for diagnostics.
    struct FrameStats {
        std::uint64_t totalRedraws = 0;
        std::uint64_t droppedFrames = 0;        ///< Frame interval > 50ms
        std::chrono::microseconds lastRenderUs{0};
        std::chrono::microseconds peakRenderUs{0};
        int currentLodLevel = 0;
    };
    [[nodiscard]] FrameStats stats() const;

signals:
    void signalAdded(const QString& signalId);
    void signalRemoved(const QString& signalId);
    void redrawCompleted(std::uint64_t redrawIndex);

private:
    void onTick();   // 30Hz timer slot

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
```

### 4.2 `ChartManager` class

Place at `src/chart/chart_manager.hpp`.

```cpp
// src/chart/chart_manager.hpp
#pragma once

#include "chart/chart.hpp"
#include "chart/time_axis_manager.hpp"

#include <QObject>
#include <QString>
#include <QStringList>
#include <memory>

namespace signalforge::chart {

/// Manager of N charts plus the global TimeAxisManager.
class ChartManager : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ChartManager)

public:
    explicit ChartManager(
        signalforge::buffer::SignalBufferRegistry& registry,
        QObject* parent = nullptr);
    ~ChartManager() override;

    /// Create a new chart. Returns chart ID.
    [[nodiscard]] QString createChart(const ChartConfig& config = {});

    /// Remove a chart by ID. Returns false if ID not found.
    bool removeChart(const QString& chartId);

    /// Look up chart by ID.
    [[nodiscard]] Chart* chart(const QString& chartId) const;

    /// All chart IDs.
    [[nodiscard]] QStringList chartIds() const;

    /// Active chart (last-clicked, used by SignalSelector).
    [[nodiscard]] QString activeChartId() const;
    void setActiveChartId(const QString& chartId);

    /// Time axis (shared across all charts).
    [[nodiscard]] TimeAxisManager& timeAxis();

    /// Save / load all charts' configs.
    void saveConfigToFile(const QString& path) const;
    [[nodiscard]] bool loadConfigFromFile(const QString& path);

signals:
    void chartCreated(const QString& chartId);
    void chartRemoved(const QString& chartId);
    void activeChartChanged(const QString& chartId);

private:
    signalforge::buffer::SignalBufferRegistry* registry_;
    std::unique_ptr<TimeAxisManager> timeAxis_;
    std::unordered_map<QString, std::unique_ptr<Chart>> charts_;
    QString activeChartId_;
};

}  // namespace signalforge::chart
```

### 4.3 `TimeAxisManager` class

Place at `src/chart/time_axis_manager.hpp`.

```cpp
// src/chart/time_axis_manager.hpp
#pragma once

#include <QObject>
#include <chrono>
#include <optional>

namespace signalforge::chart {

class TimeAxisManager : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TimeAxisManager)

public:
    using TimePoint = std::chrono::steady_clock::time_point;
    using Duration = std::chrono::nanoseconds;

    explicit TimeAxisManager(QObject* parent = nullptr);
    ~TimeAxisManager() override;

    /// Visible range start.
    [[nodiscard]] TimePoint visibleStart() const noexcept;

    /// Visible range end. In live mode, this advances with steady_clock::now().
    [[nodiscard]] TimePoint visibleEnd() const noexcept;

    /// Visible duration (end - start).
    [[nodiscard]] Duration visibleDuration() const noexcept;

    /// Live mode: visibleEnd tracks now().
    [[nodiscard]] bool liveMode() const noexcept;

    /// Pan: shift visible range by offset.
    void pan(Duration offset);

    /// Zoom: scale visible duration around a reference point.
    void zoom(double factor, TimePoint referencePoint);

    /// Pause live mode at current time.
    void pause();

    /// Resume live mode.
    void resume();

    /// Set visible range explicitly (transitions to paused mode).
    void setRange(TimePoint start, TimePoint end);

    /// Preset: 1s, 10s, 1min, 10min, 1hr — sets visibleDuration ending at now() (live mode).
    enum class TimePreset { Sec1, Sec10, Min1, Min10, Hour1 };
    void setPreset(TimePreset preset);

signals:
    void rangeChanged();
    void liveModeChanged(bool live);

private:
    TimePoint visibleStart_;
    TimePoint visibleEnd_;
    bool liveMode_ = true;
    TimePoint pausedAt_;  // valid when !liveMode_
};

}  // namespace signalforge::chart
```

### 4.4 Scene Graph node management

Inside `chart.cpp` `updatePaintNode`:

```cpp
// Pseudocode showing node reuse pattern
QSGNode* Chart::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    if (!oldNode) {
        oldNode = new QSGNode();  // root
    }
    
    // Each visible signal has one geometry node (line strip / step strip / point set)
    // Stored in impl_->signalNodes[signalId]
    
    for (auto& sigConfig : config_.signals) {
        if (!sigConfig.visible) {
            // Hide node if previously rendered
            if (auto it = impl_->signalNodes.find(sigConfig.signalId); 
                it != impl_->signalNodes.end()) {
                it->second->setFlag(QSGNode::Visible, false);
            }
            continue;
        }
        
        auto it = impl_->signalNodes.find(sigConfig.signalId);
        QSGGeometryNode* node;
        if (it == impl_->signalNodes.end()) {
            // First-time render of this signal: create node
            node = createNodeForSignal(sigConfig);
            impl_->signalNodes[sigConfig.signalId] = node;
            oldNode->appendChildNode(node);
        } else {
            node = it->second;
        }
        
        // Update geometry (vertex buffer) with current data
        updateNodeGeometry(node, sigConfig);
    }
    
    // Update time cursor (always changes position)
    updateCursor(oldNode);
    
    return oldNode;
}
```

**Key principle**: nodes persist across frames; only vertex data updates. This avoids per-frame allocation.

### 4.5 Live time cursor (RHI coalescing mitigation)

Per [Proto] Anomaly §1, Qt 6 RHI optimization may skip swap if rendered output is bit-identical. M8 ensures **every frame has visible change**:

```cpp
void Chart::updateCursor(QSGNode* root) {
    auto cursorNode = impl_->cursorNode;  // Persistent
    
    // Always compute current time and update cursor x-position
    auto now = std::chrono::steady_clock::now();
    auto axisStart = timeAxis_->visibleStart();
    auto axisEnd = timeAxis_->visibleEnd();
    auto fraction = static_cast<double>((now - axisStart).count()) /
                    static_cast<double>((axisEnd - axisStart).count());
    
    int cursorX = static_cast<int>(width() * fraction);
    
    // Move cursor by 1 pixel even if no other change
    // This ensures bit-identical-frame coalescing doesn't fire
    if (cursorX != impl_->lastCursorX) {
        cursorNode->setPosition(QPoint(cursorX, 0));
        impl_->lastCursorX = cursorX;
    } else {
        // Edge case: pause + cursor exactly at boundary — alternate sub-pixel
        cursorNode->setPosition(QPoint(cursorX, impl_->cursorYAlternate ? 0 : 1));
        impl_->cursorYAlternate = !impl_->cursorYAlternate;
    }
}
```

This subpixel-flicker fallback handles the static-data + paused-axis edge case where RHI would otherwise coalesce frames.

### 4.6 Window activation handling

In `MainWindow` constructor (or `main.cpp`):

```cpp
window->show();
window->raise();
window->requestActivate();

// Verify activation; warn if not focused
QTimer::singleShot(500, [window]() {
    if (!window->isActive()) {
        SF_LOG_WARN("Chart window not focused; rendering may be throttled by compositor");
    }
});
```

### 4.7 LOD selection per chart redraw

```cpp
auto axisDuration = timeAxis_->visibleDuration();
auto pixelWidth = static_cast<std::size_t>(width());

for (auto& sigConfig : config_.signals) {
    auto* buffer = registry_->bufferFor(sigConfig.signalId);
    if (!buffer) continue;
    
    // Query with target = pixel width; M6 selects appropriate LOD
    auto samples = buffer->queryRange(
        timeAxis_->visibleStart(),
        timeAxis_->visibleEnd(),
        pixelWidth);
    
    // Update chart node geometry with samples
    // ...
}
```

`pixel_width` is the **chart's pixel width on screen**, typically 1500-2000. M6 §4.5 LOD selection logic picks level 0/1/2/3 based on `samples_per_pixel`.

### 4.8 Per-signal type to display mode mapping

In `Chart::addSignal`:

```cpp
SignalDisplayMode autoSelectMode(const QString& signalId) {
    auto* buffer = registry_->bufferFor(signalId);
    if (!buffer) return SignalDisplayMode::Line;
    
    using signalforge::decoder::SignalType;
    switch (buffer->metadata().type) {
        case SignalType::Bool:    return SignalDisplayMode::Step;
        case SignalType::Int64:   return SignalDisplayMode::Line;
        case SignalType::Double:  return SignalDisplayMode::Line;
        case SignalType::String:  return SignalDisplayMode::Point;
    }
    return SignalDisplayMode::Line;
}
```

---

## 5. Performance gates (validated by M8 prototype + 20% margin)

All targets derived from prototype measurements ([Proto]) plus 20% safety margin.

### 5.1 Frame timing

| Metric | Prototype actual | M8 target (proto + 20%) | HALT |
|---|---|---|---|
| p99 frame interval (vsync-bound) | 33.5 ms | < 35 ms | > 50 ms |
| p99 render-loop cost | 0.60 ms | < 1.0 ms | > 5 ms |
| Mean dropped frames over 1000-frame run | 0 / 1000 | 0 / 1000 | > 5 / 1000 |

Frame interval is vsync-bound (33ms at 30Hz on 60Hz display). Render-loop cost is the actual SG work; this is what scales with signal count.

### 5.2 Capacity targets

| Metric | Prototype actual | M8 target | HALT |
|---|---|---|---|
| Visible signals per chart sustained 30Hz | 60 (Scenario 1, 2) | ≥ 60 | < 40 |
| Concurrent charts sustained 30Hz | 3 (Scenario 3) | ≥ 3 | < 2 |
| Total signals across charts | 60 (Scenario 3) | ≥ 60 | < 40 |
| Soft cap on visible signals per chart | (legibility, not perf) | 100 | — |

V1 ships with a soft cap of 100 signals per chart, enforced as warning (not error). Above 100, legibility (not rendering) is the limit.

### 5.3 LOD performance

| Metric | Prototype actual | M8 target | HALT |
|---|---|---|---|
| Single chart query at 4 zoom levels | < 130µs each | < 200µs | > 500µs |
| LOD switch latency | < 130µs | < 200µs | > 1ms |

### 5.4 Pan/zoom interaction

| Metric | Prototype actual | M8 target | HALT |
|---|---|---|---|
| Frame interval std dev during continuous pan | 1.34 ms | < 2 ms | > 5 ms |
| Frame interval p99 during pan | 33.07 ms | < 35 ms | > 50 ms |

### 5.5 Run-to-run variance

3-run mean variance for any metric < 5% (per M6 / M7 convention).

### 5.6 1-hour soak test

Run M8 with 60 signals × 1kHz updates × 30Hz redraw for 1 hour.
- Memory: no growth > 10% across the hour
- Dropped frames: < 50 across the hour (~108k frames)
- ASan / LSan clean

---

## 6. Freeze protocol

### 6.1 What freezes at M8 close

**C++ interfaces**:
- `src/chart/chart.hpp`: `Chart` class, `ChartConfig` struct, `ChartSignalConfig` struct, `SignalDisplayMode` enum, `Chart::FrameStats` struct
- `src/chart/chart_manager.hpp`: `ChartManager` class
- `src/chart/time_axis_manager.hpp`: `TimeAxisManager` class, `TimePreset` enum
- `src/chart/signal_selector.hpp`: `SignalSelector` widget public API

**Configuration schema**:
- `~/.config/signalforge/charts.yaml` schema (top-level keys + per-chart keys + per-signal keys)

Modifications require new ADR.

### 6.2 What does NOT freeze

- Internal `Chart::Impl` PIMPL layout
- Scene Graph node structures (implementation detail)
- Default colors / theming (changeable without ADR)
- Pan/zoom mouse mappings (configurable)
- LOD selection thresholds beyond what M6 §4.5 sets

### 6.3 Freeze record format

`.claude/M8-done.md` includes sha256 of the 4 frozen `.hpp` files + the canonical `charts.yaml` schema example.

---

## 7. M8-specific HALT triggers

Beyond CLAUDE.md §HALT:

1. **Modification to M2/M3/M4/M5/M6/M7 frozen `.hpp`** → HALT.
2. **30Hz redraw not sustained** at 60 signals × 1 chart (matches prototype Scenario 1 baseline) → HALT after one optimization pass.
3. **Window throttling not detectable** (no metric increment when chart is unfocused under Mutter) → HALT (mitigation incomplete).
4. **Live cursor causes RHI to drop frames** (cursor update doesn't reliably keep frames distinct) → HALT.
5. **LOD level wrong** (chart queries LOD 0 when LOD 3 expected, or vice versa) → HALT (M6 integration broken).
6. **Memory leak in 1-hour soak** (>10% growth) → HALT.
7. **Global time axis sync not working** (pan one chart, others don't follow) → HALT.

---

## 8. Acceptance criteria

### 8.1 Build and test

- [ ] Debug, Release, debug-asan all build clean under C++23 (GCC 13)
- [ ] All unit + integration tests pass under all three presets
- [ ] Coverage ≥ 80% per §2.1-13
- [ ] CI green on milestone/M8 head

### 8.2 Performance (per §5)

- [ ] Frame timing all gates met in prototype-equivalent benchmarks
- [ ] Capacity targets met (60 signals × 1 chart, 3 charts × 20 signals)
- [ ] LOD performance met
- [ ] Pan/zoom variance met
- [ ] Run-to-run variance < 5%
- [ ] 1-hour soak: memory bounded, no leaks (CI debug-asan)

### 8.3 Functional correctness

- [ ] Bool signals display as step plots
- [ ] Double / Int64 signals display as line plots
- [ ] QString signals display as points with annotations
- [ ] Global time axis sync verified across 3 charts
- [ ] Window activation: `requestActivate()` called, throttle detection works
- [ ] Live cursor: RHI does not coalesce frames

### 8.4 Configuration persistence

- [ ] `charts.yaml` saves and loads roundtrip
- [ ] Missing config file: graceful degradation (empty chart manager)
- [ ] Invalid config: log ERROR, start with defaults

### 8.5 Freeze record

- [ ] M8-done.md has Freezes section per §6.3
- [ ] Sha256s recorded for 4 hpp + 1 schema example
- [ ] No modifications to M2-M7 frozen files

### 8.6 Hand-off

- [ ] M8-done.md hand-off section covers:
  - For M9 Connection Manager: charts already work; M9 adds the UI for managing connections (drivers)
  - For M10 Session Writer: charts read from registry; M10 writes from registry; no chart change needed
  - For M11 Replay: replay populates registry; charts unchanged
  - For M12 Performance: chart redraw cost is dominant production UI cost; profile candidates listed

---

## 9. Notes for CC

- **Prototype is reference, not copy**. The prototype code at `tools/m8_prototype/` is a measurement artifact. Production M8 reuses ideas (Chart class, Scene Graph node management, LOD integration) but rewrites cleanly. Don't import prototype code wholesale; treat it as a reference.

- **Live cursor is functional, not optional**. Per §4.5, the cursor is what keeps Qt RHI from coalescing frames. Without it, even live data updates may not trigger frame rendering. The "subpixel alternate" fallback handles the paused-axis static-data case.

- **Window activation must be explicit**. Mutter on X11 throttles unfocused windows aggressively (per [Proto] Anomaly §2). `requestActivate()` after `show()` is mandatory, not optional.

- **LOD level selection is M6's responsibility**. Don't compute LOD level in chart code. Pass `target_sample_count` to `queryRange`; M6 returns appropriate LOD-decimated samples. This keeps chart code simple and ensures consistency with M11 Replay (which also queries M6).

- **30Hz redraw timer must use `Qt::PreciseTimer`**. Default coarse timer has ~16ms jitter; this would push p99 frame interval over the spec gate. PreciseTimer gives ±1ms.

- **ASan in CI catches what local can't**. Per the M5 / M6 / M7 convention, AppProtection.so blocks local ASan; CI is authoritative. Run any leak suspect through CI debug-asan.

- **Don't optimize before measuring**. Strategy "measure first, optimize only on miss" applies. Spec §5 has 20% margin; if first measurement is at 80% of margin, that's fine, don't over-tune.

- **Signal selector population is registry-driven**. `SignalSelector` calls `registry_->signalIds()` and groups by `bufferFor(id)->metadata().driverId`. As decoders register / unregister signals, the tree updates via Qt signal/slot.

---

## 10. Closing note

M8 brings SignalForge to its first user-visible state. Charts on screen, signals scrolling, pan/zoom working. From a user's perspective, this is when SignalForge "becomes a tool."

The prototype validation makes M8 implementation predictable. The 60+ signal × 30 Hz × 3+ chart capacity is established as feasible on commodity hardware (AMD Ryzen 7 5800H + radeonsi). Implementation challenges are about correctness and integration polish, not architectural feasibility.

When in doubt about a design choice between simplicity (V1 minimal) and feature richness, choose simplicity. V1.5+ exists for the things M8 deliberately defers (drag-drop, multi-axis, theming, keyboard shortcuts, annotations).

The implementation's primary risks are:
1. Live cursor edge cases triggering RHI coalescing
2. Memory growth in long-running charts (LOD bin retention, Scene Graph node leaks)
3. Window activation interaction with Mutter / KWin / other compositors

Each has a HALT trigger in §7.
