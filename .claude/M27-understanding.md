# M27 — Dashboard interaction fixes (understanding)

User-reported, found by operating the running app (feedback: tests must SIMULATE real mouse
interaction — see memory `feedback_simulate_real_interaction`):

1. **Plot drift** — the trace drifts out of frame / onto the Y axis over time. Cause: PlotView
   re-read the live (now()-tracking) axis at paint, so query-time and paint-time windows
   differed → oldest samples mapped to negative offsets. Fix: capture the window once in
   recompute() and map against it at paint; clip drawing to the plot rect. (Done — S1.)
2. **No dashboard configuration UI** — the signal list only toggles add/remove; nothing to
   configure a panel (widget type, which signal, range/unit).
3. **No layout operation** — can't move/resize widgets; panels only auto-arrange in insertion
   order; can't assign a specific signal to a specific widget.

## Delivers
- S1 plot drift fix (+ run-length spill guard test).
- S2 per-panel ⋮ config menu: **Show as ▸** (change widget type), **Add/Remove signal**
  (assign specific signals → fixes "制定某个具体图标显示具体数据"), **Move left/right**
  (position), Remove. Dashboard: setPanelType / panel signal mutation / movePanel. QTest
  mouse/menu **interaction** tests.
- (Resize/free-form drag layout assessed; coarse position via Move; true drag-resize is a
  larger follow-up if needed.)

## DoD
Build+ctest green; QTest interaction tests for the menu; clang-format; local commits (no push).
