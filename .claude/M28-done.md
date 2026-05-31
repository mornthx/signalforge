# M28 — plot gap + free-form layout (closure report)

Branch: `milestone/M28` (local, off `milestone/M27`; **not pushed**). Two user-reported issues.

## Problem A — live plot right-edge gap (`1651ea2`)

Readers only see data up to the last publish (the buffer's publish-cadence/flush lag), but the
plot mapped against wall-clock "now", so a gap grew and reset at the right edge. Fix:
- buffer: lower the M25 time-flush 200 ms → 100 ms (coincides with the 100-sample cadence for
  the densest 1 ms-spaced LOD test, so all 48 buffer tests still pass — 33 ms broke the LOD
  pyramid tests).
- PlotView: anchor the paint window's right edge to the **newest available sample** (not "now").
  New data reaches the right edge; any clip lands on the older left edge. Right-edge render test.

Verified live: the temperature trace now reaches the right edge as it scrolls.

## Problem B — free-form drag-resize (`2d5253b`)

Replaced the reflow `QGridLayout` with a **free-form surface**: panels are absolutely positioned
children carrying their own geometry (`PanelConfig.geometry`).
- **Drag the header** → move the panel (clamped to the surface).
- **Drag the bottom-right grip** → resize.
- Untouched panels **auto-place** in a flow (cards left-to-right; Plot/Table full-width rows).
- User-placed geometry **persists** across type changes and survives reflow on resize.
- Removed the now-redundant ⋮ **Move ◀/▶** menu actions (per the user: drag replaces Move).

Implemented via a `Panel` event filter on the header + a grip widget; the panel writes its new
geometry into config and emits `geometryChanged`.

## Testing (simulate-real-interaction rule)

- QTest synthesizes a mouse stream (press/move/release with explicit global positions) on the
  header → asserts the panel moved + became user-placed; on the grip → asserts it resized +
  `geometryChanged` fired.
- A type change after a user drag → asserts the geometry is preserved.
- Plot right-edge render test (Problem A).

## Verification

- Debug + Release build green. Dashboard: 9 cases / 53 assertions (dashboard_test) + plot_view
  (17) + others. All green. 48 buffer tests green. clang-format clean.
- Live (running on the user's display): plot right edge filled; panels positioned; ⋮ menu
  (Show as / Signals / Remove — no Move); drag-move + grip-resize.

## Known follow-ups (not blockers)

- No scroll when panels overflow the surface height (absolute positioning can run past the
  bottom). A QScrollArea wrapper is the next step if needed.
- Geometry isn't yet persisted to disk (lives in-memory per session).

## Status

Local on `milestone/M28`. Chain: `main → M21 … → M28`.
