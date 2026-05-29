# M23 — Dashboard P2 (progress)

Branch: `milestone/M23` (local, off `milestone/M22`; not pushed).

| Subtask | Status | Commit |
|---------|--------|--------|
| Planning | done | (this commit) |
| S1 — PlotView (QPainter) | done | axes/labels/legend/per-signal-Y, theme-aware, queryRange LOD-empty fallback; 3 cases/13 assertions (Debug+Release) |
| S2 — PlotPanel hosts PlotView; Dashboard owns time axis | done | dropped ChartManager; shared TimeAxisManager; panel virtuals |
| S3 — MainWindow rewire + close-out | todo | |

## Log
- 2026-05-29: Branched M23 off M22. Plan written (QPainter PlotView, drop ChartManager from app).
