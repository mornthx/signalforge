# M24 — Dashboard P3 (plan)

Local commits on `milestone/M24` (no push). Each subtask green (Debug+Release + ctest).

## S1 — MeterView + BarPanel + GaugePanel

- `meter_view.{hpp,cpp}` — `MeterView : QWidget` with `Style{Bar,Gauge}`; `setValue(v,hasData)`,
  `setRange(lo,hi)`, `setUnit(u)`; `paintEvent` draws a horizontal bar or a 180° arc+needle,
  theme-aware, with value+unit text + end labels.
- `meter_panel.{hpp,cpp}` — `BarPanel` / `GaugePanel : Panel`, single signal; `refresh()` reads
  `queryLatestOne`, computes range (explicit `config_.rangeMin/Max` else observed), pushes to the
  MeterView; test accessors `displayValue()`, `rangeMin()/rangeMax()`.
- `panel_types`: `PanelType::Bar`, `PanelType::Gauge` + name round-trip.
- Tests `meter_panel_test.cpp`: value + observed range; explicit range pins; no-data state;
  headless render non-empty.
- Commit: `dashboard: add bar and gauge panels`

## S2 — Dashboard + MainWindow integration

- `dashboard`: `addBarPanel(signalId)` / `addGaugePanel(signalId)`; `addPanel` handles Bar/Gauge.
- `main_window`: toolbar `+ Bar` / `+ Gauge` → bind to first registered signal; `onAddBar`/
  `onAddGauge` slots; `--auto-add-bar`/`--auto-add-gauge` harness flags for screenshots.
- Tests: extend `dashboard_test` (addBarPanel/addGaugePanel create the right type, single-signal).
- Verify GUI smoke; capture a Bar+Gauge screenshot.
- clang-format/tidy/Doxygen; `M24-progress.md`, `M24-done.md`; full ctest.
- Commits: `app: add bar/gauge toolbar actions` + `dashboard: M24 P3 close-out`

## Risks / HALT

- No frozen interface touched (new panels; range in panel config, not SignalMetadata).
- Compile/test fail ×3 on a subtask → HALT.
