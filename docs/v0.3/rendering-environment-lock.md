# Rendering Environment Contract

| Field | Value |
|---|---|
| Status | Draft (S1 deliverable; locks at S1 Phase 4 operator review) |
| Companion to | `docs/v0.3/visual-identity.md` (manifesto) + `docs/v0.3/visual-diff-contract.md` (algorithm) |
| Implementation target | M16 S5 (`tests/visual/scripts/dump_render_env.py`); referenced by S3 visual-diff algorithm (Step 1 pre-check) |
| Empirical foundation | `docs/v0.3/spike-result.md` (S0.5 cross-env env-sidecar measurements) |
| Authority | M16 spec §2.1 #3 + §2.1 #4 + §4.2 + §6 H10 (env-drift INVALID per R14) |

Defines exactly what environment a SignalForge visual baseline
is captured under. Without environment contract, "deterministic"
is unfalsifiable.

Per R14 (V0.3 charter amendment §6): environment contract +
per-capture env sidecar is the seed of every M16+ visual
measurement. The visual-diff algorithm Step 1 reads this
contract; env drift = INVALID diff (not soft-fail) per H10.

---

## 1. Tier structure

Environment requirements are tiered by determinism impact, per
S0.5 empirical findings (`docs/v0.3/spike-result.md` §4.3):

- **Tier 1 — Font cascade lock** (the determinism keystone per
  S0.5; mismatch on any T1 field = INVALID diff).
- **Tier 2 — Qt rendering stack** (forced Qt style, palette,
  platform plugin; mismatch = INVALID diff).
- **Tier 3 — Display / capture geometry** (xvfb screen size,
  devicePixelRatio; mismatch = INVALID diff).
- **Tier 4 — Advisory observability** (kernel, glibc, fontconfig
  version, FreeType version, etc.; mismatch is recorded but
  does NOT invalidate the diff).

Each tier is enforced by the visual-diff Step 1 pre-check
(`docs/v0.3/visual-diff-contract.md` §1.1).

---

## 2. Tier 1 — Font cascade lock (determinism keystone)

S0.5 measurement attributed most of V0.2 R9's 14–33 % cross-env
drift to font cascade variation. Tier 1 locks the font cascade
to a byte-identical bundled set.

### 2.1 Required values

| Field | Required value | Notes |
|---|---|---|
| `loaded_fonts.inter_regular_sha256` | `be6d709dcb730ddaf050cbdae6fe89bd56d5d14dc66885639bbf6f06bd03521b` | rsms/inter v4.0 release. `resources/fonts/Inter-Regular.otf` at M16 S4 (byte-identical to S0.5 spike artifact). |
| `loaded_fonts.inter_regular_size_bytes` | `605092` | Byte-exact match to spike artifact. |
| `loaded_fonts.inter_medium_sha256` | `1e65171fccd6f445b7742728454620ec2867356c849af78e71bf2c4f0e3d97ba` | rsms/inter v4.0; `resources/fonts/Inter-Medium.otf`. Advisory variant (M17+ widget emphasis). |
| `loaded_fonts.inter_bold_sha256` | `00fd6f4691ee8884a33b46bf45851b341927c37c30064f558e1befb242971c49` | rsms/inter v4.0; `resources/fonts/Inter-Bold.otf`. Advisory variant. |
| `loaded_fonts.inter_italic_sha256` | `f065eb148431a276995ba88b79e9225e4aa05f05b3bfeb857cf0457dcc1064a9` | rsms/inter v4.0; `resources/fonts/Inter-Italic.otf`. Advisory variant. |
| `loaded_fonts.jetbrains_mono_regular_sha256` | `a0bf60ef0f83c5ed4d7a75d45838548b1f6873372dfac88f71804491898d138f` | JetBrains/JetBrainsMono v2.304; `resources/fonts/JetBrainsMono-Regular.ttf`. Required for measurement readouts per manifesto §2.2. **License: SIL Open Font License 1.1** (pre-S4 documentation referred to Apache 2.0; corrected at S4 after license verification of the actual bundled file). |
| `loaded_fonts.jetbrains_mono_regular_size_bytes` | `274612` | Byte-exact. |
| `loaded_fonts.jetbrains_mono_medium_sha256` | `31c92d01a8a08528b718a43addf0ad3df0af2ca4b7b3290a452f70f358e14d3d` | JBM v2.304; `resources/fonts/JetBrainsMono-Medium.ttf`. Advisory variant (M17+ active-mono emphasis). |
| `loaded_fonts.app_default_family` | `Inter` | Set by `QApplication::setFont(QFont("Inter", 12))` in `SignalForgeStyle::applyAtStartup`. |
| `loaded_fonts.app_default_size_pt` | `12` | Body text scale. |
| `loaded_fonts.app_mono_family` | `JetBrains Mono` | Available for widgets requiring monospace (status-bar numerics, etc.); not set as app-default. |

### 2.2 Disallowed at Tier 1

- `QT_FONT_DPI` must be unset (forces font size in non-portable
  ways).
- Operator must not pre-load conflicting Inter / JetBrains Mono
  versions via system fontconfig + `addApplicationFont` race.
  The bundle in `resources/fonts/` is canonical.

### 2.3 Forensic / advisory at Tier 1

- `fontconfig_version`: recorded; observed identical (2.15.0)
  across operator local + CI at S0.5. Advisory only — if it
  ever differs, diff is still valid but the field is logged.
- `freetype_version`: recorded; advisory. FreeType is bundled
  within the Qt 6.10.2 runtime; system-FreeType is not invoked.

---

## 3. Tier 2 — Qt rendering stack

S0.5 showed forced Fusion style + explicit minimal palette
prevents OS theme bleed-through. Tier 2 locks the Qt stack
choices.

### 3.1 Required values

| Field | Required value | Notes |
|---|---|---|
| `qt_version_major_minor` | `6.10` | Strict major.minor. Patch flexibility within 6.10.x (see §3.3). |
| `qpa_platform` | `xcb` | X11 / xvfb both register as xcb. Wayland disallowed at M16 (capture matrix = X11). |
| `style_object` | `Fusion` | Forced via `QApplication::setStyle(QStyleFactory::create("Fusion"))` in `SignalForgeStyle::applyAtStartup()`. Verified at runtime via `qApp->style()->objectName()`. |
| `palette_signature_sha256` | (computed at S4 — sha256 of all 18 QPalette ColorRole values per active theme) | One sha256 per theme; light theme value populated at S4. |
| `wayland_disallowed_check` | `true` | Capture process checks `QT_QPA_PLATFORM` does not contain `wayland` substring. |
| `gpu_rasterization_disallowed_check` | `true` | `QSG_RHI_BACKEND` must be `software` (ADR-010 carry-over) OR unset (in which case Qt picks software for headless capture). |

### 3.2 Disallowed environment variables at Tier 2

The capture process **must verify these env vars are absent or
empty**:

```
QT_SCALE_FACTOR            # forces non-1.0 scale; corrupts pixel determinism
QT_AUTO_SCREEN_SCALE_FACTOR # auto-scaling; corrupts determinism
QT_ENABLE_HIGHDPI_SCALING  # must be unset OR equal "0"
QT_STYLE_OVERRIDE          # managed by SignalForgeStyle; external override forbidden
```

If any are set at capture time: diff is INVALID, HALT H10
fires.

### 3.3 Qt patch flexibility

Qt 6.10.x patch releases within the 6.10 minor are font-
rendering-stable per Qt's release policy. The contract permits
any 6.10.x patch (current local: 6.10.2; current CI: 6.10.2;
future bumps to 6.10.3 etc. allowed without manifest change).

If Qt major.minor changes (e.g. operator bumps to 6.11),
contract revision required — recapture all baselines under the
new version (treated like an OS-level migration).

### 3.4 Observability fields at Tier 2

| Field | Recorded | Required? |
|---|---|---|
| `qt_version_full` | e.g. `6.10.2` | Advisory — full version logged; only major.minor enforced. |
| `style_object_introspection` | `qApp->style()->objectName()` value | Required to equal `Fusion` exactly. |
| `application_palette_signature` | sha256 of palette ColorRole hex values | Required to match `palette_signature_sha256` (§3.1). |

---

## 4. Tier 3 — Display / capture geometry

### 4.1 Required values

| Field | Required value | Notes |
|---|---|---|
| `device_pixel_ratio` | `1.0` | HiDPI baselines tagged separately (see §4.4). |
| `logical_dpi` | `96` | Standard X11 logical DPI; xvfb provides this by default. |
| `physical_dpi` | (advisory) | `96` typical; not gating. |
| `screen_geometry` | `1280x720` OR `1280x800` | xvfb screen geometry as supplied at capture launch. The capture script's `--server-args="-screen 0 1280x800x24"` controls this. |
| `xvfb_screen_depth` | `24` | 24-bit color depth; non-24-bit disallowed. |
| `locale` | `C.UTF-8` OR `en_US.UTF-8` | Both acceptable; one chosen per baseline. C.UTF-8 preferred for CI portability per M16-concerns §C3. |

### 4.2 Disallowed at Tier 3

- `XKB` layout settings that produce non-default character
  rendering (e.g. compose-key sequences); not anticipated as a
  V0.3 capture variable.
- HiDPI screen geometry mixed with `device_pixel_ratio=1.0`
  (would produce undersized fonts). HiDPI captures use explicit
  tagging (§4.4).

### 4.3 Observability at Tier 3

- `xvfb_command_line`: full command-line that launched xvfb
  (advisory; useful when geometry mismatch suspected).

### 4.4 HiDPI baselines (M20+, not M16)

HiDPI variants are out of M16 scope. When M20 ships HiDPI:

- Baseline filename suffix: `<state>@2x.png`, `<state>@3x.png`,
  etc.
- Env sidecar `device_pixel_ratio: 2.0` (or 3.0).
- Separate visual-diff threshold may apply (S4 measurement).

---

## 5. Tier 4 — Advisory observability

Recorded but never gating. Forensic for post-failure analysis.

### 5.1 Fields recorded

```json
{
  "os": "Linux-<kernel>-x86_64-with-glibc<version>",
  "kernel": "<uname -r output>",
  "glibc_version": "<from python platform.libc_ver>",
  "python_version": "<sys.version major.minor.patch>",
  "fontconfig_version": "<fc-list --version stderr first line>",
  "freetype_version": "<pkg-config --modversion freetype2; may be empty on CI>",
  "freetype_runtime_version": "(advisory; from Qt's bundled FreeType — TBD how to query)",
  "xdg_current_desktop": "<env var or empty>",
  "desktop_session": "<env var or empty>",
  "gtk_theme_env": "<env var or empty>",
  "qt_im_module_env": "<env var or empty>",
  "qt_accessibility_env": "<env var or empty>",
  "host_machine": "<advisory; e.g. workstation hostname or CI runner ID>"
}
```

Per S0.5 measurement (`docs/v0.3/spike-result.md` §4.3):
operator local Tier 4 differs from CI Tier 4 substantially:

- Operator: `xdg_current_desktop=ubuntu:GNOME`,
  `desktop_session=ubuntu-xorg`, `QT_IM_MODULE=ibus`,
  `QT_ACCESSIBILITY=1`.
- CI: all empty.

Yet the M16 stack produces 0.12–0.30 % cross-env diff. Tier 4
fields are recorded for forensic context, not enforcement.

### 5.2 When Tier 4 becomes Tier 1 candidate

If a future failure traces to a Tier 4 field, that field can be
promoted to Tier 1/2/3 via contract revision (S1 Phase 4
operator approval). E.g., if FreeType runtime version becomes
detectably divergent and starts producing > 1 % diff, it
becomes Tier 1 with required value.

---

## 6. Env sidecar emission

### 6.1 Format

For each captured `<state>.png`, capture process emits
`<state>.env.json` alongside:

```json
{
  "spike": "<empty for production; 'M16-S0.5' for spike runs>",
  "captured_state": "<state name>",
  "captured_at": "<ISO 8601 UTC timestamp>",
  "captured_by": "<capture-source: 'capture_baselines.py' | 'capture_m16_spike.py' | 'M14-S1-smoke' | etc.>",

  "tier_1_font_cascade": {
    "loaded_fonts": [
      {"family": "Inter", "subfamily": "Regular",
       "path": "...", "size_bytes": 605092, "sha256": "..."},
      {"family": "JetBrains Mono", "subfamily": "Regular", ...}
    ],
    "app_default_family": "Inter",
    "app_default_size_pt": 12,
    "app_mono_family": "JetBrains Mono"
  },

  "tier_2_qt_rendering": {
    "qt_version_full": "6.10.2",
    "qt_version_major_minor": "6.10",
    "qpa_platform": "xcb",
    "style_object_introspection": "Fusion",
    "application_palette_signature_sha256": "...",
    "wayland_disallowed": true,
    "gpu_rasterization_disallowed": true,
    "qt_env_overrides_present": []
  },

  "tier_3_geometry": {
    "device_pixel_ratio": 1.0,
    "logical_dpi": 96,
    "physical_dpi": 96,
    "screen_geometry": "1280x800",
    "xvfb_screen_depth": 24,
    "locale": "C.UTF-8"
  },

  "tier_4_advisory": {
    "os": "...",
    "kernel": "...",
    "glibc_version": "...",
    "python_version": "...",
    "fontconfig_version": "...",
    "freetype_version": "...",
    "xdg_current_desktop": "...",
    "desktop_session": "...",
    "gtk_theme_env": "...",
    "qt_im_module_env": "...",
    "qt_accessibility_env": "...",
    "host_machine": "..."
  }
}
```

### 6.2 Capture-time implementation

The capture process (M16 S5: `dump_render_env.py`) builds this
JSON by:

- Querying Python `platform`, `os.environ`, `sys.version`
  (Tier 4 / Tier 3 mostly).
- Launching SignalForge with a new `--dump-render-env <path>`
  CLI flag (M16 S5 addition to main.cpp) which queries Qt at
  runtime (`QApplication::style()->objectName()`,
  `QApplication::font()`, `QGuiApplication::primaryScreen()
  ->devicePixelRatio()`, etc.), writes JSON, exits.
- Merging Python-side + Qt-side fragments into one sidecar
  next to the PNG.

The S0.5 spike used a minimal env-dump (Tier 4 only, no
Qt-side query). M16 S5 implements the full contract.

---

## 7. CI integration

`.github/workflows/ci.yml` extends at M16 S5:

- Each capture-producing test invokes `dump_render_env.py`
  post-capture so the sidecar lands alongside the PNG.
- Sidecars are auto-uploaded with the existing
  `visual-screenshots-<preset>` artifact (path glob
  `tests/screenshots/**` covers sidecars).
- On any test failure with `invalid=True` (env drift, per
  visual-diff contract §1 Step 1), CI surfaces the env diff
  via `<state>.diff-report.json` showing which Tier 1/2/3
  field violated.

No vision-LLM in CI (Phase 5 amendment continued); env dump
is pixel-diff support infrastructure, not a vision check.

---

## 8. Operator review gates

Per R8 + R14:

- **S5 close**: operator reviews the env-sidecar format on
  a sample capture; confirms Tier structure.
- **S6 cross-env verification**: each of 12 V0.2 baselines
  re-captured under M16; env sidecars must be 100 % Tier 1+2+3
  compliant.
- **S7 baseline migration**: each per-state baseline + sidecar
  promoted via `accept-baseline.sh` (extended at S5 per
  M16-concerns §C8).
- **Per-state mask review** (per `visual-diff-contract.md` §1
  Step 3): any state requiring `<state>.mask.json` needs
  operator-deliberate `rationale` field; bulk-add of masks
  forbidden.

---

## 9. Anti-patterns

What this contract does NOT do:

### 9.1 Does not pin OS distribution at Tier 1/2/3

The contract is environment-property-based (Qt version, fonts,
geometry), not distribution-based. Operator may run on Ubuntu
24.04 LTS today, 24.10 next year, Debian Trixie, etc., as long
as Tier 1/2/3 hold. The supported matrix per V0.3 charter
amendment §3 currently states "Ubuntu 24.04 operator dev + CI
runner" but the contract itself doesn't pin to a distribution
name — pinning to Qt 6.10 + Inter sha256 + Fusion + xcb is
sufficient.

### 9.2 Does not require GTK theme override

S0.5 empirically showed Yaru-implying env vars are present at
capture time without moving pixels under the M16 stack. The
contract does not require operators to clean their desktop
session. Forced Qt-stack overrides whatever GTK theme is
installed.

### 9.3 Does not enforce kernel / glibc / Python versions

Tier 4 advisory only. These are observability fields, not
enforcement. If a future drift correlates to one of these, the
contract revises (§5.2 promotion path).

### 9.4 Does not address multi-platform

Multi-platform (Windows / macOS) is V0.4+ / V1.x scope per V0.3
charter amendment §3. When multi-platform lands, this contract
extends with platform-specific Tier 1/2/3 values per platform
(e.g. macOS would need its own QStyle choice + font path
discovery). M16 ships Ubuntu 24.04 X11 / xvfb only.

---

## 10. Cross-references

- M16 spec: `docs/milestones/M16-visual-identity-ownership.md`
  §2.1 #3 + #4 (env contract + sidecar deliverables); §4.2
  (env contract sketch); §6 H10 (R14 INVALID diff)
- Manifesto: `docs/v0.3/visual-identity.md` §5 (cross-platform
  determinism principle); §5.2 (V0.2 R9 retrospective
  correction)
- Visual-diff algorithm: `docs/v0.3/visual-diff-contract.md`
  §1 Step 1 (env-sidecar pre-check)
- S0.5 spike result: `docs/v0.3/spike-result.md` §4
  (operator local vs CI env comparison; empirical Tier 4
  observability)
- V0.3 charter amendment: `docs/V0-charter-amendment-v0.3.md`
  §6 R14 (environment contract discipline)
- Capture script (M16 S5 extension target):
  `tests/visual/scripts/capture_baselines.py`
- Env dump (M16 S5 deliverable):
  `tests/visual/scripts/dump_render_env.py`
- M16 S5 main.cpp extension target (new CLI flag):
  `--dump-render-env <path>`
- Spike env dump (S0.5 minimal seed; replaced at S5):
  `tests/visual/scripts/capture_m16_spike.py` `_dump_env` helper
