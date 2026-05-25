# M19 — Plan

## S0 — Baseline Audit

- Confirm worktree is clean after M18.
- Re-read V0.3 charter amendment and M15 deferred-state inventory.
- Map the 15 M19 states to existing hooks, missing hooks, and fixture
  requirements.

## S1 — Documentation Bootstrap

- Add M19 spec, understanding, plan, and progress files.
- Keep the state list tied to the canonical M15 state numbers.

## S2 — Fixture Infrastructure

- Add dynamic visual-test helpers for Serial and TCP fixture configs.
- Use `socat` for Serial-connected state when available.
- Use a local loopback TCP server for TCP-connected state.
- Keep generated fixture YAML under ignored screenshot/run scratch.

## S3 — Visual Automation Hooks

- Add MainWindow/V0.3 widget hooks for deterministic transient
  connection-state rendering.
- Add controlled hooks for modal/fault states where production user
  paths are not deterministic under xvfb.
- Avoid changing frozen M2-M12 public contracts.

## S4 — M19 Visual State Tests

- Add `tests/visual/tests/test_states_m19_extended.py`.
- Cover states 03, 06, 07, 08, 09, 10, 11, 16, 22, 23, 27, 28, 29,
  34, and 35.
- Promote accepted actual screenshots to
  `tests/visual/baselines/`.

## S5 — Capture Orchestrator Alignment

- Update `tests/visual/scripts/capture_baselines.py` so M19 states are
  no longer listed as manual without automation notes.
- Ensure visual test discovery includes the M19 suite.

## S6 — Verification And Close

- Build the app target.
- Run focused unit tests for any changed widgets/hooks.
- Run M19 visual tests.
- Run existing M18 workflow visual tests to catch regressions.
- Run `git diff --check` and Python compile checks.
- Add M19 done report and commit.
