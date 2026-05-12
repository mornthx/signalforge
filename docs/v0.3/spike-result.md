# M16 S0.5 — R13 Minimal-Determinism Spike: Result

| Field | Value |
|---|---|
| Spike ID | M16 S0.5 — R13 preventive spike |
| Date | 2026-05-11 |
| Branch | `milestone/M16` |
| Spike commit | `d53b3e6` |
| Local capture commit | `d53b3e6` (operator local) |
| CI capture run | `25675069202` (release preset) |
| **Verdict** | **PASS — proceed to S1** |
| Gate threshold | < 3 % per baseline (loose; final M16 close gate is < 1 %) |

---

## 1. Summary

R13 spike gate: same SignalForge binary, same `--m16-spike-stack`
flag, same V0.2 baseline launch arguments, on operator local
(Ubuntu 24.04 + Yaru desktop session) and CI (Ubuntu 24.04 +
Azure runner + xvfb, no desktop session) — visual-diff well
under the 3 % loose PASS threshold:

| Baseline | LOCAL vs CI diff | Verdict |
|---|---:|---|
| 00-empty-launch (chrome-only diff surface) | **0.122 %** | PASS |
| 24-dialog-add-serial (text-heavy form diff surface) | **0.299 %** | PASS |

Both baselines clear the final M16 close gate (< 1 %) too —
not merely the spike-stage loose threshold (< 3 %). The M16
technical stack (`QApplication::setStyle("Fusion")` + bundled
Inter Regular 12pt + 6-role minimal `QPalette`) reduces the
V0.2 R9 cross-environment drift by **two orders of magnitude**
(14.09 % → 0.122 % on 00; 33.23 % → 0.299 % on 24).

R13 preventive governance worked as designed: the spike
empirically confirms the M16 design hypothesis before
committing manifesto / token / generator investment. S1 unlocks.

---

## 2. Method

Per `.claude/M16-concerns.md` §C7 and `M16-plan.md` §S0.5:

- **Baselines**: 00-empty-launch (chrome-only diff surface;
  empty Connections panel + signal selector + status bar) +
  24-dialog-add-serial (text-heavy form; Connection-add modal
  with Serial driver type pre-selected, ~30 labelled controls).
- **Prototype stack** (minimal per R13 discipline):
  - `QApplication::setStyle(QStyleFactory::create("Fusion"))`
  - `QFontDatabase::addApplicationFont(".m16-spike/fonts/Inter-Regular.otf")`
    via Inter v4.0 OTF (sha256
    `be6d709dcb730ddaf050cbdae6fe89bd56d5d14dc66885639bbf6f06bd03521b`,
    605 092 bytes; byte-identical local + CI confirmed via
    sha256 sidecar)
  - 6-role minimal `QPalette` from `M16` spec §4.3 sketch:
    `Window=#fbfbfa`, `WindowText=#1a1d23`, `Base=#ffffff`,
    `Text=#1a1d23`, `Button=#f5f5f4`, `ButtonText=#1a1d23`
  - `QApplication::setFont(QFont("Inter", 12))`
  - **No QSS**, **no token generator**, **no manifesto**, **no
    full palette** — R13 minimal-stack discipline.
- **Capture path**: `xvfb-run --auto-servernum
  --server-args="-screen 0 1280x800x24"` →
  `signalforge --m16-spike-stack <V0.2 baseline flags>
  --capture-screenshot-after-ms 2500
  --capture-screenshot-path <png> --exit-after-ms 3500`
  (state 24 uses `--capture-fullscreen-*` for the modal-dialog
  full-screen grab).
- **XDG isolation**: per-capture `XDG_CONFIG_HOME` +
  `XDG_STATE_HOME` under a temporary directory so the binary
  does not pick up the operator's persisted
  `ConnectionManager::defaultConfigPath()` config.
- **Diff algorithm**: per `M16-plan.md` §S3-prefigured —
  PIXEL_THRESHOLD = 4 / 255 per-channel, per-pixel max-channel
  delta count, no clustering check at S0.5 (S3 work).
  `PERCENT_THRESHOLD = 3.0 %` for the spike gate
  (`tests/visual/lib/compare.py compare_baseline`).

---

## 3. Measurement

### 3.1 R13 spike gate: LOCAL vs CI (the only result that decides PASS / HALT)

```
=== R13 spike gate: M16-spike LOCAL vs M16-spike CI ===
  (PASS if both < 3 %; HALT/H11 if either >= 3 %)

  00-empty-launch                diff =  0.122 %   PASS
  24-dialog-add-serial           diff =  0.299 %   PASS
```

Both PASS the loose 3 % gate by ~25× and ~10× margin
respectively. Both also PASS the final M16 close gate (< 1 %).

### 3.2 Forensic context — spike vs V0.2 baselines

The V0.2-era committed baselines reflect CI rendering at
`6ab0e34` (V0.2 R8 re-baseline against CI xvfb Qt-default
inheritance). Comparing the M16 spike output against those
baselines:

```
=== M16-spike LOCAL vs V0.2 CI-canonical baseline ===
  00-empty-launch                diff = 14.178 %
  24-dialog-add-serial           diff = 34.384 %

=== M16-spike CI vs V0.2 CI-canonical baseline ===
  00-empty-launch                diff = 14.178 %
  24-dialog-add-serial           diff = 34.381 %
```

Two observations:

1. **The spike output is its own deterministic rendering**, not
   V0.2's CI rendering. Expected — the V0.2 CI baselines use
   the OS Fusion-fallback font cascade with whatever default
   sans-serif the CI runner has installed; the M16 spike forces
   Inter via `addApplicationFont`. These are genuinely different
   visual states.
2. **The spike rendering is identical regardless of host**:
   M16-spike-LOCAL vs V0.2-CI ≡ M16-spike-CI vs V0.2-CI (14.178 %
   / 34.38 %). The spike collapses the LOCAL-vs-CI dimension to
   sub-percent. The 14 % / 34 % residual is now the M16-stack
   vs V0.2-stack signature, not an environment artifact.

This is precisely the R13 outcome the spike was designed to
prove: M16's owned rendering pipeline produces a single
deterministic output regardless of host environment within the
declared supported matrix (Ubuntu 24.04 operator dev + CI
runner).

### 3.3 Forensic context — spike vs no-spike on the SAME host

To validate that the spike actually changes rendering (i.e. the
flag isn't a no-op):

```
M16-spike LOCAL vs no-spike LOCAL (state 00): diff = 2.91 %
```

The spike-stack shifts ~3 % of LOCAL pixels — modest but
measurable. The contribution is mostly font substitution
(operator's local sans-serif → Inter) + slight palette delta
(Yaru cascade → 6-role minimal). Combined with the CI's much
larger shift (V0.2-CI uses Fusion-fallback fonts on a desktop-
session-less Azure runner; the spike replaces those with the
identical Inter OTF), both ends converge onto the same target.

---

## 4. Environment context (R14 forensic capture)

Captured at spike runtime via
`tests/visual/scripts/capture_m16_spike.py`'s minimal env-dump.

### 4.1 Operator local (Ubuntu 24.04 + Yaru / GNOME)

```
{
  "os": "Linux-6.8.0-111-generic-x86_64-with-glibc2.39",
  "kernel": "6.8.0-111-generic",
  "python_version": "3.12.3",
  "fontconfig_version": "fontconfig version 2.15.0",
  "freetype_version": "26.1.20",
  "xvfb_screen": ":0",
  "xdg_current_desktop": "ubuntu:GNOME",
  "desktop_session": "ubuntu-xorg",
  "gtk_theme_env": "",
  "qt_env_overrides": {
    "QT_ACCESSIBILITY": "1",
    "QT_IM_MODULE": "ibus"
  },
  "spike_font_path": ".m16-spike/fonts/Inter-Regular.otf",
  "spike_font_sha256": "be6d709dcb730ddaf050cbdae6fe89bd56d5d14dc66885639bbf6f06bd03521b",
  "spike_font_size_bytes": 605092
}
```

### 4.2 CI runner (Ubuntu 24.04 Azure runner + xvfb, no desktop)

```
{
  "os": "Linux-6.17.0-1010-azure-x86_64-with-glibc2.39",
  "kernel": "6.17.0-1010-azure",
  "python_version": "3.12.13",
  "fontconfig_version": "fontconfig version 2.15.0",
  "freetype_version": "",      // pkg-config freetype2 not present on CI image
  "xvfb_screen": "",            // captured before xvfb-run set DISPLAY
  "xdg_current_desktop": "",
  "desktop_session": "",
  "gtk_theme_env": "",
  "qt_env_overrides": {
    "QT_PLUGIN_PATH": "/home/runner/work/signalforge/Qt/6.10.2/gcc_64/plugins",
    "QT_ROOT_DIR": "/home/runner/work/signalforge/Qt/6.10.2/gcc_64"
  },
  "spike_font_path": ".m16-spike/fonts/Inter-Regular.otf",
  "spike_font_sha256": "be6d709dcb730ddaf050cbdae6fe89bd56d5d14dc66885639bbf6f06bd03521b",
  "spike_font_size_bytes": 605092
}
```

### 4.3 Same-on-both / different-on-each

**Same on both** (the determinism-relevant ones):

- glibc 2.39
- fontconfig 2.15.0
- Inter Regular OTF sha256 + bytes (byte-identical font file)
- Spike-stack code path (the patched main.cpp at `d53b3e6`)
- xvfb screen geometry `1280×800×24` (capture script forces)
- Qt 6.10.2 (CI install-qt-action; local same per `qt_compat.hpp`)
- `QSG_RHI_BACKEND=software` (ADR-010 carry-over)

**Different on each** (advisory; absorbed within the 0.12–0.30 %
diff envelope):

- Kernel (6.8 generic vs 6.17 azure)
- Python (3.12.3 vs 3.12.13) — affects Python-side glue only
- XDG / desktop session env vars — local has them, CI doesn't
- `QT_PLUGIN_PATH` / `QT_ROOT_DIR` — only on CI (install-qt-action)
- `QT_ACCESSIBILITY` / `QT_IM_MODULE` — only on local (GNOME)
- FreeType version — recorded local (26.1.20), not recorded on
  CI (advisory; not gating; the actual FreeType is bundled in
  the Qt 6.10.2 runtime which is identical on both ends)

That `XDG_CURRENT_DESKTOP=ubuntu:GNOME` + `QT_IM_MODULE=ibus`
on local — and `QT_PLUGIN_PATH` + `QT_ROOT_DIR` on CI — could
plausibly have affected rendering. They did not, within < 0.3 %.
The M16 spike stack neutralises all of these.

---

## 5. Interpretation

The spike empirically confirms the M16 design hypothesis from
`.claude/M16-concerns.md` §C5 + §C6: forcing Fusion + loading a
specific bundled font + setting an explicit minimal `QPalette`
is sufficient to produce a deterministic visual output across
the declared supported environment matrix.

Key inferences:

1. **The OS theme cascade does NOT bleed through** when
   `QApplication::setStyle("Fusion")` is called explicitly +
   palette is explicitly set. V0.2 R9 hypothesis that "operator
   Yaru theme leaks into Qt rendering" is empirically rebutted
   for the M16 stack — operator's Yaru session env vars are
   present at capture time and they do not move the pixels.
2. **Bundled font is doing the heavy lifting**. Without the
   spike, V0.2 LOCAL vs V0.2 CI = 14 % / 33 %. With the spike
   (which forces a single Inter OTF) the diff collapses to
   0.1 % / 0.3 %. Font cascade was the dominant cross-env
   variable; closing it via `addApplicationFont` closes most
   of the gap.
3. **6-role minimal palette is sufficient at S0.5**. The spike
   only sets `Window/WindowText/Base/Text/Button/ButtonText` —
   not the full 18-role `QPalette` mapping (which lands at S4).
   Other roles still inherit Fusion defaults, and yet the diff
   is < 0.3 %. The full 18-role palette at S4 is a refinement,
   not a determinism requirement; light-theme close-gate of
   < 1 % is comfortably achievable.

---

## 6. Implications for M16 S1+

PASS unlocks the full M16 implementation arc per
`.claude/M16-plan.md` §S1–§S8. No M16 scope amendment needed.
The technical foundation works; the work ahead is design /
documentation / instrumentation / migration:

- **S1 manifesto + algorithm + env contracts**: proceed as
  planned. The spike's success means manifesto principles can
  cite "M16 owned rendering" as a delivered constraint, not an
  aspirational one. R10 reference traceability gate (≥ 70 %)
  still applies.
- **S2 token source + generator**: the 6-role palette at S0.5
  is the seed; S2 expands to full token set per spec §4.3.
- **S3 visual-diff algorithm**: extend `compare.py` to add the
  clustering metric (CLUSTER_THRESHOLD = 200 px) per §4.1, plus
  env-sidecar pre-check (R14 / H10).
- **S4 SignalForgeStyle + bundled fonts**: replace the S0.5
  ephemeral `--m16-spike-stack` code path with a proper
  `SignalForgeStyle` class + Qt-resource-compiled fonts in
  `resources/fonts/`. The spike's success confirms the design
  before the (heavier) S4 implementation lands.
- **S5 env dump**: extend this S0.5 minimal env dump (which
  captured what was useful for the spike) into the full
  rendering-environment-lock.md contract dump.
- **S6 cross-env verification**: re-capture all 12 V0.2
  production-fidelity baselines under the proper S4 stack;
  expect similar < 0.3 % diff per state.
- **S7 baseline migration**: archive V0.2-era baselines;
  install M16 deterministic baselines (which will replace the
  current 6ab0e34-era CI-canonical PNGs).
- **S8 widget-styling-guide + M16-done.md + PR**.

The Inter Regular OTF used at S0.5
(`.m16-spike/fonts/Inter-Regular.otf`, sha256 recorded above)
is the same file S4 will resource-compile via
`resources/fonts/fonts.qrc`. Continuity preserved.

---

## 7. Spike artifacts (forensic trail)

- Local capture PNG + env sidecar: `tests/screenshots/m16-spike/`
  (gitignored; per-run regenerable via
  `python3 tests/visual/scripts/capture_m16_spike.py`).
- CI capture PNG + env sidecar:
  GitHub Actions run `25675069202` → artifact
  `visual-screenshots-release` → `m16-spike/` subdirectory
  (14-day retention).
- Inter Regular OTF: `.m16-spike/fonts/Inter-Regular.otf`
  (gitignored; per-spike workdir; replaced by
  `resources/fonts/Inter-Regular.otf` at S4).
- Spike code path: `src/app/main.cpp` `--m16-spike-stack` block
  (removable at S4 when `SignalForgeStyle` replaces it).

---

## 8. R13 preventive governance — verdict

The R13 discipline worked as designed. The spike measured
**0.12 % / 0.30 % cross-env diff** against a target of < 3 %,
catching the M16 stack's viability before manifesto / token /
generator investment. If the spike had failed (e.g. ≥ 3 %),
M16 scope would be amended (fontconfig pin, FreeType version
pin, alternate platform plugin, etc.) before the work above
begins.

**Cost saved by R13**: had the M16 stack been infeasible
without fontconfig pinning, ~5 days of S1–S5 design /
implementation work would have produced final-stage HALT.
R13 preventive cost ≈ 0.5 day of S0.5; reactive HALT cost ≈
4–5 days of S1–S4 unwound. R13 paid for itself many times
over in expected value, even though in this case the spike
passed cleanly.

**Verdict**: R13 spike PASS — proceed to S1.

---

## 9. Cross-references

- M16 spec: `docs/milestones/M16-visual-identity-ownership.md`
  §5.4 (S0.5 acceptance criterion) + §6 H11 (failure HALT)
- M16 plan: `.claude/M16-plan.md` §S0.5 (sequence) + §4 H11
- M16 concerns: `.claude/M16-concerns.md` §C7 (spike scope
  locked at Phase 4)
- V0.3 charter amendment: `docs/V0-charter-amendment-v0.3.md`
  §6 R13 (preventive governance discipline)
- V0.2 R9 retrospective: `.claude/M15-done.md` §10 R9
  (cross-environment measurement coupling — the problem M16
  solves)
- M16-progress (to be created at S0 close commit): per spec §8
