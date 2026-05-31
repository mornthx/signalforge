# DR-002 — Three-tier workbench (raw → parsed → dashboard)

- **Date:** 2026-05-30
- **Trigger:** Owner direction shift while reviewing the M21–M28 dashboard; effectively a v0.3 → v0.4 bootstrap.
- **Scope:** Whole-product information architecture — what surfaces a bring-up engineer actually needs.
- **Headline verdict:** **Drifting (subtle).** DR-001's dashboard redesign fixed *visualization* but the product still has only **one** observation surface. The architecture's intended **Decode** workspace (raw frame + dissection) was never built. Recover via an explicit three-tier model.
- **Status:** Open — execution begun (M29 = Phase A).
- **Relates to:** [DR-001](DR-001-2026-05-29-ui-visualization.md) (Resolved).

## What the owner reported

After operating the post-M28 build, four issues — but the framing matters more than the list:

1. Unchecking a signal does not remove its widget.
2. Re-enabling a signal should restore its *last-chosen* widget form.
3. Widgets overlap with no push/pull; the viewport is finite so push must be bounded.
4. **The default should be raw data first; the dashboard is a page built *on top of* the raw data.**

Then the structural reframe: the product should have **three tiers** —
1. **原报文 (raw packets)** — protocol-level dissection, **display aligned with Wireshark**.
2. **解析数据 (parsed signals)** — decoded per message definition; Wireshark-grade **filtering**.
3. **Dashboard** — visualization on top of tier 2.

## Diagnosis (drop the spec lens)

The four bugs are not four bugs. They are symptoms of **one** structural gap: the app conflates *observing signals* with *composing a dashboard*, and keeps **no ground-truth layer beneath the dashboard** and **no persistent per-signal intent**.

| Report | Root cause |
|---|---|
| 4 (no raw view) | No data surface exists except the dashboard. "Observe the device" and "design a view" are the same act — backwards for bring-up (observe first, *then* visualize). |
| 1 (uncheck lingers) | check→create / uncheck→remove lifecycle is asymmetric: unchecking a signal inside a multi-signal Plot/Table strips the trace but orphans the now-empty widget (`dashboard.cpp` `removeSignalEverywhere`). |
| 2 (forgets form) | Widget type lives only on a live panel; once removed, "show pressure as a Gauge" is lost (`addSignal` re-runs `suggestPanelType`). No preference outlives the widget. |
| 3 (overlap) | M28 free-form absolute positioning has no spatial contract. |

## Key finding — this recovers the architecture, it does not contradict it

`docs/architecture/architecture.md` **already specifies the target**:

- §3.2 In Scope / Observation lists **"Raw frame inspection"**, **"Signal tree"**, value/status cards, charts, **log table**.
- §7.1 main window: center = tabbed primary workspace; **bottom = "logs, raw frames, events"**.
- §7.2 Primary Workspaces: **Decode** = *"raw frame list, per-frame preview, message recognition results, field structure, decode-rule editing, error messages"*; **Observe** = *"value cards, status cards, charts, log stream, alarm strip."*
- Principle #3: *"Build an engineering **workbench**, not a presentation-oriented dashboard tool."*

The implementation drifted into building **only** a dashboard-flavored Observe and **never built Decode at all**. The owner's three tiers map directly:

| Owner tier | Architecture home | State today |
|---|---|---|
| 1. 原报文 (Wireshark) | **Decode**: raw frame list + field structure (+ hex, filters) | **Not built** |
| 2. 解析数据 (filtering) | Signal tree + decoded values + filter | Partial (signal tree only) |
| 3. Dashboard | **Observe**: cards / charts | Built (M21–M28) |

**Consequence:** No `architecture.md` edit is needed; the vision is already in §7.2. The work is to *implement the missing Decode workspace* and *re-root the dashboard as Observe under a tabbed shell.*

## Decisions captured

- **Layout (report 3):** free pixel placement with **push-then-stop at the viewport edge** — neighbors shove on collision but clamp at the wall; a push with no room is refused. No infinite cascade, no scroll-off. (Owner choice.)
- **Navigation:** tabbed center workspace per §7.1/§7.2 (Observe now; Decode/Parsed added in B/C). (Architecture-endorsed.)
- **Filter engine:** one reusable engine, built in Phase B for tier 2, reused by tier 1 in Phase C. The load-bearing core of "向 Wireshark 看齐".
- **Freeze policy:** owner authorized thawing frozen interfaces/schemas where the redesign needs it, prudently — see memory `freeze_thaw_authorized`. Not extended to `architecture.md` / `CLAUDE.md`.

## Phased recovery (top-down; owner agreed "同意执行")

- **Phase A / M29** — dashboard-tier fixes + tabbed workspace shell: (1) uncheck removes emptied widget; (2) persist per-signal widget intent; (3) bounded free+push-stop layout; (4) reframe dashboard as the Observe tab.
- **Phase B / M30** — Parsed-signals view (tier 2) + reusable filter engine.
- **Phase C / M31** — Wireshark raw-packet view (tier 1): packet list → schema-driven dissection tree → hex, reusing the filter engine.

Design detail: `docs/v0.4/three-tier-workbench-design.md`.

## Checklist verdicts (delta vs DR-001)

- **A2 workbench basics:** Drifting → only one observation surface; raw/decode inspection absent.
- **B6 one obvious path per goal:** Drifting → "see what the device is actually sending" has *no* path.
- **C7 capability vs polish:** M25–M28 were polish on the dashboard; this DR redirects to new capability.
- **D9 representation per signal:** OK (dashboard) but **D-new:** no representation for *frames* at all.
