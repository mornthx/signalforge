# M30 — Parsed-signals view + filter engine (closure report)

Phase B of the three-tier recovery ([DR-002](direction-review/DR-002-2026-05-30-three-tier-workbench.md)).
Branch `milestone/M30` (local, off M29; **not pushed**).

## Delivered

- **S1 — `signalforge_query`** (`5533284`): a reusable Wireshark-style display-filter engine
  (`FilterExpr::parse` → `matches(FieldLookup)`), grammar `field OP value` with `&& || !` + parens,
  parse errors with position. Leaf module, 10 cases / 83 assertions.
- **S2 — `signalforge_inspect`** (`1863c4c`): `ParsedSignalsView` — Tier 2 (解析数据) live table of every
  decoded signal (Name · Source · Value · Unit · Type · Age) with a filter bar over the S1 engine.
  3 cases / 12 assertions simulating filter typing.
- **S3 — tabbed workspace** (`f7b66fe`): `main_window` center is a `QTabWidget` with **Parsed** (default)
  + **Dashboard**; adding a panel switches to Dashboard. Report 4 ("默认应该首先显示原始数据；dashboard 是在
  原始数据基础上的页面") is now structurally satisfied.

## Reusable core for M31

The filter engine is the load-bearing piece: M31's raw-packet view reuses `FilterExpr` verbatim over
frame fields. The tabbed shell takes a third **Raw** tab additively.

## Verification

- 697/697 ctest Debug + Release. One visual baseline (`00-empty-launch`) re-accepted (tab bar added);
  other 10 unchanged. clang-format clean. No frozen interface or schema touched (all-new modules +
  additive main_window wiring).

## Deviations and concerns

- **Onboarding placement:** the guided empty-state panel now sits on the Dashboard tab, not the default
  Parsed tab. Reachable via the Connections dock + menus. Flagged for the owner — may want an onboarding
  hint on the Parsed tab.
- **Freshness:** the Age column is derived from `LatestValue.age`; the architecture's richer
  GOOD/STALE/UNCERTAIN/BAD quality enum is noted for a later pass.
- **MainWindow tab state** is not unit-tested (heavy construction); covered by integration smoke + the
  empty-launch visual baseline.

## Status

Local on `milestone/M30`. Chain: `main → M21 … → M29 → M30`. Next: **M31 (Phase C)** — Wireshark
raw-packet view (Tier 1).
