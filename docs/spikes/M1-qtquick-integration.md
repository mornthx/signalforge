# M1 — Qt Quick Integration Spike Report

**Milestone**: M1
**Spike program**: `tools/spike/qquick_dock_test/`
**CI workflow**: `.github/workflows/m1-spike.yml`
**CI run (latest green)**: https://github.com/mornthx/signalforge/actions/runs/24810079353
**Report author**: Claude Code (evidence-gathering only; no recommendation)

This report is intentionally descriptive. The go / no-go /
downgrade decision in M1 spec §8 is the human's to make from the
data below.

---

## Executive verdict matrix

Verdict legend: **Pass** = spec criteria met; **Partial** = check
ran with a caveat (environmental or spec-criterion ambiguity);
**Fail** = spec criterion not met with real data; **Blocked** =
check could not be exercised in this environment; **N/A** = check
intentionally not run in that environment per spec.

| Check | Local (dev host) | CI (GitHub Actions headless) | Final verdict |
|---|---|---|---|
| 1. Floating / re-docking | **Pass** — 5 cycles, 0 QSG/QML warnings, 1280×800 end-state screenshot captured | **Pass** — same three-dock binary, xvfb, 0 warnings | **Pass** (high confidence, two environments) |
| 2. HiDPI scaling (125/150/175/200 %) | **Partial** — objective machinery (DPR reported, geometry scales 1:1 with scale factor, 0 QSG/QML warnings) verified on real X11; four xvfb-rendered screenshots captured because scrot on DISPLAY=:0 targeted the root window not the spike (portrait-rotated WM). Real-compositor visual crispness needs human inspection. | N/A (spec skips — no real display on runner) | **Partial** — machinery Pass, human eyeball verification still owed |
| 3. Context-menu propagation | **Pass** — all 3 docks fire `contextMenuRequested`, QMenu appears via `QApplication::activePopupWidget()`, "Action A" triggers programmatically, popup clears | **Pass** — identical behavior under xvfb | **Pass** (high confidence, two environments) |
| 4. Hide / show lifecycle | **Mixed** — 20-cycle VmRSS growth across 5 runs: 7.05 / 11.19 / 13.42 / 14.50 / 17.38 MB (mean 12.7 MB, min 7.05, max 17.4). Spec says "< 10 MB (i.e., not unbounded)". Strict threshold met in 1 of 5 runs; growth is bounded in all 5. FD count constant (31–32). Valgrind clean for spike code — all "definitely lost" blocks trace to fontconfig / unstripped Qt paths. | **Pass** — 5-cycle short variant, no warnings | **Partial / nuanced** — CI short run is clean; local 20-cycle growth sits 3–7 MB over the strict 10 MB threshold on this iGPU/xvfb config; not "unbounded" in any run |
| 5. Multi-instance GPU | **Mixed** — real DISPLAY run (hardware-accelerated OpenGL via Mesa radeonsi) shows gpu_pct mean 47.8% (**Pass**, < 60%), cpu_pct mean 48.7% for three widgets (**Fail** against < 30% single-core, but ≈ 16% per widget), system-VRAM delta ≈ 77 MB for three widgets (≈ 25 MB per widget). xvfb baseline is not representative (software fallback). | N/A (spec skips — no discrete GPU on runner) | **Nuanced** — GPU headroom OK, CPU% per-widget is OK at a point but linear extrapolation to M6's 20-chart workload is not guaranteed; per-process VRAM attribution is unavailable from free-tier AMD telemetry |

---

## Environment

Captured at S1 preflight (`.claude/M1-progress.md`) and the S7
env snapshots.

| Field | Value |
|---|---|
| OS | Ubuntu 24.04 LTS (noble) |
| Kernel | `6.8.0-110-generic` |
| Qt install | `~/Qt/6.10.2/gcc_64/` (official online installer) |
| Display server | X11 (`XDG_SESSION_TYPE=x11`, `DISPLAY=:0`) |
| GPU | AMD Radeon Vega (Cezanne / Renoir iGPU) at `/dev/dri/card1` |
| GPU driver | Mesa 25.2.8 (`radeonsi`, ACO compiler, DRM 3.57) |
| CPU | AMD Ryzen 5 class (8 cores implied by radeontop reporting) |
| AppProtection.so | `/etc/ld.so.preload` still loads `libAppProtection.so` (M0 C2). Did **not** affect valgrind in Check 4 — only ASan was previously demonstrated to recurse with it. |
| valgrind | 3.22.0, usable on this host for this spike |
| CI runner image | `ubuntu-24.04`, GCC 13, Qt 6.10.2 via `jurplel/install-qt-action@v4` |

---

## Per-check results

### Check 1 — Floating / re-docking

**Method**: `--auto-check 1` executes 5 cycles on Dock 1: programmatic
`setFloating(true)` → 800 ms settle → `QWindow::setPosition` (to
approximate cross-monitor drag; real user drag cannot be
synthesized under xvfb) → 400 ms → `setFloating(false)` → 800 ms.
After cycle 5 a 500 ms settle precedes a `QWidget::grab()`
screenshot.

**Result (local)**: **Pass**. `[check1] end-state screenshot saved:
true`, rc=0, `concerning_warnings=0`. All five `topLevelChanged`
pairs fire correctly.

**Result (CI)**: **Pass**. Identical machinery under `xvfb-run`;
artifacts uploaded as `check1-artifacts` on CI run 24810079353.

**Artifacts**:
- `docs/spikes/M1-artifacts/check1-end-state.png` (1280×800, 8-bit RGB)
- `docs/spikes/M1-artifacts/check1-log.txt`

**Issues found**: None.

**Fallback impact** (from spec §S3): *"If Check 1 fails, QQuickWidget
in dock panels is effectively unusable for V1. Either fallback
becomes the primary recommendation candidate."*

### Check 2 — HiDPI rendering at 125 / 150 / 175 / 200 %

**Method**: re-launches the spike four times with
`QT_SCALE_FACTOR=<scale> QT_AUTO_SCREEN_SCALE_FACTOR=0`; logs the
observed `devicePixelRatio` and derived physical geometry, holds for
3 s, then exits. An external driver (`run-check2.sh`) captures each
scale's rendering via `scrot`.

**Result (local)**: **Partial**.

*Objective machinery (verifiable by CC)*:

| Scale | devicePixelRatio | logical | physical (= logical × scale) | observed physical | match |
|---|---|---|---|---|---|
| 1.25 | 1.25 | 1280 × 800 | 1600 × 1000 | 1600 × 1000 | ✓ |
| 1.50 | 1.5  | 1280 × 800 | 1920 × 1200 | 1920 × 1200 | ✓ |
| 1.75 | 1.75 | 1280 × 800 | 2240 × 1400 | 2240 × 1400 | ✓ |
| 2.00 | 2.0  | 1280 × 800 | 2560 × 1600 | 2560 × 1600 | ✓ |

Zero QSG / QML warnings at any scale. Geometry scales cleanly.

*Capture path caveat*: per approval update #2, `scrot` on a real
compositor is the intended capture mechanism. A first pass on
DISPLAY=:0 produced four byte-identical PNGs (md5 matched) because
`scrot -u` fell back to the root window — the spike didn't reliably
take focus under the user's portrait-rotated WM. Switched to a
dedicated Xvfb instance (3200 × 2000 × 24) so `scrot` captures
spike-only framing. **This keeps the scrot tool but trades the
real-compositor authenticity** for reproducible per-scale artifacts.
M1 spec §S4 explicitly permits xvfb evidence with caveat; visual
crispness under real compositor remains a human verification item.

**Result (CI)**: N/A — spec §S8 does not include Check 2 on CI.

**Artifacts**:
- `docs/spikes/M1-artifacts/check2-scale-1.25.png`
- `docs/spikes/M1-artifacts/check2-scale-1.50.png`
- `docs/spikes/M1-artifacts/check2-scale-1.75.png`
- `docs/spikes/M1-artifacts/check2-scale-2.00.png`
- `docs/spikes/M1-artifacts/check2-log.txt`
- Driver: `tools/spike/qquick_dock_test/run-check2.sh`

**Issues found**: Screenshot capture on DISPLAY=:0 requires a WM
that reliably gives focus to newly-opened windows or tooling like
`xdotool` for targeted capture. Neither is the case here; the
Xvfb fallback works but is not the fidelity the approval update
intended. **Human action required** to eyeball each scale on the
real monitor and judge text/edge crispness subjectively before M6
commits to QQuickWidget.

**Fallback impact** (from spec §S4): *"HiDPI issues in QQuickWidget
are the most common failure mode historically. If this check fails,
the fallback decision depends on severity — blur alone might be
tolerable for V1; geometry misalignment is not."* On the evidence
captured here, geometry alignment is correct at all four scales.

### Check 3 — Context-menu propagation across Widgets ↔ Quick boundary

**Method**: QML `MouseArea` emits `contextMenuRequested(point)` on
right-click. C++ slot in `MainWindow` uses `QObject::sender()` to
identify the originating `QQuickWidget`, maps QML-local point to
global screen coords, and shows a non-modal `QMenu` via `popup()`
with "Action A" / "Action B". `--auto-check 3` sends
`QTest::mouseClick(RightButton)` at each dock's center, polls
`QApplication::activePopupWidget()` up to 500 ms, triggers
`actions[0]` programmatically, verifies `last_chosen_action_ ==
"Action A"` and that the popup cleared.

**Result (local)**: **Pass**. All three docks — sized 637 × 240
(L/R) and 1280 × 491 (bottom) — pass. Screenshot of the middle dock
with its menu open was captured via `QWidget::grab()`.

**Result (CI)**: **Pass**. Same behavior under xvfb; the CI log shows
`[check3] dock 1 pass`, `dock 2 pass`, `dock 3 pass`. Artifacts
uploaded (CI run 24810079353).

**Artifacts**:
- `docs/spikes/M1-artifacts/check3-menu-screenshot.png` (1280×800)
- `docs/spikes/M1-artifacts/check3-log.txt`

**Issues found**: initial run hit y=0 because dock-widget layout had
not assigned a height by the time the auto-check dispatched; fixed
with `setMinimumSize(320, 240)` on each QQuickWidget plus a layout-
settle poll (up to 2 s). Both mitigations are defensive — not
QQuickWidget concerns.

**Fallback impact** (from spec §S5): *"If context-menu propagation
fails, the Control page (M7) and chart interaction (M6) lose a major
interaction paradigm. This is a medium-severity failure."*

### Check 4 — Hide / show lifecycle without leaks

**Method**: `--auto-check 4` toggles Dock 1 `hide()` / `show()` for
20 cycles (`--short` for 5). Between cycles samples
`/proc/self/status` VmRSS and `/proc/self/fd` count, one CSV row
per state transition. A separate valgrind pass on the `--short`
variant collects leak information.

**Result (local) — VmRSS/FD sampling (5 independent 20-cycle runs)**:

| run | baseline (KB) | final (KB) | Δ (MB) |
|---|---|---|---|
| 1 | 222288 | 237136 | 14.50 |
| 2 | 224084 | 241876 | 17.38 |
| 3 | 227584 | 239040 | 11.19 |
| 4 | 225912 | 233132 | 7.05 |
| 5 | 219508 | 233248 | 13.42 |

Summary: min 7.05, max 17.38, mean 12.71, median 13.42 MB.

Spec criterion: "< 10 MB (i.e., not unbounded)". Strict bound met in
1 of 5; **growth is bounded in all 5** (the explicit "not unbounded"
part of the criterion). The spike is neither leak-free in the strict
sense nor runaway — it sits in a noisy band 3–7 MB over the threshold
on this hardware/env combination.

FD count: constant at 31 or 32 across all cycles and all five runs.
No FD leak.

**Result (local) — valgrind** (`memcheck --leak-check=full
--error-exitcode=2` on `--short`, 5 cycles):

```
LEAK SUMMARY:
   definitely lost: 4,040 bytes in 9 blocks
   indirectly lost: 204,241 bytes in 2,347 blocks
     possibly lost: 7,841 bytes in 64 blocks
   still reachable: 7,380,911 bytes in 26,234 blocks
```

All 9 "definitely lost" blocks trace to `libfontconfig.so.1.12.1`
and unstripped Qt / X11 plugin paths. `grep -E "main_window|
signalforge::spike|qquick_dock_test"` over the 819-line log returns
zero spike-code matches. Per spec criterion *"If valgrind ran: no
'definitely lost' bytes attributable to spike code"* → **Pass**.

Note: this run also confirms that AppProtection.so (the M0 C2
preload) does **not** recurse with valgrind's malloc interception —
unlike ASan, which did recurse and made local ASan unusable.

**Result (CI)**: **Pass** on `--short` variant (VmRSS+FD only; no
valgrind in CI per §S8). Matrix job `check 4 — hide/show lifecycle
(short)` green on CI run 24810079353.

**Artifacts**:
- `docs/spikes/M1-artifacts/check4-memory-trace.csv`
- `docs/spikes/M1-artifacts/check4-rss-runs.txt`
- `docs/spikes/M1-artifacts/check4-valgrind.txt`
- `docs/spikes/M1-artifacts/check4-log.txt`

**Issues found**: VmRSS growth sits right around the spec threshold
with visible run-to-run noise. This is typical of a short test
window (20 cycles × 1 s ≈ 20 s) against a working set that includes
scene-graph caches. Worth retesting with a longer run (e.g., 500
cycles over ~10 min) if the 10 MB criterion is load-bearing for the
go/no-go decision — the current evidence cannot distinguish a tiny
per-cycle leak from normal allocator jitter.

**Fallback impact** (from spec §S6): *"If hide/show leaks, live
observation use cases (floating a chart panel, minimizing to focus
on another) become memory hazards for long sessions.
High-severity."*

### Check 5 — Multi-instance GPU resource usage

**Method**: `--auto-check 5` starts all three QQuickWidgets
rendering (each with a 30 Hz Canvas) and samples for 30 s at
500 ms cadence: CPU% from `/proc/self/stat` utime+stime deltas,
RSS/VSize from `/proc/self/status`, GPU% and VRAM% from
`radeontop -d - -i 1 -l 0` (text-parsed; no JSON mode —
approval update #3), and system-VRAM in bytes from
`/sys/class/drm/card1/device/mem_info_vram_used` (survives radeontop
format surprises).

Two runs committed:

**(a) xvfb baseline — software fallback, not representative**

Qt's RHI fell back to llvmpipe under xvfb (no GPU access). Numbers
included in the artifact bundle but explicitly labeled:

| Metric | min | max | mean |
|---|---|---|---|
| cpu_pct | 278.00 | 319.44 | 293.98 |
| gpu_pct | 0.83 | 10.00 | 4.81 |
| rss_kb | 227 444 | 232 316 | 229 491 |
| vram_mb_radeontop | 1479.09 | 1485.96 | 1480.48 |

**The 3-core CPU load and near-zero GPU are artifacts of
software rasterization, not a measurement of the spike on target
hardware.** Retained as evidence that xvfb must not be used for
Check 5 going forward.

**(b) real display — hardware-accelerated (primary evidence)**

Same spike, `DISPLAY=:0 QSG_INFO=1`. Qt Scene Graph reports:

```
qt.rhi.general: Created OpenGL context (4.6 Compatibility Profile)
qt.rhi.general: OpenGL VENDOR: AMD RENDERER: AMD Radeon Graphics
                (radeonsi, renoir, ACO, DRM 3.57, 6.8.0-110-generic)
                VERSION: 4.6 (Compatibility Profile) Mesa 25.2.8
```

Hardware-accelerated. Readings:

| Metric | min | max | mean | Spec threshold | Verdict |
|---|---|---|---|---|---|
| cpu_pct (whole spike, 3 widgets) | 43.82 | 55.56 | 48.67 | < 30% single-core | **Fail against strict threshold** |
| gpu_pct (system) | 41.67 | 54.17 | 47.83 | sustained < 60% | **Pass** |
| rss_kb | 179 120 | 179 912 | 179 553 | (no spec) | Stable (no leak over 30 s) |
| vram_bytes_sysfs (system) | 1 718 595 584 | 1 720 692 736 | 1 720 133 495 | per-process < 200 MB | Unattributable — see below |
| vram_mb_radeontop (system) | 1 638.98 | 1 649.42 | 1 640.62 | ↑ | ↑ |

System-VRAM delta from env snapshots:

| Moment | radeontop VRAM | Δ vs pre-spike |
|---|---|---|
| Pre-spike | 1563.80 MB | — |
| During spike (mean) | 1640.62 MB | **+76.8 MB** (≈ 25 MB × 3 widgets) |
| Post-spike | 1572.04 MB | +8.2 MB (within noise) |

**Per-process VRAM attribution limitation**: on the AMD iGPU, VRAM
is shared with system RAM and both radeontop and the sysfs
`mem_info_vram_used` export **system-wide** VRAM usage. There is no
free-tier AMD tool that reports per-process VRAM on this hardware;
the ~77 MB incremental during the spike's run is the best
attribution available. Post-spike VRAM returned to near-baseline
(within 8 MB), indicating the spike did release its resources on
exit.

**Result (CI)**: N/A — spec §S8 skips Check 5 (no GPU on runner).

**Artifacts**:
- `docs/spikes/M1-artifacts/check5-realdisplay-trace.csv` (primary)
- `docs/spikes/M1-artifacts/check5-realdisplay-summary.md`
- `docs/spikes/M1-artifacts/check5-realdisplay-qsginfo.txt` (RHI confirmation)
- `docs/spikes/M1-artifacts/check5-realdisplay-env.txt`
- `docs/spikes/M1-artifacts/check5-realdisplay-log.txt`
- `docs/spikes/M1-artifacts/check5-xvfb-baseline-*` (background only)

**Issues found**:
- CPU% ~49% for **three** QQuickWidgets = ~16% per widget at a
  single data point. M6's 20-chart target would scale to ~320% CPU
  **if** the relationship is linear, which is unlikely — M6 charts
  will be a different rendering pipeline (Scene Graph
  custom-node + per-pixel downsampling per `[Arch §8]`) than the
  spike's QML Canvas. Take as a signal, not a prediction.
- `glxinfo` not installed on this host; confirmation of the
  OpenGL RHI backend relies on QSG_INFO=1 output.

**Fallback impact** (from spec §S7): *"If three QQuickWidgets
already strain the GPU, 20 of them (M6 target) won't work. This
check is the most predictive of M6 feasibility."* The GPU side is
comfortably inside the threshold (47.8% vs 60%). The CPU side is
outside for the 3-widget case but per-widget is inside. The per-
process VRAM question remains open due to tooling limits on this
hardware.

---

## Headless vs. local discrepancies

| Check | Local | CI | Notes |
|---|---|---|---|
| 1 | Pass | Pass | Consistent across both environments |
| 3 | Pass | Pass | Consistent |
| 4 (short) | Pass | Pass | Consistent |
| 4 (full) | Nuanced (9–17 MB growth) | N/A | Full 20-cycle run not in CI by spec design |
| 2 | Partial | N/A | — |
| 5 | Nuanced | N/A | — |

No environment gave a Pass on one side and Fail on the other for
any shared check.

---

## Blocked items

`.claude/M1-partial-results.md` is not required because no check
hit the "couldn't run at all" state. The Check 2 capture-path
concession (xvfb instead of real-compositor scrot) and the Check 5
per-process VRAM attribution gap are the only items outside the
Pass band; neither is Blocked in the spec's sense.

---

## Data for the human's decision

### Unambiguous Pass

- **Check 1 — Floating / re-docking** (local + CI). 5 cycles
  clean, no warnings, end-state screenshot OK.
- **Check 3 — Context-menu propagation** (local + CI). All 3 docks
  route right-click → QMenu → Action trigger cleanly.
- **Valgrind on Check 4 short**: definitely-lost bytes all trace to
  system libs, zero spike-code attribution.
- **Check 5 GPU%**: 47.8% mean sustained under 60% threshold on
  real iGPU hardware.

### Unambiguous Fail (against strict spec criteria)

- **Check 5 CPU%** against "< 30% single-core": 48.7% mean for the
  three-widget spike. Caveat: per-widget ≈ 16%; M6 chart pipeline
  differs from the spike's Canvas workload, so linear extrapolation
  to 20 charts (≈ 320%) is not directly predictive.

### Ambiguous / requires human verification

- **Check 2 visual crispness** at 125 / 150 / 175 / 200 %:
  the four screenshots in `docs/spikes/M1-artifacts/check2-scale-*.png`
  were captured under Xvfb (software renderer). Real-compositor
  crispness on the user's actual display is not in these files.
  Human needs to open the spike on the physical monitor, eyeball
  text edges and the Canvas sine-wave at each of the four
  `QT_SCALE_FACTOR` values, and judge whether the rendering is
  production-quality.
- **Check 4 VmRSS growth** against "< 10 MB over 20 cycles":
  observed 7–17 MB across five runs. Human calls whether
  "bounded but noisy at ~13 MB median" counts as "not unbounded"
  (pass by spec spirit) or as "fail against the numeric threshold"
  (fail by the letter).
- **Check 5 per-process VRAM**: free-tier AMD telemetry does not
  isolate per-process VRAM on a shared iGPU. The ~77 MB system-wide
  delta observed during the spike is the best attribution; whether
  this scales linearly to M6's 20-chart target is unknown.

### Evidence-strength summary per check (per approval update #4)

| Check | Evidence strength | Reason |
|---|---|---|
| 1 — floating | **High** | Pass on both local and CI |
| 2 — HiDPI | **Medium** | Objective machinery Pass; visual crispness unverified |
| 3 — context menu | **High** | Pass on both local and CI |
| 4 — lifecycle | **Medium** | Local 20-cycle run is right at spec threshold; CI short-run Pass; valgrind clean for spike code |
| 5 — GPU | **Medium** | Real-display hardware run is primary evidence but on a single AMD iGPU host; no CI alternative by design; per-process VRAM unattributable |

### Downstream-milestone impact map

| Failure mode observed here | First affected milestone | Consequence |
|---|---|---|
| Check 2 visual crispness (if unacceptable to human) | M6 (chart rendering), M5 (Observe page) | QQuickWidget rendering is the shared delivery path |
| Check 4 strict threshold interpretation | M5 (long Observe dwell), M6 (floating chart panels) | Long-running session memory behavior |
| Check 5 CPU% linearity (if real) | M6 (20-chart target) | Scene-Graph pipeline in M6 is a distinct, potentially-more-efficient path — but a CPU ceiling at 3 widgets is a warning sign worth M6 early benchmarking |
| Per-process VRAM gap | M6, M11 (packaging) | A production app running on unknown user hardware cannot easily auto-reject "VRAM exhausted" states without better telemetry |

---

## Verdict legend recap

Verdicts in the executive matrix aggregate local + CI columns into a
single Final verdict. Where evidence is nuanced, the Final verdict
names the shape of the nuance (Partial, Mixed, Nuanced) rather than
collapsing to Pass or Fail. The human chooses the rendering approach
per M1 spec §8 from the full table, not just the Final column.
