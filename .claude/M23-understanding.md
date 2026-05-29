# M23 — Dashboard P2 (understanding)

## Source

Executes **P2** of `docs/v0.3/dashboard-interaction-design.md`: a **readable plot** —
Y-axis tick labels + units, X-axis time labels, in-canvas colored legend, configurable line
width (≥2 px), per-signal-Y / shared-normalized option, theme-aware colors. Continues M22.
Branched off `milestone/M22`.

## Approach (decision)

Build a **new `PlotView` QWidget that paints with `QPainter`**, parallel to the frozen legacy
`chart::Chart` (which is left untouched per DR-001 §9 #1). Rationale:
- Qt-painting a QWidget is simple, theme-aware, and **headless-capturable** — fixing the P0
  offscreen blank-plot problem (a `QQuickWidget`'s RHI framebuffer doesn't land in
  `QWidget::grab()`; see the M14 §F4 saga). All other panels are QWidgets, so this is consistent.
- The new plot reads `SignalBuffer::queryRange()` directly over a shared `TimeAxisManager`
  window (frozen M8 class, used standalone). No ChartManager needed.

`PlotPanel` is reworked to host a `PlotView`; the `Dashboard` owns one shared `TimeAxisManager`
(driven by the toolbar Live/preset). The legacy `ChartManager`/`Chart` are **dropped from the
app's plot path** (still in the chart lib with their own tests; can be deleted later).

## What P2 delivers vs. the legacy chart

| Gap (DR-001 #10) | Legacy chart | New PlotView |
|---|---|---|
| Y tick labels + values + unit | none | yes |
| X time tick labels | none | yes |
| in-canvas colored legend | grey header text | yes |
| line width | 1 px hardcoded | ≥2 px, configurable |
| Y scaling | one shared auto Y | shared **or** per-signal normalized |
| colors | `tokens::light` hardcoded | theme-aware palette |
| headless screenshot | blank (RHI) | renders (QPainter) |

## Key facts

- Data: `SignalBuffer::queryRange(start, end, targetPx)` (LOD-decimated) over
  `TimeAxisManager::visibleStart()/visibleEnd()`. Same 100-sample publish caveat (C1, next).
- `TimeAxisManager`: live/pause/pan/zoom/preset + `rangeChanged` — repaint on it.
- MainWindow toolbar Live toggle + time-preset combo now drive the dashboard's TimeAxisManager
  (previously ChartManager's). Smoke Tier A (`grabChartImage`) re-pointed to grab the PlotView.

## Decisions (autonomous, logged)

- **D1** New plot = QPainter QWidget (`PlotView`), not a QQuickItem — testable + capturable.
- **D2** Drop `ChartManager` from the app/dashboard plot path; `Dashboard` owns the shared
  `TimeAxisManager`. Legacy `Chart`/`ChartManager` remain in the chart lib (unused by app).
- **D3** Default Y = shared auto-range; per-signal normalize is an opt-in toggle (V1 default off).
- **D4** Theme-aware fixed palette (distinct saturated colors legible on dark + light).

## DoD

`PlotView` test ≥70% public surface; Debug+Release build & ctest green; clang-format clean;
clang-tidy matches baseline; Doxygen; progress current; local commits (no push). GUI smoke
Tier A must still pass (now via PlotView pixels) + Tier D (clean exit).
