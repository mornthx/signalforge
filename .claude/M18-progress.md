# M18 — Workflow Rebuild Progress

## S0 — State Observation

- Branch at session start: `main`.
- Recent base: M17 merged via `d938452`.
- Current dirty worktree contains production UI workflow changes and new
  visual workflow harness work.
- `ctest -N -L visual` discovers `M15-visual-test_states_ux_workflows`.

## S1 — Documentation Bootstrap

- Added `.claude/M18-spec.md`.
- Added `.claude/M18-understanding.md`.
- Added `.claude/M18-plan.md`.
- Added this progress log.

## S2 — First Workflow Batch

In progress.

Completed so far:

- Status strip grouping exists for connection, recording, replay, chart,
  and buffer.
- Connection dialog advanced auto-connect commands are collapsed by
  default and expand when existing config has commands.
- Replay seek and speed changes emit immediate position feedback through
  `PlaybackController`.
- Replay toolbar/status UI distinguishes loaded, playing, paused, and
  ended states.
- Buffer warning/full visual states are exposed through the visual test
  harness.
- Recording status now includes a heartbeat-style elapsed timer and byte
  count while recording, plus stopped/error summaries.
- Status strip now has an explicit workflow Mode cell with semantic
  Live / Recording / Replay state, so mode is not inferred from toolbar
  visibility alone.
- New connection dialogs default to UDP while edited connections still
  load their persisted driver type.
- Closing the main window while Replay is active now prompts before
  exiting Replay; if Replay paused live connections, the prompt includes
  the resume-vs-stay-disconnected choice.
- Closing the main window while Recording is active remains a production
  closeEvent confirmation path and is now covered by the M18 visual
  workflow harness.
- Startup auto-connect now honors persisted opt-in
  `autoConnectOnStartup: true` configs via
  `ConnectionManager::connectStartupConnections()` after the default
  config file is loaded. The default remains manual because new
  connections persist `false`.
- Connection configuration auto-save now reports success/failure through
  `configurationSaveStateChanged`; the status strip has a Config cell,
  and closeEvent prompts before exiting after a failed save.
- Config save failures are also surfaced in the Connections dock body,
  so the connection workflow explains the risk before the user reaches
  the exit prompt.
- Replay Paused and Ended states now use explicit semantic classes
  (`mode-paused`, `mode-ended`) generated from the design tokens.
- Chart data interruption is surfaced as an `Interrupted` chart status
  with a warning class and explanatory tooltip.
- Replay status strip copy is compacted to state + elapsed/duration
  (`Playing`, `Paused`, `Ended`) with filename and record counts kept in
  tooltip, so the workflow state stays visible in the 1280px baseline
  width.
- The Mode cell mirrors Replay sub-states (`Paused`, `Ended`) with the
  same semantic classes, instead of only showing generic `Replay`.
- Chart dropped-frame counters stay in the `Drops` value and tooltip;
  the separate `Interrupted` label is now reserved for explicit data
  interruption states.

Open in S2:

- Review whether replay state needs additional semantic classes beyond
  `mode-replay` and warning-on-ended.
- Decide whether additional end-to-end operation-flow tests should be
  added on top of the current unit + visual harness coverage.

## S3 — Visual Workflow States

In progress.

- New workflow states 35-49 have screenshots under `tests/screenshots/`.
- States 35-49 have been promoted to `tests/visual/baselines/` after
  local visual review. Replay states 37/39/40 now use a real generated
  `.sfreplay` fixture with signal records rather than the 16-byte
  skeleton fixture.
- Baselines 35-44 were refreshed after adding the explicit Mode cell and
  UDP-default dialog behavior.
- Added state 45 (`45-replay-close-confirm`) to cover the Replay
  close/exit confirmation path via `--auto-close-window-after-ms`.
- Added state 46 (`46-recording-close-confirm`) to cover the Recording
  close confirmation path via `--auto-record-to` plus
  `--auto-close-window-after-ms`.
- State 46 was visually reviewed and promoted to
  `tests/visual/baselines/46-recording-close-confirm.png`.
- Added state 47 (`47-config-save-failed-close-confirm`) to cover the
  connection-config save failure status plus close confirmation path.
- Added state 48 (`48-config-save-failed-connection-panel`) to cover
  the Connections dock save-failure explanation before close.
- Added state 49 (`49-chart-data-interrupted`) to cover chart data
  interruption status visibility.
- Refreshed baselines 37, 39, 40, and 49 after compacting Replay/Chart
  status-strip copy. State 39 and 40 now prove Paused/Ended in both
  Replay and Mode cells.
- Added `SIGNALFORGE_VISUAL_FORCE_CAPTURE` to the M18 workflow visual
  harness so selected states can be re-captured without manually
  deleting ignored screenshot caches.

## S4 — Repository Hygiene

In progress.

- Submission surface is now limited to M18 production/test/docs/baseline
  files in `git status`.
- Local scratch and generated outputs are ignored:
  `FETCH_HEAD`, `Testing/`, `tools/crash_test/_deps/`,
  `tests/benchmark/results/`,
  `scripts/fix_codex_sandbox_permissions.sh`, and
  `tests/visual/scripts/capture_hardware_rhi_chart.py`.
- Ignored test captures remain under `tests/screenshots/`; committed
  visual evidence is under `tests/visual/baselines/`.

## S5 — Operation Flow Coverage

In progress.

- Added `tests/visual/tests/test_workflows_operation.py` as a non-pixel
  operation-flow gate. It drives the production binary through UDP
  fixture recording to a real `.sfreplay`, then launches Replay and steps
  to the Ended state.
- `ctest -N -L visual` now discovers
  `M15-visual-test_workflows_operation` in addition to the visual
  baseline state tests.

## Verification Log

- `cmake --build build/debug --target signalforge connection_dialog_test playback_controller_test` — PASS.
- `build/debug/tests/unit/connection/connection_dialog_test` — PASS.
- `build/debug/tests/unit/replay/playback_controller_test` — PASS.
- `SIGNALFORGE_BINARY=/home/shuai/Music/signalforge/build/debug/src/app/signalforge python3 tests/visual/tests/test_states_ux_workflows.py` — PASS.
- `python3 -m py_compile tests/visual/tests/test_states_ux_workflows.py tests/visual/scripts/capture_baselines.py` — PASS.
- `git diff --check` — PASS.
- `cmake --build build/debug --target signalforge connection_dialog_test playback_controller_test` — PASS after Mode/UDP-default changes.
- `build/debug/tests/unit/connection/connection_dialog_test` — PASS after Mode/UDP-default changes.
- `build/debug/tests/unit/replay/playback_controller_test` — PASS after Mode/UDP-default changes.
- `SIGNALFORGE_BINARY=/home/shuai/Music/signalforge/build/debug/src/app/signalforge python3 tests/visual/tests/test_states_ux_workflows.py` — PASS after refreshing baselines.
- `SIGNALFORGE_BINARY=/home/shuai/Music/signalforge/build/debug/src/app/signalforge python3 tests/visual/tests/test_states_ux_workflows.py` — PASS after adding state 45.
- `SIGNALFORGE_BINARY=/home/shuai/Music/signalforge/build/debug/src/app/signalforge python3 tests/visual/tests/test_states_ux_workflows.py` — PASS after adding state 46.
- `python3 -m py_compile tests/visual/tests/test_states_ux_workflows.py tests/visual/scripts/capture_baselines.py` — PASS after state 46 baseline promotion.
- `git diff --check` — PASS after state 46 baseline promotion.
- `cmake --build build/debug --target signalforge connection_dialog_test playback_controller_test` — PASS after state 46 baseline promotion.
- `build/debug/tests/unit/connection/connection_dialog_test` — PASS after state 46 baseline promotion.
- `build/debug/tests/unit/replay/playback_controller_test` — PASS after state 46 baseline promotion.
- `SIGNALFORGE_BINARY=/home/shuai/Music/signalforge/build/debug/src/app/signalforge python3 tests/visual/tests/test_states_ux_workflows.py` — PASS after state 46 baseline promotion.
- `git diff --check` — PASS after startup auto-connect implementation.
- `cmake --build build/debug --target signalforge connection_manager_test connection_persistence_test connection_dialog_test playback_controller_test` — PASS after startup auto-connect implementation.
- `build/debug/tests/unit/connection/connection_manager_test` — PASS after startup auto-connect implementation.
- `build/debug/tests/unit/connection/connection_persistence_test` — PASS after startup auto-connect implementation.
- `build/debug/tests/unit/connection/connection_dialog_test` — PASS after startup auto-connect implementation.
- `build/debug/tests/unit/replay/playback_controller_test` — PASS after startup auto-connect implementation.
- `python3 -m py_compile tests/visual/tests/test_states_ux_workflows.py tests/visual/scripts/capture_baselines.py` — PASS after startup auto-connect implementation.
- `SIGNALFORGE_BINARY=/home/shuai/Music/signalforge/build/debug/src/app/signalforge python3 tests/visual/tests/test_states_ux_workflows.py` — PASS after startup auto-connect implementation.
- `cmake --build build/debug --target signalforge connection_manager_test` — PASS after config-save status implementation.
- `build/debug/tests/unit/connection/connection_manager_test` — PASS after config-save status implementation.
- `python3 -m py_compile tests/visual/tests/test_states_ux_workflows.py tests/visual/scripts/capture_baselines.py` — PASS after adding state 47.
- `SIGNALFORGE_BINARY=/home/shuai/Music/signalforge/build/debug/src/app/signalforge python3 tests/visual/tests/test_states_ux_workflows.py` — PASS after adding state 47.
- `git diff --check` — PASS after state 47 baseline promotion.
- `cmake --build build/debug --target signalforge connection_manager_test connection_persistence_test connection_dialog_test playback_controller_test` — PASS after state 47 baseline promotion.
- `build/debug/tests/unit/connection/connection_manager_test` — PASS after state 47 baseline promotion.
- `build/debug/tests/unit/connection/connection_persistence_test` — PASS after state 47 baseline promotion.
- `build/debug/tests/unit/connection/connection_dialog_test` — PASS after state 47 baseline promotion.
- `build/debug/tests/unit/replay/playback_controller_test` — PASS after state 47 baseline promotion.
- `python3 -m py_compile tests/visual/tests/test_states_ux_workflows.py tests/visual/scripts/capture_baselines.py` — PASS after state 47 baseline promotion.
- `SIGNALFORGE_BINARY=/home/shuai/Music/signalforge/build/debug/src/app/signalforge python3 tests/visual/tests/test_states_ux_workflows.py` — PASS after state 47 baseline promotion.
- `cmake --build build/debug --target signalforge` — PASS after adding the Connections-dock config save failure banner.
- `python3 -m py_compile tests/visual/tests/test_states_ux_workflows.py tests/visual/scripts/capture_baselines.py` — PASS after adding state 48.
- `SIGNALFORGE_BINARY=/home/shuai/Music/signalforge/build/debug/src/app/signalforge python3 tests/visual/tests/test_states_ux_workflows.py` — PASS after adding state 48.
- `git diff --check` — PASS after state 48 baseline promotion.
- `cmake --build build/debug --target signalforge connection_manager_test connection_persistence_test connection_dialog_test connection_widgets_test playback_controller_test` — PASS after state 48 baseline promotion.
- `build/debug/tests/unit/connection/connection_manager_test` — PASS after state 48 baseline promotion.
- `build/debug/tests/unit/connection/connection_persistence_test` — PASS after state 48 baseline promotion.
- `build/debug/tests/unit/connection/connection_dialog_test` — PASS after state 48 baseline promotion.
- `build/debug/tests/unit/connection/connection_widgets_test` — PASS after state 48 baseline promotion.
- `build/debug/tests/unit/replay/playback_controller_test` — PASS after state 48 baseline promotion.
- `python3 -m py_compile tests/visual/tests/test_states_ux_workflows.py tests/visual/scripts/capture_baselines.py` — PASS after state 48 baseline promotion.
- `SIGNALFORGE_BINARY=/home/shuai/Music/signalforge/build/debug/src/app/signalforge python3 tests/visual/tests/test_states_ux_workflows.py` — PASS after state 48 baseline promotion.
- `git check-ignore -v FETCH_HEAD Testing/Temporary/LastTest.log tools/crash_test/_deps/sentry_native-src/README.md scripts/fix_codex_sandbox_permissions.sh tests/benchmark/results/m9-s5s/soak-1hour.stderr tests/visual/scripts/capture_hardware_rhi_chart.py` — PASS after repo hygiene ignore update.
- `git diff --check` — PASS after repo hygiene ignore update.
- `python3 -m py_compile tests/visual/tests/test_states_ux_workflows.py tests/visual/scripts/capture_baselines.py` — PASS after repo hygiene ignore update.
- `SIGNALFORGE_BINARY=/home/shuai/Music/signalforge/build/debug/src/app/signalforge python3 tests/visual/tests/test_states_ux_workflows.py` — PASS after repo hygiene ignore update.
- `SIGNALFORGE_BINARY=/home/shuai/Music/signalforge/build/debug/src/app/signalforge python3 tests/visual/tests/test_workflows_operation.py` — PASS after adding operation-flow coverage.
- `cmake --build build/debug --target signalforge` — PASS; refreshed visual test glob for operation-flow discovery.
- `ctest --test-dir build/debug -N -L visual` — PASS; discovers `M15-visual-test_workflows_operation`.
- `python3 tools/generate_style_assets.py --check` — PASS after adding `mode-paused` / `mode-ended`.
- `cmake --build build/debug --target signalforge` — PASS after chart interruption and replay semantic class updates.
- `SIGNALFORGE_BINARY=/home/shuai/Music/signalforge/build/debug/src/app/signalforge python3 tests/visual/tests/test_states_ux_workflows.py` — PASS after adding state 49.
- `cmake --build build/debug --target signalforge` — PASS after compact Replay/Chart status-strip updates.
- `SIGNALFORGE_BINARY=/home/shuai/Music/signalforge/build/debug/src/app/signalforge SIGNALFORGE_VISUAL_FORCE_CAPTURE=37-replay-playing,39-replay-paused,40-replay-ended,49-chart-data-interrupted python3 tests/visual/tests/test_states_ux_workflows.py` — expected FAIL against old baselines, used to recapture affected states.
- `SIGNALFORGE_BINARY=/home/shuai/Music/signalforge/build/debug/src/app/signalforge python3 tests/visual/tests/test_states_ux_workflows.py` — PASS after refreshing baselines 37/39/40/49.
- `SIGNALFORGE_BINARY=/home/shuai/Music/signalforge/build/debug/src/app/signalforge python3 tests/visual/tests/test_workflows_operation.py` — PASS after refreshing baselines.
- `git diff --check` — PASS after refreshing baselines.
- `python3 tools/generate_style_assets.py --check` — PASS after refreshing baselines.
- `python3 -m py_compile tests/visual/tests/test_states_ux_workflows.py tests/visual/tests/test_workflows_operation.py tests/visual/scripts/capture_baselines.py` — PASS after refreshing baselines.
- `ctest --test-dir build/debug -N -L visual` — PASS; discovers 9 visual tests including `M15-visual-test_states_ux_workflows` and `M15-visual-test_workflows_operation`.
- `cmake --build build/debug --target signalforge connection_manager_test connection_persistence_test connection_dialog_test connection_widgets_test playback_controller_test` — PASS after refreshing baselines.
- `build/debug/tests/unit/connection/connection_manager_test` — PASS (79 assertions / 14 test cases).
- `build/debug/tests/unit/connection/connection_persistence_test` — PASS (70 assertions / 7 test cases).
- `build/debug/tests/unit/connection/connection_dialog_test` — PASS (46 assertions / 12 test cases).
- `build/debug/tests/unit/connection/connection_widgets_test` — PASS (64 assertions / 17 test cases; existing replay-driver teardown warnings only).
- `build/debug/tests/unit/replay/playback_controller_test` — PASS (51 assertions / 8 test cases).
