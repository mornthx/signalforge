# M21 — Dashboard P0 (plan)

New module `src/dashboard/` → static lib `signalforge_dashboard` (links Qt Core/Gui/Widgets/
Quick, signalforge_chart, signalforge_buffer, signalforge_decoder, signalforge_observability).
Tests under `tests/unit/dashboard/`. Each subtask ends green (Debug+Release build + ctest) and
is its own commit. Commits are local on `milestone/M21` (no push without authorization).

## Subtask S1 — Panel abstraction + factory (no UI wiring)

Files: `src/dashboard/panel_types.hpp` (PanelType enum, PanelConfig struct),
`src/dashboard/panel.{hpp,cpp}` (abstract `Panel : QFrame` — header w/ title + remove button,
body slot, `config()`, `setSignals()`, `removeRequested`/`configChanged` signals, virtual
`refresh()`), `src/dashboard/panel_factory.{hpp,cpp}` (`suggestPanelType(SignalType)` +
`createPanel(PanelConfig, registry, chartManager*)`).
Tests: `panel_factory_test.cpp` — suggest mapping (Bool/String→State, Int64/Double→Numeric);
PanelConfig defaults.
Commit: `dashboard: add panel abstraction and factory`

## Subtask S2 — NumericPanel + StatePanel

Files: `numeric_panel.{hpp,cpp}`, `state_panel.{hpp,cpp}`.
- NumericPanel: single signal; `refresh()` reads `queryLatestOne()`, formats value+unit
  (unitOverride else metadata.unit; decimals from config), tracks observed min/max, shows
  "no data" when empty/stale.
- StatePanel: bool → ●/○ + true/false label (color when true if configured); string → verbatim;
  numeric → rounded label. `refresh()` from `queryLatestOne()`.
Tests: `numeric_panel_test.cpp`, `state_panel_test.cpp` — push samples into a registry buffer,
call refresh(), assert displayed text / observed min-max via test accessors.
Commit: `dashboard: add numeric and state panels`

## Subtask S3 — PlotPanel (wrap legacy Chart)

Files: `plot_panel.{hpp,cpp}`. Embeds a `QQuickWidget` (ChartHost.qml), takes a `Chart*` (created
by caller via ChartManager), parents chart to root, syncs size — logic lifted from
`MainWindow::rebuildChartWidgets`. `refresh()` no-op (Chart self-drives). `addSignal/removeSignal`
delegate to the Chart.
Tests: `plot_panel_test.cpp` — construct with a Chart, add/remove signal reflected in
`chart->visibleSignals()`; QQuickWidget child present. (offscreen QPA)
Commit: `dashboard: add plot panel wrapping legacy chart`

## Subtask S4 — Dashboard container

Files: `dashboard.{hpp,cpp}` (`Dashboard : QWidget`). Owns reflow grid (default 3 cols; Plot
spans full width), a ~15 Hz refresh QTimer ticking all panels, and API: `addPanel(PanelConfig)`,
`addSignal(signalId)` (auto-suggest → reuse existing panel for the signal or create one),
`removeSignalEverywhere(signalId)`, `removePanel(id)`, `panelIds()`, `setEditMode(bool)`.
Holds refs to `SignalBufferRegistry&` + `ChartManager&` (for PlotPanel chart creation).
Tests: `dashboard_test.cpp` — addSignal(bool)→State panel exists; addSignal(double)→Numeric;
remove; panel count; idempotent re-add.
Commit: `dashboard: add dashboard grid container with auto-suggest`

## Subtask S5 — MainWindow integration

- Replace `chartContainer_`/`chartLayout_`/`rebuildChartWidgets`/`onAddChart` chart-stack with a
  `Dashboard` placed in `centralSplitter_` (keep empty-state frame above it).
- SignalSelector toggle routes to `dashboard_->addSignal/removeSignalEverywhere` (replaces
  active-chart routing; keep ChartManager for PlotPanel backing).
- Toolbar `+ Chart` → `+ Panel` (adds an empty plot panel, preserving legacy behavior).
- Re-point visual-test hooks (`autoSelectSignal`, `autoAddCharts`, `grabChartImage`,
  `updateEmptyStateVisibility`) at the dashboard. `grabChartImage` still finds the QQuickWidget
  via `findChildren` since PlotPanel embeds one.
Tests: extend/adjust affected unit tests; run full ctest.
Commit: `app: mount dashboard as central workbench surface`

## Subtask S6 — Close-out

- `clang-format -Werror` on all changed files; Doxygen pass on public decls.
- Update `.claude/M21-progress.md`, `.claude/M21-concerns.md`, mark DR-001 progress note.
- Full Debug + Release ctest green.
Commit: `dashboard: M21 P0 close-out (docs + format)`

## Risks / HALT awareness

- AUTOMOC + Qt `signals` keyword: avoid a field named `signals` (chart.hpp C1 precedent — use
  `signalIds`).
- If MainWindow integration cascades into the frozen `Chart`/`ChartManager` signatures → STOP
  (HALT #4): the design is additive; reuse only their public API.
- Compile fail ×3 / test fail ×3 on one subtask → HALT per CLAUDE.md.
- ≤800 net lines honored by splitting across S1–S6 commits.
