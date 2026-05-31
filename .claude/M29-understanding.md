# M29 — Dashboard tier: persistent intent + bounded push layout (understanding)

Phase A of the three-tier recovery ([DR-002](direction-review/DR-002-2026-05-30-three-tier-workbench.md),
design `docs/v0.4/three-tier-workbench-design.md`). Branch `milestone/M29` (local, off M28).

## Scope decision

Of the four owner reports, **1–3 are dashboard-tier bugs fixable now**; **report 4 ("raw data first;
dashboard is a page on top") is the three-tier restructure itself** and is delivered structurally in
Phase B/M30 (Parsed tab) + Phase C/M31 (Raw tab), not by an empty one-tab shell now. M29 fixes 1–3 and
re-roots the dashboard's mental model so B/C are additive.

## The unifying idea

The dashboard conflated "observe a signal" with "own a widget." Split them: **per-signal *intent*
(preferred widget type + config) persists on the Dashboard independently of whether a widget currently
exists.** The signal-list checkbox becomes one honest boolean — "on the dashboard or not."

## Delivers

- **S1 — persistent intent + symmetric lifecycle (fixes reports 1 & 2).**
  - Dashboard holds `signalId → remembered {PanelType, unit/range/decimals}`.
  - Promote: remembered type if known, else `suggestPanelType` (first time only).
  - Demote (uncheck): remove the signal from every panel **and delete any panel left empty**, including
    multi-signal Plot/Table when their last signal leaves. (Report 1.)
  - Re-promote restores the remembered form. (Report 2.)
  - Changing a panel's type/signals updates the remembered intent.

- **S2 — bounded free+push layout (fixes report 3).**
  - Keep M28 pixel-precise geometry, but drag/resize **pushes** directly-overlapped neighbors.
  - Pushes **clamp at the viewport edge**; **single-hop only** (a pushed neighbor is not allowed to push a
    third panel) — honors "不要无限推挤". If a neighbor cannot be separated within the surface, the
    originating move is **refused** (revert to last good geometry).
  - Collision resolution centralized on the Dashboard (testable in isolation), driven by the Panel's drag.

## Out of scope (Phase B/C)

Tabbed workspace shell, Parsed-signals table, filter engine, Wireshark raw view, drill-through. Report 4.

## DoD

Build Debug+Release green; ctest green; **QTest interaction tests** simulating real check/uncheck/
type-change and mouse drag-into-neighbor / drag-into-corner; clang-format clean; local commits, no push.
Freeze note: M29 is expected to stay within the dashboard module (unfrozen, M21+); no frozen-interface
thaw anticipated — if one becomes necessary it is taken deliberately per `freeze_thaw_authorized`.
