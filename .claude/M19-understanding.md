# M19 — Understanding

## Objective

M19 follows the V0.3 charter amendment and closes the residual visual
coverage gap left by M15: the 15 states that were operator-manual because
they needed hardware, transient timing, fault injection, or modal capture.

The expected outcome is a repeatable automated visual suite for those
states, not a new redesign pass.

## Source Of Truth

- `docs/V0-charter-amendment-v0.3.md` §3 defines M19.
- `.claude/M15-done.md` §5 identifies the 15 operator-manual states.
- Existing M15/M18 harnesses provide the capture model:
  `tests/visual/scripts/capture_baselines.py`,
  `tests/visual/tests/test_states_ux_workflows.py`, and
  `tests/visual/lib/capture.py`.

## Working Interpretation

- Serial/TCP connected states should use real local fixtures where
  possible. Serial can use `socat` PTY pairs when available; TCP can use
  a local Python socket server.
- Transient connection states should be rendered through the production
  connection widgets but may use a visual hook, because real UDP
  Connecting/Disconnecting windows are shorter than deterministic xvfb
  capture timing.
- Existing M18 states already cover some equivalent workflows under
  M18 names. M19 still needs the M15 canonical state numbers/names so
  the V0.2 38-baseline target can be audited directly.
- M19 should not modify frozen M2-M12 public contracts to create visual
  hooks. MainWindow and V0.3 widgets are acceptable extension points.

## Risks

- `socat` may be absent. M19 should detect this clearly; if absent, the
  Serial-connected visual test can skip with an explicit reason, but M19
  completion requires an environment where the state has been captured.
- Modal top-level windows require fullscreen capture timing. Existing
  `captureFullScreen` support should be reused.
- Replay file dialogs may use native platform dialogs unless forced by
  Qt. M19 should prefer deterministic in-process Qt dialogs or a
  controlled modal hook if native dialogs are unstable headlessly.

## Definition Of Done

All 15 M19 states have automated tests, accepted baselines, documented
fixture behavior, and recorded verification evidence.
