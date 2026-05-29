# M21 — concerns & autonomous decisions

This milestone runs under an explicit session directive: *"execute P0 to completion, commit
per plan, make your own call on problems and record them; I'll review at the end and revert if
wrong."* So ambiguities are resolved in-line (decided + logged here) rather than HALTing on
CLAUDE.md ambiguity rule #9.

## Decisions taken (review these)

- **D1 — scalar auto-suggest = Numeric, not Plot.** Checking a Double/Int64 signal now creates a
  NumericPanel (current value), not a chart. This is a deliberate behavioral change toward the
  workbench direction (DR-001). To get a trend, use "+ Panel" → Plot or switch the panel type.
  *If you'd rather scalars default to Plot, this is a one-line change in `panel_factory`.*
- **D2 — panels are QWidgets** (Numeric/State label-based; PlotPanel embeds the QQuickWidget).
- **D3 — legacy `ChartManager` kept** as PlotPanel's backing store (minimal MainWindow churn,
  preserves visual-test hooks).
- **D4 — Dashboard 15 Hz refresh** for Numeric/State; PlotPanel keeps Chart's 30 Hz self-drive.

## Known environment block

- **ASan/UBSan not runnable locally**: `/etc/ld.so.preload` loads `libAppProtection.so`, which
  conflicts with the ASan runtime (matches prior milestones). The `debug-asan` preset is
  therefore validated by **CI**, not this host. Not a HALT — documented per CLAUDE.md Required #2.

## Open items surfaced during execution

- **C1 (slow-signal display latency) — surfaced S2.** The M6 `SignalBuffer` publishes to
  readers only every `kDefaultPublishCadence` = **100 samples**; the time-based publish
  fallback is an explicit deferred TODO in `signal_buffer.cpp` (~line 37). So a NumericPanel /
  StatePanel (and the existing chart) only sees a signal's value after 100 samples have
  arrived. At ≥50 Hz this is ≤2 s (fine); at **1 Hz it is 100 s** before the first value shows
  — bad for exactly the slow scalars NumericPanel targets. **Not fixed in P0** (the buffer is
  M6-frozen; the fix is the already-planned time-based flush, a buffer-tuning milestone).
  Logged here as the proper place; recommend prioritizing the time-based publish flush next.

- **D5 (new dashboard-aware selector) — decided S5.** The frozen M8 `SignalSelector` routes
  checkbox toggles to the ChartManager *active chart* and rebuilds checkbox state from it on
  every `refresh()`. Reusing it for a dashboard would either (a) require editing the frozen
  class (HALT #4) or (b) make checkboxes reset every ~1 s because the dashboard isn't its
  source of truth. Per the milestone's parallel-additive philosophy I added a NEW
  `SignalListPanel` (dashboard module) whose checkbox state derives from
  `Dashboard::showsSignal` — so panel Remove buttons and the list stay in sync automatically.
  The frozen `SignalSelector` is left untouched (still built, still has its tests) but is no
  longer mounted in the central area. *Review: confirm dropping the old selector from the UI
  is acceptable; it can be deleted in a later cleanup once the new one is proven.*
