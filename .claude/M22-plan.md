# M22 — Dashboard P1 (plan)

Builds in `src/dashboard/` (existing `signalforge_dashboard` lib). Each subtask ends green
(Debug + Release build + ctest) and is its own local commit on `milestone/M22` (no push).

## S1 — TablePanel + Panel base virtuals

- `panel.hpp`: add virtual `addSignal(QString)` / `removeSignal(QString)` (default no-op),
  `isMultiSignal()` (default false), `detachChart()` (default no-op).
- `panel_types`: add `PanelType::Table` + name round-trip.
- `plot_panel`: mark `addSignal`/`removeSignal`/`detachChart` `override`; `isMultiSignal()`→true.
- New `table_panel.{hpp,cpp}`: `TablePanel : Panel`, `QTableWidget` with columns
  Signal/Value/Unit/Updated; rebuild rows on add/removeSignal; `refresh()` fills Value+Updated
  from `queryLatestOne()`; `isWide()`→true; test accessors `rowCount()`, `valueTextFor(id)`.
- Tests: `table_panel_test.cpp` — add signals → rows; refresh shows value/unit/age; removeSignal
  drops a row; no-data shows "—".
- Commit: `dashboard: add table panel`

## S2 — Dashboard integration (polymorphic, no downcasts)

- `dashboard`: `addTablePanel(QStringList)`; `addPanel` handles `PanelType::Table`; refactor
  `removePanel` (use virtual `detachChart()` + `plotChartIds_` map) and `removeSignalEverywhere`
  (use `isMultiSignal()` → `removeSignal` else `removePanel`) — drop the `static_cast`s.
- Tests: extend `dashboard_test.cpp` — addTablePanel makes a wide Table hosting N signals;
  unticking a tabled signal drops its row not the panel; panel count.
- Commit: `dashboard: wire table panel into dashboard`

## S3 — MainWindow `+ Table` + close-out

- `main_window`: toolbar `+ Table` action → `dashboard_->addTablePanel(registry signalIds)`;
  `onAddTable` slot. (Keeps `+ Plot`.)
- clang-format + clang-tidy pass; Doxygen; update `.claude/M22-progress.md`, `M22-done.md`.
- Full Debug + Release ctest green; GUI smoke green (incl. Tier D).
- Commit: `app: add table panel toolbar action` + `dashboard: M22 P1 close-out`

## Risks / HALT awareness

- No frozen interface touched (Table is new; refactor is on M21 code).
- Compile/test fail ×3 on a subtask → HALT per CLAUDE.md.
