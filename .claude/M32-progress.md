# M32 — progress (IA realignment)

## S1 — promote-from-Parsed; remove the global Signals dock  ✅
- Dropped `SignalListPanel` (class + test + mount). The dashboard's signal picker no longer lives at the
  global level.
- `ParsedSignalsView`: right-click a signal row → "Add to dashboard ▸ Numeric/State/Plot/Bar/Gauge",
  emitting `addToDashboardRequested(signalId, typeToken)`. `main_window` routes it to
  `Dashboard::addSignalAs(signalId, type)` (records intent) + switches to the Dashboard tab.
- Parsed is now the single global signal browser; dashboard composed by promotion (drill-through).
- Test: the add-menu emits the right (signalId, token). Removed `signal_list_panel_test`.

## S2 — app-level onboarding  ✅
- Moved the "Start a SignalForge workflow" panel out of the empty-Dashboard into a workspace-level
  `QStackedWidget`: onboarding (page 0) when `connectionCount()==0`, the workspace tabs (page 1) once a
  connection exists. Driven by `connectionStateChanged`. Updated the caption (dropped "tick signals").
- Visual: `00-empty-launch` re-accepted — now shows app-level onboarding + Connections dock, no Signals
  dock.

## S3 — modal per-panel config dialog  ⏳ (next)

Verified S1+S2: 704/704 ctest Debug + Release; clang-format clean. No frozen interface/schema touched.
