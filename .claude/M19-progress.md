# M19 — Progress

## S0 — Baseline Audit

- Started from clean `main` after `c332657 feat: complete M18 workflow rebuild`.
- M19 source scope confirmed from
  `docs/V0-charter-amendment-v0.3.md` §3 and `.claude/M15-done.md`
  §5.
- Canonical M19 state list: 03, 06, 07, 08, 09, 10, 11, 16, 22, 23,
  27, 28, 29, 34, 35.

## S1 — Documentation Bootstrap

- Added `.claude/M19-spec.md`.
- Added `.claude/M19-understanding.md`.
- Added `.claude/M19-plan.md`.
- Added this progress log.

## S2 — Fixture Infrastructure

- Added `tests/visual/tests/test_states_m19_extended.py`.
- Serial connected state uses `socat` to create a temporary PTY pair
  under `tests/screenshots/_runs/m19/`; the generated YAML points the
  production Serial driver at one PTY.
- TCP connected state uses a local Python socket server and generated
  YAML pointing the production TCP driver at the loopback port.
- Serial/TCP idle states use generated YAML plus `--auto-no-connect`.

## S3 — Visual Automation Hooks

- Added V0.3 widget-level visual hooks:
  `ConnectionListWidget::setVisualStateForTest` and
  `ConnectionStatusWidget::setVisualStateForTest`.
- Added MainWindow/CLI M19 hooks:
  `--auto-connection-state` for Connecting / Disconnecting / Error, and
  `--auto-m19-modal` for replay-open, live-to-replay, replay-to-live,
  recording-error, and replay-error modal states.
- The hooks avoid changing M2-M12 frozen `Connection` /
  `ConnectionManager` public contracts.

## S4 — Visual State Tests

- M19 visual test covers all canonical states:
  03, 06, 07, 08, 09, 10, 11, 16, 22, 23, 27, 28, 29, 34, 35.
- Promoted accepted PNG baselines for all 15 states under
  `tests/visual/baselines/`.

## S5 — Capture Orchestrator Alignment

- Updated `tests/visual/scripts/capture_baselines.py`: static M19 states
  now have CLI recipes; dynamic Serial/TCP states point to
  `test_states_m19_extended.py` fixture automation instead of stale
  operator-manual notes.

## Verification Log

- `cmake --build build/debug --target signalforge` — PASS after M19 hooks.
- `SIGNALFORGE_BINARY=/home/shuai/Music/signalforge/build/debug/src/app/signalforge SIGNALFORGE_VISUAL_FORCE_CAPTURE=all python3 tests/visual/tests/test_states_m19_extended.py` — PASS; captured all 15 M19 states.
- `SIGNALFORGE_BINARY=/home/shuai/Music/signalforge/build/debug/src/app/signalforge python3 tests/visual/tests/test_states_m19_extended.py` — PASS after baseline promotion.
- `SIGNALFORGE_BINARY=/home/shuai/Music/signalforge/build/debug/src/app/signalforge SIGNALFORGE_VISUAL_FORCE_CAPTURE=22-mode-live-to-replay python3 tests/visual/tests/test_states_m19_extended.py` — expected FAIL against old baseline; recaptured state 22 with active UDP fixture behind the confirmation.
- `SIGNALFORGE_BINARY=/home/shuai/Music/signalforge/build/debug/src/app/signalforge python3 tests/visual/tests/test_states_m19_extended.py` — PASS after refreshing state 22 baseline.
- `cmake --build build/debug --target signalforge connection_widgets_test` — PASS after widget-hook unit tests.
- `build/debug/tests/unit/connection/connection_widgets_test` — PASS (72 assertions / 19 test cases).
- `python3 -m py_compile tests/visual/tests/test_states_m19_extended.py tests/visual/tests/test_states_ux_workflows.py tests/visual/scripts/capture_baselines.py` — PASS.
- `ctest --test-dir build/debug -N -L visual` — PASS; discovers 10 visual tests including `M15-visual-test_states_m19_extended`.
- `SIGNALFORGE_BINARY=/home/shuai/Music/signalforge/build/debug/src/app/signalforge python3 tests/visual/tests/test_states_ux_workflows.py` — PASS; M18 visual workflow regression check.
- `git diff --check` — PASS.
- `python3 -m py_compile tests/visual/tests/test_states_m19_extended.py tests/visual/tests/test_states_ux_workflows.py tests/visual/tests/test_workflows_operation.py tests/visual/scripts/capture_baselines.py` — PASS.
- `ctest --test-dir build/debug -N -L visual` — PASS; final discovery still shows 10 visual tests.
- `cmake --build build/debug --target signalforge connection_widgets_test` — PASS.
- `build/debug/tests/unit/connection/connection_widgets_test` — PASS final (72 assertions / 19 test cases; existing replay-driver teardown warnings only).
- `SIGNALFORGE_BINARY=/home/shuai/Music/signalforge/build/debug/src/app/signalforge python3 tests/visual/tests/test_states_m19_extended.py` — PASS final.
- `SIGNALFORGE_BINARY=/home/shuai/Music/signalforge/build/debug/src/app/signalforge python3 tests/visual/tests/test_workflows_operation.py` — PASS final.
- `SIGNALFORGE_BINARY=/home/shuai/Music/signalforge/build/debug/src/app/signalforge python3 tests/visual/tests/test_states_ux_workflows.py` — PASS final M18 regression check.
