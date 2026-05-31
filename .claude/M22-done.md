# M22 — Dashboard P1 (closure report)

Branch: `milestone/M22` (local, off `milestone/M21`; **not pushed**). Implements P1 of
`docs/v0.3/dashboard-interaction-design.md` — the Table/DataGrid panel. Continues P0 (M21).

## What shipped

| Deliverable | Where | Tests |
|---|---|---|
| `TablePanel` — multi-signal card, rows of Signal/Value/Unit/Updated, from `queryLatestOne` | `src/dashboard/table_panel.*` | table_panel_test (2) |
| `PanelType::Table` + name round-trip | `panel_types`, `panel.cpp` | panel_factory_test |
| `Panel` base virtuals `addSignal`/`removeSignal`/`isMultiSignal`/`detachChart` | `panel.hpp` | — |
| `Dashboard::addTablePanel`; polymorphic `removePanel`/`removeSignalEverywhere` (no downcasts) | `dashboard.*` | dashboard_test (+1) |
| MainWindow `+ Table` toolbar action (tables all current signals) + `--auto-add-table` harness | `main_window.*`, `main.cpp` | full ctest + smoke |

Commits: `(plan)` → `a26c975` S1 → `6ade0e2` S2 → (this) S3.

## Verification

- **Debug + Release** build green. **ctest** 100% (table_panel_test, extended dashboard_test).
- **GUI release smoke** green incl. the new **Tier D** crash-exit guard (rc=0 — C2 fix holds).
- **clang-format** clean. **clang-tidy**: matches module baseline; the S2 refactor *removed*
  the 2 P0 `static_cast`-downcast warnings.
- **Live visual** (/tmp, not committed): `+ Table` → "Live values" table showing crc/padding/
  pressure(103.100 kPa)/alarm(false)/calibration(true) with "59ms ago" freshness — the
  "is this device sane?" view. Clean exit (rc=0).

## Decisions (logged in M22-understanding.md)

- **D1** `+ Table` tables all currently-registered signals (per-driver scoping = follow-up).
- **D2** Table is "wide" (full grid row).
- **D3** refactored Plot/Table dispatch to `Panel` virtuals (no downcasts) — touches only
  M21 (unmerged, non-frozen) code. No frozen interface modified.

## Deferred (later phases)

- Per-driver auto-grouped tables, column sort. P2 readable PlotPanel · P3 Bar/Gauge.
- M21 concern C1 (slow-signal publish latency) still open — applies to Table rows too.

## Status

P1 implemented and green on `milestone/M22` (local, unpushed). Decisions autonomous per the
session directive; revert any commit on review. Chain: `main` → M21 (P0+C2) → M22 (P1), all
local pending your review/merge.
