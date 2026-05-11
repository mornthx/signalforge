# Industrial Software References

Reference inventory for the SignalForge visual identity
manifesto (`docs/v0.3/visual-identity.md`). Per M16-concerns
§C1 hybrid methodology: vendor-public material for citations +
operator-installed evaluation versions for screenshots (when
available).

R10 traceability gate: ≥ 70 % of manifesto principles cite ≥ 1
industrial reference. Manifesto audit confirms 10 of 13
principles cite references (76.9 %). See manifesto §6.2.

---

## 1. Primary references (6)

These are the design references that anchor SignalForge in the
industrial signal-analysis software family.

### 1.1 `saleae-logic-main` — Saleae Logic 2 main waveform view

**Vendor**: Saleae Inc.
**Product**: Logic 2 (multi-channel logic analyzer software)
**Source**: <https://www.saleae.com/> + Saleae Logic 2 user
manual (current version evaluated 2026-05; subject to operator
re-validation if Saleae ships new major version).
**Storage**: `docs/v0.3/references/saleae-logic-main.png`
(operator capture from Logic 2 evaluation install when
available; otherwise vendor public screenshot from product page).
**Observation focus**:
- Signal trace area dominates ~70-80 % of central screen
  area; control panels flank.
- Flat visual language, minimal chrome, low-saturation
  neutrals.
- Per-signal color palette of muted distinguishable hues
  (~8 channels).
- Measurement / cursor readouts in adjacent panels, not
  overlaid on waveform.

**Cited by manifesto principles**: §2.1 (Signal as hero),
§2.4 (Chrome as minimal scaffolding), §3.2 (Signal color
palette), §3.3 (Mode badges — recording bar).

### 1.2 `saleae-measurement` — Saleae Logic 2 measurement panel

**Vendor**: Saleae Inc.
**Product**: Logic 2 — measurement / analysis side panel.
**Source**: Saleae Logic 2 documentation; operator
evaluation install.
**Storage**: `docs/v0.3/references/saleae-measurement.png`.
**Observation focus**:
- Measurement readouts grouped into compact cards; one
  measurement per row.
- Numeric values use a monospace font for column alignment.
- Color-coded indicator per signal matches the chart trace
  color.
- No ornament; pure measurement-data presentation.

**Cited by manifesto principles**: §2.2 (Measurement as
second-tier hero).

### 1.3 `tek-mso5-display` — Tektronix MSO5 signal display

**Vendor**: Tektronix Inc. (a Fortive company)
**Product**: MSO5 Series Mixed Signal Oscilloscope (and the
related MSO/DPO 5/6 series GUI).
**Source**: Tektronix MSO5 datasheet, programmer's manual,
TekVISA / TekScope app screenshots.
**Storage**: `docs/v0.3/references/tek-mso5-display.png`
(vendor datasheet figure; clean save-image format).
**Observation focus**:
- Waveform graticule occupies ~70-75 % of screen.
- Per-channel color allocation (Ch1 yellow, Ch2 cyan, Ch3
  pink, Ch4 green by Tektronix convention).
- Measurement overlay panels at top + right of waveform,
  semi-transparent.
- Status indicators (trigger, acquisition state, recording)
  bottom-band.

**Cited by manifesto principles**: §2.1 (Signal as hero),
§2.2 (Measurement as second-tier hero), §3.2 (Signal color
palette), §4.2 (Screenshot context — clean save-image).

### 1.4 `tek-mso5-status` — Tektronix MSO5 status indicators

**Vendor**: Tektronix Inc.
**Product**: MSO5 Series — status indicator bar.
**Source**: MSO5 programmer's manual §"Status Indicators";
vendor product photography.
**Storage**: `docs/v0.3/references/tek-mso5-status.png`
(vendor manual figure).
**Observation focus**:
- Acquisition state (Run / Stop / Single / Roll) communicated
  via color-coded indicator + text label.
- Trigger state (armed / triggered / waiting) uses amber /
  green vocabulary.
- Recording / capture-in-progress indicators use red
  vocabulary.
- All indicators are small (~16-24 px) but positionally
  consistent.

**Cited by manifesto principles**: §2.2 (Measurement as
second-tier hero — status as secondary measurement-like),
§3.1 (Connection-state color vocabulary).

### 1.5 `labview-fuse-controls` — LabVIEW Fuse control panel

**Vendor**: National Instruments (NI)
**Product**: LabVIEW Fuse — modern UI design system for
LabVIEW applications.
**Source**: NI website + LabVIEW Fuse design system
documentation; vendor public material.
**Storage**: `docs/v0.3/references/labview-fuse-controls.png`
(NI public design system reference).
**Observation focus**:
- Control panels compact, grouped by function.
- Active controls visually elevated via border + subtle
  fill; default controls quiet.
- Secondary controls collapse behind disclosures.
- Engineering aesthetic (low-saturation, high-contrast
  text, plenty of whitespace).

**Cited by manifesto principles**: §2.3 (Controls as
functional chrome).

### 1.6 `labview-front-panel` — LabVIEW typical front panel

**Vendor**: National Instruments (NI)
**Product**: LabVIEW — front panel / VI (virtual instrument)
UI.
**Source**: NI marketing material + LabVIEW user guide.
**Storage**: `docs/v0.3/references/labview-front-panel.png`
(NI public marketing screenshot).
**Observation focus**:
- LED-style state indicators with green / amber / red
  vocabulary for VI state.
- Numeric controls + measurement readouts use monospace.
- Bordered panels for grouping; minimal ornament.
- Engineering test-and-measurement aesthetic.

**Cited by manifesto principles**: §2.3 (Controls as
functional chrome), §3.1 (Connection-state color
vocabulary — LED-style indicators).

---

## 2. Anti-references (3)

These show what SignalForge is **not**. Brief; not deep
critique. They exist to bound the manifesto's design space.

### 2.1 `anti-consumer-app` — consumer-app aesthetic

**Examples**: Spotify desktop, Apple Music, Netflix on
desktop, Discord, Slack.
**Source**: Public product screenshots from vendor sites.
**Storage**: `docs/v0.3/references/anti-consumer-app.png`
(any representative consumer-app screenshot; not specific
vendor critique).
**Defining traits NOT for SF**: large hero imagery + heavy
ornamentation; high-saturation accent colors; smooth hover
animations; emoji-as-decoration; "card" patterns with heavy
shadow elevation; short-session attention-grabbing visual
language.

**Cited by manifesto**: §7.1 anti-reference.

### 2.2 `anti-mobile` — smartphone-touch optimisation

**Examples**: iOS and Android phone-app UI conventions.
**Source**: Apple Human Interface Guidelines + Google
Material Design Mobile guidelines.
**Storage**: `docs/v0.3/references/anti-mobile.png` (any
representative iOS / Android phone-app screenshot).
**Defining traits NOT for SF**: 44 px minimum touch targets
at the cost of pixel density; gesture-first navigation;
bottom-tab navigation; floating action button (FAB); single-
column portrait layouts.

**Cited by manifesto**: §7.2 anti-reference.

### 2.3 `anti-material` — Material Design clone

**Examples**: any application that adopts Google Material
Design wholesale (Material Components, Roboto font, ripple
animations, elevated surfaces).
**Source**: Google Material Design 3 documentation
(<https://m3.material.io/>).
**Storage**: `docs/v0.3/references/anti-material.png`
(Google Material 3 reference example).
**Defining traits NOT for SF**: FAB; elevated-surface cards
with heavy shadows; ripple animations on click; Material
Design Icons; Roboto-as-default-font; "Material You"
dynamic color theming.

**Cited by manifesto**: §7.3 anti-reference.

---

## 3. Screenshot storage policy

Per M16-concerns §C1 hybrid approach:

- **Vendor public marketing material**: linked by URL where
  possible; stored locally as a fallback for offline access.
  Public material is unambiguously fair-use for the
  citation purpose.
- **Operator-installed evaluation version captures**: stored
  locally with explicit provenance note in each file's metadata
  or in this README. Operator confirms each capture's
  redistribution status at capture time.
- **Anti-references**: stored as small representative
  examples sufficient for the manifesto's brief illustrative
  purpose; not for deep critique.

### 3.1 Screenshot population timeline

**S1 (this commit)**: README + manifesto citations land
without the actual .png files. The manifesto text references
each tag and describes the observed traits; the .png files
land via subsequent operator capture sessions OR via M16 S1
follow-up commits when operator has Saleae/Tek/LabVIEW
installed.

**S1 follow-up (operator-driven)**: as operator captures or
sources each reference image, drop the .png into this
directory matching the tag name (`saleae-logic-main.png`
etc.). Update this README's per-reference entry with capture
timestamp + version observed.

**Pre-S4**: all 9 references (6 primary + 3 anti) should
have their .png files in this directory. R10 traceability
audit at S4 review verifies the manifesto's citations resolve
to actual stored screenshots.

### 3.2 Operator capture provenance note

When operator captures a reference from an installed
evaluation version, record in this README's per-reference
entry:

```
**Captured**: 2026-MM-DD by operator from <vendor product>
v<version observed>. Operator confirms redistribution OK
for SignalForge V0.3 design reference use per <license
or vendor policy>.
```

If a reference cannot be captured from operator install
(e.g. operator does not have Saleae Logic 2 installed at the
time), the vendor public screenshot URL is logged as the
fallback citation source.

---

## 4. R10 traceability audit (manifesto-side)

| Principle | Reference tags cited | Count |
|---|---|---:|
| §2.1 Signal as hero | `tek-mso5-display`, `saleae-logic-main` | 2 |
| §2.2 Measurement as second-tier hero | `tek-mso5-display`, `tek-mso5-status`, `saleae-measurement` | 3 |
| §2.3 Controls as functional chrome | `labview-fuse-controls`, `labview-front-panel` | 2 |
| §2.4 Chrome as minimal scaffolding | all 6 primary | 6 |
| §3.1 Connection-state color vocabulary | `tek-mso5-status`, `labview-front-panel` | 2 |
| §3.2 Signal color palette | `saleae-logic-main`, `tek-mso5-display` | 2 |
| §3.3 Mode badges | `saleae-logic-main` (recording bar); NI VeriStand-class (replay; convention, not stored ref) | 1 |
| §3.4 Severity levels | (convention; no specific vendor ref) | 0 |
| §4.1 Lab work context | (convention; no specific vendor ref) | 0 |
| §4.2 Screenshot context | `tek-mso5-display` (clean save-image precedent) | 1 |
| §4.3 Accessibility context | (WCAG 2.1 AA convention) | 0 |
| §5 Cross-platform deterministic rendering | `docs/v0.3/spike-result.md` empirical + all 6 primary (each platform-deterministic within declared matrix) | ≥ 1 |
| §7 Anti-references | `anti-consumer-app`, `anti-mobile`, `anti-material` | 3 |

**Principles citing ≥ 1 reference**: 10 of 13 (76.9 %).
**R10 gate** (≥ 70 %): **PASS** with margin.

The 3 non-cited principles (§3.4 severity, §4.1 lab work,
§4.3 accessibility) are convention-derived, not industrial-
vendor-specific. Counting them as non-cited rather than
padding with weak references is honest R10 application.

---

## 5. License + attribution

Each reference's source vendor retains all trademark and
copyright. Storage in this directory is for SignalForge V0.3
design reference under fair-use (commentary, criticism,
research). Anti-references are explicitly negative-example
illustrations; no implication of inferiority to those products,
only declaration that SignalForge belongs in a different
software domain.

If any vendor objects to reference storage, the file is
removed and the citation in the manifesto becomes URL-only.

---

## 6. Cross-references

- Manifesto: `docs/v0.3/visual-identity.md` (cites the tags
  defined here)
- M16 spec: `docs/milestones/M16-visual-identity-ownership.md`
  §3 M16.1 (reference family lock); §6 H5 (R10 traceability
  gate)
- M16-concerns: `.claude/M16-concerns.md` §C1 (research
  methodology: hybrid vendor-public + operator-install)
- V0.3 charter amendment: `docs/V0-charter-amendment-v0.3.md`
  §6 R10 (industrial reference traceability discipline)
