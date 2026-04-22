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

| # | Subtask | Commit | Status |
|---|---|---|---|
| — | Understanding + plan recorded | `47924d3` | done |
| S1 | Environment preflight | — | in progress (this file is the commit content) |
| S2 | Spike skeleton | — | pending |
| S3 | Check 1 — floating/re-docking | — | pending |
| S4 | Check 2 — HiDPI scrot capture | — | pending |
| S5 | Check 3 — context menu | — | pending |
| S6 | Check 4 — hide/show lifecycle | — | pending |
| S7 | Check 5 — multi-instance GPU | — | pending |
| S8 | CI headless workflow | — | pending |
| S9 | Report generation | — | pending |
| S10 | Completion report | — | pending |
| S11 | Hand-off | n/a | pending |
