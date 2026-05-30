# M29 — progress (Phase A)

## S1 — persistent per-signal intent + symmetric lifecycle  ✅
- `Dashboard` gained `signalIntent_` (`signalId → PanelConfig` template; type+format, no geometry).
- `addSignal` restores the remembered form (else suggests + records first-time). (Report 2.)
- `removeSignalEverywhere` now removes a multi-signal panel once its last signal leaves. (Report 1.)
- `setPanelType` / `setPanelSignals` write the chosen form back into intent so it survives demote→promote.
- Tests: re-check restores Gauge (not Numeric); unchecking a 2-signal plot's last signal removes it.
- Verified: dashboard_test 63/63 Debug + Release; clang-format clean.

## S2 — bounded free+push layout  ✅
- `Panel` no longer self-moves: on drag it emits `dragProposed(id, rect)`; the `Dashboard`
  resolves bounds + neighbor collisions centrally (`resolvePanelDrag`) and drives `setUserGeometry`.
- Push: directly-overlapped neighbors are shoved along the axis of least penetration, clamped to the
  surface. **Single-hop only** — if a pushed neighbor would hit a third panel, or can't separate within
  bounds, the whole move is **refused** (no cascade, nothing leaves the viewport). (Report 3.)
- Tests: drag-into-neighbor pushes it aside (asserts separation + in-bounds); drag with no room (3 cards,
  one-row-tall surface) is refused (nothing moves). M28 drag tests still pass through the new resolver.
- Verified: 684/684 ctest Debug + Release; dashboard_test 13 cases / 76 assertions; clang-format clean.
  (clang-tidy is the CI gate, per project convention.)

## S3 — header-less draggable card chrome  ✅ (follow-up from inspection)
- Owner feedback: the header bar is redundant — left-press the card to drag, right-click to configure,
  show signal name + source inside the card.
- Removed the header frame + ⋮ button. The whole card is the drag handle (`Panel` mouse overrides +
  a recursive drag event-filter so dragging works over the body, incl. tables). Right-click anywhere →
  `configureRequested` → menu at the cursor.
- In-card identity strip: signal **name** (heading) + **source/driver** (caption), transparent to mouse.
- Tests updated: drags target the card directly; config is invoked by a synthesized right-click
  (`QContextMenuEvent`). dashboard_test 13/76, panel_factory_test 4/25.
- Verified: 684/684 ctest Debug + Release (visual baselines unaffected); clang-format clean.

## Report 4 (raw-data-first) — carried to Phase B/C (Parsed/Raw tabs).

M29 (Phase A) complete. Local on `milestone/M29`; not pushed.
