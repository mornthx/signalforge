# M18 — Workflow Rebuild Plan

## S0 — State Observation

- Record current branch, recent commits, dirty worktree, and discovered
  M18 work already present.
- Confirm visual test discovery includes the new workflow-state test.

## S1 — M18 Documentation Bootstrap

- Add M18 spec, understanding, plan, and progress files.
- Keep scope tied to the V0.3 charter and M17 hand-off rather than a
  narrow patch list.

## S2 — First Workflow Batch

Implement and verify the first production UI batch:

- Status strip grouped by connection / recording / replay / chart /
  buffer.
- Guided empty chart state.
- Recording progress with elapsed time, byte count, stopped summary, and
  error surfacing.
- Replay play/pause/end/seek/speed status feedback.
- Connection dialog advanced commands collapsed by default.
- Buffer pressure semantic status.

## S3 — Visual Workflow States

- Keep `tests/visual/tests/test_states_ux_workflows.py` as the workflow
  acceptance harness for states 35-49.
- Generate or refresh screenshots for review.
- Promote accepted screenshots to `tests/visual/baselines/` with masks
  only where there is a justified dynamic region.

## S4 — Remaining Workflow Closure

After S2/S3 land, finish or explicitly defer:

- Auto-connect startup semantics: honor persisted opt-in configs while
  keeping the high-noise toggle out of the default dialog path.
- Default driver behavior for embedded UDP-heavy workflows.
- Mode transition dialogs and recovery paths.
- Quit-while-recording visual/interaction coverage.
- Replay record-count edge cases for catalog-only sessions.

## S5 — Verification And Repository Hygiene

- Build app target.
- Run focused unit tests.
- Run the workflow visual test.
- Run `git diff --check`.
- Classify untracked files into committed artifacts, generated ignored
  outputs, or local-only scratch.
