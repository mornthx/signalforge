# M29 — Dashboard tier: persistent intent + bounded push (closure report)

Phase A of the three-tier recovery ([DR-002](direction-review/DR-002-2026-05-30-three-tier-workbench.md)).
Branch `milestone/M29` (local, off M28; **not pushed**). Three of the four owner reports; report 4 is the
three-tier restructure itself, carried to Phase B/C.

## S1 — persistent per-signal intent + symmetric lifecycle (`c2fc2c7`) — reports 1 & 2

The dashboard conflated "observe a signal" with "own a widget." Split them: a `signalId → PanelConfig`
intent map on the Dashboard remembers each signal's last widget form (type + format), independent of
whether a widget currently exists.

- **Report 2 (forgets form):** `addSignal` restores the remembered type/format; only the first sighting
  falls back to `suggestPanelType`. `setPanelType`/`setPanelSignals` write the chosen form back, so a
  later demote→promote restores it.
- **Report 1 (uncheck lingers):** `removeSignalEverywhere` now removes a multi-signal Plot/Table once its
  last signal leaves, instead of orphaning a blank widget. Single-signal cards already removed.

## S2 — bounded free+push layout (`76a42a4`) — report 3

Drag/resize is now mediated by the Dashboard. The `Panel` emits a proposed geometry; the Dashboard
`resolvePanelDrag` clamps it to the surface and pushes directly-overlapped neighbors along the axis of
least penetration.

- **Bounded (owner: "推挤是可以的，但不要无限推挤"):** pushes clamp at the viewport edge; **single-hop only**
  — a push that would cascade into a third panel, or that can't separate within bounds, **refuses the whole
  move** (nothing leaves the surface, no runaway cascade).

## Report 4 — raw-data-first (carried)

"默认应该首先显示原始数据；dashboard 是在原始数据基础上的页面" is the three-tier restructure: it is delivered
structurally by **Phase B/M30** (Parsed-signals tab, the default landing surface) and **Phase C/M31**
(Wireshark raw-packet tab), under the tabbed center workspace (architecture §7.1/§7.2). Building an
empty one-tab shell now would add no user value, so M29 deliberately defers it.

## Verification

- Build green Debug + Release. **684/684 ctest on both presets.** Visual baselines unchanged (no
  rebaseline). `dashboard_test`: 13 cases / 76 assertions, incl. 4 new M29 interaction tests
  (re-check-restores-form, empty-plot-removed, push-aside, refuse-no-room) simulating real menu + mouse
  drag. clang-format clean; clang-tidy is the CI gate (project convention).
- No frozen interface or schema was thawed — M29 stayed within the dashboard module (M21+, unfrozen).

## Status

Local on `milestone/M29`. Chain: `main → M21 … → M28 → M29`. Awaiting owner inspection.
Next: Phase B / M30 — Parsed-signals view + reusable filter engine.
