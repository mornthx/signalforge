# M32 — IA realignment: control placement matches scope (understanding)

Owner PM-level feedback (3 points, one root cause): dashboard-scoped controls sit at the global level,
and app-level onboarding is buried in a tab-local state. Fix = align placement with scope, recovering
`architecture.md §7.1/§7.3` (never built). Branch `milestone/M32` (local, off M31).
See memory `ia_control_scope_principle`.

## Delivers

- **S1 — promote-from-Parsed; remove the global Signals dock (owner #1).** The Signals checkbox tree only
  toggled dashboard cards yet lived globally beside all tabs. Drop it. In the **Parsed** tab, right-click a
  signal row → "Add to dashboard ▸ Numeric/State/Plot/Bar/Gauge" (or add to an existing Plot). Parsed
  becomes the single global signal browser; the dashboard is composed by promotion (drill-through).
  `ParsedSignalsView` emits an add-request; `main_window` routes it to the dashboard + switches tab.
  Removes `SignalListPanel` (class + test + mount).

- **S2 — app-level onboarding (owner #2).** Move the "Start a SignalForge workflow" panel out of the empty
  Dashboard into a workspace-level state: a `QStackedWidget` center shows the onboarding when there are no
  connections, and the tabs once a connection exists.

- **S3 — modal per-panel config dialog (owner #3).** Replace the right-click "Show as ▸ / Signals ▸"
  submenus with **Configure…** → a modal `PanelConfigDialog` editing the full `PanelConfig`: type, signals
  (multi for Plot/Table), title, range min/max, unit override, decimals ("细化"). "Remove panel" stays.
  **Deviation:** §7.3 #4 wants a right-hand inspector, not a modal; owner explicitly chose modal — executed
  per owner override, logged here + in done.md.

## Out of scope (flagged, next)

The dashboard toolbar (+Plot/+Table/+Bar/+Gauge, Live, time-axis) is the same mis-placement class; moving
it into the Dashboard tab is deferred to a follow-up. Schema-driven dissection tree still pending (M33).

## DoD

Build Debug+Release green; ctest green both; QTest interaction tests (promote from Parsed; config dialog
applies); clang-format clean; re-accept changed visual baselines (onboarding + no Signals dock); local
commits, no push. No frozen interface/schema touched.
