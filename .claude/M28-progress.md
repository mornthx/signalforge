# M28 — plot gap + free-form layout (progress)

Branch: `milestone/M28` (local, off `milestone/M27`; not pushed).

| Subtask | Status | Commit |
|---------|--------|--------|
| S1 — plot right-gap fix (A) | done | flush 100ms + window-follows-newest-sample; right-edge test; 48 buffer + plot tests green |
| S2 — free-form drag-resize (B) + remove Move | done | per-panel geometry, free-form container (drag header=move, grip=resize), auto-place flow; Move menu removed; QTest drag tests; 9 dashboard cases/53 assertions |

## Log
- 2026-05-30: M28 off M27. S1: buffer flush 200->100ms (LOD tests still pass), PlotView window
  anchored to newest sample so new data reaches the right edge. Verified live (offscreen capture).
