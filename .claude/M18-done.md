# M18 — Completion Report

## Scope

M18 completes the first Workflow Rebuild pass: connection, observation,
recording, replay, exit/switch confirmations, state visibility, visual
system coverage, and regression baselines.

## Acceptance Matrix

| Requirement | Evidence |
| --- | --- |
| Connection create/edit defaults to UDP and hides advanced noise | `ConnectionDialog` defaults add-flow to UDP; advanced command group is collapsed unless existing config has commands; covered by `connection_dialog_test` and visual states 38/41. |
| Recording shows active status, elapsed time, bytes, filename, stopped result, and errors | `MainWindow` recording status heartbeat and stopped/error labels; close protection in state 46; covered by `test_states_ux_workflows.py` and replay/record unit coverage. |
| Replay loaded/playing/paused/ended/seek/speed is visible in toolbar and status strip | Replay toolbar action states, compact Replay status cell, Mode sub-state cell, seek/speed immediate feedback; covered by states 37/39/40 and `playback_controller_test`. |
| Live/Recording/Replay/Paused/Ended mode boundaries are visible | Mode cell uses `mode-live`, `mode-recording`, `mode-replay`, `mode-paused`, and `mode-ended`; covered by states 35/36/37/39/40. |
| Recording, replay, and failed-config-save exits confirm before destructive close | Production `closeEvent` prompts and visual states 45/46/47 prove replay, recording, and config-save failure paths. |
| Status strip groups key system state | Connection, Config, Recording, Replay, Mode, Chart, and Buffer cells are built in `MainWindow`; visible across states 35-49. |
| Semantic styles cover normal/warning/error/recording/replay/paused/ended | Token-driven QSS includes mode and severity classes; generator check passes. |
| Buffer pressure is visible and explained | Buffer warning/full state hooks and status cell tooltip; covered by visual states 43/44. |
| Chart empty/no-signal/no-selected/data-interrupted states are visible | Guided empty workflow, signal selector empty copy, chart frame no-selection copy, and state 49 interruption status. |
| Errors persist outside popups | Connection error, config-save failure, recording/replay error labels and banners; covered by states 42/47/48 plus unit tests. |
| Main workbench uses the visual system | Toolbar/dialog/status/chart-frame styling moved to token/QSS-backed component classes; `chart.cpp` and generated style assets updated. |
| Formal workflow visual baselines exist | Baselines 35-49 are present under `tests/visual/baselines/`; 37/39/40/49 were refreshed after the final status-strip changes. |
| Test layers are separated | Unit tests cover state/config behavior; `test_states_ux_workflows.py` covers visual states; `test_workflows_operation.py` drives record-to-replay-ended workflow. |
| Repository hygiene is controlled | `.gitignore` covers local scratch/generated outputs; `git status --short` shows only M18 production, docs, tests, and baselines. |

## Final Verification

- `git diff --check` — PASS.
- `python3 tools/generate_style_assets.py --check` — PASS.
- `python3 -m py_compile tests/visual/tests/test_states_ux_workflows.py tests/visual/tests/test_workflows_operation.py tests/visual/scripts/capture_baselines.py` — PASS.
- `ctest --test-dir build/debug -N -L visual` — PASS; 9 visual tests discovered.
- `cmake --build build/debug --target signalforge connection_manager_test connection_persistence_test connection_dialog_test connection_widgets_test playback_controller_test` — PASS.
- `build/debug/tests/unit/connection/connection_manager_test` — PASS.
- `build/debug/tests/unit/connection/connection_persistence_test` — PASS.
- `build/debug/tests/unit/connection/connection_dialog_test` — PASS.
- `build/debug/tests/unit/connection/connection_widgets_test` — PASS.
- `build/debug/tests/unit/replay/playback_controller_test` — PASS.
- `SIGNALFORGE_BINARY=/home/shuai/Music/signalforge/build/debug/src/app/signalforge python3 tests/visual/tests/test_states_ux_workflows.py` — PASS.
- `SIGNALFORGE_BINARY=/home/shuai/Music/signalforge/build/debug/src/app/signalforge python3 tests/visual/tests/test_workflows_operation.py` — PASS.

## Residual Notes

- `connection_widgets_test` still emits existing replay-driver teardown
  warnings; assertions pass and this is not introduced by M18.
- Hardware-backed serial/TCP workflows remain outside M18; M18 focuses on
  UDP-heavy embedded debug and replay/record workflow closure.
