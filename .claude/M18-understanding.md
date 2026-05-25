# M18 — Workflow Rebuild Understanding

## Objective

M18 turns the M16 visual-identity foundation and M17 widget rebuild into
workflow-level UX. The target is not isolated widget polish; it is a
clear operator path through:

1. Create or edit a connection.
2. Observe live signal state.
3. Record a session with visible progress.
4. Open and control replay with visible mode/state feedback.
5. Exit or switch modes without hidden destructive effects.

## Current Inheritance

- M16 owns visual identity: Fusion, app palette, fonts, token-driven QSS,
  and cross-environment visual discipline.
- M17 rebuilt core widgets and left M18 with explicit hand-off items:
  dialog audit, recording/replay workflow chrome, toolbar styling, and
  semantic classes for workflow state.
- M15 catalogued the V1 UX gaps M18 must close or explicitly defer:
  double-percent text, buffer pressure visibility, replay play-state
  feedback, auto-connect startup semantics, replay record-count display,
  seek feedback, default driver friction, advanced command noise,
  recording progress, and general replay seek feedback.

## Working Interpretation

M18 should make state visible in production UI and make that visibility
testable. Automation hooks are acceptable only when they drive or expose
production UI states that a real operator can also reach.

The first batch already underway covers:

- Status strip grouping for connection, recording, replay, chart, and
  buffer status.
- Connection dialog simplification by collapsing advanced auto-connect
  commands.
- Replay toolbar/status feedback for loaded, playing, paused, ended,
  seek, and speed changes.
- Buffer warning/full visual states.
- Guided chart empty state for first-run workflow entry.
- Visual workflow states 35-44 for M18 review.

## Non-Goals For This Batch

- Hardware fixture automation belongs to M19 unless a small hook is
  required to verify an M18 UI state.
- Full dark-theme support remains outside this batch unless token
  changes are needed for semantic classes.
- M0-M13 historical milestone artifacts are not retroactively rewritten.

## Completion Evidence

M18 completion requires stronger evidence than passing unit tests:

- Unit tests for changed controller/dialog behavior.
- Visual workflow tests for all accepted workflow states.
- Baseline screenshots accepted for new visual states.
- A clean list of committed production changes, test harness changes,
  and intentionally ignored/generated artifacts.
