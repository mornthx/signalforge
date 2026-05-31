# M19 — Hardware Fixtures + Extended State Coverage Spec

## Objective

Automate the remaining V0.2 operator-manual visual states called out by
`docs/V0-charter-amendment-v0.3.md` so V0.3 can close with a complete,
repeatable baseline set.

M19 is not a new visual redesign milestone. It is a verification and
fixture milestone: take the states that previously required an operator,
real hardware, transient timing, or ad hoc fault injection, and make them
capturable by the repository's visual test harness.

## Required State Coverage

M19 must lift these 15 states from manual/operator status to automated
visual coverage:

| State | Name | Required coverage |
| --- | --- | --- |
| 03 | `03-conn-udp-connecting` | Connection list/status strip show Connecting. |
| 06 | `06-conn-udp-disconnecting` | Connection list/status strip show Disconnecting. |
| 07 | `07-conn-udp-error` | Connection error is visible outside logs. |
| 08 | `08-conn-serial-idle` | Serial connection loaded but not connected. |
| 09 | `09-conn-serial-connected` | Serial connection connected through an automated fixture. |
| 10 | `10-conn-tcp-idle` | TCP connection loaded but not connected. |
| 11 | `11-conn-tcp-connected` | TCP connection connected through an automated fixture. |
| 16 | `16-replay-open-dialog` | Replay file-open modal flow visible. |
| 22 | `22-mode-live-to-replay` | Live-to-Replay confirmation flow visible. |
| 23 | `23-mode-replay-to-live` | Replay-to-Live exit/restore flow visible. |
| 27 | `27-dialog-quit-recording` | Quit-while-recording prompt visible. |
| 28 | `28-dialog-recording-error` | Recording error dialog/state visible. |
| 29 | `29-dialog-replay-error` | Replay error dialog/state visible. |
| 34 | `34-status-buffer-warn` | Extreme buffer warning state visible. |
| 35 | `35-status-buffer-full` | Extreme buffer full state visible. |

## Implementation Boundaries

- Prefer real fixtures for hardware-dependent connected states:
  Serial uses a virtual PTY when available; TCP uses a local loopback
  server fixture.
- Transient states may use a visual-test hook on V0.3 UI widgets when
  the production transition is too brief to capture deterministically.
  The hook must render through the same production widget styles.
- Fault-injection and modal states may use CLI automation flags, but the
  visible UI must be production UI, not a synthetic screenshot.
- Do not extend M2-M12 frozen public contracts for visual-only needs.
- Committed artifacts are baselines and tests only; screenshots and
  generated fixture scratch remain ignored.

## Required Test Layers

- Visual tests verify every state above against committed baselines.
- Fixture tests or script-level assertions verify Serial/TCP helper setup
  where the state depends on external processes.
- `capture_baselines.py` must no longer classify these M19 states as
  manual without an explanation.

## Completion Evidence

M19 completion requires:

- `.claude/M19-understanding.md`, `.claude/M19-plan.md`,
  `.claude/M19-progress.md`, and `.claude/M19-done.md`.
- Automated visual test coverage for all 15 M19 states.
- Formal baseline PNGs for all 15 M19 states under
  `tests/visual/baselines/`.
- Verification commands recorded in the progress/done files.
- Clean repository status with only M19-intended files changed before
  commit.
