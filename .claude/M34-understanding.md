# M34 — UI redesign Phase 1: the frame (understanding)

First **visible** phase of the redesign (`docs/v0.4/ui-redesign-proposal.md` §6, §12 P1). Build the
activity-rail frame + component library, then port today's Raw/Parsed/Dashboard into it. Branch
`milestone/M34` (off M33). Builds on the M33 services (SelectionModel, SignalIdentity).

## Strategy: safe increments (don't break the running app mid-flight)

- **S1 — component primitives** (`src/workbench/`, additive, not yet mounted): `ActivityRail` (+ rail
  buttons), `StatusChip`, `SectionHeader`, `EmptyState`. Unit-tested (construction, selection, signals,
  QSS-class). No app change → zero risk.
- **S2 — design tokens v2**: extend `tokens.qss`/`tokens.json` with the spacing/type/role tokens the
  components + frame consume; QSS rules for the new objectNames. (Visual, but only affects the new
  components until they're mounted.)
- **S3 — WorkbenchFrame shell** (`src/workbench/`): a standalone widget composing **rail | content
  (segmented Inspect: Raw/Parsed/Dashboard) | right inspector | bottom drawer** + the slim top bar.
  Built with placeholder/real content, tested standalone.
- **S4 — integration**: mount `WorkbenchFrame` in `MainWindow`, porting connections, onboarding, the
  three views, and relocating the dashboard toolbar into the Dashboard context. **Regenerate the whole
  visual-baseline set** here (one wholesale re-accept). The risky step — done last, after the pieces are
  proven.

## Decisions (decide-and-log)

- Component library lives in `src/workbench/` (the redesign home; now links Qt6::Widgets).
- Rail buttons use text labels (+ glyph) now; a real line-icon set is a later polish (avoid the
  QIcon/xcb teardown-crash class — see memory `qt_xcb_teardown_crash` — and don't block the frame on art).
- Frame is built as a **new widget mounted into the existing MainWindow** (not a parallel window), so all
  the existing pipeline/connection/tap wiring is reused, not duplicated.

## Out of scope (later phases)

Parsed identity UI / sparklines = P2. Raw dissection tree = P3. Dashboard view/edit + widget registry =
P4. Drill-through wiring = P5. Control reserved; Replay frozen.

## DoD

Build Debug+Release green; ctest green both; unit tests for each component; clang-format clean; visual
baselines regenerated at S4; local commits, no push. No frozen interface/schema touched.
