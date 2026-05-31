# M23 — Dashboard P2 (plan)

Local commits on `milestone/M23` (no push). Each subtask ends green (Debug+Release + ctest).

## S1 — PlotView widget (QPainter)

New `src/dashboard/plot_view.{hpp,cpp}` — `PlotView : QWidget`:
- ctor(`SignalBufferRegistry&`, `TimeAxisManager&`).
- `addSignal(id)` / `removeSignal(id)` / `signalIds()`; auto color from a theme-aware palette.
- `setPerSignalNormalize(bool)`; `setYRange(opt<double> min, max)` (unset = observed).
- `paintEvent`: margins for Y labels (left) + X labels (bottom) + legend (top); grid; Y ticks
  (~5) with values + unit; X ticks with relative-time labels; per-signal polyline from
  `queryRange(start,end,plotWidthPx)`, pen width 2, signal color; "now" cursor; legend swatches.
- repaint on `TimeAxisManager::rangeChanged` + an external refresh() (dashboard tick).
- Test accessors: `colorFor(id)`, `computedYRange()`, `hasData()`.
Tests `plot_view_test.cpp`: add/remove/signalIds; distinct colors; computedYRange (explicit +
observed after pushing ≥100 samples); `grab()` image non-empty + non-uniform after data.
Commit: `dashboard: add QPainter plot view`

## S2 — PlotPanel hosts PlotView; Dashboard owns the time axis

- `dashboard`: own a `TimeAxisManager timeAxis_`; expose `timeAxis()`. `addPanel(Plot)` builds a
  `PlotPanel(config, registry, timeAxis_)`. Drop `chartManager_` member + `plotChartIds_` +
  `<chart/chart_manager.hpp>` include. `removePanel` no longer special-cases charts.
- `plot_panel`: rework to host a `PlotView` (ctor takes registry + TimeAxisManager); add/remove
  delegate to the view; drop the Chart/QQuickWidget/QPointer machinery.
- `Dashboard` ctor signature changes to `(SignalBufferRegistry&, QWidget*)` (no ChartManager).
- Tests: update dashboard_test + plot_panel_test for the new ctors (no ChartManager).
Commit: `dashboard: back plot panels with PlotView and a shared time axis`

## S3 — MainWindow rewire + close-out

- `main_window`: drop `chartManager_`; `Dashboard(*signalBufferRegistry_, ...)`. Toolbar Live
  toggle + time-preset drive `dashboard_->timeAxis()`. `grabChartImage()` → grab the first
  `PlotView`. `autoSelectSignal`/`autoAddCharts` via dashboard. `refreshStatusBar` chart-stats
  loop → simple "live/idle" (no ChartManager). Keep legacy chart lib linked (tests).
- Verify GUI smoke Tier A (PlotView pixels) + Tier D; rebaseline visual states that show a plot.
- clang-format/tidy/Doxygen; `M23-progress.md`, `M23-done.md`; full ctest.
Commits: `app: drive dashboard plots via PlotView` + `dashboard: M23 P2 close-out`

## Risks / HALT

- No frozen interface modified (PlotView new; TimeAxisManager used via public API; Chart/
  ChartManager untouched, just unused).
- Smoke Tier A depends on grabChartImage → must re-point to PlotView and still yield non-white
  pixels. Validate explicitly.
- Compile/test fail ×3 on a subtask → HALT.
