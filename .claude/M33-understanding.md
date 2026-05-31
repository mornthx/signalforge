# M33 — UI redesign Phase 0: foundations (understanding)

The settled UI redesign (`docs/v0.4/ui-redesign-proposal.md`) ships foundations-first: Phase 0 builds the
invisible, load-bearing infrastructure so every later (visible) phase is small. **Nothing ships visually
in M33.** Owner mandate: long-term value over the simplest build. Branch `milestone/M33` (local, off M32).
See memory `ui_redesign_direction`.

## Why foundations first

The current UI drifted into patches because there was no shared infrastructure — every screen reinvented
chrome, and nothing tied the tiers together. Phase 0 lays the four pillars the whole redesign rests on:

## Delivers (P0 pillars)

- **S1 — SelectionModel** (`src/workbench/`): an app-wide "current selection" service (a signal / packet /
  message-type / widget) that every view will observe. Pure `QObject`, no UI. This is what makes
  cross-tier highlight + drill-through (Parsed → source packets in Raw) systematic rather than ad-hoc.
  Unit-tested in isolation.
- **S2 — Signal/message identity service** (`src/workbench/`): assigns each signal a **stable color** from
  the `visual-identity.md` palette and exposes a **quality** state (GOOD/STALE/UNCERTAIN/BAD derived from
  freshness). One source of truth for the swatches/badges every view will show. Unit-tested.
- **S3 — Design tokens v2**: extend `tokens.qss` / the token layer with the named color/spacing/type
  scales the component library consumes (per `widget-styling-guide.md`'s token-consumption rule). No
  visual change yet (nothing consumes them until P1) — but the vocabulary is fixed here.
- **S4 — Component-library skeleton** (`src/workbench/`): the reusable building blocks the frame will be
  composed from (e.g. `StatusChip`, `SectionHeader`, `EmptyState`, `RailButton`) — start with the
  lowest-risk, non-data ones; data-heavy components (DataTable/Inspector/WidgetCard) follow in P1/P2 where
  they're actually mounted. Each with a construction + QSS-class test.

## Out of scope (later phases)

The frame (activity rail / top bar / inspector / drawer) = P1. Parsed identity UI = P2. Raw dissection =
P3. Dashboard view/edit + widget registry = P4. Drill-through wiring = P5. Control reserved; Replay frozen.

## Decisions (decide-and-log)

- New module `src/workbench/` is the redesign's home (services now; frame + components later). Keeps the
  redesign cohesive and lets old modules (`dashboard`, `inspect`, `chart`) be migrated/retired
  incrementally rather than in a big bang.
- SelectionModel uses an enum-tagged `Selection {kind, id}` — string id keeps it decoupled from the
  concrete view types.

## DoD

Build Debug+Release green; ctest green both; ≥70% on the services' public surface; unit tests (no UI
needed for S1/S2); clang-format clean; local commits, no push. No frozen interface/schema touched. No
visual baseline changes in M33 (foundations aren't mounted yet).
