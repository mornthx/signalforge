# M23 — Dashboard P2 (closure report)

Branch: `milestone/M23` (local, off `milestone/M22`; **not pushed**). Implements P2 of
`docs/v0.3/dashboard-interaction-design.md` — the readable plot.

## What shipped

A new `PlotView` (QWidget/QPainter) replaces the QQuickWidget+legacy-`Chart` plot:

| DR-001 #10 gap | Now |
|---|---|
| Y tick labels + values + unit | ✅ (e.g. 30.49 … 19.5, unit "C") |
| X time tick labels | ✅ (−10s … 0, relative) |
| in-canvas colored legend | ✅ (swatch + name per signal) |
| line width | ✅ ≥2 px |
| Y scaling | ✅ shared auto / explicit / per-signal-normalized |
| theme-aware colors | ✅ fixed legible palette |
| headless screenshot | ✅ renders (QPainter) — fixes the P0 blank-plot |

| Deliverable | Where | Tests |
|---|---|---|
| `PlotView` QPainter plot | `src/dashboard/plot_view.*` | plot_view_test (3) |
| `PlotPanel` hosts `PlotView`; multi-signal | `plot_panel.*` | plot_panel_test (2) |
| `Dashboard` owns a shared `TimeAxisManager`; `ChartManager` dropped from app | `dashboard.*` | dashboard_test (4) |
| MainWindow rewire (toolbar→dashboard axis, hooks→PlotView, drop chartManager_) | `main_window.*` | full ctest + GUI smoke |

Commits: `(plan)` → `4d8a4dd` S1 → `4c8f575` S2+S3 → (this) close-out.

## Verification

- Debug + Release build green; dashboard unit tests green.
- GUI release smoke: **Tier A** now via `PlotView` pixels (QWidget::grab), **Tier D** clean exit.
- Live visual (/tmp): large readable temperature sine — Y values + unit "C", X time labels,
  legend, fills the panel; rc=0.
- clang-format clean.
- Visual baselines showing the central plot rebaselined (blank QQuickWidget → rendered plot).

## Decisions / notes

- **D1** plot = QPainter `PlotView` (testable + capturable, fixes P0 offscreen blank).
- **D2** dropped `ChartManager` from the app; Dashboard owns the `TimeAxisManager`. Legacy
  `Chart`/`ChartManager` remain in the chart lib (unused by app; their tests still pass).
- **Bug fixed mid-S3:** the vertical-fill relayout initially hung the event loop via a
  `QGridLayout::rowCount()` runaway in the stretch-reset loop; switched to a bounded reset.
- **C1-adjacent:** `PlotView` falls back to undecimated `queryRange` when the LOD-decimated
  query returns empty (sparse/recent data) — more robust than the legacy chart.

No frozen interface modified (`Chart`/`ChartManager`/`TimeAxisManager`/`SignalSelector`/
`SignalMetadata` untouched; `TimeAxisManager` used via its public API).

## Status

P2 implemented and green on `milestone/M23` (local, unpushed). Next: P3 (Bar/Gauge), then C1
(buffer slow-signal publish latency). Chain: `main → M21 → M22 → M23`.
