# Direction Reviews

A **direction review** is a periodic step-back that judges whether the *whole product* is
still heading where it should — independent of any single milestone spec.

Per-milestone `M<n>-done.md` / `M<n>-concerns.md` answer *"did we build the spec correctly?"*
They cannot catch the failure mode where **every spec is executed well and the product still
walks off a cliff**, because each spec only sees its own slice. The single-visualization UI
drift (DR-001) is the canonical example: M8 froze "line/step/point", and M9–M20 each
correctly built on that frozen base, so no milestone-local review ever flagged that the
*product* had no way to show a current value, a state, or a unit.

## When to run

- When closing any milestone whose number is a multiple of **3** (M21, M24, …), **or**
- When the human asks for a "direction check", **or**
- Before bootstrapping a new minor version (v0.x → v0.(x+1)).

A review is cheap (~30 min of reading the running app + key UI code). Skipping one is fine;
two skipped in a row is itself a drift signal.

## How to run

1. **Drop the spec lens.** Judge as a skeptical first-time user/operator, not as the author
   of the milestones. Do **not** justify a decision by "the spec said so".
2. **Look at the actual product, not the plan.** Launch the app or read the visual baselines
   (`tests/visual/baselines/*.png`); read the top-level UI code
   (`src/app/main_window.cpp`, `src/chart/*`). Ground every finding in something you can see
   or a `file:line`, never in a spec sentence.
3. **Run the checklist below.** For each item, write a verdict: `OK` / `Drifting` / `Off-course`.
4. **Write a new entry** `DR-NNN-<yyyy-mm-dd>-<slug>.md` from the template. Number
   sequentially. Link prior DRs that are still open.
5. **Surface to the human.** Findings that require breaking a frozen interface or rewriting a
   subsystem are *proposals*, not autonomous changes (CLAUDE.md HALT #4 / disagreement rule).

## Drift-indicator checklist

Score each as OK / Drifting / Off-course and justify in one line.

### A. Product purpose
1. Could a *new* bring-up engineer, given a device, complete a first integration without
   reading docs or a config format? (charter Goal #5)
2. Does the product still match "engineering **workbench**, not presentation dashboard"
   — and does it cover the *workbench* basics (read a value, see a state, compare a few),
   not just one fancy capability?
3. Name the top 3 things a target user does in a session. Does the UI make those 3 the
   easiest things to do?

### B. Information architecture
4. Menu bar: is the order conventional and is each command findable where a user expects it?
   Is each command in exactly one sensible place (no N-entry-points, no orphan-in-toolbar)?
5. Is any screen region (status bar, docks, headers) the result of per-milestone accretion
   rather than a single owned layout? Look for duplicated labels / redundant titles.
6. For each primary user goal, is there exactly one obvious path?

### C. Core surface vs. polish ratio
7. Of the last 3–4 milestones, how many added a *new user capability* vs. *polished an
   existing surface*? A long run of pure polish on a narrow base = drift.
8. Is there a frozen interface (M8 `Chart`, schemas, etc.) that an early guess locked in and
   that later work has been contorting around instead of revisiting?

### D. Visualization / output (this product's heart)
9. Can the user pick a representation appropriate to each signal's *type and semantics*
   (value, state, trend, table, gauge), or is everything forced through one widget?
10. Is each shown number/trace self-describing — title, unit, scale, legend — without the
    user cross-referencing another panel?

### E. Honesty of state
11. Do indicators tell the truth at rest (no false warnings on idle, e.g. "Drops 1" at
    empty launch)?
12. Do themes/states actually differ where the product claims they do?

## Index of reviews

| ID | Date | Scope | Headline verdict | Status |
|----|------|-------|------------------|--------|
| [DR-001](DR-001-2026-05-29-ui-visualization.md) | 2026-05-29 | UI / menu / visualization | Off-course: single-plot shell; full redesign landed (M21–M26) | **Resolved** (local, unpushed) |
| [DR-002](DR-002-2026-05-30-three-tier-workbench.md) | 2026-05-30 | Whole-product IA: raw → parsed → dashboard | Drifting: only Observe built; recover the §7.2 Decode workspace via 3 tiers | **Open** — Phase A (M29) done; B/C pending |
