# M34 — progress (UI redesign Phase 1: the frame)

## S1 — component primitives  ✅
- `src/workbench/components/` (module now links Qt6::Widgets): the reusable building blocks the frame is
  composed from —
  - **ActivityRail** — vertical exclusive mode buttons (Connect/Inspect/…); `addMode`/`setCurrentMode`/
    `modeSelected`. The signature left-nav.
  - **StatusChip** — labeled pill with a semantic `state` property for QSS (connection/mode/quality).
  - **SectionHeader** — title + caption + trailing actions; one header for every surface.
  - **EmptyState** — title/caption/action buttons; reused for onboarding + per-context empties.
- Additive, not yet mounted → zero app risk. Tests: 4 cases / 17 assertions; clean xcb teardown
  (leaked-QApplication pattern). 715/715 ctest Debug + Release; fmt clean.

## S2 — design tokens v2  ⏳ (next)
## S3 — WorkbenchFrame shell (rail | content | inspector | drawer)  ⏳
## S4 — integrate into MainWindow + regenerate baselines  ⏳ (the risky step, last)
