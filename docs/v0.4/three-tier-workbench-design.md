# Three-Tier Workbench — Design (v0.4)

Status: **active**. Origin: [DR-002](../../.claude/direction-review/DR-002-2026-05-30-three-tier-workbench.md).
Supersedes the single-surface assumption of `docs/v0.3/dashboard-interaction-design.md` (that doc's
dashboard remains valid as **Tier 3 / the Observe tab**).

## 1. Goal

Give a bring-up engineer the three things the architecture (§7.2) always intended, as switchable
workspaces over one live data flow:

```
   wire bytes ──► [Tier 1: 原报文 / Raw]  ──► [Tier 2: 解析数据 / Parsed] ──► [Tier 3: Dashboard]
   FramePipeline      packet list +            signal table +                 cards / charts /
   (frozen, M4)       dissection + hex          filtering                     gauges (M21–M28)
                      "向 Wireshark 看齐"        "尤其是筛选"                   "page on top of raw"
```

Data flows bottom-up; each tier is a *view* on data the pipeline already produces. None of the tiers
invents data — Tier 1 shows `RawFrame`s already flowing through `FramePipeline`, Tier 2 shows the
`SignalBuffer` values decoders already write, Tier 3 visualizes Tier 2.

## 2. Navigation (architecture §7.1 / §7.2)

A **tabbed center workspace** in the `QMainWindow`. v0.4 tabs, left→right in pipeline order:

- **Raw** (Tier 1) — Phase C.
- **Parsed** (Tier 2) — Phase B.
- **Dashboard** (Tier 3 / "Observe") — exists; reframed in Phase A.

Left dock keeps the signal tree; right dock is the per-view inspector (§7.1). **Drill-through** is the
glue: select a frame in Raw → highlight the signals it produced in Parsed → "Add to dashboard" promotes
one into Tier 3. (Drill-through wiring lands incrementally; not required for Phase A.)

Phase A introduces the tab shell with only **Dashboard** populated, so B and C are purely additive
(`addTab`), never a rewrite.

## 3. Tier 3 — Dashboard (Phase A, M29)

The Observe tab. Fixes the four DR-002 reports as one coherent model: **the dashboard is a composed
layer; per-signal intent persists independently of whether a widget currently exists.**

- **Persistent intent map** `signalId → {PanelType, config}` on the Dashboard, surviving widget removal.
  - Promote (check / "Add to dashboard"): use the remembered type; first time, `suggestPanelType`.
  - Demote (uncheck): remove the signal from every panel **and delete any panel left empty** — including
    multi-signal Plot/Table when their last signal goes. (Fixes report 1.)
  - Re-promote: restore the remembered form. (Fixes report 2.)
  - The signal-list checkbox becomes one honest boolean: "on the dashboard or not."
- **Bounded free+push layout** (fixes report 3): panels keep pixel-precise geometry (M28), but on
  drag/resize they **push** overlapping neighbors; pushes **clamp at the viewport edge** and a push with
  no remaining room is **refused** (the dragged panel stops). No cascade past the surface, no scroll-off.
- **Re-root as Observe** (fixes report 4): dashboard becomes a tab; raw/parsed tabs become the default
  observation surfaces in B/C, making the dashboard visibly a layer *on top of* raw data.

## 4. Tier 2 — Parsed signals + filter engine (Phase B, M30)

- A signal **table**: one row per signal — id · value · unit · quality (`GOOD/STALE/UNCERTAIN/BAD`,
  §6.1) · update rate · last-change time. Always complete, zero-config; the default landing surface.
- **Filter engine** (the reusable core of "向 Wireshark 看齐 … 尤其是筛选"): a display-filter expression
  evaluated per row. Grammar (incremental): `field op value` with `==,!=,<,<=,>,>=,contains`, combined
  with `&&`/`||`/`!` and parentheses. Phase B ships the parser + evaluator + a filter bar; Phase C reuses
  it verbatim against frame fields. Lives in a new leaf module (e.g. `src/query/`), no UI deps, unit-tested
  in isolation.

## 5. Tier 1 — Raw packets, Wireshark-style (Phase C, M31)

Three-pane Decode workspace (§7.2):

- **Packet list** — one row per `RawFrame`: No. · Time · Source(driver) · Protocol(message type) ·
  Length · Info(summary). Virtualized for high frame rates; backpressure-aware.
- **Dissection tree** — expandable per selected frame: transport → message type → fields, driven by the
  **schema/decode rule** (SignalForge's "protocol" is the message definition). Field selection highlights
  the matching bytes.
- **Hex pane** — raw bytes with the selected field's range highlighted.
- **Display filter** — the Phase-B engine over frame fields (`driver == "udp" && len > 8`).

Deferred Wireshark niceties: color rules, follow-stream, time references, capture-file export.

## 6. Constraints honored

- `FramePipeline` (M4) feeds Tier 1 read-only; if a tap is needed it is added without breaking the frozen
  signature where possible, and if not, thawed deliberately per `freeze_thaw_authorized`.
- No new third-party dependency (architecture §4.1): packet list / table / tree all use Qt Model/View
  (§4.1 "Qt Model/View plus custom core data layer"); the filter engine is hand-rolled C++ (ExprTk is
  reserved for DerivedSignal only).
- GUI work is tested by simulating real interaction (QTest mouse/menu/keyboard), per the standing rule.
