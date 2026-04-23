# M1 — Progress Log

Chronological record of M1 execution. Each entry records a subtask
boundary or material event. Commit hashes recorded after landing.

## 2026-04-23 — S1 preflight

All preflight probes against M1 spec §S1 and this plan's S1 step list.

### Display / platform

| Check | Observed | Status |
|---|---|---|
| `XDG_SESSION_TYPE` | `x11` | ✅ real display available |
| `DISPLAY` | `:0` | ✅ |
| `WAYLAND_DISPLAY` | *(empty)* | — |
| Qt platform to be used | `xcb` (Qt auto-selects on X11) | documented; `wayland` not applicable |
| `xvfb-run` available | `/usr/bin/xvfb-run` | ✅ |

### Qt 6.10.2

| Path | Observed | Status |
|---|---|---|
| `~/Qt/6.10.2/gcc_64/lib/cmake/Qt6/Qt6Config.cmake` | present | ✅ |

### Required tools (HALT if any missing per approval update #1)

| Tool | Path | Version | Status |
|---|---|---|---|
| `scrot` | `/usr/bin/scrot` | 1.10 | ✅ |
| `radeontop` | `/usr/bin/radeontop` | (version string prints "RadeonTop unknown"; functional probe succeeded, see Operational note below) | ✅ |
| `valgrind` | `/usr/bin/valgrind` | 3.22.0 | ✅ |

No HALT.

### QT_SCALE_FACTOR behavior (from Qt 6.10 docs)

- `QT_SCALE_FACTOR=<float>` multiplies logical-to-physical scaling
  uniformly across all screens. Applied at `QApplication`
  construction; changes after that are not respected.
- Used together with `QT_AUTO_SCREEN_SCALE_FACTOR=0` to suppress
  per-screen auto detection, so the manual value wins.
- S4 Check 2 will set both env vars for each launch: e.g.
  `QT_SCALE_FACTOR=1.25 QT_AUTO_SCREEN_SCALE_FACTOR=0
  ./qquick_dock_test --auto-check 2`.

### VRAM sysfs (approval update #3)

| Path | Observed | Status |
|---|---|---|
| `/sys/class/drm/card*/device/mem_info_vram_used` | readable at `card1` | ✅ |
| Current reading | `1634496512` bytes ≈ 1558.83 MB | cross-checked against radeontop `vram 38.91% 1558.78mb` — agreement ≤ 0.1 MB |

VRAM sampling for S7 Check 5 will read `card1`.

### AppProtection preload

| Path | Observed |
|---|---|
| `/etc/ld.so.preload` | `/usr/local/lib/AppProtection/libAppProtection.so` (still active) |

Flagged for S6: one valgrind attempt; if malloc-recursion error fires (same signature as M0 C2), fall back to non-valgrind arm per M1 §5.3, report immediately.

### Operational notes discovered during probing

- **radeontop calibration delay**: a 3 s invocation produced no output (killed by timeout). A 6 s invocation produced 3 valid samples. radeontop needs ~1–2 s of startup calibration before emitting the first dump line. S7's sampler must account for this — either pre-spawn radeontop before the 30 s measurement window starts and discard the first line, or extend the window to ≥ 32 s and use only the last 30 s. I'll pre-spawn 2 s early.
- **radeontop output format confirmed**: `<timestamp>: bus <hex>, gpu <pct>%, ee <pct>%, vgt <pct>%, ta <pct>%, ..., vram <pct>% <MB>mb, gtt <pct>% <MB>mb, mclk <pct>% <GHz>ghz, sclk <pct>% <GHz>ghz` — plain text, space/comma delimited. Fields relevant to Check 5: `gpu`, `vram` (pct + MB). Other engine stages (`ee`, `vgt`, `ta`, ...) are captured to CSV for completeness but not assessed against thresholds.
- **GPU device path**: `/dev/dri/card1` (not `card0`) and `/dev/dri/renderD128`. User is not in `video`/`render` groups but accesses `card1` via seat ACLs (`+` on the perm string). radeontop works without sudo on this host.
- **ninja resolution**: `ninja` on PATH resolves to `/home/shuai/Xilinx/2025.2.1/Vivado/bin/ninja` (Xilinx-bundled `1.11.1.git.kitware.jobserver-1`). No apt `ninja-build` installed. The Xilinx ninja is the same version used during M0 and worked cleanly; no change needed. Noted for reproducibility — if the Xilinx install moves, the spike build would need a different ninja.

### S1 verdict

Preflight passes. No HALT. Ready to proceed to S2.

## Subtask ledger

| # | Subtask | Commit | Status | Verdict |
|---|---|---|---|---|
| — | Understanding + plan recorded | `47924d3` | done | n/a |
| S1 | Environment preflight | `c815125` | done | Pass |
| S2 | Spike skeleton | `c1c79d4` | done | Pass — zero-warning build, xvfb smoke-run clean |
| S3 | Check 1 — floating/re-docking | `e140acf` | done | Pass local + Pass CI |
| S4 | Check 2 — HiDPI scrot capture | `1fce8b0` | done | Partial — objective machinery Pass, real-compositor crispness deferred to human visual verification |
| S5 | Check 3 — context menu | `ba9b27e` | done | Pass local + Pass CI |
| S6 | Check 4 — hide/show lifecycle | `3c8c35f` | done | Mixed — 20-cycle VmRSS growth 7–17 MB across 5 runs (spec < 10 MB); FD stable; valgrind clean for spike code |
| S7 | Check 5 — multi-instance GPU | `0c32be1` | done | Nuanced — GPU% PASS (47.8%/<60%), CPU% FAIL against < 30% single-core (48.7% for 3 widgets), per-process VRAM unattributable |
| S8 | CI headless workflow | `9e76ae8`, `e46115c` | done | Pass — CI run 24810079353 all three matrix jobs green |
| S9 | Report generation | `ed0e03b` | done | — |
| S10 | Completion report | *this commit* | in progress | — |
| S11 | Hand-off | n/a | pending | — |

## 2026-04-23 — S4 Partial event (reported in-chat per approval update #5)

First attempt at Check 2 on DISPLAY=:0 with `scrot -u` produced four
byte-identical 2160×3840 PNGs — scrot captured the root window (the
user's portrait-rotated desktop), not the spike. Root cause: the
spike did not reliably take focus under the user's WM within the
1.8 s pre-capture window, so `-u` fell back to root. Reported at
that moment in chat with two remediation paths. Took the xvfb
fallback (M1 spec §S4 explicitly permits it with caveat), labeled
the artifacts accordingly, and flagged real-compositor crispness as
a human visual-verification item in the spike report.

Also reported during S4: the spike's `install_check_logging()` was
opening the per-check log file with `Truncate`, which each
re-invocation nuked. Fixed to `Append`; driver shell scripts take
responsibility for one-time truncation at run start.

## 2026-04-23 — S6 finding reported inline

Check 4's 20-cycle VmRSS growth was characterized across 5
independent runs (committed as `check4-rss-runs.txt`): min 7.05 MB,
max 17.38 MB, mean 12.7 MB, median 13.4 MB. The spec criterion is
"< 10 MB (i.e., not unbounded)". The strict threshold is met in 1
of 5 runs; growth is bounded in all 5. Left the call as "mixed" in
the report for the human to judge.

## 2026-04-23 — S7 Partial reported, re-run on real display per user direction

Check 5 under xvfb gave misleading numbers (cpu_pct ~295%, gpu_pct
~5%) because Qt's RHI fell back to llvmpipe software rendering with
no GPU access. Reported in chat. User directed option (a) — run on
real DISPLAY=:0 with `QSG_INFO=1` so the RHI backend is logged.
Real-display run confirmed OpenGL via Mesa radeonsi on AMD Radeon
Graphics (hardware-accelerated). Both runs committed:
`check5-xvfb-baseline-*` (labeled "software-rendering fallback") and
`check5-realdisplay-*` (primary evidence). CPU% fell from ~295% to
~49% (for 3 widgets; ~16% per widget), GPU% rose to 47.8%.

## 2026-04-23 — S8 artifact-upload fix

First CI run (24809967504) passed all three matrix jobs but warned
"No files found" on the check 3 and check 4 artifact uploads. Root
cause: `actions/upload-artifact@v4` does not split a single-line
space-separated string into multiple paths. Collapsed to one glob
per job (`docs/spikes/M1-artifacts/check<N>-*`). Re-pushed; CI run
24810079353 all three green, artifacts now upload cleanly.

## 2026-04-23 — S9 report published

`docs/spikes/M1-qtquick-integration.md` 435 lines. Structure per
M1 spec §S9: verdict matrix, environment, per-check sections, local
vs CI discrepancies, blocked items (none), data-for-decision with
evidence-strength summary and downstream-milestone impact map. No
recommendation — the rendering-approach decision belongs to the
human per §8.
