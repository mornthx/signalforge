# Dashboard & heterogeneous-widget interaction design (target)

Status: **Proposal** — drafted in response to DR-001 (`.claude/direction-review/DR-001-2026-05-29-ui-visualization.md`).
Parts of this break the frozen M8 `Chart` interface and therefore require explicit human
approval before implementation (CLAUDE.md HALT #4 / disagreement rule). This document is the
*target*; it is not yet a milestone plan.

## 1. The problem this fixes

Today the central area is a vertical stack of one widget type — a shared-axis line chart
(`MainWindow::chartLayout_`, `chart/chart.cpp`). A bring-up operator's most common questions
are **"what is X right now?"**, **"what state is the FSM in?"**, **"are these 8 values sane?"**
— none of which a line trace answers well, and one of which (string/enum) it cannot answer at
all. We force every signal through an oscilloscope.

The fix is a **conceptual** one, borrowed from serial-studio: stop treating "the chart" as the
output. The output is a **dashboard** — a grid of **panels**, each bound to one or more signals
and rendered by a **widget chosen to fit that signal's type and semantics**. The line plot
becomes *one* panel type among several.

## 2. Mental model

```
Dashboard
 └─ Panel  (a card in the grid)
     ├─ binding: 1..N signalIds (from SignalBufferRegistry)
     ├─ widgetType: Plot | Numeric | State | Table | Bar | Gauge | (future: XY, FFT)
     └─ view options: title, per-signal color, unit override, range, decimals, ...
```

- A **signal** stays exactly what it is today: an id `<driverId>/<field>` in
  `SignalBufferRegistry`, with `SignalMetadata { name, unit, type }` and a ring buffer
  exposing `queryLatest(n)` and `queryRange(start,end,target)`.
- A **panel** is the new unit of layout. It is *not* the chart — the chart is one
  implementation of the panel interface.
- The **dashboard** owns the grid layout, add/remove/resize, and the single shared
  refresh driver (replaces each Chart's private 30 Hz `QTimer`).

This is the smallest change that unlocks the workbench: **"signal → choose how to show it"**
instead of **"everything is a line"**.

### 2.1 Per-panel configuration (decision: zero-config default, per-panel override)

A panel works with **zero configuration**: on creation it auto-derives everything from the
signal — widget type from `SignalMetadata.type`, unit label from `SignalMetadata.unit`, and
value range from the data itself. The user can then override any of it **per panel**
(the same signal may appear in two panels with different settings):

| Setting | Default (zero-config) | Override |
|---------|-----------------------|----------|
| Data interpretation / display mode | auto from `SignalType` (Bool→state, scalar→numeric/plot, String→state) | per-panel choice of widget + mode |
| **min / max (range)** | **none set → use the signal's actual observed min/max** (running over the buffered window) | explicit fixed min/max per panel |
| Unit label | `SignalMetadata.unit` | per-panel override string |
| Decimals / format | sensible default per type | per-panel |
| Color / value→label map (enum) | auto palette | per-panel |

The **range rule is the key one**: a Bar/Gauge/Plot with no configured min/max auto-scales to
the actual data; set an explicit range and it pins to that (so "0–100 %" stays 0–100 % even
when live data only spans 40–60 %). This resolves the old §6 gap **without** touching the
frozen `SignalMetadata` — range lives in *panel config*, never in signal metadata.

## 3. Widget catalog

Default widget is auto-suggested from `SignalMetadata.type`; the user can always override.

| Widget | Best for | Auto-suggested when | Data access | New frozen-iface change? |
|--------|----------|---------------------|-------------|--------------------------|
| **Numeric** | a scalar you watch ("3.31 V") | Double/Int64, slow-changing | `queryLatest(1)` + small window for spark/Δ | none |
| **State / LED** | boolean or enum/string state | Bool, String | `queryLatest(1)` | none |
| **Table / DataGrid** | many signals' current values at a glance | multi-select, mixed types | `queryLatest(1)` per row | none |
| **Plot** (new `PlotPanel`) | trends over time | Double/Int64, fast-changing | `queryRange(...)` | none — parallel new class; M8 `Chart` kept as legacy |
| **Bar** | scalar vs. a range | Double/Int64 | `queryLatest(1)` + per-panel range | none |
| **Gauge** | scalar vs. range, analog feel | Double/Int64 | `queryLatest(1)` + per-panel range | none |

Key consequence (post-decision, DR-001 §9): **no panel type requires breaking a frozen
interface.** Numeric / State / Table are new widgets reading the same registry. **Plot is a
*new* `PlotPanel` built in parallel** — the frozen M8 `Chart` is retained as legacy under
CLAUDE.md's "add new interfaces alongside the old ones" rule, not modified. Bar/Gauge get
their range from per-panel config (§2.1 / §6), so they are no longer blocked.

### 3.1 Numeric panel

```
┌──────────────────────────────┐
│ Battery voltage          ⋮    │   ← title (metadata.name), ⋮ = panel menu
│                               │
│        3.314 V                │   ← big value + unit (metadata.unit), color-coded
│   ▁▂▃▅▆▅▃▂  ▲ +0.02           │   ← optional sparkline + Δ since N s ago
│   min 3.28  max 3.41          │   ← optional running min/max (session)
└──────────────────────────────┘
```

### 3.2 State / LED panel

```
┌──────────────────────────────┐
│ FSM state                ⋮    │
│   ●  RUNNING                  │   ← string value verbatim; color from value map
└──────────────────────────────┘
┌──────────────────────────────┐
│ Fault flag               ⋮    │
│   ○  clear   (false)          │   ← bool: ○/● + label; red when true if configured
└──────────────────────────────┘
```

### 3.3 Table / DataGrid panel

```
┌─────────────────────────────────────────────┐
│ udp:sensor-rig — live values            ⋮    │
│ Signal          Value      Unit   Updated    │
│ temperature     24.7       °C     0.1s ago   │
│ pressure        101.3      kPa    0.1s ago   │
│ alarm           false      —      0.3s ago   │
│ fsm_state       RUNNING    —      0.1s ago   │
└─────────────────────────────────────────────┘
```

A Table bound to a whole driver answers "is this device sane?" in one card — the single most
useful bring-up view, currently impossible.

### 3.4 Plot panel (new `PlotPanel`, parallel to legacy `Chart`)

```
┌────────────────────────────────────────────────────┐
│ Temperature & setpoint                          ⋮   │
│ ── temperature  ── setpoint        (colored legend) │   ← legend maps color→signal
│ 30°C┤        ╭──────╮                                │   ← Y axis ticks + unit
│ 25°C┤   ╭────╯      ╰───                             │
│ 20°C┤───╯                                            │
│     └────┬─────┬─────┬─────┬──→                      │
│        -8s   -6s   -4s   -2s  now                     │   ← X axis time labels
└────────────────────────────────────────────────────┘
```

`PlotPanel` is a **new class built in parallel** with the legacy `Chart` (decision DR-001 §9
#1) — the frozen M8 `Chart` is left untouched and kept as legacy. The new plot does what the
old one couldn't:
- Y axis: tick labels + scale numbers + unit; **per-signal axis or normalize** option
  (the legacy chart shares one auto-scaled Y — `chart.cpp:320-360`).
- X axis: time tick labels.
- In-canvas colored legend (legacy shows only a grey text line in the panel header,
  `main_window.cpp:1252`).
- Configurable line width, default ≥ 2 px (legacy hardcodes `setLineWidth(1)`).
- Theme-aware colors (legacy hardcodes `tokens::light` + a white cursor — `chart.cpp:65,525`).
- Honors the §2.1 per-panel range (fixed min/max, else observed).

It may privately reuse the legacy chart's scene-graph rendering internals, but it does **not**
modify `Chart`'s public (frozen) surface — so no approval gate. Once `PlotPanel` is proven, the
legacy `Chart`/`ChartManager` path can be retired in a later, separate step.

## 4. Layout & interaction

### 4.1 Screen

```
┌──────────────────────────────────────────────────────────────────────┐
│ File   Connection   View   Session   Help        (owned menu, §7 / DR) │
├───────────┬──────────────────────────────────────────────────────────┤
│ Signals   │  Dashboard                              [+ Panel ▾] [⚙ Edit]│
│ ┌───────┐ │  ┌─────────────┬─────────────┬────────────────┐           │
│ │filter │ │  │ Numeric     │ State/LED   │ Numeric        │           │
│ └───────┘ │  │ 3.314 V     │ ● RUNNING   │ 24.7 °C        │           │
│ ▾ udp:rig │  ├─────────────┴─────────────┴────────────────┤           │
│   ☑ temp  │  │ Plot: temperature & setpoint               │           │
│   ☐ press │  │  (trends)                                  │           │
│   ☐ alarm │  ├────────────────────────────────────────────┤           │
│ ▾ Derived │  │ Table: udp:rig live values                 │           │
│   ☐ slope │  │  temperature 24.7 °C · pressure 101.3 kPa…  │           │
└───────────┴──┴────────────────────────────────────────────┴───────────┘
  Connection ● 1/1   ·   Mode Live   ·   Buffer 3%        (slimmed, §7 / DR)
```

The left **signal selector** stays (it already groups by driver and filters —
`signal_selector.cpp`), but its checkbox semantics change (§4.3). The right area is the
**dashboard grid**, not a vertical chart stack.

### 4.2 Adding a panel

Two equivalent paths (consolidated — today there is no panel concept, only `+ Chart`):

1. **`+ Panel ▾`** on the dashboard → choose widget type → choose signal(s). The dialog
   pre-selects the auto-suggested widget for the chosen signal's type.
2. **Drag a signal** from the selector onto the dashboard → a panel with the auto-suggested
   widget appears; drop onto an existing panel to add the signal to it.

### 4.3 Selector checkbox semantics

Today checking a signal adds it to the *active chart* (`signal_selector.cpp:139`). New rule:
checking a signal means **"show this signal somewhere"** — if no panel hosts it, create one
with the auto-suggested widget; unchecking removes it from all panels. "Active panel" replaces
"active chart" for routing. This keeps the one-click path while supporting heterogeneous output.

### 4.4 Edit vs. run mode

A lightweight **Edit** toggle (`⚙ Edit`) makes panels draggable/resizable and shows the
`+ Panel` affordances; off, the dashboard is a clean read-only instrument wall. (serial-studio
splits "project editor" from "dashboard"; we fold it into one in-place toggle to honor charter
Goal #5 — no separate config artifact to learn.)

## 5. Component architecture (proposed)

```
src/dashboard/
  panel.hpp              // abstract: binding + widgetType + draw; replaces "Chart is the unit"
  panel_factory.{hpp,cpp}// type -> concrete panel; auto-suggest from SignalType
  dashboard.{hpp,cpp}    // owns grid layout, add/remove/resize, ONE shared refresh tick
  panels/
    numeric_panel.{hpp,cpp}
    state_panel.{hpp,cpp}
    table_panel.{hpp,cpp}
    plot_panel.{hpp,cpp} // NEW plot; may reuse legacy Chart SG internals privately,
                         //   but does not modify Chart's frozen public API
```

- `Dashboard` subsumes `ChartManager`'s ownership role. The legacy `Chart`/`ChartManager`
  stay in place, unmodified, until `PlotPanel` is proven and they are retired separately
  (decision DR-001 §9 #1 — parallel, not in-place rewrite).
- One shared refresh driver (e.g. 30 Hz for plots, throttled ~5–10 Hz for numeric/state/table
  which don't need 30 Hz) replaces N private `QTimer`s in `Chart` — also kills the
  `cursorYAlternate` RHI-coalescing hack for non-plot panels.
- Panels read the **same** `SignalBufferRegistry`; live vs. replay already routes through it
  (`PlaybackController` dispatches into the same registry), so **all panel types work in
  replay for free** — a real payoff of the existing pipeline design.

## 6. Data-model integration & gaps

| Need | Source today | Gap / action |
|------|--------------|--------------|
| current value | `SignalBuffer::queryLatest(1)` | exists ✔ |
| trend window | `SignalBuffer::queryRange(...)` | exists ✔ |
| name, unit | `SignalMetadata.name/.unit` | exists ✔ (unused by chart today) |
| type → widget suggest | `SignalMetadata.type` | exists ✔ |
| **min/max for Bar/Gauge/Plot** | — | **resolved (decision):** range lives in **per-panel config**, not in `SignalMetadata` (which stays frozen, untouched). No range set → use the signal's **observed** min/max over the buffered window (auto-range). Explicit range pins the axis. See §2.1. |
| enum value → label/color | — | optional per-panel value map in panel config; no frozen change. |

**Phasing implication:** Numeric + State + Table + the new `PlotPanel` cover the DR-001 gaps,
and **none of them touch a frozen interface** — Plot is a parallel new class, range is
per-panel config. Bar/Gauge are unblocked.

## 7. Out of scope here (tracked in DR-001, separate work)

Menu-bar rewrite and status-bar consolidation are real DR-001 findings but are IA fixes, not
visualization; they get their own design when scheduled. Noted so this doc stays focused.

## 8. Suggested phasing (for a future milestone plan)

Every phase is additive — none modifies a frozen interface.

1. **P0:** `Dashboard` + `Panel` abstraction; wrap the legacy chart as a `plot_panel` so the
   grid works end-to-end; add **Numeric** and **State** panels with per-panel config (§2.1);
   drag-to-add + auto-suggest. Delivers "see a value / see a state" — the biggest gap.
2. **P1 — Table panel** (per-driver live values).
3. **P2 — new `PlotPanel`** (axes/labels/legend/per-signal-Y, per-panel range). Parallel to
   legacy `Chart`; no freeze break.
4. **P3 — Bar / Gauge** (per-panel range with observed-min/max fallback).
5. **P-later — retire** legacy `Chart`/`ChartManager` once `PlotPanel` is proven.

## 9. Decisions (resolved 2026-05-29)

1. **Plot:** build a **parallel new `PlotPanel`**; keep frozen M8 `Chart` as legacy, retire
   later. (No freeze break — removes the prior approval gate.)
2. **Range / config:** **zero-config by default; per-panel override** of data
   interpretation, min/max, unit, etc. **No min/max set → use the signal's actual observed
   min/max.** Range lives in panel config, never in frozen `SignalMetadata`. (§2.1)
3. **Edit model:** in-place Edit toggle (§4.4) is **approved, but provisional** — kept under
   observation; revisit if the real implementation feels wrong in use.
