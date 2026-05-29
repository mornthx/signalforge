# DR-001 — UI, menu & visualization direction

| Field | Value |
|---|---|
| Date | 2026-05-29 |
| Reviewer lens | Skeptical first-time bring-up operator (spec lens dropped) |
| Product state | post-M20 (`main` @ a647ecf, "modern dark workbench default") |
| Evidence | `tests/visual/baselines/*.png`; `src/app/main_window.cpp`; `src/chart/{chart,signal_selector}.cpp`; `resources/qml/ChartHost.qml` |
| Headline | **Off-course.** Engine is solid; the UI is a single-plot oscilloscope shell that doesn't do the workbench basics. |

## Checklist verdicts

| # | Item | Verdict | One-line justification |
|---|------|---------|------------------------|
| 1 | First-touch w/o docs | Drifting | Three different "add connection" entry points; "Load schema" button actually opens the add-connection dialog (`main_window.cpp:1226`). |
| 2 | Workbench basics covered | **Off-course** | Cannot show a current value, a boolean state, or a unit — only line/step/point traces. |
| 3 | Top-3 tasks easiest | Drifting | "See current value / state of N signals" — the most common task — is impossible, not just hard. |
| 4 | Menu order/findability | **Off-course** | Bar is `Connections \| Session \| File \| View` (File 3rd); built piecemeal by each `buildXxxUi`. |
| 5 | Owned layout vs accretion | **Off-course** | Status bar assembled per-milestone via `insertWidget`; redundant cells `Recording Record idle`, `Buffer Buffer 0%`. |
| 6 | One path per goal | Drifting | Add-connection: dock button + menu + empty-state card. Record vs Open-Session split across Session/File menus. |
| 7 | Capability vs polish (last 4 M) | **Off-course** | M14 audit, M15 vision-infra, M16 identity, M17 widget-rebuild, M18 workflow, M19 visual states, M20 themes — **7 straight polish milestones**, zero new visualization capability. |
| 8 | Frozen early guess | **Off-course** | M8 froze `Chart` = line/step/point. Later work contorts around it; string signals still impossible (`chart.cpp:88-99`). |
| 9 | Representation fits signal | **Off-course** | One widget for everything; shared auto-scaled Y axis flattens mixed-range signals (`chart.cpp:320-360`). |
| 10 | Self-describing output | **Off-course** | No axis tick labels, scale numbers, units, or in-chart legend; grid is decorative; 1px lines (`chart.cpp:464`). Confirmed in `37-replay-playing.png`: selected temperature signal shows only empty grid. |
| 11 | Honest at rest | Drifting | `Drops 1` shows in warning color at empty launch (`00-empty-launch.png`). |
| 12 | States actually differ | Drifting | Signal colors hardcoded to `tokens::light` regardless of theme; cursor hardcoded white, invisible on light (`chart.cpp:65,525`). |

## Root cause

Not taste — **process**. Milestones reward vertical feature-add and the interface-freeze rule
(CLAUDE.md Required #9) turns early local guesses into permanent global constraints. No role
owns "the UI as a whole", so the menu, status bar, and visualization model each grew by
accretion. The result reads as thorough inside each spec and incoherent across them.

## Recommendation

1. **Reframe the central area as a dashboard of heterogeneous widgets**; the plot becomes ONE
   panel type. Add numeric, state/LED, and data-table panels first. Design:
   `docs/v0.3/dashboard-interaction-design.md`.
2. **Make the plot readable**: axis labels + units + colored legend + ≥2px lines +
   per-signal / normalized Y.
3. **Rewrite the menu bar top-down** as one owned function; consolidate duplicate entry points.
4. **Rebuild the status bar** as one owned layout; stop the per-milestone `insertWidget` habit.

### Decisions taken (2026-05-29)

The redesign direction is approved. Decisions that keep all of it **additive — no frozen
interface is broken** (so no HALT #4 gate):

1. Plot readability ships as a **new `PlotPanel` built in parallel**; the frozen M8 `Chart`
   is kept as legacy and retired later (CLAUDE.md "add alongside" rule).
2. **Zero-config by default, per-panel override** of data interpretation / min-max / unit;
   no min/max set → use the signal's **observed** min/max. Range lives in panel config, so
   frozen `SignalMetadata` is untouched.
3. In-place Edit toggle approved **provisionally** — judged by real-use feel.

Target design: `docs/v0.3/dashboard-interaction-design.md` (§9 records these).

**Update 2026-05-29 — P0 landed (milestone M21, local).** The dashboard-of-panels redesign's
first phase is implemented: Numeric/State cards + wrapped PlotPanel in a reflow grid, driven by
a dashboard-aware signal list; empty-launch decluttered (findings #2/#3/#10 addressed for
value/state display; the empty-grid clutter is gone). Remaining DR-001 findings still open:
plot readability (P2), menu-bar order (#4) and status-bar accretion (#5) — separate IA work.
This DR stays **Open** until those land. New issues surfaced during M21: concern C1 (slow-signal
publish latency) and C2 (pre-existing teardown UAF) — see `.claude/M21-concerns.md`.

## Process note

This is the first direction review. It exists because no M8–M20 milestone-local review could
have caught items 7–10: each was outside the slice any single spec was asked to judge. Cadence
and checklist: `.claude/direction-review/README.md`.
