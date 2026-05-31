# M19 — Completion Report

## Scope

M19 completes the V0.3 extended-state automation pass: the 15 M15
operator-manual residual states are now automated, tested, and backed by
formal baselines.

## Acceptance Matrix

| State | Evidence |
| --- | --- |
| 03 `03-conn-udp-connecting` | `--auto-connection-state connecting`; baseline and `test_states_m19_extended.py`. |
| 06 `06-conn-udp-disconnecting` | `--auto-connection-state disconnecting`; baseline and visual test. |
| 07 `07-conn-udp-error` | `--auto-connection-state error`; baseline and visual test. |
| 08 `08-conn-serial-idle` | Generated Serial YAML + `--auto-no-connect`; baseline and visual test. |
| 09 `09-conn-serial-connected` | `socat` PTY fixture + production Serial driver; baseline and visual test. |
| 10 `10-conn-tcp-idle` | Generated TCP YAML + `--auto-no-connect`; baseline and visual test. |
| 11 `11-conn-tcp-connected` | Local loopback TCP server + production TCP driver; baseline and visual test. |
| 16 `16-replay-open-dialog` | Deterministic non-native QFileDialog via `--auto-m19-modal replay-open-dialog`; baseline and visual test. |
| 22 `22-mode-live-to-replay` | Active UDP fixture plus confirmation modal; baseline and visual test. |
| 23 `23-mode-replay-to-live` | Replay fixture plus 3-option exit modal; baseline and visual test. |
| 27 `27-dialog-quit-recording` | Production closeEvent while recording; baseline and visual test. |
| 28 `28-dialog-recording-error` | Fault modal plus recording status-strip error state; baseline and visual test. |
| 29 `29-dialog-replay-error` | Fault modal plus replay status-strip error state; baseline and visual test. |
| 34 `34-status-buffer-warn` | Buffer warning visual state; baseline and visual test. |
| 35 `35-status-buffer-full` | Buffer full visual state; baseline and visual test. |

## Implementation Notes

- `ConnectionListWidget` and `ConnectionStatusWidget` gained visual-test
  hooks so transient states can be rendered through production widget
  styling without changing frozen `Connection` / `ConnectionManager`
  contracts.
- `MainWindow` gained M19 CLI hooks:
  `--auto-connection-state` and `--auto-m19-modal`.
- `capture_baselines.py` now records M19 automation notes instead of
  stale operator-manual instructions.

## Final Verification

- `git diff --check` — PASS.
- `python3 -m py_compile tests/visual/tests/test_states_m19_extended.py tests/visual/tests/test_states_ux_workflows.py tests/visual/tests/test_workflows_operation.py tests/visual/scripts/capture_baselines.py` — PASS.
- `ctest --test-dir build/debug -N -L visual` — PASS; 10 visual tests discovered.
- `cmake --build build/debug --target signalforge connection_widgets_test` — PASS.
- `build/debug/tests/unit/connection/connection_widgets_test` — PASS.
- `SIGNALFORGE_BINARY=/home/shuai/Music/signalforge/build/debug/src/app/signalforge python3 tests/visual/tests/test_states_m19_extended.py` — PASS.
- `SIGNALFORGE_BINARY=/home/shuai/Music/signalforge/build/debug/src/app/signalforge python3 tests/visual/tests/test_states_ux_workflows.py` — PASS.
- `SIGNALFORGE_BINARY=/home/shuai/Music/signalforge/build/debug/src/app/signalforge python3 tests/visual/tests/test_workflows_operation.py` — PASS.

## Residual Notes

- Serial-connected state requires `socat` in the test environment.
- `connection_widgets_test` still emits existing replay-driver teardown
  warnings; assertions pass and M19 does not introduce them.
