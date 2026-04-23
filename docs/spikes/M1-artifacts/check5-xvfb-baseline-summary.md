# Check 5 — multi-instance GPU summary (xvfb baseline — software rendering fallback)

**Interpretive label**: This run was captured under xvfb. xvfb has no
GPU acceleration, so Qt's RHI fell back to **llvmpipe software
rendering** (confirmed indirectly by the 3-core CPU load of ~295% and
the near-zero GPU% reading — radeontop saw only ambient desktop GPU
activity, not spike-induced load). **This is not representative of
how the spike behaves on target hardware.** Retained as baseline for
comparison against the real-display run in
`check5-realdisplay-summary.md`, which is the primary evidence.

30 s @ 500 ms cadence; three QQuickWidgets each running a 30 Hz canvas.

| Metric | Stats |
|---|---|
| cpu_pct (%) | min=278.00 max=319.44 mean=293.98 n=60 |
| rss_kb | min=227444 max=232316 mean=229491 n=60 |
| vsize_kb | min=4056456 max=4132648 mean=4118514 n=60 |
| gpu_pct (%) | min=0.83 max=10.00 mean=4.81 n=60 |
| vram_bytes_sysfs | min=1550934016 max=1558142976 mean=1552572416 n=60 |
| vram_mb_radeontop | min=1479.09 max=1485.96 mean=1480.48 n=60 |

## Spec thresholds (§S7)

- gpu_pct sustained < 60% → see max above
- cpu_pct < 30% single-core → see max above
- spike-process GPU memory < 200 MB total — the vram_bytes_sysfs column reports system-wide VRAM usage (shared iGPU, cannot be attributed per-process); vram_mb_radeontop is also system-wide. Per-process VRAM attribution is not available from free-tier AMD telemetry. The report treats this as partial evidence and flags the attribution gap.
