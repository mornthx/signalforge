# M18 — Workflow Rebuild Spec

## Objective

Upgrade SignalForge from a collection of reachable features into a
workflow-oriented industrial tool UI. The operator must be able to move
through the full loop:

1. Create or edit a connection.
2. Observe live signals and chart state.
3. Record a session with visible progress and outcome feedback.
4. Replay a recorded session with synchronized transport/status/chart
   feedback.
5. Exit or switch modes with clear confirmations and recovery choices.

## Required Workflow Behavior

- Connection creation defaults to the UDP-heavy embedded debugging path.
  Advanced auto-connect command fields stay out of the default path, but
  remain available and persist when configured.
- Recording shows active state, elapsed time, bytes written, target
  filename, stop result, and error state in the UI.
- Replay shows loaded, playing, paused, ended, seek, and speed feedback
  in the toolbar and status strip. Paused and ended states must be
  semantically distinct from generic Replay mode.
- Live, Recording, Replay, Paused, and Ended mode boundaries must be
  visible without inferring state from hidden toolbars or logs.
- Quit paths must protect active work: recording in progress, replay in
  progress, and failed connection-config save all require explicit user
  confirmation with an understandable path forward.

## Required State Visibility

- The status strip is grouped into Connection, Config, Recording,
  Replay, Mode, Chart, and Buffer cells.
- Key states use semantic style classes: normal/info, warning, error,
  recording, replay, paused, and ended.
- Buffer pressure is visible through color, threshold wording, and a
  tooltip.
- Chart empty, no-signal, no-selected-signal, and data-interrupted
  states are visible in the workbench.
- Errors must not live only in modal dialogs; relevant panels or status
  cells must retain the state.

## Required Visual System Coverage

- The main workbench applies token-driven style to toolbar, dock,
  dialog, status strip, and chart-frame surfaces.
- UI density should stay appropriate for an industrial tool: compact,
  stable, scannable, and not decorative.
- Replay toolbar, connection dialog, recording controls, status cells,
  and chart frame must have stable component styling.

## Required Verification

- Workflow visual states 35-49 are formal baselines under
  `tests/visual/baselines/`.
- Visual acceptance covers idle, recording, replay playing, replay
  paused, replay ended, buffer warning, buffer full, connection error,
  replay-close confirmation, recording-close confirmation, config-save
  failure, and chart data interruption.
- Unit tests cover changed state transitions, signals, and config
  persistence behavior.
- An operation-flow test drives a real workflow from launch through
  recording to replay-ended.
- Temporary screenshots, fixtures, and local scratch must be either
  ignored or promoted to committed artifacts.
- Any UI change made during M18 must either update the relevant baseline
  or document why no baseline update is required.

## Completion Evidence

M18 is complete only when the implementation, baselines, tests, and
repository status prove every item above. Passing tests are necessary but
not sufficient unless their coverage maps to the corresponding workflow
requirement.
