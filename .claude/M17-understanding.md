# M17 Understanding — V0.3 Widget Rebuild

## 1. What M17 is

M17 is the **operator-facing first application** of the M16 visual-identity
infrastructure. M16 built the foundation (tokens, fonts, palette, QSS
selectors, baselines); M17 wires that foundation into the widgets the
operator actually interacts with so that state and identity become
visually legible.

The driving observation: after M16 merged, the `tokens.qss` global
stylesheet defines `QLabel[class="status-connected"]` (green),
`QLabel[class="status-error"]` (red), and friends — but **no widget in
the codebase actually sets a `class` property**. The selectors are
dormant. M17 wakes them up.

## 2. Who asked for what

| Source | Ask |
|---|---|
| `.claude/M16-done.md` §12 "Hand-off to next session (M17 spec drafting)" | Rebuild **SignalSelector**, **ChartConfigDialog**, and **ConnectionListPanel** using widget-styling-guide patterns |
| `docs/v0.3/widget-styling-guide.md` §3 + §4 | Apply QSS `class` property pattern to state-dependent widgets; apply `objectName="panelHeader"` to layout-stable chrome surfaces |
| `docs/v0.3/widget-styling-guide.md` §9 anti-pattern table | Eliminate `QHash` / `std::unordered_map` iteration in user-visible output (ADR-014 anti-pattern row 6) |
| `.claude/M16-done.md` §10 (PR scope) | Net diff target preserved at ≤ 800 lines per CLAUDE.md §Required #4 |
| Session prompt 2026-05-20 | "完成计划中的ui/使用体验进化工作" — complete planned UI/UX evolution work, with autonomous authorization |

## 3. What I am building, concretely

Three widget rebuilds + one main-window naming pass + one visual-baseline
pass + closure.

### 3.1 ConnectionStatusWidget (S1)

**Before**: A `QLabel` showing "X/N connected · errors: K" in default
text colour regardless of state.

**After**: Same text content (preserved for backward-compat tests), but
the label sets a `class` property reflecting an aggregated state across
all connections. The label renders in green / yellow / red / grey based
on the aggregated state. Aggregation rule per M17 spec §6.1.

### 3.2 ConnectionListWidget (S2)

**Before**: A `QListWidget` with rows like "`displayName [TCP] —
Connected`" all rendered in default text colour.

**After**: Same `QListWidget` structure (Option B per spec §6.2 — no
switch to `QTreeWidget`), with per-row `Qt::ForegroundRole` set to the
token-mirrored colour for the row's state. Above the list: a
`QFrame#panelHeader` with `QLabel` "Connections" (drives the M16
`QFrame#panelHeader` QSS rule).

### 3.3 SignalSelector (S3)

**Before**: Filter `QLineEdit` + `QTreeWidget` with `QTreeWidget::header()`
set to "Signals". Groups built from `std::unordered_map` iteration —
order is hash-dependent, not deterministic cross-host.

**After**:

- New `QFrame#panelHeader` with `QLabel` "Signals" at the top.
- The `QTreeWidget::header()` is hidden (panel header takes its role).
- Filter input unchanged.
- Below the filter: a `QLabel` with `class="caption"` showing the
  visible/total signal count.
- Internal `groups` / `leaves` storage switches to `std::map<QString, ...>`
  so iteration is alphabetical. Tree-population walks `registry_->signalIds()`
  (already deterministic post-ADR-014) and inserts groups in sorted label
  order.

### 3.4 MainWindow naming pass (S4)

Add `objectName` to the `ConnectionListWidget` parent dock and any
status-bar chrome that lacks it. No layout / behaviour change. This
helps AT-SPI / xdotool / visual-test scaffolding target by name.

### 3.5 Visual baselines (S5)

Two new baselines for the operator-visible M17 wins:

- `30-all-connected.png` — one replay connection driven to Connected
  state; status-bar label renders green.
- `31-with-errors.png` — one replay connection driven to Error state
  (bad path); status-bar label renders red.

Plus R8 re-acceptance of any of the 12 M16 baselines whose layout shifts
because of new chrome (most likely candidates: states involving the
connections-dock, since the new panelHeader adds a fixed-height row at
the top of the dock).

### 3.6 Closure (S6)

`M17-done.md` mirroring `M16-done.md` structure. PR to main. CI green.
Tag (per session authorization, autonomously).

## 4. What I am NOT building

Strict scope discipline per spec §2:

- **ChartConfigDialog** — deferred to M18 (modal dialog rebuilds bundle
  better with workflow rebuild).
- **Dark theme** — M20 scope.
- **New tokens** — M17 consumes existing tokens only.
- **Chart QML surface** — separate track.
- **Layout changes** — only chrome / colour / typography.

## 5. How M17 connects to the V0.3 charter

The V0.3 charter's keystone gate was M16's cross-environment visual
determinism (`< 1 %` for 12 baselines). M17 extends that gate by two
baselines and keeps the existing 12 green — proving the M16 contract
**holds under realistic widget evolution**, not just at the M16 close
snapshot. This is the V0.3 R8 / R12 governance contract first applied
in M16 S6 (signal-tree non-determinism) and S7 (V0.2 → M16 baseline
migration), now applied a third time as it shifts from one-off remedy
to routine governance discipline.

## 6. Why the M0–M13 roadmap doesn't mention M17

`docs/milestones/milestone-roadmap.md` was authored at 2026-04-24 (v2.1)
covering the **V1 path** (M0–M13). The V0.3 sub-track (M14 GUI audit,
M15 vision infrastructure, M16 visual identity, M17 widget rebuild) is
documented in the M14+ done reports and the V0.3 charter amendments,
not in the v2.1 roadmap. A roadmap refresh covering V0.3 explicitly is
M18 scope at earliest; M17 does not block on it.

## 7. Risk awareness

The biggest risk is the visual-baseline impact of new chrome. The
`QFrame#panelHeader` row adds ~28 px of vertical space at the top of
each of the SignalSelector + ConnectionListWidget docks. This shifts
the layout of every baseline that captures those docks. R8 acceptance
in S5 covers this; the M16 S7 baseline-migration pattern is the
playbook.

The second-biggest risk is the ADR-014 anti-pattern re-surfacing in a
new place. M17 S3 directly closes this in SignalSelector. M17 PR review
checks `grep -rn "unordered_map\|QHash" src/chart/ src/connection/` to
confirm no new hash-iteration on a user-visible surface.

## 8. Authorization context

This document is being drafted autonomously per session-prompt blanket
authorization (2026-05-20). The standard 5-phase milestone-closure flow's
Phase 2 (review of M16-done) and Phase 4 (review of M17 understanding +
plan) are waived for this milestone. The "hold" / "stop" runtime escape
remains in effect per CLAUDE.md §Forbidden #4 last paragraph.

Audit trail of this deviation: M17-done.md §11.
