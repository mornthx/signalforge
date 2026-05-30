# M33 — progress (UI redesign Phase 0: foundations)

## S1 — SelectionModel  ✅
- New module `signalforge_workbench` (the redesign's home; Qt6::Core only so far).
- `SelectionModel`: app-wide current `Selection {kind, id}` (Signal/Packet/MessageType/Widget), with
  `select` (deduped emit), `clear`, `isSelected`, and `selectionChanged`. The backbone for cross-tier
  highlight + drill-through (Parsed → source packets in Raw).
- Pure QObject, no UI/data deps. Tests: 4 cases / 12 assertions. Debug + Release green; fmt clean.

## S2 — signal/message identity service  ⏳ (next)
## S3 — design tokens v2  ⏳
## S4 — component-library skeleton  ⏳
