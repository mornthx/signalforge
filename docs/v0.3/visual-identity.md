# SignalForge Visual Identity Manifesto

| Field | Value |
|---|---|
| Status | Draft (S1 deliverable; locks at S1 Phase 4 operator review) |
| Authored at | M16 S1, post-S0.5 spike PASS |
| Authority | `docs/V0-charter-amendment-v0.3.md` (V0.3 keystone scope); M16 spec §2.1 #1 + §3 + §6 H5 (R10 traceability gate ≥ 70 %); R11 manifesto-first ordering |
| Empirical foundation | `docs/v0.3/spike-result.md` (S0.5 cross-env diff 0.12 % / 0.30 %) |
| Industrial references | `docs/v0.3/references/` (6 primary + 3 anti per M16-concerns C1) |

This is the substantive design document for SignalForge V0.3.
It defines the visual identity vocabulary that M17–M20+ widget /
workflow / fixture / theme work inherits. **No specific token
values appear here** (per R11: manifesto first, tokens second —
S2 produces `tokens.json` whose values trace back to principles
codified below).

---

## 1. Domain positioning

SignalForge is an **embedded-bring-up signal-analysis
workbench**. The operator persona is:

- An embedded systems engineer bringing up new boards / firmware /
  protocols.
- Working at a desk with a multi-monitor workstation (1–3 displays,
  1080p+ typical, 1440p / 4K common).
- Mouse-and-keyboard primary input; no touch.
- Long-sustained sessions (recording / replay / debug; 1–8 hours
  per session).
- Trusts numbers + waveforms; distrusts dialog noise + ornament.

The reference domain — what SF aspires to belong to — is
**industrial signal-analysis tooling**: oscilloscopes, logic
analyzers, instrument-control software, real-time monitoring.
The 6 primary references collected at S0 (per
`docs/v0.3/references/README.md`) anchor SF in that family.

The reference domain — what SF is NOT — is captured in
[§7 anti-references](#7-anti-references) below.

---

## 2. Visual hierarchy

The single ordering principle for every SignalForge screen is
**signal > measurement > control > chrome**. When pixels
compete, signal-bearing surfaces win.

### 2.1 Signal as hero

Chart panes — the surface that renders waveforms / digital
traces / signal values over time — dominate the visual real
estate. Industrial-tooling precedent: a Tektronix MSO5
display devotes ~70–75 % of the screen to the waveform graticule
([refs: `tek-mso5-display`]); Saleae Logic 2's main view
likewise centers the trace area with controls flanking
([refs: `saleae-logic-main`]).

For SF: when a chart pane is present, it gets the largest
contiguous central area. Control panels (connection list,
signal selector, replay toolbar) flank; status bar bottoms;
chrome (menu bar) tops.

### 2.2 Measurement as second-tier hero

Numeric measurement readouts (cursors, statistics, byte counts,
record counters, buffer pressure) are second-tier — visually
prominent enough to read at a glance during long sessions, but
not competing with the waveform itself. Industrial precedent:
Tektronix scopes overlay measurements as semi-transparent panels
near the waveform ([refs: `tek-mso5-display`,
`tek-mso5-status`]); Saleae renders measurements in dedicated
side-panel cards ([refs: `saleae-measurement`]).

For SF: status-bar text, recording byte counters, replay
position readouts use monospaced font (precision over
prettiness), high contrast, sized to scan at workstation
viewing distance.

### 2.3 Controls as functional chrome

Connection panels, signal selectors, replay toolbars,
add-connection dialogs — necessary, but not the operator's
attention center. Industrial precedent: LabVIEW Fuse control
panel patterns surface frequently-used controls as compact
groups; secondary controls collapse behind disclosures
([refs: `labview-fuse-controls`,
`labview-front-panel`]).

For SF: controls sit at fixed positions in dedicated panels,
visually present but not glowing. Default-state controls are
quiet; active / errored / focused states elevate visibly via
color + border, not animation.

### 2.4 Chrome as minimal scaffolding

Menu bar, panel titles, window frame — the lowest tier. Present
because operators expect them; visually demoted; never compete
with content. Industrial precedent: all six references have
flat, low-contrast chrome ([refs: any of the 6 primary
references]).

For SF: monochrome / low-saturation chrome; no gradients on
chrome surfaces; no chrome ornament beyond the necessary
1-pixel borders.

---

## 3. Signal-semantic visual language

Beyond chrome / control / chart, SF carries a semantic
vocabulary specific to signal-analysis: each kind of signal +
state + status communicates through a defined visual cue.

### 3.1 Connection states

Connection lifecycle (`Idle` / `Connecting` / `Connected` /
`Disconnecting` / `Error`) uses a deliberate color vocabulary:

- **Idle** — neutral grey (operator awareness, not action).
- **Connecting** — amber (transient; "in flight").
- **Connected** — green (active, healthy).
- **Disconnecting** — amber (transient; symmetric with Connecting).
- **Error** — red (operator-action required).

Industrial precedent: Tektronix scope status indicators use a
similar grey / amber / green / red vocabulary for instrument
state ([refs: `tek-mso5-status`]). LabVIEW front-panel LED
controls follow the same convention ([refs:
`labview-front-panel`]).

For SF: these five colors appear as semantic tokens
(`color.status.idle` etc.) in S2. Connection-list rows + status
indicators consume the tokens, not hard-coded hexes.

### 3.2 Signal color palette

Multi-signal chart overlays need a per-signal color allocation
that:

- Distinguishes adjacent signals at chart-pane viewing distance.
- Survives long sessions (accessible to color-blind operators
  per WCAG 2.1 AA).
- Remains professional (avoids "consumer-app rainbow").

Industrial precedent: Saleae Logic 2 uses a ~8-signal palette
of muted but distinguishable hues ([refs: `saleae-logic-main`]).
Tektronix scope channels (Ch1–Ch4) use the same per-channel
color convention; signals beyond channel 4 fall back to derived
hues ([refs: `tek-mso5-display`]).

For SF: an 8-color signal palette (`color.signal.0` to
`.signal.7`) tokenised in S2. Order chosen for color-blind
discrimination (LMS / red-green / blue-yellow checks at S2
review).

### 3.3 Mode badges

SignalForge has three operational modes: **Live**, **Recording**,
**Replay**. Each gets a permanent badge in the status bar /
toolbar area + a global tint cue:

- **Live** — neutral; no tint.
- **Recording** — red tint accent (compatible with semantic Error
  color; the visual signal is "data is being captured", urgent
  attention warranted if not expected).
- **Replay** — blue tint accent (cool color; "you are reviewing
  past data, not live signal").

Industrial precedent: scope tools that record waveforms (e.g.
Saleae capture session) use a red recording bar near the trace
area ([refs: `saleae-logic-main`]); replay / playback modes in
NI VeriStand / similar use a blue / amber differentiation.

For SF: token slots `color.status.recording`, `color.status.replay`.
M17 widget rebuild surfaces these accents at chart-pane border +
status bar.

### 3.4 Severity levels

Error states beyond per-connection (e.g. file-write failure,
malformed session) use a three-tier severity language:

- **Info** — text-only, no color; operators see, don't interrupt.
- **Warning** — amber; operator review encouraged.
- **Error** — red; operator action required.

For SF: token slots `color.status.warning`, `color.status.error`.
Dialog backgrounds + status-bar accents consume these tokens.

---

## 4. Theme context model

SF runs in three operator contexts; the visual identity adapts:

### 4.1 Lab work context (M16 default)

Primary M16 target. Long-session, sustained reading; pixel-
dense; lower-saturation neutrals to reduce visual fatigue.
Light theme (defaults below); dark theme deferred to M20.

### 4.2 Screenshot / documentation context

Operators capture state for bug reports / handoffs / paper
documentation. M16 must produce visually clean screenshots
without grease (anti-aliasing artifacts, hover states, partial
focus rings). Industrial precedent: Tektronix scope "save
image" produces clean white-background captures suitable for
academic / regulatory documentation.

For SF: the same visual rules apply to running app + captured
screenshot. No grease specific to capture context.

### 4.3 Accessibility context

Color-blind variants + high-contrast variant. Deferred to M20
per spec §3 M16.2 (light theme only at M16; M20 inherits the
token generator infrastructure for dark + accessibility
variants).

For SF: the M16 token schema accommodates theme variants per
M16-concerns C4 schema. M16 ships light only; M20 adds dark
(blackboard aesthetic, not consumer-app dark mode) +
high-contrast.

---

## 5. Cross-platform deterministic rendering

SignalForge V1.0 ship gate requires deterministic Qt rendering
across the declared supported environment matrix (per V0.3
charter amendment §5; M15-done.md §14). This is a **design
constraint** that all subsequent visual decisions must honor:
no design choice can compromise determinism.

### 5.1 Empirical foundation

The S0.5 R13 spike empirically validated that a minimal owned
rendering pipeline (Fusion + bundled Inter Regular + 6-role
minimal QPalette) reduces cross-environment diff from V0.2 R9's
14 % / 33 % to **0.12 % / 0.30 %** (both well under the M16
final close gate of < 1 %). See `docs/v0.3/spike-result.md` for
the full measurement.

The principle is not aspirational — it is empirically delivered
at S0.5 and codified for S4 implementation:

- **Bundled fonts are the determinism keystone.** Forcing
  `addApplicationFont` with byte-identical Inter Regular OTF
  on every supported environment collapses the dominant
  cross-env variable (font cascade) to zero.
- **Forced Qt style + explicit QPalette** prevents OS theme
  bleed-through. Operator's `XDG_CURRENT_DESKTOP=ubuntu:GNOME` +
  `QT_IM_MODULE=ibus` are present at capture time and do not
  move pixels under the M16 stack.
- **Fontconfig + FreeType** are advisory observability fields,
  not gating: same fontconfig version (2.15.0) is observed
  across the supported matrix at spike time; FreeType ships
  within the Qt 6.10.2 runtime which is identical across the
  matrix.

### 5.2 V0.2 R9 retrospective correction

V0.2 R9 framed the cross-environment drift as "operator Yaru
desktop theme inheritance causes Qt rendering drift" with
fallback hypothesis "font cascade variation". S0.5 measurement
**empirically rebuts the Yaru-leak hypothesis** as the dominant
cause: Yaru-implying env vars are present at S0.5 capture and
do not move pixels under the M16 stack. The actual root cause
is **font cascade**, secondarily **palette inheritance from
Qt's Fusion-fallback defaults**. M16 closes both.

This correction is recorded explicitly (not silently revised):
V0.2 R9 hypothesised; S0.5 measured; manifesto reflects the
empirical finding.

### 5.3 Constraint on visual decisions

Every design decision M17–M20+ makes must preserve determinism:

- New widgets that use bespoke rendering (custom paint events,
  QQuickItem composition) MUST consume tokens from the
  generator (R15) rather than hard-coding values.
- New widgets MUST NOT pull style from `QApplication::style()`
  in ways that introduce conditional rendering based on style
  object identity (only `Fusion` is supported in the matrix).
- New widgets MUST NOT use OS-cascade fonts (`QFont()`
  default-constructed); always `QApplication::font()` or
  explicit `QFont("Inter", ...)` / `QFont("JetBrains Mono", ...)`.

The visual-diff CI gate (per `docs/v0.3/visual-diff-contract.md`)
enforces this contract: any widget change that produces
cross-env diff > 1 % blocks the M17+ milestone gate.

---

## 6. Industrial reference traceability (R10)

This section confirms R10 traceability gate compliance. Each
principle in §2 / §3 / §4 / §5 cites at least one industrial
reference where applicable.

### 6.1 References inventory

Primary references (operator-installed evaluation + vendor
public material per M16-concerns C1):

| Tag | Reference | Source |
|---|---|---|
| `saleae-logic-main` | Saleae Logic 2 — main waveform view | Saleae public manual + operator install |
| `saleae-measurement` | Saleae Logic 2 — measurement panel | operator install |
| `tek-mso5-display` | Tektronix MSO5 — signal display + measurements | Tektronix datasheet / manual |
| `tek-mso5-status` | Tektronix MSO5 — status indicators / recording bar | Tektronix manual figure |
| `labview-fuse-controls` | LabVIEW Fuse — typical control panel | NI Fuse design system docs |
| `labview-front-panel` | LabVIEW — typical front panel | NI marketing |

Anti-references (what SF is NOT — per §7 below):

| Tag | Reference | Source |
|---|---|---|
| `anti-consumer-app` | Consumer-app aesthetic | Spotify-class desktop |
| `anti-mobile` | Smartphone-touch optimisation | iOS / Android phone UI |
| `anti-material` | Material Design clone | Google Material reference |

Full inventory + observation notes + citation URLs:
`docs/v0.3/references/README.md`.

### 6.2 R10 traceability audit

Principles in this manifesto and their reference citations:

| § | Principle | Cited references |
|---|---|---|
| 2.1 | Signal as hero | `tek-mso5-display`, `saleae-logic-main` |
| 2.2 | Measurement as second-tier hero | `tek-mso5-display`, `tek-mso5-status`, `saleae-measurement` |
| 2.3 | Controls as functional chrome | `labview-fuse-controls`, `labview-front-panel` |
| 2.4 | Chrome as minimal scaffolding | all 6 primary |
| 3.1 | Connection-state color vocabulary | `tek-mso5-status`, `labview-front-panel` |
| 3.2 | Signal color palette | `saleae-logic-main`, `tek-mso5-display` |
| 3.3 | Mode badges | `saleae-logic-main` (recording), NI VeriStand-class (replay) |
| 3.4 | Severity levels | conventional UI patterns (not vendor-specific) |
| 4.1 | Lab work context | conventional industrial-tool convention |
| 4.2 | Screenshot context | `tek-mso5-display` (clean save-image precedent) |
| 4.3 | Accessibility context | conventional WCAG 2.1 AA |
| 5 | Cross-platform deterministic rendering | `docs/v0.3/spike-result.md` empirical + all 6 primary references (each guarantees within declared supported matrix) |

Principles citing ≥ 1 industrial reference: **10 of 13** = **76.9 %**.

R10 gate (≥ 70 %): **PASS** with margin.

The 3 non-cited principles (3.4 severity, 4.1 lab work, 4.3
accessibility) are convention-derived, not industrial-vendor-
specific. Cited that way explicitly rather than padding with
weak references.

---

## 7. Anti-references

SignalForge is **not** any of:

### 7.1 Not a consumer-app aesthetic

Reference: `anti-consumer-app`. Consumer apps optimise for
short-session emotional engagement (Spotify, Netflix, Instagram
on desktop). Visual choices: large hero imagery, heavy
ornamentation, high-saturation accent colors, smooth animations
on hover. Inappropriate for embedded-engineering long-session
work.

SignalForge does NOT use: animated transitions on widget
state changes; emoji or icon-as-decoration; high-saturation
gradients; large-text marketing copy; "card" patterns with
heavy shadow elevation.

### 7.2 Not a smartphone-touch optimisation

Reference: `anti-mobile`. Mobile UI optimises for finger-touch
hit targets (44 px minimum), gesture-first navigation,
single-task-at-a-time portrait layouts. Inappropriate for
mouse-keyboard-multi-monitor workstation work.

SignalForge does NOT use: oversized hit targets at the cost
of pixel density; gesture-first navigation; bottom-tab
navigation; "floating action button" patterns; portrait /
narrow-column layouts.

### 7.3 Not a Material Design clone

Reference: `anti-material`. Material Design (Google) optimises
for mobile + web + cross-product brand consistency. Its visual
language (FAB, elevation shadows, ripples, color-on-color
elevation) is well-engineered for Google's domain; it is the
wrong domain for industrial signal analysis.

SignalForge does NOT use: floating action buttons; "elevated
surface" cards with heavy shadows; ripple animations on click;
Material Design Icons; Roboto-as-default-font.

---

## 8. Closing principle

> "Signal first. Measurement second. Controls third. Chrome
> fourth. Determinism always."

— SignalForge visual identity, M16 close.

Specific tokens (S2), widget styling guide (S8), widget
implementations (M17), workflow implementations (M18), hardware
fixtures (M19), theme variants (M20) all inherit and refine
this manifesto. The principles defined here govern every visual
decision V0.3 ships.

---

## 9. Cross-references

- M16 spec: `docs/milestones/M16-visual-identity-ownership.md`
  (§2.1 #1 deliverable; §3 M16.1 reference family; §6 H4
  manifesto-token contradiction; H5 R10 traceability gate; H9
  manifesto rejection)
- M16 plan: `.claude/M16-plan.md` §S1 (this milestone's S1
  deliverable scope)
- M16 concerns: `.claude/M16-concerns.md` C1 (reference research
  methodology; this manifesto's reference inventory grounding)
- V0.3 charter amendment: `docs/V0-charter-amendment-v0.3.md`
  §3 (V0.3 pillar A keystone scope); §5 (V1.0 cross-env ship
  gate); §6 R10 / R11 (governance disciplines)
- S0.5 spike result: `docs/v0.3/spike-result.md` (empirical
  foundation for §5 cross-platform determinism principle)
- Visual-diff algorithm contract: `docs/v0.3/visual-diff-contract.md`
  (companion document; CI gate formalisation)
- Rendering environment contract:
  `docs/v0.3/rendering-environment-lock.md` (companion document;
  per-capture env-sidecar contract)
- References inventory: `docs/v0.3/references/README.md` (per-
  reference observation notes + citation URLs)
- M15-done.md §10 R9 retrospective: `.claude/M15-done.md`
  (V0.2 R9 hypothesis; §5.2 above corrects it empirically)
- M15-done.md §4 V1 UX gap inventory: V1 UX gaps #1-#11 that
  V0.3 widget / workflow work (M17 / M18) closes; M16 closes
  only #11 (visual identity ownership)
- V0 charter §8 quality-first: `docs/V0-series-charter.md`
  ("Reality > schedule" — manifesto takes the time it needs)
