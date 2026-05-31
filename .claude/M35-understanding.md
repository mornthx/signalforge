# M35 — Understanding (bootstrap)

## Where we are
- **M34 merged** (PR #32 → `main`, merge `7289cd7`, tag `v0.0.34.1`). Branch `milestone/M35`
  created from `main` and pushed.
- M34 delivered the **entire v0.4 UI redesign plan P0–P5** (`docs/v0.4/ui-redesign-proposal.md §12`):
  activity-rail workbench, Parsed signal browser, Raw Wireshark dissection, Dashboard colour
  unification + scroll, and the right inspector with cross-tier selection / drill-through /
  highlight / recolor / rename / panel config / top-bar chip.
- CI gate: build + ~750 unit/logic tests green on debug / release / **debug-asan** (ASan/UBSan/LSan
  clean). Full-window visual baselines are **non-blocking** (env-specific font rendering; see
  `memory/ci_visual_baseline_divergence.md`).

## The gap M35 must resolve
**There is no M35 spec.** The redesign plan ended at P5 — which is done. Recent milestones
(M18–M34) were not tracked as `docs/milestones/M<n>-*.md`; they were driven by the v0.4 proposal +
`.claude/` files. So M35 is a **new direction the owner must set**. I will not invent a plan
(CLAUDE.md ambiguity rule).

## What the redesign explicitly left open
The product pipeline (proposal §6): **Connect → Decode/verify → Observe → Visualise → Control →
Record/Replay**.
- **Control (reserved, design-only)** — the *command/control path*: sending commands **to** the
  device (handshake / polling / manual dispatch / macros / ACK matching). The activity rail kept a
  reserved slot for it. A distinct, substantial **new core capability** (≠ replay control). See
  proposal §7.5.
- **Replay — FROZEN** (owner-frozen; not to be built).
- **Redesign polish backlog** (optional, from `.claude/M34-progress.md`): two-style selection
  highlight (selected-panel vs shows-selected-signal); propagate signal rename to dashboard panel
  titles; strict bottom-left cascade placement; Raw filter autocomplete (P3 stretch); Raw tier
  observing `selectionChanged`; re-tighten the visual CI gate by capturing baselines in the CI env.

## Candidate scopes for M35 (owner picks — see `M35-concerns.md`)
- **A — Control mode**: build the reserved command/control tier (the next pipeline capability).
- **B — Polish & harden**: clear the redesign backlog + hardening before new capability.
- **C — Owner-defined**: a different direction (perf, a specific feature, etc.).

## Status
Awaiting the owner's M35 scope decision before writing `.claude/M35-plan.md`. Merge/tag/bootstrap
(Phase 3 a–i) are complete; the plan (Phase 3 j–k) is blocked on scope.
