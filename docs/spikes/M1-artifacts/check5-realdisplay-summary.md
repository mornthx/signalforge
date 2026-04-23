# Check 5 — multi-instance GPU summary (real display — primary evidence)

**Interpretive label**: This run was captured on the developer's real
X11 DISPLAY=:0. Qt Scene Graph info (via `QSG_INFO=1`, see
`check5-realdisplay-qsginfo.txt`) confirms:

- **RHI backend**: OpenGL
- **VENDOR**: AMD
- **RENDERER**: `AMD Radeon Graphics (radeonsi, renoir, ACO, DRM 3.57, 6.8.0-110-generic)`
- **VERSION**: 4.6 Compatibility Profile, Mesa 25.2.8-0ubuntu0.24.04.1

Hardware-accelerated rendering via Mesa radeonsi on the AMD Cezanne
iGPU. This is the **primary evidence** for Check 5.

System-VRAM deltas (from the env snapshot in `check5-realdisplay-env.txt`):

| Moment | radeontop VRAM | Delta vs pre |
|---|---|---|
| Pre-spike | 1563.80 MB | — |
| During spike (mean) | 1640.62 MB | **+76.8 MB** (≈ 25 MB × 3 widgets) |
| Post-spike | 1572.04 MB | +8.2 MB (noise / other apps) |

30 s @ 500 ms cadence; three QQuickWidgets each running a 30 Hz canvas.

| Metric | Stats |
|---|---|
| cpu_pct (%) | min=43.82 max=55.56 mean=48.67 n=60 |
| rss_kb | min=179120 max=179912 mean=179553 n=60 |
| vsize_kb | min=2524356 max=2526404 mean=2525073 n=60 |
| gpu_pct (%) | min=41.67 max=54.17 mean=47.83 n=60 |
| vram_bytes_sysfs | min=1718595584 max=1720692736 mean=1720133495 n=60 |
| vram_mb_radeontop | min=1638.98 max=1649.42 mean=1640.62 n=60 |

## Spec thresholds (§S7)

- gpu_pct sustained < 60% → see max above
- cpu_pct < 30% single-core → see max above
- spike-process GPU memory < 200 MB total — the vram_bytes_sysfs column reports system-wide VRAM usage (shared iGPU, cannot be attributed per-process); vram_mb_radeontop is also system-wide. Per-process VRAM attribution is not available from free-tier AMD telemetry. The report treats this as partial evidence and flags the attribution gap.
