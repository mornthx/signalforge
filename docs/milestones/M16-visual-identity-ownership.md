# M16 — Visual Identity Ownership

| Field | Value |
|---|---|
| Milestone ID | M16 |
| Series | V0.3 (V0 charter amendment §3 — keystone) |
| Estimated effort | **No calendar commitment** (quality-first per V0 charter §4 + §8) |
| Prerequisites | V0.2 closed (M15, v0.2.0 tagged) |
| Next milestone | M17 widget rebuild (blocked until M16 closes) |
| Hard-stop type | Cross-environment determinism verified per algorithm contract + V0.2 baselines regress to deterministic baselines |
| Soft-HALT allowed | No |
| Branch | `milestone/M16` |
| Revision | v2 (per operator review 2026-05-11; 6 substantive revisions) |

---

## 1. Goal

M16 takes SignalForge from "OS-styled application" to "self-styled
application" **on the declared supported environment matrix** (Ubuntu
24.04 operator dev + CI runner for M16). After M16 closes:

- Same SignalForge binary on supported-matrix environments produces
  visual-diff under defined tolerance (< 1% per algorithm contract)
  for the same application state
- SignalForge has an explicit **visual identity manifesto** —
  documented design philosophy specific to embedded-bring-up
  signal-analysis tooling
- All visual decisions are grounded in **design tokens** traceable
  to manifesto principles
- Tokens flow from a **single canonical source** (`tokens.json`)
  through a **generator** producing QSS / C++ headers / Python
  test consumers
- A **rendering environment contract** locks Qt version, QPA platform,
  DPI, font configuration, style — without environment contract,
  "deterministic" is unfalsifiable
- A **visual-diff algorithm contract** defines exactly what `< 1%`
  means (algorithm, color space, masking, thresholds)
- V0.2's 12 baselines are re-captured under M16 deterministic
  rendering and become V0.3's authoritative measurement reference
- M17 / M18 / M19 / M20+ work has stable visual foundation

This is V0.3 **keystone milestone**. Without it, all subsequent
V0.3 work has cross-environment fragility (R9 from V0.2 close).

**Scope of determinism**: Ubuntu 24.04 operator dev + CI runner only.
Multi-platform (Windows / macOS / other distros) deferred to V0.4+
or V1.x extensions. Unsupported environments are best-effort.

## 2. Scope

### 2.1 Must deliver

1. **Visual identity manifesto** (`docs/v0.3/visual-identity.md`)
   - SignalForge domain positioning (embedded bring-up workbench)
   - Visual hierarchy priorities (signal > measurement > control > chrome)
   - Signal-semantic visual language (how connections/signals/states differentiate)
   - Theme context model (lab work / screenshot / accessibility)
   - Cross-platform determinism as design constraint
   - Industrial reference traceability (R10)

2. **Visual-diff algorithm contract** (`docs/v0.3/visual-diff-contract.md`)
   - Image size mismatch handling (immediate fail)
   - Comparison color space (sRGB, 8-bit RGBA)
   - Alpha handling (premultiplied vs normalized; pick one)
   - Primary metric: percent of pixels with any-channel absolute delta > threshold
   - Secondary metrics: max channel delta, mean channel delta, optional SSIM
   - Dynamic-region masking (cursors, timestamps, progress counters)
   - devicePixelRatio fixed at 1.0 for baseline capture (HiDPI explicit-test only)
   - Acceptable threshold definition: `< 1% changed pixels AND no changed cluster > N px without operator approval`
   - Per-pixel-channel delta threshold value (e.g., 4/255 ≈ 1.5%; tunable)

3. **Rendering environment contract** (`docs/v0.3/rendering-environment-lock.md`)
   - Required Qt version (locked: Qt 6.10.2 + minor variations allowed within manifest)
   - Required QPA platform (`xcb` for X11; `xvfb` permitted via xcb)
   - Required QStyle (`Fusion`, enforced at QApplication init)
   - Required font family + size (locked: Inter + JetBrains Mono at specified sizes)
   - Required devicePixelRatio (1.0 for V0.2 baseline regression; HiDPI baselines optional + tagged)
   - Required locale (`C.UTF-8` or `en_US.UTF-8` documented)
   - Disallowed environment variables (`QT_SCALE_FACTOR`, `QT_AUTO_SCREEN_SCALE_FACTOR` etc. — must not be set at capture)
   - Wayland-vs-X11 declaration (X11 / xvfb canonical for V0.3 baselines)
   - GPU vs software rasterization (software RHI for headless captures; per ADR-010)

4. **Capture-time environment dump** (`tests/visual/scripts/dump_render_env.py`)
   - Invoked at each capture run
   - Outputs JSON sidecar next to PNG: `tests/screenshots/<state>.env.json`
   - Captures: Qt version, QPA platform, style object name, font family + size, devicePixelRatio, logical DPI, physical DPI, screen geometry, font database families, relevant env vars
   - CI uploads as part of `visual-screenshots-<preset>` artifact
   - Visual-diff fails fast if env contract violated (per H10 below)

5. **Token source-of-truth + generator** (per R15)
   - `resources/styles/tokens.json` — canonical source (operator-readable + editable)
   - `tools/generate_style_assets.py` — generator producing:
     - `resources/styles/tokens.qss` (Qt stylesheet snippet)
     - `src/app/generated_style_tokens.hpp` (C++ constexpr values)
     - `tests/visual/lib/generated_tokens.py` (Python test consumer)
   - CI gate: `python3 tools/generate_style_assets.py --check` fails if generated files differ from committed versions
   - Theme variants supported (M20 dark inherits same generator infrastructure)

6. **Qt rendering pipeline ownership** (`src/app/app_style.{hpp,cpp}` + `src/app/main.cpp`)
   - `SignalForgeStyle` class encapsulating QApplication palette + style
   - Force Fusion style (`QApplication::setStyle("Fusion")`) regardless of OS
   - QPalette setup with explicit ColorRole values from generated tokens
   - Font registration via `QFontDatabase::addApplicationFont` at QApplication init
   - Stylesheet (QSS) loaded at startup applying token-driven styling
   - **QSS discipline** (per operator review §R6):
     - Selectors use objectName / class-name / dynamic-property, NOT `*` or deep descendants
     - Build-time linter checks QSS file for prohibited selectors
     - Runtime: NO `setStyleSheet` / `polish` / `unpolish` in hot paths (recording, replay tick, chart update); these are listed as forbidden in widget-styling-guide

7. **Bundled fonts** (in `resources/fonts/`)
   - Inter (regular / medium / bold / italic)
   - JetBrains Mono (regular / medium)
   - LICENSE files (SIL OFL + Apache 2.0)
   - CMake compile-into-resource (`fonts.qrc`)
   - DEB installer integration
   - Fail-fast at QApplication init if font load fails (per H10)

8. **Cross-environment determinism verification**
   - Re-capture V0.2's 12 production-fidelity baselines under M16 rendering
   - Capture once in operator local environment + once in CI environment
   - Visual-diff per algorithm contract: < 1% per baseline
   - If any baseline ≥ 1%: HALT (H1)

9. **V0.2 baseline regression**
   - Existing `tests/visual/baselines/*.png` regenerated under M16 rendering
   - Old V0.2-era CI-captured baselines moved to `tests/visual/baselines-v0.2-archive/`
   - V0.3 measurement reference = M16 deterministic baselines

10. **Foundation primitives for M17-M20** (`docs/v0.3/widget-styling-guide.md`)
    - How to consume design tokens from a widget
    - QSS class naming convention + prohibited patterns
    - Theme switch hook (light-only for M16; dark slot for M20)
    - Token addition workflow

11. **`.claude/M16-done.md`** with hand-off to M17

### 2.2 Must not do

1. **No V1 UX gap fixes during M16** (M15-done.md §4 items #1-#10 target M17/M18; only #11 closes via M16)
2. **No widget rebuild during M16** (M17 scope)
3. **No new functional features** (Backend frozen per V0 charter §3)
4. **No M2-M12 frozen .hpp modification** without ADR
5. **No dark theme variant in M16** (light-only ships; dark M20 per Q2 default)
6. **No multi-platform scope expansion** (Ubuntu 24.04 + CI only)
7. **No design without manifesto basis** (R11)
8. **No tokens without generator-driven single-source** (R15)
9. **No baseline acceptance without environment contract compliance** (R14)

## 3. Design Decisions (locked at Phase 4)

Per operator's "all default" Phase 4 decision + 2026-05-11 operator
review revisions. CC may revisit at S0 only if empirical evidence at
S0 (especially S0.5 spike) contradicts assumption.

### M16.1 — Visual identity reference family (Q1 = D)

**Locked**: SignalForge own visual identity, synthesizing from
industrial references. Codified in manifesto.

Reference priorities:
- **Saleae Logic 2** — flat, modern, signal-centric (primary
  influence for layout + flat visual language)
- **Tektronix MSO series** — high info density, signal-as-hero
  (primary influence for chart pane hierarchy + status indicators)
- **LabVIEW Fuse design system** — industrial engineering tool
  (primary influence for control styling)

Anti-references (what SF is NOT):
- Not a consumer app aesthetic
- Not a smartphone-touch optimization (mouse + keyboard primary)
- Not a Material Design clone

### M16.2 — Theme variant order (Q2 = D)

**Locked**: Light theme first; dark theme deferred to M20.

M16 ships:
- Single light theme, production-ready
- Theme switch infrastructure (token generator supports variants;
  light-only implemented)

### M16.3 — Font bundle (Q3 = Inter + JetBrains Mono)

**Locked**: Inter (SIL OFL) + JetBrains Mono (Apache 2.0).

Both fonts bundle in repo + DEB installer via Qt resource compile.
Fail-fast if font load fails at QApplication init.

### M16.4 — Qt rendering technical stack

**Locked**: `QT_STYLE_OVERRIDE=Fusion` (enforced via
`QApplication::setStyle`) + explicit `QPalette` + bundled fonts +
global QSS stylesheet generated from tokens.

QSS discipline (per operator review R6):
- objectName / class-name / dynamic-property selectors only
- No `*` / deep descendants
- No hot-path `setStyleSheet`
- Build-time linter validates

### M16.5 — Cross-environment determinism gate

**Locked**: Visual-diff `< 1%` per algorithm contract (defined in
`docs/v0.3/visual-diff-contract.md`) between operator local + CI
captures of V0.2's 12 baselines re-captured under M16 rendering.

**Algorithm summary** (full spec in contract document):
- Primary: percent of pixels with any-channel absolute delta > threshold (e.g., 4/255)
- Plus: no changed cluster > N px without operator approval
- Plus: image size mismatch = immediate fail
- Plus: env contract compliance pre-check (env drift = invalid diff, not soft fail)

### M16.6 — Token source-of-truth (NEW per operator review §R5)

**Locked**: `resources/styles/tokens.json` as canonical source.
Generator (`tools/generate_style_assets.py`) produces all consumers
(QSS / C++ / Python). CI enforces freshness.

### M16.7 — Rendering environment contract (NEW per operator review §R3)

**Locked**: Environment lock document + per-capture env dump
sidecar. CI rejects capture if env contract violated.

## 4. Key Implementation Details

### 4.1 Visual-diff algorithm contract sketch

`docs/v0.3/visual-diff-contract.md` content (drafted at S1):

```
## Algorithm

For two PNG images A (baseline) and B (capture):

1. Pre-check: env contract compliance
   - Both images have env.json sidecar
   - Sidecars match required contract values (Qt version, style,
     font, DPR, locale, QPA platform)
   - If sidecars differ on any required field: diff INVALID
     (HALT, not soft-fail)

2. Pre-check: size match
   - A.width == B.width AND A.height == B.height
   - Mismatch: FAIL immediately, no further computation

3. Optional masking pass:
   - Apply mask regions from `tests/visual/baselines/<state>.mask.json`
   - Masked pixels excluded from comparison
   - Mask file optional; absent = no masking

4. Primary metric: per-pixel delta percent
   - For each non-masked pixel position (x, y):
     - A_rgb = A.pixel(x, y)  # 8-bit RGB triple, sRGB
     - B_rgb = B.pixel(x, y)  # ditto
     - delta = max(|A_r - B_r|, |A_g - B_g|, |A_b - B_b|)
     - pixel_differs = delta > PIXEL_THRESHOLD (default 4)
   - Result: percent_differing = (count(pixel_differs) / total_pixels) * 100

5. Secondary metric: clustering check
   - Identify contiguous regions of differing pixels (4-connected)
   - Max cluster size = max(region_size for each region)
   - If max cluster > CLUSTER_THRESHOLD (default 200): flag for review

6. Acceptance:
   - PASS if percent_differing < 1.0 AND max_cluster <= 200
   - Otherwise: FAIL with diagnostic report

## Tunable parameters (default values)

PIXEL_THRESHOLD = 4        # per-channel delta threshold (out of 255)
CLUSTER_THRESHOLD = 200    # max contiguous diff cluster
PERCENT_THRESHOLD = 1.0    # max percent pixels differing

These are tunable per-test via test metadata; defaults are M16 baseline.
```

### 4.2 Rendering environment contract sketch

`docs/v0.3/rendering-environment-lock.md` content (drafted at S1):

```
## Required at capture time

| Variable | Required value |
|---|---|
| Qt version major.minor | 6.10 (patch flexibility per Qt-stable; recorded in env dump) |
| QPA platform | xcb (X11 / xvfb) |
| QStyle object name | Fusion |
| App default font family (sans) | Inter |
| App default font family (mono) | JetBrains Mono |
| App default font point size | 12 |
| devicePixelRatio | 1.0 (HiDPI baselines tagged separately) |
| Locale | C.UTF-8 OR en_US.UTF-8 |
| Wayland | DISALLOWED for canonical baselines |
| GPU rasterization | DISALLOWED (software RHI only per ADR-010) |

## Disallowed environment variables at capture

QT_SCALE_FACTOR
QT_AUTO_SCREEN_SCALE_FACTOR
QT_ENABLE_HIGHDPI_SCALING (must be 0 or unset)
QT_FONT_DPI (must be unset)
QT_STYLE_OVERRIDE (managed by SignalForgeStyle; external override forbidden)

## Env dump sidecar format

For each <state>.png, capture process emits <state>.env.json:

{
  "qt_version": "6.10.2",
  "qpa_platform": "xcb",
  "style_object": "Fusion",
  "default_font_family": "Inter",
  "default_font_size": 12,
  "mono_font_family": "JetBrains Mono",
  "device_pixel_ratio": 1.0,
  "logical_dpi": 96,
  "physical_dpi": 96,
  "screen_geometry": "1280x720",
  "locale": "C.UTF-8",
  "loaded_fonts": ["Inter Regular", "Inter Medium", ...],
  "env_overrides_present": [],
  "freetype_version": "2.13.2",
  "fontconfig_version": "2.14.2"
}

Visual-diff tool reads both sidecars; mismatch on required keys =
diff INVALID (per algorithm §1).
```

### 4.3 Token source + generator sketch

`resources/styles/tokens.json` canonical source (drafted at S2):

```json
{
  "$schema": "tokens.schema.json",
  "version": "1.0",
  "themes": {
    "light": {
      "color": {
        "bg.primary": "#fbfbfa",
        "bg.surface": "#ffffff",
        "bg.elevated": "#f5f5f4",
        "border": "#d6d6d4",
        "border.focus": "#3b7ddd",
        "text.primary": "#1a1d23",
        "text.secondary": "#5a5d63",
        "text.disabled": "#9ea1a7",
        "status.idle": "#5a5d63",
        "status.connecting": "#d4a72c",
        "status.connected": "#2d8a3e",
        "status.error": "#c8392a",
        "status.recording": "#c8392a",
        "status.replay": "#3b7ddd",
        "signal.0": "#2d6cb3",
        "signal.1": "#c8392a",
        "signal.2": "#2d8a3e",
        "signal.3": "#d4a72c",
        "signal.4": "#7e3eb3",
        "signal.5": "#2da3a3",
        "signal.6": "#d4622c",
        "signal.7": "#5a5d63"
      },
      "font": {
        "family.sans": "Inter",
        "family.mono": "JetBrains Mono",
        "size.display": 18,
        "size.heading": 14,
        "size.body": 12,
        "size.caption": 11,
        "size.mono": 12,
        "weight.regular": 400,
        "weight.medium": 500,
        "weight.bold": 700
      },
      "spacing": {
        "xs": 4,
        "sm": 8,
        "md": 16,
        "lg": 24,
        "xl": 32
      },
      "icon": {
        "size.sm": 16,
        "size.md": 20,
        "size.lg": 32
      }
    }
  }
}
```

`tools/generate_style_assets.py` generates:

```
resources/styles/tokens.qss
  -> Qt stylesheet snippets with color: #fbfbfa; etc.
  -> Class-based selectors (no `*`, no deep descendants)
  -> Loaded at QApplication init by SignalForgeStyle

src/app/generated_style_tokens.hpp
  -> namespace SignalForge::Tokens::Light
  -> constexpr QColor / int values for each token
  -> Consumed by SignalForgeStyle palette setup + chart rendering

tests/visual/lib/generated_tokens.py
  -> dict-like access for Python test consumers
  -> Used by visual tests for expected-value assertions
```

CI workflow check:
```bash
# In .github/workflows/ci.yml
python3 tools/generate_style_assets.py --check
# Fails CI if generated files differ from committed; ensures
# tokens.json modifications regenerate consumers.
```

### 4.4 Qt rendering ownership

In `main.cpp` (sketch — implementation at S4):

```cpp
int main(int argc, char** argv) {
    // BEFORE QApplication: lock env if needed (in launcher script)
    
    QApplication app(argc, argv);
    
    // 1. Force Fusion style
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    
    // 2. Load bundled fonts (fail-fast)
    if (!SignalForgeStyle::loadBundledFonts()) {
        qFatal("Bundled fonts failed to load — M16 rendering contract violated");
    }
    
    // 3. Apply token-driven palette
    SignalForgeStyle::applyLightPalette(&app);
    
    // 4. Apply token-driven QSS stylesheet
    SignalForgeStyle::applyGlobalStylesheet(&app);
    
    // 5. Set application default font (from tokens)
    QFont defaultFont(SignalForge::Tokens::Light::FontFamilySans, 
                      SignalForge::Tokens::Light::FontSizeBody);
    app.setFont(defaultFont);
    
    // 6. Verify environment compliance (optional but recommended for CI)
    if (qEnvironmentVariableIsSet("SF_VERIFY_RENDER_ENV")) {
        if (!SignalForgeStyle::verifyEnvironmentContract()) {
            qFatal("Rendering environment contract violated");
        }
    }
    
    MainWindow window;
    window.show();
    return app.exec();
}
```

### 4.5 Capture-time env dump

`tests/visual/scripts/dump_render_env.py` (drafted at S3):

```python
"""Dump rendering environment to JSON sidecar.

Invoked alongside each baseline capture; output as
<screenshot_path>.env.json. Read by visual-diff tool for
env contract verification.
"""

import sys
import json
import subprocess
from pathlib import Path

def dump_env(out_path: Path) -> None:
    # Spawn a Qt program (or use signalforge --dump-env) to query
    # QApplication state. Capture all required keys per
    # rendering-environment-lock.md.
    
    env = {
        "qt_version": _query_qt_version(),
        "qpa_platform": _query_qpa_platform(),
        "style_object": "Fusion",  # locked
        # ... etc.
    }
    out_path.write_text(json.dumps(env, indent=2))

# Called by capture_baselines.py or visual test harness after each capture.
```

### 4.6 Visual-diff implementation update

`tests/visual/lib/compare.py` extended to:
1. Read sidecar env.json for both A + B
2. Validate env contract compliance
3. Apply masking from `<state>.mask.json` (optional)
4. Compute per-pixel + clustering metrics per contract
5. Return structured result (PASS / FAIL with diagnostic)

Old `compare.py` API maintained for backward compat; new
`compare_with_contract()` is M16 canonical.

## 5. Acceptance criteria

### 5.1 Visual identity manifesto
- [ ] `docs/v0.3/visual-identity.md` published
- [ ] Industrial reference screenshots stored in `docs/v0.3/references/`
- [ ] Each design principle cites at least one industrial reference (R10)

### 5.2 Visual-diff algorithm contract
- [ ] `docs/v0.3/visual-diff-contract.md` published
- [ ] Algorithm steps documented
- [ ] Tunable parameters with defaults defined
- [ ] Implementation matches contract spec (`tests/visual/lib/compare.py`)

### 5.3 Rendering environment contract
- [ ] `docs/v0.3/rendering-environment-lock.md` published
- [ ] Required + disallowed environment variables documented
- [ ] `tests/visual/scripts/dump_render_env.py` working
- [ ] Env sidecar attached to every capture
- [ ] Visual-diff pre-check rejects env-contract violations

### 5.4 S0.5 technical spike
- [ ] Spike captures 2 representative V0.2 baselines (e.g., 00-empty-launch + 24-dialog-add-serial) under prototype M16 rendering (Fusion + bundled fonts + minimal palette)
- [ ] Captures on operator local + CI
- [ ] Visual-diff < 3% (loose threshold for spike; final < 1% for full M16 close)
- [ ] If spike fails: HALT, redesign technical stack before continuing
- [ ] If spike passes: continue full M16 implementation

### 5.5 Token source + generator
- [ ] `resources/styles/tokens.json` exists with all required token categories (color / font / spacing / icon for "light" theme)
- [ ] `tools/generate_style_assets.py` produces tokens.qss + generated_style_tokens.hpp + generated_tokens.py
- [ ] CI check `python3 tools/generate_style_assets.py --check` integrated
- [ ] Schema validation working (tokens.schema.json)

### 5.6 Qt rendering ownership
- [ ] `src/app/app_style.{hpp,cpp}` implements `SignalForgeStyle` class
- [ ] `main.cpp` invokes SignalForgeStyle at QApplication init
- [ ] Fusion style forced
- [ ] QPalette explicit (every QPalette::ColorRole set from tokens)
- [ ] Stylesheet applied globally
- [ ] QSS selector linter passes (no `*` / deep descendants)
- [ ] Frozen-surface counter: ≤ 2 (HALT #5 at > 2)

### 5.7 Bundled fonts
- [ ] Inter + JetBrains Mono in `resources/fonts/` with LICENSE files
- [ ] CMake compile-into-resource working
- [ ] QFontDatabase::addApplicationFont at QApplication init
- [ ] Fail-fast on font load failure
- [ ] DEB installer includes fonts

### 5.8 Cross-environment determinism
- [ ] V0.2's 12 baselines re-captured under M16 rendering (operator local)
- [ ] Same 12 baselines captured under CI rendering (CI workflow run)
- [ ] Env contract compliance: 100% (all sidecars match required values)
- [ ] Visual-diff < 1% for all 12 states
- [ ] If any state ≥ 1%: HALT (H1)

### 5.9 V0.2 baseline regression
- [ ] Old V0.2-era baselines moved to `tests/visual/baselines-v0.2-archive/`
- [ ] New M16 deterministic baselines installed at `tests/visual/baselines/`
- [ ] Operator visually approves M16 baselines (one-time review per R8)
- [ ] ctest visual suite green on local + CI

### 5.10 QSS discipline + performance
- [ ] QSS selector linter integrated into build
- [ ] No hot-path `setStyleSheet` (documented in widget-styling-guide; CI grep check optional)
- [ ] QSS startup overhead < 50ms (measured at S3)

### 5.11 M17+ foundation
- [ ] `docs/v0.3/widget-styling-guide.md` published with:
  - Token consumption examples
  - QSS class naming convention
  - Prohibited patterns + linter info
  - Theme switch hook (light-only; dark slot for M20)
  - Token addition workflow

### 5.12 Documentation
- [ ] All 4 V0.3 docs published (visual-identity / visual-diff-contract / rendering-environment-lock / widget-styling-guide)
- [ ] References folder populated
- [ ] `.claude/M16-done.md` with V0.3 progression + M17 hand-off

## 6. M16-specific HALT triggers

Beyond CLAUDE.md §HALT:

1. **H1**: Cross-environment visual-diff ≥ 1% on any V0.2 baseline after M16 rendering — remaining OS coupling not closed
2. **H2**: Frozen-surface count > 2 — HALT #5 standard; ADR required
3. **H3**: Font licensing concern discovered at S4 — revisit Q3 with operator
4. **H4**: Manifesto contradicts tokens at S2 review — revisit manifesto or tokens (R11)
5. **H5**: Industrial reference traceability gap (> 30% of design decisions without citation) — manifesto over-reaches or under-researches (R10)
6. **H6**: CMake font integration fails at S4 — revisit §4.4 approach
7. **H7**: QSS startup performance regression > 50ms — investigate per-widget styling
8. **H8**: V0.2 baseline regression breaks > 3 baselines visually (R8 operator rejection) — investigate tokens vs manifesto
9. **H9**: Operator rejects manifesto at S1 Phase 4 — revisit design direction
10. **H10** (NEW per operator review §R3 / §R4): **Environment drift** — if local/CI capture environment env.json sidecars differ on any required field (Qt version, QPA platform, style, font family, DPR, locale), visual-diff results are INVALID. Fix environment first; do not accept/reject baselines. R14 enforcement.
11. **H11** (NEW per operator review §R4): **S0.5 spike fails** — minimal determinism stack cannot reduce 2 baseline diff to < 3%; HALT before manifesto/token investment. Investigate font rasterization / DPI / platform plugin / Qt patch behavior. May result in M16 scope amendment (e.g., add fontconfig pinning, force specific FreeType version).
12. **H12** (NEW per operator review §R5): **Token generator drift** — CI `--check` fails (committed generated files differ from generator output). Indicates tokens.json modified without regenerating consumers. Re-run generator + commit; HALT only if generator itself broken.

Plus CLAUDE.md standard set.

## 7. Subtask sequence (revised per operator review §R4)

Updated 10-subtask sequence with S0.5 spike inserted:

| ID | Title | Output | Operator-blocking? |
|---|---|---|---|
| S0 | Concerns + reference research | M16-concerns.md, references/ folder populated | Phase 4 (direction) |
| S0.5 | Minimal determinism spike | Captures 2 baselines local + CI; visual-diff < 3% loose threshold OR HALT | yes (proceed/HALT decision) |
| S1 | Visual identity manifesto + algorithm + env contracts | docs/v0.3/{visual-identity, visual-diff-contract, rendering-environment-lock}.md | Phase 4 (manifesto approval) |
| S2 | Token source + generator | tokens.json + generate_style_assets.py + CI check + generated files | partial (token review) |
| S3 | Visual-diff algorithm implementation | compare.py extended with contract + env sidecar pre-check; QSS linter | no |
| S4 | Qt rendering ownership + fonts | app_style.{hpp,cpp} + main.cpp wiring + bundled fonts + DEB integration | no |
| S5 | Env dump + capture infrastructure | dump_render_env.py + capture_baselines.py integration + CI artifact updates | no |
| S6 | Cross-env determinism verification | Re-capture V0.2 12 baselines local + CI; verify < 1% per state | yes (verify + baseline review) |
| S7 | V0.2 baseline migration | archive old; install M16 baselines; ctest green | yes (final baseline approval) |
| S8 | M17+ foundation docs + close | widget-styling-guide.md + M16-done.md + PR | Phase 2 review |

### Time budget

**No calendar commitment** per V0 charter §4 + §8. Nominal sketch:
- S0 (concerns + research): 1 day
- S0.5 (spike): 0.5-1 day (preventive; saves rework if fails)
- S1 (manifesto + contracts): 1-2 days
- S2 (tokens + generator): 1 day
- S3 (diff + linter): 1 day
- S4 (Qt rendering + fonts): 1-2 days
- S5 (env dump): 0.5-1 day
- S6 (cross-env verify): 0.5-1 day
- S7 (baseline migration): 0.5 day
- S8 (docs + close): 0.5-1 day

Estimate ~1-1.5 weeks concentrated work; actual driven by quality.
S0.5 saves time if spike reveals infeasibility (catches before
manifesto + tokens commit).

### Operator-blocking points

CC-blocking (autonomous):
- S0 concerns + reference research
- S0.5 spike implementation + measurement
- S2 token implementation
- S3 diff + linter implementation
- S4 Qt rendering wiring
- S5 env dump implementation
- S7 baseline migration mechanics

Operator-blocking:
- Phase 4 reference direction review (post-S0)
- **S0.5 proceed/HALT decision** (post-spike; if spike fails, redesign before continuing)
- Phase 4 manifesto approval (post-S1)
- S2 token review (rough check)
- S6 cross-env baseline approval (per-baseline review under M16 rendering)
- S7 final baseline approval before old V0.2 archival
- S8 Phase 2 PR review

## 8. V0.3 governance discipline (R10-R15)

R10 — Industrial reference traceability:
- Every design decision cites industrial reference
- Reference screenshots in `docs/v0.3/references/`

R11 — Manifesto-first ordering:
- S1 produces manifesto BEFORE S2 fixes token values
- S2 tokens trace to manifesto principles
- If token needed without manifesto basis → H4

R12 — Baseline regression discipline:
- M16 close requires V0.2 baselines re-captured under M16 rendering
- Any intentional baseline change accepted via deliberate operator review (R8)
- Unintentional regression blocks M16 close

R13 — Technical spike before design investment (NEW):
- S0.5 minimal determinism spike before S1 manifesto
- Spike either passes (continue) or fails (HALT + redesign)
- Caught before manifesto/token investment

R14 — Environment contract discipline (NEW):
- `rendering-environment-lock.md` + per-capture env sidecars
- H10 enforces — env drift = invalid diff
- Without contract, "deterministic" is unfalsifiable

R15 — Generated assets single-source discipline (NEW):
- `tokens.json` canonical; generator produces all consumers
- CI `--check` ensures freshness
- Prevents drift between runtime + tests + docs

## 9. M17+ hand-off

`.claude/M16-done.md` must include:

1. **Design tokens reference**: how M17+ consumes tokens via
   `generated_style_tokens.hpp` / `tokens.qss` / `generated_tokens.py`
2. **Widget styling guide**: QSS class patterns + theme hooks + prohibited patterns
3. **Cross-env determinism state**: V0.2 baseline regression results
4. **Visual-diff contract reference**: how M17+ uses the algorithm + how to add masks for dynamic regions
5. **Environment contract reference**: required env values; how M17 widget tests must dump + verify
6. **V0.3 progression**: M17 widget rebuild scope retained; M18+ scope retained
7. **V1 UX gaps**: M15-done.md §4 items #1-#10 remain V0.3 work; item #11 closed by M16
8. **Industrial reference inventory**: collected references with citations, ready for M17+ consumption

## 10. Closing principle

> "AI must see the GUI" (V0 charter §1)
> "Reality > schedule" (V0 charter §8)
> "Screenshots are the spec" (M15 understanding §7)
> "Spike before design" (R13, this milestone)
> "Environment contract before measurement" (R14, this milestone)

V0.2 made AI see the GUI. M16 makes AI see the GUI **deterministically
across the declared supported environment matrix**, with explicit
algorithm + environment + single-source-tokens scaffolding. After
M16: same binary on Ubuntu 24.04 operator local + CI runner, same
pixels (within < 1% per algorithm) — V0.3 redesign measures change
reliably; V1.0 ship gate becomes achievable for the supported
matrix.

M16 is the keystone. Build it well. Spike before design.
