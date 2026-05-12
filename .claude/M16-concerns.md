# M16 — Concerns (C1–C9)

S0 deliverable. Resolves the 9 open questions from
`.claude/M16-understanding.md` §5. Each entry: question, options,
**proposed resolution + rationale** (CC's recommendation;
operator locks at Phase 4 review).

CC autonomy boundary: research + analysis + recommendation is
CC-side; final value choice (especially C2 thresholds, C3
env-contract values, C7 spike scope) is operator-locked. After
Phase 4 review the concerns doc gets a "Locked at Phase 4
2026-MM-DD by operator" note + the resolved value substituted
where this draft uses "**Recommended**: …".

---

## C1 — Industrial reference research methodology

**Question** (spec §3 M16.1 + R10 traceability + understanding
§5 C1): which references get studied, how screenshots are
sourced, citation format for R10.

### Options

- **A. Vendor public material only** (datasheets, marketing
  shots, manuals). Pros: zero legal ambiguity, citable URLs.
  Cons: marketing screenshots are often retouched / idealised;
  may not reflect real-world dense UI.
- **B. Operator-installed evaluation versions** (Saleae Logic 2
  installer + Tektronix scope simulator / TekVISA app +
  LabVIEW Community Edition). Pros: real production rendering;
  can capture specific high-density states. Cons: licensing
  ambiguity on screenshot redistribution; evaluation version
  may differ from production.
- **C. Hybrid** — A for primary citations + B-derived screenshots
  stored locally with explicit "operator capture" provenance
  notes. Use B-screenshots inside `docs/v0.3/references/`
  with watermark / metadata; use A-URLs in citation lines.

### Recommended: C (hybrid)

Most defensible legally + most useful technically. Reference
inventory:

| Reference | Source | Stored at | Citation tag |
|---|---|---|---|
| Saleae Logic 2 — main waveform view | Saleae public manual + operator install screenshot | `docs/v0.3/references/saleae-logic-main.png` | "Saleae Logic 2 — main view" |
| Saleae Logic 2 — measurement panel | operator install | `docs/v0.3/references/saleae-measurement.png` | "Saleae Logic 2 — measurement panel" |
| Tektronix MSO5 — signal display + measurements | Tektronix datasheet + manual | `docs/v0.3/references/tek-mso5-display.png` | "Tektronix MSO5 — signal display" |
| Tektronix MSO5 — status indicators / recording bar | manual figure | `docs/v0.3/references/tek-mso5-status.png` | "Tektronix MSO5 — status bar (Programmer's Manual §X)" |
| LabVIEW Fuse — typical control panel | NI website / Fuse design system docs | `docs/v0.3/references/labview-fuse-controls.png` | "LabVIEW Fuse — control panel" |
| LabVIEW — front panel + block diagram | NI marketing | `docs/v0.3/references/labview-front-panel.png` | "LabVIEW — typical front panel" |

Anti-references (what SF is NOT):

| Anti | Stored at |
|---|---|
| Consumer-app aesthetic (e.g., Spotify desktop) | `docs/v0.3/references/anti-consumer-app.png` |
| Smartphone-touch optimisation (large hit targets, gesture-first) | `docs/v0.3/references/anti-mobile.png` |
| Material Design clone (heavy elevation shadows, FAB-style accents) | `docs/v0.3/references/anti-material.png` |

Citation format in `visual-identity.md`:

```markdown
> **Principle**: Signal-as-hero hierarchy — chart pane dominates
> visual real estate, controls demote to chrome.
>
> Refs: [Tektronix MSO5 — signal display](references/tek-mso5-display.png);
> [Saleae Logic 2 — main view](references/saleae-logic-main.png).
```

R10 gate: ≥ 70 % of manifesto principles cite ≥ 1 reference.
H5 fires below 70 %.

Operator-locked at Phase 4: confirm hybrid approach, confirm
reference inventory list, confirm citation format.

---

## C2 — Visual-diff algorithm tunable parameter values

**Question** (spec §4.1 + understanding §5 C2): PIXEL_THRESHOLD,
CLUSTER_THRESHOLD, PERCENT_THRESHOLD; per-state override
mechanism.

### Spec defaults (per §4.1)

```
PIXEL_THRESHOLD = 4        # per-channel delta (out of 255)
CLUSTER_THRESHOLD = 200    # max contiguous diff cluster
PERCENT_THRESHOLD = 1.0    # max percent pixels differing
```

### Justification

PIXEL_THRESHOLD = 4 (≈ 1.5 % of 255) absorbs:

- 8-bit sRGB quantisation noise.
- Font antialiasing variance between FreeType micro-versions
  (within the env-contract band).
- Sub-pixel positioning rounding.

It does NOT absorb:

- Font-family change (different glyphs).
- Color-token change (intentional palette shift).
- Widget-position change (layout-engine difference).

CLUSTER_THRESHOLD = 200 px catches:

- A single glyph re-flowed by ~2 px (1 line, 10 chars, 10×8 ≈
  80 px per char × few chars).

So a single relocated label fires; a single relocated text
button fires; layout drift fires. Antialiasing-spread does
not (because individual AA pixels don't cluster — they're
scattered along glyph edges).

PERCENT_THRESHOLD = 1.0 %:

- 1 % of 1280 × 720 = 9 216 px.
- Empty-state baseline (00) has < 50 % "filled" area (mostly
  empty chart pane); 1 % differing = ~9k px scattered across
  toolbar + status bar + panel borders.
- This is the close-gate threshold (spec §5 close gate). Spike
  uses 3 % loose for R13 pass.

### Per-state override mechanism

Per spec §4.1 ("These are tunable per-test via test metadata;
defaults are M16 baseline"). Mechanism:

`tests/visual/baselines/<state>.thresholds.json` (optional
sidecar):

```json
{
  "pixel_threshold": 4,
  "cluster_threshold": 200,
  "percent_threshold": 1.0
}
```

Compare contract reads sidecar; falls back to defaults if absent.

V0.2 state 13 (multi-5-drivers) had 7.5 % tolerance for FLAKY
layout reflow. Under M16 deterministic rendering, state 13
should not flake; if it does at S6 verification, sidecar
override raises percent_threshold for that state only.

### Recommended

Lock spec defaults (4 / 200 / 1.0). Sidecar override allowed
per-state but discouraged; each sidecar entry needs operator
approval at S6 / S7. Document the rationale above in
`docs/v0.3/visual-diff-contract.md` at S1.

---

## C3 — Environment contract values

**Question** (spec §4.2 + understanding §5 C3): Qt patch
flexibility; locale standardisation; xvfb args; DPR forcing.

### Recommended values

```
qt_version:           6.10.x  (band; CI runs 6.10.2 currently)
qpa_platform:         xcb (X11 / xvfb both register as xcb)
style_object:         Fusion (locked)
default_font_family:  Inter
default_font_size:    12 pt
mono_font_family:     JetBrains Mono
mono_font_size:       12 pt
device_pixel_ratio:   1.0 (HiDPI baselines tagged separately + deferred)
locale:               C.UTF-8 (chosen over en_US.UTF-8 for CI portability)
wayland:              DISALLOWED for canonical baselines
gpu_rasterization:    DISALLOWED (software RHI only per ADR-010)
```

### Justifications

- **Qt version band 6.10.x**, not strict 6.10.2: Qt patch
  releases within a minor are font-rendering-stable per Qt
  policy; band-flexibility prevents env-drift HALT for routine
  patch updates. Strict 6.10.2 would force every CI / dev
  machine to lockstep upgrade; impractical.
- **Locale `C.UTF-8`** over `en_US.UTF-8`: `C.UTF-8` is
  glibc-minimal, available on CI Ubuntu without
  `locale-gen` step; `en_US.UTF-8` requires explicit generation
  in some minimal containers. `C.UTF-8` avoids that friction.
- **xvfb args**: `-screen 0 1280x800x24` (matches existing V0.2
  capture script default). Document in env contract that
  baselines are captured at 1280×800; widget tests at other
  geometries are explicit override states (not M16 scope).
- **DPR = 1.0**: HiDPI explicitly out of M16 scope per spec
  §3 M16.5 + §4.2. M16 baselines tagged DPR=1.0; future
  HiDPI baselines (M20 or later) are separately tagged.

### Disallowed environment variables (per spec §4.2)

```
QT_SCALE_FACTOR
QT_AUTO_SCREEN_SCALE_FACTOR
QT_ENABLE_HIGHDPI_SCALING  (must be 0 or unset)
QT_FONT_DPI                (must be unset)
QT_STYLE_OVERRIDE          (managed by SignalForgeStyle; external override forbidden)
```

Env-dump sidecar emits `env_overrides_present: []` when contract
holds; entries listed when violated (H10 trigger).

### Recommended advisory fields (not gating)

- `freetype_version` — recorded for forensics.
- `fontconfig_version` — recorded for forensics.

If H1 fires at S6 verification and env sidecars match exactly
on required fields, advisory fields give the first clue (e.g.,
"FreeType bumped from 2.13.2 to 2.13.3 — investigate font
hinting").

### Operator-locked at Phase 4

Confirm Qt band vs strict; confirm locale `C.UTF-8`; confirm
xvfb geometry 1280×800; confirm DPR=1.0; confirm advisory
fields list.

---

## C4 — `tokens.json` schema design

**Question** (spec §4.3 + understanding §5 C4): JSON Schema
file structure; theme variants; semantic naming; versioning.

### Recommended schema

`resources/styles/tokens.schema.json` (JSON Schema 2020-12):

```json
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "$id": "https://signalforge.dev/schemas/tokens.schema.json",
  "title": "SignalForge design tokens",
  "type": "object",
  "required": ["$schema", "version", "themes"],
  "properties": {
    "$schema": {"const": "tokens.schema.json"},
    "version": {"type": "string", "pattern": "^[0-9]+\\.[0-9]+$"},
    "themes": {
      "type": "object",
      "required": ["light"],
      "properties": {
        "light": {"$ref": "#/$defs/theme"},
        "dark":  {"$ref": "#/$defs/theme"}
      }
    }
  },
  "$defs": {
    "theme": {
      "type": "object",
      "required": ["color", "font", "spacing", "icon"],
      "properties": {
        "color":  {"$ref": "#/$defs/colorMap"},
        "font":   {"$ref": "#/$defs/fontMap"},
        "spacing":{"$ref": "#/$defs/spacingMap"},
        "icon":   {"$ref": "#/$defs/iconMap"}
      }
    },
    "colorMap": {
      "type": "object",
      "patternProperties": {
        "^[a-z]+(\\.[a-z0-9_]+)*$": {
          "type": "string",
          "pattern": "^#[0-9a-fA-F]{6}([0-9a-fA-F]{2})?$"
        }
      },
      "additionalProperties": false
    },
    "fontMap": {
      "type": "object",
      "properties": {
        "family.sans": {"type": "string"},
        "family.mono": {"type": "string"},
        "size.display":{"type": "integer", "minimum": 8, "maximum": 72},
        "size.heading":{"type": "integer", "minimum": 8, "maximum": 72},
        "size.body":   {"type": "integer", "minimum": 8, "maximum": 72},
        "size.caption":{"type": "integer", "minimum": 8, "maximum": 72},
        "size.mono":   {"type": "integer", "minimum": 8, "maximum": 72},
        "weight.regular":{"type": "integer", "minimum": 100, "maximum": 900},
        "weight.medium": {"type": "integer", "minimum": 100, "maximum": 900},
        "weight.bold":   {"type": "integer", "minimum": 100, "maximum": 900}
      },
      "required": ["family.sans","family.mono","size.body","weight.regular"]
    },
    "spacingMap": {
      "type": "object",
      "patternProperties": {
        "^(xs|sm|md|lg|xl|2xl|3xl)$": {
          "type": "integer",
          "minimum": 0,
          "multipleOf": 4
        }
      },
      "additionalProperties": false,
      "required": ["xs","sm","md","lg","xl"]
    },
    "iconMap": {
      "type": "object",
      "patternProperties": {
        "^size\\.(sm|md|lg)$": {"type": "integer", "minimum": 8, "maximum": 64}
      },
      "additionalProperties": false,
      "required": ["size.sm","size.md","size.lg"]
    }
  }
}
```

### Justifications

- **Themes top-level with `light` required + `dark` optional**:
  per spec §3 M16.2 (light M16; dark M20). Schema enforces
  "light" presence so M16 implementation cannot ship without
  a light theme; dark schema slot ready for M20 without
  schema bump.
- **Color hex-only with optional alpha** (`#RRGGBB` or
  `#RRGGBBAA`): named colors create lookup ambiguity;
  rgba() syntax doesn't carry through QSS uniformly. Hex
  uniform across Qt API.
- **Spacing multiples of 4**: 4 / 8 / 16 / 24 / 32 scale per
  industrial UI density practice. Enforced via `multipleOf: 4`.
- **Semantic key naming** (`bg.primary` / `signal.0..7` /
  `status.recording` etc.): manifesto-traced; H4 fires if
  S2 token key doesn't map to a manifesto principle.
- **Versioning** (`version: "MAJOR.MINOR"`): bump MINOR for
  additive token additions (M16 ships v1.0; M20 adds dark
  theme + bumps to v1.1); MAJOR bump for breaking changes
  to existing tokens (avoid in V0.3; expected stable).

### Recommended

Lock schema design above. Operator reviews at S2 token-content
review (not Phase 4 — schema decisions are mechanical given
the spec).

---

## C5 — Qt rendering implementation choices

**Question** (spec §4.4 + understanding §5 C5): QSS resource
compile vs runtime load; QPalette ColorRole mapping;
`SignalForgeStyle` static vs instance.

### Recommended

**QSS resource compile** (not runtime load). Rationale:

- Lockstep binary + QSS: any QSS change requires a binary
  rebuild, eliminating "deployed binary + stale QSS file on
  disk" failure mode.
- DEB installer ships single binary; no QSS in
  `/usr/share/signalforge/` to worry about.
- Resource compile via `styles.qrc` containing `tokens.qss` +
  any hand-written `global.qss`.
- M16 generator emits `tokens.qss` into the source tree; CMake
  compiles into resource; CI `--check` enforces freshness.

Runtime load (rejected for M16): would allow per-environment
theme override — desirable for M20+ dark theme toggle BUT
introduces drift risk between binary + on-disk QSS that M16
explicitly closes. M20 will add a runtime theme-switch hook
(spec §3 M16.2 / spec §2.1 #10 / spec §5.11) that re-applies
the generated bundled QSS variant in-memory; that's lockstep,
not on-disk drift.

**QPalette ColorRole mapping**: ALL 18 standard QPalette
ColorRole values set explicitly from tokens. No defaults.
Mapping (M16.4):

| QPalette ColorRole | Token |
|---|---|
| Window | color.bg.primary |
| WindowText | color.text.primary |
| Base | color.bg.surface |
| AlternateBase | color.bg.elevated |
| ToolTipBase | color.bg.elevated |
| ToolTipText | color.text.primary |
| Text | color.text.primary |
| Button | color.bg.elevated |
| ButtonText | color.text.primary |
| BrightText | color.text.primary |
| Highlight | color.border.focus |
| HighlightedText | color.bg.primary |
| Link | color.border.focus |
| LinkVisited | color.signal.4 |
| Light | color.bg.surface |
| Midlight | color.bg.elevated |
| Mid | color.border |
| Dark | color.text.secondary |
| Shadow | color.text.primary |
| PlaceholderText | color.text.disabled |

For ColorGroup variants (Active / Inactive / Disabled), use
deltas (Disabled → text.disabled overrides) explicitly set
at S4 wiring.

**`SignalForgeStyle` static class** (not instance). Rationale:

- No per-window state needed — palette / fonts / QSS are
  process-global.
- Static methods cleaner than singleton.
- Mockable from tests via dependency injection if needed
  (not anticipated).

### Recommended

Lock all three choices above.

---

## C6 — Bundled fonts approach

**Question** (spec §3 M16.3 + understanding §5 C6): Qt
resource compile vs filesystem install; DEB integration.

### Options

- **A. Qt resource compile only** (`fonts.qrc` → compiled into
  binary). Pros: single artifact, no on-disk drift, works
  offline. Cons: binary size grows by font payload (~500 KB
  for Inter+Mono); font updates require rebuild.
- **B. Filesystem install only** (`/usr/share/fonts/signalforge/`
  in DEB). Pros: smaller binary; fonts shareable with system
  font database. Cons: DEB install required for fonts; running
  from build tree without install needs fallback.
- **C. Hybrid** (resource compile + filesystem install). Pros:
  binary always works (resource fallback); installed fonts
  also discoverable via fontconfig for system tools. Cons:
  duplication on disk.

### Recommended: A (resource compile only)

Single artifact discipline. Binary size growth (~500 KB) is
negligible compared to current build output. DEB still includes
fonts in `/usr/share/fonts/signalforge/` as advisory (for tools
outside SignalForge that want consistent rendering), but
SignalForge itself ALWAYS uses the resource-compiled fonts via
`QFontDatabase::addApplicationFont(":/fonts/Inter-Regular.ttf")`
etc.

Approach detail:

- `resources/fonts/Inter-Regular.ttf`, `Inter-Medium.ttf`,
  `Inter-Bold.ttf`, `Inter-Italic.ttf` (4 files).
- `resources/fonts/JetBrainsMono-Regular.ttf`,
  `JetBrainsMono-Medium.ttf` (2 files).
- `resources/fonts/LICENSE.OFL.Inter` (SIL Open Font License
  1.1; full text).
- `resources/fonts/LICENSE.Apache2.JetBrainsMono` (Apache 2.0
  full text).
- `resources/fonts/fonts.qrc` listing all 6 TTFs.
- CMake `target_sources(signalforge PRIVATE resources/fonts/fonts.qrc)`
  for resource compile.
- DEB `CPACK_DEBIAN_PACKAGE_DEPENDS` does NOT depend on system
  Inter/JBM (we ship them).
- DEB payload: copy `resources/fonts/*.ttf` to
  `/usr/share/fonts/signalforge/` AND
  `resources/fonts/LICENSE.*` to `/usr/share/doc/signalforge/fonts/`.

Fail-fast at QApplication init: `SignalForgeStyle::loadBundledFonts`
returns false if any of the 6 `addApplicationFont` calls fails;
`main.cpp` `qFatal("Bundled fonts failed to load — M16
rendering contract violated")`.

### Operator-locked at Phase 4

Confirm A (resource compile); confirm DEB filesystem advisory
copy; confirm 4 + 2 font file count.

---

## C7 — S0.5 spike scope details (critical for R13 enforcement)

**Question** (spec §5.4 + spec §6 H11 + understanding §5 C7):
which 2 baselines, exact prototype stack, success / HALT
thresholds, operator-local environment documentation.

### Recommended

**Selected baselines** (covering complementary diff modes):

1. **00-empty-launch** — empty MainWindow with no fixture
   loaded. Diff surface: chrome only (toolbar, panel borders,
   status bar). Tests inter-component background-fill drift
   (the V0.2 R8-identified main drift source) +
   font-metric drift on labels.
2. **24-dialog-add-serial** — Connection-add modal with Serial
   driver type pre-selected. Diff surface: text-heavy form
   (labels, line edits, combos), modal-window chrome. Tests
   font-metric drift × ~30 controls + default-value drift
   (`/dev/ttyS0` vs `/dev/ttyS10` per V0.2 R7 finding).

These two cover empty-chrome + dense-form diff modes; passing
both at < 3 % means M16 rendering stack handles both diff
categories.

**Exact prototype stack** for S0.5:

- `QApplication::setStyle(QStyleFactory::create("Fusion"))` in
  `main.cpp`.
- Bundled Inter Regular at point size 12 via
  `QFontDatabase::addApplicationFont`. Font TTF stored in a
  per-spike `/tmp/m16-spike/fonts/` workdir (NOT committed to
  repo at S0.5 — that's S4 work).
- Minimal `QPalette` setting only 6 ColorRoles (Window /
  WindowText / Base / Text / Button / ButtonText) with
  manifesto-anticipated values from spec §4.3 sketch:

  ```
  Window         #fbfbfa
  WindowText     #1a1d23
  Base           #ffffff
  Text           #1a1d23
  Button         #f5f5f4
  ButtonText     #1a1d23
  ```

- **No QSS**, **no token generator**, **no manifesto** at S0.5
  — that's the R13 spike point: test the technical foundation
  isolated.
- Capture under existing V0.2 `--capture-fullscreen-after-ms`
  / `--capture-screenshot-after-ms` infrastructure (mech B / C).
- Apply minimal Python visual-diff with PIXEL_THRESHOLD = 4
  + PERCENT_THRESHOLD comparison (no clustering yet — S3 work).

**Success / HALT thresholds**:

- Per-baseline `percent_differing < 3.0` = PASS.
- Either baseline ≥ 3.0 % = H11 + HALT report. Investigation
  hypotheses listed in spec §6 H11: font rasterization (try
  fontconfig pin); DPI mismatch (try `QT_FONT_DPI=96`);
  platform plugin (try forcing `xcb` explicitly); Qt patch
  delta (capture both Qt patch versions).

**Operator-local environment documentation** (recorded in
`docs/v0.3/spike-result.md` at S0.5 close):

- OS: Ubuntu 24.04 LTS (`/etc/os-release` capture).
- Desktop session: per `$XDG_CURRENT_DESKTOP` + `$DESKTOP_SESSION`.
- GTK theme: per `gsettings get org.gnome.desktop.interface gtk-theme`.
- Icon theme: per `gsettings get org.gnome.desktop.interface icon-theme`.
- Pre-installed fonts: `fc-list :family` count + first 20
  entries.
- `QT_*` env vars present: full enum.
- xvfb args used for capture: `-screen 0 1280x800x24`.

**CI-environment documentation** (same fields captured from
GitHub Actions runner).

**Deliverable**: `docs/v0.3/spike-result.md` containing:

- Two diff numbers (local vs CI percent_differing) per baseline.
- Two env dumps (local + CI per spike-time minimum schema).
- Interpretation (PASS / FAIL).
- If FAIL: hypothesis ranking + proposed redesign per H11.

### Operator-locked at Phase 4

Confirm 00 + 24 as spike baselines; confirm prototype stack
(Fusion + bundled Inter 12pt + 6-role minimal palette); confirm
< 3 % threshold; confirm env-doc fields. **S0.5 proceed/HALT
decision is operator-blocking** after spike runs.

---

## C8 — V0.2 baseline migration mechanics

**Question** (spec §2.1 #9 / §5.9 + understanding §5 C8):
archive directory structure; accept-baseline.sh updates;
index file; operator review process.

### Recommended

**Archive directory structure**:

```
tests/visual/
├── baselines/                          # NEW M16 deterministic baselines (12 + sidecars)
│   ├── 00-empty-launch.png
│   ├── 00-empty-launch.env.json        # M16-introduced sidecar
│   ├── 02-conn-udp-idle.png
│   ├── 02-conn-udp-idle.env.json
│   ├── ... (12 PNGs + 12 sidecars)
│   └── m14-s1-smoke.png                # optional Tier C baseline if operator accepted at V0.2 close
│
├── baselines-v0.2-archive/             # NEW at S7
│   ├── INDEX.md                        # mapping + provenance per baseline
│   ├── 00-empty-launch.png             # V0.2-era CI-rendered (commit 6ab0e34)
│   ├── 02-conn-udp-idle.png
│   ├── ... (12 archived PNGs; no sidecars — pre-M16 didn't have them)
│   └── ...
```

**`baselines-v0.2-archive/INDEX.md`** content:

```markdown
# V0.2 baseline archive

Archived at M16 S7 (commit <S7-sha>) after M16 deterministic
baselines installed at `tests/visual/baselines/`. Per V0 charter
§3 R9 + M16 spec §2.1 #9.

| State | Archived from | V0.2-era commit | Notes |
|---|---|---|---|
| 00-empty-launch | tests/visual/baselines/00-empty-launch.png | 6ab0e34 | CI xvfb Fusion-fallback rendering. R8/R9 cross-env drift documented in M15-done.md §10. |
| 02-conn-udp-idle | … | … | … |
| ... (12 rows) ||||

The M16 deterministic baselines at `tests/visual/baselines/`
should produce visual-diff < 1 % cross-env per M16
visual-diff-contract.md, whereas these V0.2-era baselines
produced 14–33 % cross-env diff (V0.2 R9).

These archived baselines are kept for forensic / historical
reference only. They do NOT serve as V0.3 measurement reference.

Forensic use cases:
- Comparing V1 inherited-OS rendering to M16 owned rendering
  (qualitative "before / after").
- If a V0.3 milestone regresses against M15-era operator
  acceptance criteria, archived baselines provide a sanity
  check.
```

**`scripts/accept-baseline.sh` updates** (S7):

Likely no change needed. The script already:
- Reads `tests/screenshots/baseline-candidate/<state>.png`
  (or alt subdir per arg 2) → writes
  `tests/visual/baselines/<state>.png` + `git add`.

S7 adds the archive-old step (one-time, not part of normal
operator workflow):

```bash
# S7 one-shot (in plan.md §S7 detail):
mkdir -p tests/visual/baselines-v0.2-archive
mv tests/visual/baselines/*.png tests/visual/baselines-v0.2-archive/
# (and write INDEX.md cataloguing)

# Then operator runs accept-baseline.sh per-state for the 12
# new M16 baselines, populating tests/visual/baselines/ fresh.
```

If sidecar files exist alongside baselines (which they will
from M16 capture path), `accept-baseline.sh` should promote
sidecars too. Proposed extension:

```bash
# accept-baseline.sh extension at S5 (when sidecars introduced):
cp "$ACTUAL"          "$BASELINE"
cp "${ACTUAL%.png}.env.json"   "${BASELINE%.png}.env.json" 2>/dev/null || true
( cd "$REPO_ROOT" && git add "${BASELINE#$REPO_ROOT/}" "${BASELINE%.png}.env.json" )
```

### Operator-locked at Phase 4

Confirm archive directory naming; confirm INDEX.md format;
confirm accept-baseline.sh sidecar-promotion extension; confirm
the S7 one-shot archive step pattern.

---

## C9 — M17+ foundation interface

**Question** (spec §9 + understanding §5 C9): what M17 needs
from M16; theme switch hook API; how M17 widget tests
integrate with env-dump contract.

### Recommended

**M17 widget rebuild consumes from M16**:

```cpp
#include "generated_style_tokens.hpp"   // C++ constexpr access
#include "app_style.hpp"                // SignalForgeStyle::loadBundledFonts etc.
```

Examples:

```cpp
// Inside M17 chart-widget rewrite:
QColor signalColor(int index) {
    using namespace SignalForge::Tokens::Light;
    constexpr QColor signals[] = {
        Signal0, Signal1, Signal2, Signal3,
        Signal4, Signal5, Signal6, Signal7
    };
    return signals[index % 8];
}

// Status indicator using semantic color:
indicator->setColor(SignalForge::Tokens::Light::StatusRecording);
```

QSS class naming convention for M17 (documented in
`widget-styling-guide.md` at S8):

```
QPushButton {                                 /* base button styling */ }
QPushButton[class="primary"] {                /* primary action button */ }
QPushButton[class="danger"] {                 /* destructive action button */ }
SignalListView { /* SignalForge custom widgets get their classname directly */ }
SignalListView#connection-list { /* objectName for the canonical instance */ }
```

Prohibited patterns (linter enforces, per spec §3 M16.4 / R6):

- `*` (universal selector) — too broad; defeats determinism.
- `QWidget QPushButton` (deep descendants) — fragile to widget
  tree changes.
- `QWidget > QPushButton` (direct-child) — fragile to layout
  re-parenting.

**Theme switch hook API** (M16 ships light-only slot, M20
dark implementation):

```cpp
// app_style.hpp
class SignalForgeStyle {
public:
    enum class Theme { Light, Dark };

    // M16: only Light supported; calling with Dark logs warning
    // + falls back to Light. M20 implements Dark.
    static void setActiveTheme(Theme t);
    static Theme activeTheme();
    static QString activeThemeName();  // "light" / "dark"

    // M16: light is constant; M20 will emit on Dark→Light or
    // Light→Dark transitions.
    // QObject*-based signal would couple SignalForgeStyle to
    // QObject; use std::function<> callback or Qt::ApplicationModal-style
    // signal-from-static class via a singleton helper.
    using ThemeChangedHandler = std::function<void(Theme)>;
    static void onThemeChanged(ThemeChangedHandler h);
};
```

Decision for M20 vs runtime-switch-at-M16:
**defer runtime hot-switch to M20**. M16 ships `setActiveTheme`
that takes effect at next QApplication init (restart). Live
QPalette swap during running app introduces complex re-render
of all widgets; M20 handles that.

**M17 widget tests integrate with env-dump contract**:

- Widget unit tests don't capture screenshots — they test
  widget behavior in isolation. No env sidecar needed.
- Widget integration tests (visual baselines per
  M15 pattern) DO capture screenshots: they invoke
  `tests/visual/scripts/dump_render_env.py` after each capture
  exactly like M16 baselines. M17 doesn't introduce a separate
  env-dump path.
- Widget tests that need a different env (e.g. HiDPI variant)
  set the env explicitly in test setup + tag the baseline with
  the env override (e.g.
  `tests/visual/baselines/widget-foo-dpr2.png` +
  `tests/visual/baselines/widget-foo-dpr2.env.json` recording
  `device_pixel_ratio: 2.0`).

### Operator-locked at Phase 4

Confirm M17 token-consumption pattern; confirm QSS class naming
convention; confirm theme switch defers hot-switch to M20;
confirm M17 widget tests reuse M16 env-dump contract.

---

## Summary

| Concern | Resolution |
|---|---|
| **C1** Reference research | Hybrid: vendor public material for citations + operator-installed evaluation versions for screenshots, stored in `docs/v0.3/references/`; 6 primary + 3 anti-references; citation tag format. ≥ 70 % traceability gate. |
| **C2** Visual-diff tunables | Spec defaults: PIXEL_THRESHOLD=4, CLUSTER_THRESHOLD=200, PERCENT_THRESHOLD=1.0. Per-state override via `<state>.thresholds.json` sidecar (discouraged; operator approval required). |
| **C3** Env contract values | Qt 6.10.x band; locale `C.UTF-8`; xvfb 1280×800; DPR=1.0; advisory: freetype + fontconfig versions recorded. Disallowed env vars per spec §4.2. |
| **C4** tokens.json schema | JSON Schema 2020-12; light required + dark optional slot; hex-only colors; multiples-of-4 spacing; semantic key naming; MAJOR.MINOR versioning. |
| **C5** Qt rendering implementation | QSS resource-compile (lockstep binary); all 18 QPalette ColorRoles mapped explicitly to tokens; `SignalForgeStyle` static class. |
| **C6** Bundled fonts | Resource-compile only (`fonts.qrc`); DEB advisory filesystem copy to `/usr/share/fonts/signalforge/`; 4 Inter + 2 JBM TTFs; fail-fast at QApplication init. |
| **C7** S0.5 spike scope | Baselines: 00-empty-launch + 24-dialog-add-serial; stack: Fusion + bundled Inter Regular 12pt + 6-role minimal palette (no QSS, no manifesto, no generator); threshold: < 3 % loose PASS; ≥ 3 % → H11 HALT + redesign. |
| **C8** V0.2 migration | Archive to `tests/visual/baselines-v0.2-archive/` with INDEX.md; S7 one-shot mv + accept-baseline.sh per-state for 12 M16 baselines; sidecar promotion extension to accept-baseline.sh at S5. |
| **C9** M17+ foundation | M17 consumes generated tokens via `#include "generated_style_tokens.hpp"`; QSS class naming convention with linter; theme switch hook ships light-only + M20 implements Dark; M17 widget tests reuse M16 env-dump contract. |

All nine are documentation-only at S0 (no code touched). S0
commit produces this `M16-concerns.md` + reference inventory
+ scaffolds `M16-progress.md`.

S0 close gate (Phase 4 operator review): operator confirms /
revises C1–C9 resolutions, then S0.5 spike begins. **S0.5
proceed/HALT decision is the next operator-blocking gate**
(R13 preventive governance).
