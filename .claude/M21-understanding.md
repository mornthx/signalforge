# M21 — Dashboard P0 (understanding)

## Source

This milestone executes **P0** of `docs/v0.3/dashboard-interaction-design.md`, which itself
answers direction review `.claude/direction-review/DR-001-2026-05-29-ui-visualization.md`.
DR-001 found the product is a single line-chart shell that cannot show a current value, a
boolean/enum state, or a unit — the workbench basics. P0 fixes the biggest gap.

## What P0 delivers

1. A `Dashboard` that owns a **grid of heterogeneous panels** (replaces the vertical stack of
   identical charts in `MainWindow`).
2. A `Panel` abstraction + per-panel config (zero-config default, per-panel override of
   display mode / min-max / unit — design §2.1).
3. **NumericPanel** (big value + unit + observed min/max) and **StatePanel** (●/○ + label for
   bool, verbatim for string) — the new "see a value / see a state" capability.
4. **PlotPanel**: a *new* class wrapping the legacy `Chart` (hosted in a `QQuickWidget`) so the
   existing trend view keeps working inside the grid. The frozen M8 `Chart`/`ChartManager` are
   **reused unmodified** (no freeze break — decision DR-001 §9 #1).
5. Selector → dashboard routing with **auto-suggest** of panel type from `SignalType`, plus a
   toolbar "+ Panel" affordance.

Out of scope (later phases): Table panel (P1), the richer PlotPanel axes/labels/legend (P2),
Bar/Gauge (P3), menu-bar & status-bar IA rewrite (separate DR-001 work).

## Key facts grounded in code

- Current value source: `SignalBuffer::queryLatestOne()` → `optional<LatestValue{value,
  timestamp, age}}` (signal_buffer.hpp). No new buffer API needed.
- Type for auto-suggest: `SignalMetadata.type` ∈ {Bool, Int64, Double, String}; unit in
  `.unit`; name in `.name` (decoder_interface.hpp). `SignalMetadata` is **frozen at M5** — so
  per-panel range lives in panel config, never in metadata.
- Plot reuse: `ChartManager::createChart()/chart(id)/removeChart(id)/timeAxis()` (frozen M8).
  The QQuickWidget hosting currently in `MainWindow::rebuildChartWidgets` moves into PlotPanel.
- Panels are plain `QWidget`/`QFrame` (Numeric/State are label-based; only PlotPanel embeds a
  QQuickWidget). Simpler + headless-testable than all-QML.

## Decisions (made autonomously per session directive; flagged for end review)

- **D1 — auto-suggest default for scalars = Numeric** (not Plot). Rationale: P0's purpose is to
  surface the missing "current value" view; rate is unknown at suggest time. The user can
  switch a panel to Plot. *Behavioral change*: checking a scalar signal now yields a number
  card, not a chart. Logged in `.claude/M21-concerns.md` for review.
- **D2 — panels are QWidgets, not QQuickItems.** Keeps Numeric/State trivial and unit-testable
  without a QML scene; PlotPanel still hosts the QQuickWidget+Chart.
- **D3 — keep legacy `ChartManager` alive** as PlotPanel's backing store, so existing MainWindow
  visual-test hooks (`grabChartImage`, `autoSelectSignal`, `autoAddCharts`) keep functioning
  with minimal churn.
- **D4 — Dashboard refresh tick** at ~15 Hz drives Numeric/State `refresh()`; PlotPanel keeps
  the Chart's own 30 Hz self-drive (refresh() is a no-op for it).

## Definition of done (per CLAUDE.md)

New modules under `src/dashboard/` each get a `tests/unit/dashboard/*` with ≥70% public-surface
coverage; Debug + Release build & ctest green; clang-format clean; Doxygen on public decls;
`.claude/M21-progress.md` kept current; conforming commits. ASan is CI-authoritative (local
host blocked by `/etc/ld.so.preload` AppProtection — see [[host_asan_preload]] / M21-concerns).
