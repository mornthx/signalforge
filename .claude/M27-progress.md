# M27 — Dashboard interaction fixes (progress)

Branch: `milestone/M27` (local, off `milestone/M26`; not pushed).

| Subtask | Status | Commit |
|---------|--------|--------|
| S1 — plot drift fix (#1) | done | capture window once + clip; run-length spill guard test |
| S2 — per-panel ⋮ config menu (type/signal/move/remove) + QTest interaction tests | done | always-visible ⋮ button; menu-driven ops; 7 dashboard cases/44 assertions |
| S3 — relaunch + close-out | done | live-verified (plot contained, ⋮ menu); full ctest 677/677 (no rebaseline) |

## Log
- 2026-05-30: M27 off M26. S1 plot drift fixed (window captured in recompute, clip to plot rect).
