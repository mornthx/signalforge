# SignalForge UI/UX Redesign Proposal (v0.4)

Status: **proposal for owner review** (no code yet). Author: Claude (PM/design hat).
Mandate (owner, 2026-05-30): treat the old UI architecture (`architecture.md §7`) as **reference only —
keep the valid parts**; fix the real problems (dated look, ill-fitting layout, clumsy interaction); take
the strengths of best-in-class tools (**Wireshark** for raw, **Serial Studio** for dashboard); think
systematically as a PM; **optimise for long-term project value, not the simplest short-term build.**

This document is meant to be read slowly and argued with. Section 11 lists the decisions I'm explicitly
leaving open for you.

---

## 1. Why redesign (stated plainly)

The current UI is an honest accretion of correct-but-local milestones. As a *whole product* it has three
real faults the owner named:

- **Dated / generic look.** It reads as a stock-Qt dark app: text-only buttons, no iconography, no
  meaningful accent colour, inconsistent panel chrome (Connections header ≠ Parsed header ≠ tab body), a
  flat jargon status row ("Chart idle", "Mode Live").
- **Layout doesn't fit the work.** Dashboard-only controls live in the global toolbar; there's no home
  for *selection* (no inspector); Control and Replay aren't first-class; the three observation tabs sit
  side-by-side but don't express that they're one pipeline at different depths.
- **Clumsy interaction.** No drill-through between raw ↔ parsed ↔ dashboard; no signal identity carried
  across views; promoting a signal gives no feedback in Parsed; onboarding is a sparse afterthought.

What we are **not** lacking: the `visual-identity.md` manifesto (signal-as-hero, a semantic colour
language, a signal palette, severity/connection states) and a `tokens.qss` token system already exist.
The redesign **applies and extends** them inside a coherent frame — it does not start from zero.

## 2. Who it's for, and the session arc

**Users:** firmware/bring-up engineers, test & production-line engineers. Dark room, long sessions, high
information appetite, keyboard-comfortable, distrustful of anything that hides data.

**The session arc (the spine of the whole design):**

```
   Connect ──▶ Decode/verify ──▶ Observe ──▶ Visualise ──▶ Control ──▶ Record/Replay
   (source)    (does it parse?)  (signals)   (dashboard)   (poke it)   (capture/play)
```

They iterate up and down this arc constantly: tweak a decode rule, glance raw vs parsed, build a widget,
send a command, capture a session. **The UI's first job is to make that arc visible and frictionless.**

## 3. Design principles (the north stars)

1. **One pipeline, many depths.** Raw → Parsed → Dashboard are *views of the same live data at increasing
   abstraction*, not three apps. The design must show the flow and let you move along it.
2. **Identity carries through.** A signal (and a message type) has a stable colour + identity visible in
   every view; selecting it highlights it everywhere. This is the single most differentiating idea.
3. **Controls live where they act** (the scope principle, finally applied everywhere).
4. **Data is the hero; chrome is scaffolding** (inherited from `visual-identity.md` — make it true).
5. **Progressive disclosure.** Empty → connect → data appears → drill in → visualise. Never a wall of
   empty panels.
6. **Keyboard-first power, mouse-friendly discovery.** A command palette and shortcuts for pros; obvious
   affordances for newcomers.
7. **Everything is a saveable project.** Layout, decode rules, filters, colour assignments, command panels
   persist and restore (the architecture already has project files — surface them).
8. **Build a system, not screens.** A token-driven component library reused everywhere, so the product
   stays coherent as it grows (the long-term-value requirement).

## 4. Competitive synthesis — what to take, what to leave

| Tool | Take | Leave |
|---|---|---|
| **Wireshark** (→ Raw tier) | 3-pane *packet list / dissection tree / hex*; a powerful **display-filter bar** (syntax, autocomplete, history, saved filters); **colour rules** per protocol/message; follow-stream; configurable columns; time references. | Its dated toolbar density; menu sprawl; Windows-90s chrome. |
| **Serial Studio** (→ Dashboard tier) | A **grid of heterogeneous widgets** (gauge, bar, plot, LED, terminal, FFT, XY, compass, 3D, map); a clean modern dark aesthetic; **project/JSON-driven** dashboards; per-widget config; drag to arrange. | Its modal-heavy config; its weaker raw/decode story (we're stronger there). |
| **Grafana** | **View mode vs edit mode** separation; a **time-range control scoped to the dashboard**; a panel/widget **library**; variables/templating (future). | Its web/cloud heaviness; over-config. |
| **Saleae Logic / scopes** | **Signal colour identity**; per-signal lanes; **measurement/cursor readouts**; crisp data rendering. | Channel-centric rigidity. |
| **VS Code / Linear** | **Left activity rail** navigation; **command palette (⌘K)**; design-system discipline; collapsible panels; keyboard-first. | Editor-specific concepts. |

The synthesis is not "pick one to clone" — it's: **Wireshark-grade inspection at the bottom, Serial-Studio
-grade visualisation at the top, tied together by an identity/selection model that none of them have**,
inside a VS-Code-class frame.

## 5. The conceptual model: a navigable pipeline with shared identity

Two services make the whole thing feel like one product (these are the long-term backbone, §10):

- **Signal/Packet identity.** On registration, each signal is assigned a stable colour from the
  `visual-identity.md` palette and a quality state (GOOD/STALE/UNCERTAIN/BAD). Each message type gets a
  colour too. These appear as swatches/badges in every view.
- **Selection model.** A single app-wide "current selection" (a signal, a packet, a message type, a
  widget) that every view observes. Selecting in one view highlights/scrolls in the others and feeds the
  **inspector**. This is what makes **drill-through** (§9) systematic rather than a pile of one-off jumps.

## 5b. Division of labor — Raw vs Parsed vs Dashboard (SETTLED 2026-05-30)

The owner asked the load-bearing question: if Raw dissects a packet in-place (Wireshark-style), what is
Parsed *for*? Answer: **they differ by axis, not by "decoded vs not."**

- **Raw's axis is the packet/time axis.** A row is *one message instance* in arrival order; selecting it
  reveals *that packet's* fields (dissection) + bytes. It is about **discrete events on the wire**.
- **Parsed's axis is the signal axis.** A row is *a named quantity* — a running entity with a current
  value + history; it collapses thousands of packets into "each signal's live state."

Both show decoded values, but Raw shows a value *as a property of one packet* (beside its bytes) and Parsed
*as the live state of a signal* (rate, quality, trend, divorced from any packet). Wireshark has no Parsed
equivalent because it's a packet analyzer, not a telemetry monitor — SignalForge is both.

| | **Raw** | **Parsed** | **Dashboard** |
|---|---|---|---|
| A row is | a **packet** (a moment) | a **signal** (a named quantity) | a **widget** |
| Ordered by | arrival time | signal (grouped/filtered) | spatial layout |
| Select shows | this packet's fields + hex | this signal's stats + history | this widget's config |
| Aggregation | none (per-instance) | across all packets (rate/min-max/trend/quality) | rendered |
| **Job** | **debug the wire / decoding** | **monitor signal state** | **present chosen signals** |
| Mirrors | Wireshark | watch-window / SCADA tag list | Serial Studio |

This recovers the valid §7 split: **Raw = the "Decode" workspace** (frames + field structure, long-term the
decode-rule editing surface — where decoding is made *correct*); **Parsed + Dashboard = "Observe"** (monitor,
then present — where decoding is *consumed*).

**Operation logic that follows:** bring-up → live in Raw (tune decode, verify per-packet, single-click =
in-pane dissection + hex, no jump needed); decode trusted → Parsed (survey/monitor/filter all signals);
worth watching → promote to Dashboard.

**Drill-through direction (decided):** because Raw self-dissects, the jump *out of* Raw is the weak one. The
primary link is **Parsed signal → "show source packets" → Raw filtered to that message** ("show me the
bytes behind this signal"), plus **Dashboard widget → Parsed → Raw**. Double-click Raw packet → Parsed is
*optional/secondary*. Newcomer confusion ("is Raw's dissection the parsed data?") is prevented by **visual
framing**: Raw is unmistakably a packet stream (packet no. / timestamps / hex); Parsed is unmistakably a
signal list (one row per signal name, current value, sparkline, rate).

## 6. Proposed frame (navigation & layout)

**Recommendation: an activity-rail frame that expresses the pipeline.**

```
┌───────────────────────────────────────────────────────────────────────────┐
│  ◈ SignalForge   ⟢ udp:m14-smoke ● Live   00:03:21   ⌘K   ◐ theme   ⚙       │  top bar
├──┬────────────────────────────────────────────────────────────┬───────────┤
│⟢ │  Inspect ▸  [ Raw │ Parsed │ Dashboard ]      ⟂ context toolbar          │
│  │ ┌──────────────────────────────────────────────┐ ┌───── Inspector ─────┐│
│▤ │ │ (active sub-view content — e.g. Parsed table) │ │ temperature  ●      ││
│≈ │ │  ● temp     21.0 °C   double   54ms   ▸on dash│ │ udp:m14-smoke-udp   ││
│▦ │ │  ● pressure 98.3 kPa  double   54ms           │ │ double · °C         ││
│⌘ │ │  ○ alarm    false     bool     54ms           │ │ min 18.2 max 24.9   ││
│↺ │ │                                                │ │ rate 50 Hz          ││
│  │ └──────────────────────────────────────────────┘ │ [Add to dashboard ▾]││
│⚙ │                                                    └─────────────────────┘│
├──┴────────────────────────────────────────────────────────────┴───────────┤
│  ▸ Console / Logs / Events / Performance            (collapsible drawer)     │
└───────────────────────────────────────────────────────────────────────────┘
 rail: ⟢ Connect · ▤ Inspect · ≈ Control · ▦ Replay   (⌘ palette · ↺ history · ⚙ settings)
```

Pieces:

- **Top bar (slim, always present):** app mark · **active-connection chip** (name + Live/●REC state, the
  live truth at a glance) · session clock · **command palette (⌘K)** · theme toggle · settings. This
  replaces today's jargon status strip *and* the most-used menus with something meaningful and compact.
  (Full menus remain available, but the bar surfaces what's live.)
- **Left activity rail:** the primary modes, ordered to mirror the data flow. **v1 rail = Connect ·
  Inspect** (+ palette/settings); **Control** keeps a reserved slot (design-only), **Replay** is frozen
  (omitted). Collapsible to icons. The rail answers "where am I"; it scales to new modes without
  tab-cramming.
- **Inspect = the observation triad grouped as one activity.** Its content opens on a **segmented control
  `[Raw | Parsed | Dashboard]`** (the pipeline *depth*), each with its **own context toolbar** (Raw's
  filter bar; Parsed's filter bar; Dashboard's +widget / time-range / edit-mode — the owner's "+Plot
  belongs in the dashboard" point, resolved). Drill-through switches the segment for you.
- **Right inspector (collapsible, contextual):** the home for *selection* — a selected packet's fields, a
  selected signal's metadata + live stats + "Add to dashboard", a selected widget's config. One reusable
  component; the §7.1 "right: property inspector" zone, finally built.
- **Bottom drawer (collapsible):** console / logs / events / performance — the §7.1 bottom zone, on demand.

**Why this over the alternatives:**
- *Flat 6-item rail (Raw/Parsed/Dashboard each top-level):* rejected — it hides that the triad is one
  activity at different depths, and bloats the rail.
- *Keep docks + center tabs (today):* rejected — it's the current frame; doesn't fix scope leakage or give
  selection a home.
- *Pure §7.1 zoned (left tree dock):* the Parsed table already *is* the signal browser, so a permanent
  left signal-tree dock is redundant. We keep §7.1's *intent* (left nav, center work, right inspector,
  bottom logs) but modernise "left" from a tree-dock into an activity rail.

This frame takes the **valid parts of §7** (the zones, the Connect/Decode/Observe/Control/Replay workspace
idea, bottom logs/raw frames) and drops the parts that don't fit (a heavy permanent tree dock; config in
modals on first screen).

## 7. Per-context design

### 7.1 Connect (sources)
A device manager, not a bare list: **connection cards** (name · transport · status dot · throughput
sparkline · last error) + add/edit, recent connections, post-connect handshake setup, and the
**onboarding empty-state lives here naturally** ("Add your first device"). This is also where the
app-level empty state from M32 belongs.

### 7.2 Raw (原报文) — Wireshark-grade
Three-pane: **packet list** (No · Time · Source · Message · Len · Info, message-coloured rows) over a
**dissection tree** (transport → message → fields, schema-driven, click a field → byte range highlights)
and a **hex pane**; a **display-filter bar** (the existing `signalforge_query` engine + autocomplete +
history + saved filters) and **colour rules**. Double-click a packet → drill to Parsed (§9). *(The
dissection tree is the one deferred piece from M31 — it lands here.)*

### 7.3 Parsed (解析数据) — the signal browser
The signal table, elevated: a **colour swatch + quality badge** per signal, a **mini-sparkline** of recent
trend, value · unit · rate · last-change, the filter bar, **group-by-driver**, and — the owner's point —
a clear **"on dashboard" marker** with the row action flipping between *Add to dashboard ▾* and *Remove
from dashboard*. Selecting a row drives the inspector; double-click → drill to Raw filtered to its source
(§9). This is the single global signal browser (M32 decision, kept).

### 7.4 Dashboard — Serial-Studio-grade
A **widget grid** with **view vs edit mode** (Grafana): in view mode it's a clean wall of live widgets; in
edit mode you get the **+widget palette**, drag/resize/push (M29), and per-widget config. A
**dashboard-local toolbar** owns +widget / **time-range** / live / edit (owner's point #1, resolved).
Widget types grow over time (gauge, bar, plot, numeric, state, table → later FFT, XY, LED, terminal, map)
via an **extensible widget registry** (§10) — Serial Studio's real strength. **Per-widget config lives in
the right inspector** (non-modal, live preview) — owner-decided; the M32 modal is retired into the
inspector. (Lands back on what §7.3 #4 wanted.)

### 7.5 Control (reserved) & 7.6 Replay (FROZEN)
**Control** = the *command/control path* — sending commands **to** the device (handshake / polling / manual
dispatch / macros / ACK matching). A distinct core capability (≠ replay control). **Reserved / design-only**
for this redesign: we keep its rail slot in the design but do not build it now.

**Replay** (session playback: timeline / speed / bookmarks) is **FROZEN** by owner decision (2026-05-30) —
out of the v1 frame; not surfaced in the rail. Revisit later.

**v1 rail = Connect · Inspect.** (Control's slot is reserved; Replay is omitted.)

## 8. The visual design system (extend, don't reinvent)

Built on the existing `tokens.qss` + `visual-identity.md`:

- **Colour:** keep signal-as-hero + the **categorical signal palette** (identity) and **semantic states**
  (connection, quality GOOD/STALE/UNCERTAIN/BAD, severity, mode badges). Add **one brand accent** used
  sparingly for primary actions/active nav. Dark-first; the light/screenshot/accessibility theme contexts
  already exist — keep them.
- **Type scale & spacing:** a strict scale (e.g. 8-px rhythm; display/heading/body/caption/mono roles).
  Monospace for hex, ids, raw values; proportional for labels.
- **Iconography:** adopt a single line-icon set (lucide/feather-style) — rail, toolbars, status. No more
  text-only buttons.
- **Components (the library):** one definition each for **Panel/Card, SectionHeader, Toolbar, FilterBar,
  DataTable (with swatch+badge+sparkline cells), Inspector, RailButton, StatusChip, EmptyState, WidgetCard**
  — reused across every context so the product is coherent by construction.
- **Density:** "comfortable-dense" — Wireshark density in Raw/Parsed tables; Serial-Studio breathing room
  in the Dashboard.

## 9. Cross-tier interaction & drill-through (differentiator; several OPEN questions)

The pipeline becomes *navigable* via the selection model. Proposed interactions (★ = my recommendation;
all the specifics below are owner-reserved per the brief):

- **Raw → Parsed:** double-click a packet → switch to Parsed, highlighting the signals that packet
  updated. ★ *(Owner raised exactly this; recommended, but see open Q.)*
- **Parsed → Raw:** signal row → "show source packets" → Raw filtered to that message type.
- **Parsed → Dashboard:** the M32 promote flow, now with the "on dashboard" marker + Remove.
- **Dashboard → Parsed/Raw:** widget → "inspect signal" (Parsed) / "show packets" (Raw).
- **Hover/select anywhere** highlights the same signal/message everywhere (selection model).

These links are what make SignalForge *one tool*; the exact gestures need your call (§11).

## 10. UI architecture for the long term (the value requirement)

To avoid another accretion of patches, the redesign rests on real infrastructure, not screens:

- **Design tokens** (extend `tokens.qss`): all colour/space/type/elevation as named tokens; themes are
  token swaps. No hardcoded styling.
- **Component library** (§8): the small set of reusable widgets; every screen is composed from them.
- **SelectionModel service** (§5): app-wide current-selection, observed by all views — powers drill-through
  + the inspector without per-view glue.
- **Signal/message identity service** (§5): stable colour + quality, one source of truth.
- **Widget registry** (Dashboard): a typed registry so new widget types are *additive* (register → appears
  in the palette + config), enabling the Serial-Studio breadth over time.
- **Workspace/project model:** layout + filters + colour assignments + decode rules + command panels
  persist as a project (architecture already defines project files) — make save/restore first-class.
- **Reuse of the `signalforge_query` engine** as the one filter/search core (Raw, Parsed, later command
  matching), extended with autocomplete + saved filters.

These are the things that make v1.x → v2 cheap instead of another rewrite.

## 11. Decisions & remaining open questions

**Settled (owner, 2026-05-30):**
- ✅ **Raw vs Parsed division of labor** — packet-axis vs signal-axis (§5b).
- ✅ **Drill-through** — primary link is **Parsed signal → source packets in Raw** (Raw self-dissects, so
  the raw→Parsed jump is optional/secondary). (§5b)
- ✅ **Inspect grouping** — segmented `[Raw | Parsed | Dashboard]` inside one "Inspect" rail mode.
- ✅ **Config surface** — per-widget/signal config in the **right inspector** (non-modal, live preview);
  retire the M32 modal.
- ✅ **Frame** — the activity-rail frame (§6).

**Also settled (owner, 2026-05-30):**
- ✅ **Selection scope** — highlight everywhere; jump/filter only on an explicit "show source packets"
  action (no disruptive auto-filter).
- ✅ **Top bar + menu bar (hybrid)** — slim top bar (live connection · Live/REC · clock · ⌘K) *and* a
  compact menu bar for discoverability.
- ✅ **Density** — dense Raw/Parsed tables + breathing-room Dashboard.
- ✅ **v1 scope** — build **Connect + Inspect** (Raw/Parsed/Dashboard) to parity; **Control reserved**
  (design-only); **Replay frozen**.

The proposal is now a **settled spec.** Implementation begins at Phase 0 (§12).

## 12. Phased delivery (right sequence, not cheapest)

A redesign this size ships in coherent phases, each independently green and reviewable — but sequenced for
*correctness of foundation*, not for the quickest visible win:

- **P0 — Foundations (invisible but load-bearing):** design tokens v2, the component library, the
  SelectionModel + identity services. *Nothing ships visually yet; everything after is cheap.*
- **P1 — The frame:** activity rail + top bar + inspector + drawer shell; port the existing Raw/Parsed/
  Dashboard into it (no feature change). Regenerate the whole visual-baseline set here (one wholesale
  re-accept).
- **P2 — Parsed + identity:** swatches, quality badges, sparklines, "on dashboard" marker, selection →
  inspector. Toolbar relocation (dashboard-local) lands with the frame.
- **P3 — Raw to Wireshark parity:** dissection tree + hex highlight + colour rules + filter autocomplete.
- **P4 — Dashboard to Serial-Studio parity:** view/edit mode, widget palette, widget registry, time-range.
- **P5 — Drill-through:** wire the cross-tier links via the selection model (Parsed → source packets in
  Raw; Dashboard → Parsed → Raw).
- **Control** — reserved (design-only); built when scheduled, in its rail slot. **Replay — frozen.**

(P0 before anything visible is exactly the "not the simplest short-term thing" the owner asked for: it
front-loads the infrastructure that keeps every later phase small.)

## 13. The baseline regime changes

The pixel baselines are a **regression net, not a design spec.** For this redesign they are **regenerated
wholesale** at P1 and per-phase thereafter; they will not constrain the design. (They keep doing their real
job — catching *unintended* drift between phases.)

---

### What I need from you
Read §6 (the frame), §9 + §11 (the open interaction questions), and §12 (sequence). Tell me where you
disagree, which §11 questions you want to decide now vs. defer, and whether the activity-rail frame is the
direction. I'll revise this doc to a settled spec before writing any code.
