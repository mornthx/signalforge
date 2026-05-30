# M28 — plot refresh gap + free-form layout (understanding)

User-reported after operating the app:
A. Plot right-side gap: the trace is contained now, but the right edge often has an empty
   segment with no waveform. Cause: readers only see data up to the last publish (buffer
   publish-cadence/flush lag); the plot mapped against "now", so a growing gap appeared on the
   right.
B. No free-form drag-resize of panels (only the ⋮ Move). User: implement drag-resize; once done,
   the Move menu items can be removed.

## Delivers
- S1 (A): lower the buffer time-flush 200ms→100ms (still safe vs the LOD tests, which use 1ms
  spacing where 100ms coincides with the 100-sample cadence) AND make PlotView's window follow
  the newest sample (right edge = latest data) — no right gap. Right-edge render test.
- S2 (B): per-panel geometry; Dashboard free-form container (absolute positioning, auto-place);
  drag the header to move, drag a corner grip to resize; persist into PanelConfig; QTest drag
  interaction tests. Then remove the ⋮ Move actions.

## DoD
Build+ctest green; QTest interaction (mouse drag) tests; clang-format; local commits (no push).
