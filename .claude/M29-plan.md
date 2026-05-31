# M29 — plan (Phase A)

Order matters: S1 (data/lifecycle) before S2 (layout). Each subtask = one local commit after
build+ctest+clang-format green.

## S1 — persistent per-signal intent + symmetric lifecycle  (reports 1 & 2)

Files: `src/dashboard/dashboard.{hpp,cpp}`, maybe `panel_types.hpp`.

1. Add a remembered-intent store on `Dashboard`:
   `QHash<QString /*signalId*/, PanelConfig>` recording the last widget type + format fields a signal
   was shown with (geometry NOT remembered — intent is type/format, placement is per-widget).
2. `addSignal(signalId)`: if intent exists, build the panel from remembered type+format; else
   `suggestPanelType` and record it. (Report 2.)
3. `removeSignalEverywhere(signalId)`: after `removeSignal` on a multi-signal panel, if that panel now
   has **zero** signals → `removePanel`. Single-signal cards already remove. Net: no orphaned empty
   widgets. (Report 1.)
4. `setPanelType` / `setPanelSignals`: write the new type/format back into the intent store for each
   signal the panel carries, so the choice survives later demote→promote.
5. Tests (`dashboard_test.cpp`, simulate real toggles via the signal list + menu actions):
   - check temp (Numeric) → ⋮ Show-as-Gauge → uncheck (assert panel gone, `panelCount` drops) →
     re-check → assert it returns as **Gauge**, not Numeric.
   - Plot with two signals → uncheck one (assert plot kept, one trace) → uncheck the other (assert plot
     **removed**).

## S2 — bounded free+push layout  (report 3)

Files: `src/dashboard/dashboard.{hpp,cpp}`, `src/dashboard/panel.{hpp,cpp}`.

1. Add `Dashboard::resolveDrag(panelId, QRect proposed) -> bool`:
   - clamp `proposed` to the surface rect;
   - find panels overlapping `proposed` (excluding self);
   - for each, compute the minimal push (axis of least penetration) to separate; clamp the neighbor to
     the surface; if any neighbor cannot be fully separated within bounds, **or** a pushed neighbor would
     overlap a *third* panel → return `false` (no geometry applied);
   - else apply dragged + neighbor geometries, write each `config.geometry`, return `true`.
2. Refactor `Panel` drag: on `MouseMove`, compute proposed geometry and ask the parent Dashboard to
   `resolveDrag`; the Dashboard drives the actual `setGeometry` (Panel no longer self-moves). On a
   `false` result the panel stays put. On `MouseButtonRelease`, emit `geometryChanged`.
   - Keep `Panel` decoupled: emit `dragProposed(id, QRect)`; Dashboard connects and calls `resolveDrag`.
3. Tests:
   - two cards side by side; drag A rightward into B → assert B pushed right, both inside the surface.
   - shrink surface so there's no room; drag A into B at the wall → assert `resolveDrag` returns false
     and neither moved (refusal).
   - three in a row; drag A into B where pushing B would hit C → assert refused (single-hop rule).

## Close-out

`.claude/M29-progress.md` updated per subtask; `.claude/M29-done.md` at the end with the test counts and
a note that report 4 is carried to Phase B/C. No push (awaits owner review, per the M21+ local chain).
