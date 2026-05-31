# M24 — Dashboard P3 (understanding)

## Source

Executes **P3** of `docs/v0.3/dashboard-interaction-design.md`: **Bar and Gauge** panels — a
scalar shown against a range. Per design §2.1 the range is **per-panel config**: explicit
min/max if set, else the signal's **observed** min/max (no frozen `SignalMetadata` change).
Continues M23; branched off `milestone/M23`.

## What P3 delivers

1. `BarPanel` — horizontal bar filling (value−min)/(max−min) of the track, with value+unit and
   end labels.
2. `GaugePanel` — 180° arc gauge with a needle, value+unit readout.
3. `PanelType::Bar` / `PanelType::Gauge`; `Dashboard::addBarPanel(signalId)` /
   `addGaugePanel(signalId)`; toolbar `+ Bar` / `+ Gauge` entries.
4. Both single-signal QWidgets painted with `QPainter` (testable + headless-capturable),
   sharing one inner `MeterView` renderer.

## Decisions (autonomous, logged)

- **D1** Bar/Gauge are **opt-in** (toolbar), not auto-suggested — rate/range unknown when a
  signal is first ticked, so the zero-config default stays Numeric (M21 D1). Toolbar `+ Bar`/
  `+ Gauge` bind to the **first registered signal** (matching the `+ Table`-tables-all D1
  pragmatism); per-panel signal/type selection is a later follow-up.
- **D2** Range = per-panel `rangeMin`/`rangeMax` (config §2.1); unset → observed running
  min/max. A fixed explicit range keeps the bar/gauge scale stable.
- **D3** One shared `MeterView` (QWidget/QPainter, Bar|Gauge style) hosted by both panels.

## Key facts

- Value + staleness: `SignalBuffer::queryLatestOne()` (same as Numeric/State; C1 publish-cadence
  caveat applies — fixed next in M25).
- `Dashboard::showsSignal` already covers any panel via `hasSignal`, so the signal-list
  checkbox reflects Bar/Gauge membership for free.

## DoD

`BarPanel`/`GaugePanel` tests ≥70% public surface; Debug+Release build & ctest green;
clang-format clean; clang-tidy matches baseline; Doxygen; progress current; local commits
(no push). GUI smoke Tier A + Tier D still green.
