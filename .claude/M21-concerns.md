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

(appended as they arise)
