# M33 — progress (UI redesign Phase 0: foundations)

## S1 — SelectionModel  ✅
- New module `signalforge_workbench` (the redesign's home; Qt6::Core only so far).
- `SelectionModel`: app-wide current `Selection {kind, id}` (Signal/Packet/MessageType/Widget), with
  `select` (deduped emit), `clear`, `isSelected`, and `selectionChanged`. The backbone for cross-tier
  highlight + drill-through (Parsed → source packets in Raw).
- Pure QObject, no UI/data deps. Tests: 4 cases / 12 assertions. Debug + Release green; fmt clean.

## S2 — signal/message identity service  ✅
- `SignalIdentity`: assigns each signal a stable **palette color index** (0..7, first-seen order, wraps) —
  theme-independent (the view resolves index → `color.signal.<i>` from tokens). Single source of truth for
  signal color across Raw/Parsed/Dashboard.
- `Quality` enum (Good/Stale/Uncertain/Bad) + `qualityFromAge(age, staleAfter, badAfter, decodeError)` +
  `qualityName`. Pure, no deps. Tests: 3 cases / 22 assertions. Debug + Release green; fmt clean.

## S3 — design tokens v2 · S4 — component skeleton → MOVED to P1 (the frame)
Rationale (decide-and-log): tokens without consumers and components without a mount point are speculative
and untestable in isolation. Building them **with** the frame (P1) means designing their API against real
use, not a guess — better engineering and avoids rework. M33 (Phase 0) therefore delivers the two pure
**services** (the genuine invisible backbone); the visual layer starts in P1.

## Status
M33 (Phase 0 services) complete. The redesign's data backbone — SelectionModel + SignalIdentity — is in
place for P1 (the activity-rail frame) to build on.
