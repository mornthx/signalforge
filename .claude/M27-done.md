# M27 — Dashboard interaction fixes (closure report)

Branch: `milestone/M27` (local, off `milestone/M26`; **not pushed**). Fixes three issues the user
found by operating the running app, plus a testing-methodology change.

## Problems fixed

1. **Plot trace drifted out of frame / onto the Y axis over time** (`2d4c60b`). Root cause:
   `PlotView` re-read the live (now()-tracking) `TimeAxisManager` at paint, so the query-time
   and paint-time windows differed → the oldest samples mapped to negative x. Fix: capture the
   window once in `recompute()` and map samples against it at paint; clip series+cursor to the
   plot rect. Run-length spill guard test added.

2 & 3. **No way to configure or operate panels** (`b2534bd`). The panel Remove button was hidden
   behind an `editMode` the dashboard never enabled — so panels had *no* operable control.
   Replaced it with an always-visible **⋮ config button** opening a per-panel menu:
   - **Show as ▸** — change the widget type (Numeric/State/Plot/Bar/Gauge/Table) in place,
     preserving signals + position.
   - **Signals ▸** — check/uncheck registered signals to **assign specific data to that
     specific widget** (single-signal cards take one; Plot/Table take many).
   - **Move ◀ / ▶** — reposition a panel in the layout (was insertion-order only).
   - **Remove panel**.
   Dashboard gained `setPanelType`/`setPanelSignals`/`movePanel`/`buildPanelMenu` (panels
   recreated in place, preserving id + grid position).

## Testing-methodology change (user feedback)

Added **interaction-simulating** tests (the gap that let #2/#3 ship unnoticed — see memory
`feedback_simulate_real_interaction`):
- `QTest::mouseClick` on the ⋮ button → asserts `configureRequested` fires.
- `buildPanelMenu` → `QAction::trigger()` for "Show as Plot", "Signals ▸ rig/alarm",
  "Move right", "Remove panel" → asserts the panel actually changed type / gained the signal /
  reordered / was removed. Single-signal reassignment covered too.

## Verification

- Debug + Release build green. Dashboard unit tests: 7 cases / 44 assertions (dashboard_test) +
  4 cases (panel_factory_test) + plot_view (15 assertions incl. spill guard). All green.
- Live (relaunched on the user's display): plot contained; ⋮ button visible on each panel;
  menu changes type / assigns signal / moves / removes.
- clang-format clean.

## Remaining (not in M27)

- **Free-form drag-resize / drag-to-reposition** of panels (true canvas layout). M27 delivers
  configuration + reorder (Move) + type/signal assignment; corner-drag resize is a larger
  layout-engine change — flagged as the next step if needed.

## Status

Local on `milestone/M27` (unpushed). Chain: `main → M21 … → M27`.
