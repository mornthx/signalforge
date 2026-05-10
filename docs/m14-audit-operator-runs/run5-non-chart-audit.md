# M14 GUI Audit — Run 5: Non-Chart Audit Findings

**Date**: 2026-05-09 → 2026-05-10 (CST)
**Operator**: shuai
**Branch / commit**: `milestone/M14` @ `4038191` (S2 chart-geometry fix)
**Binary under test**: `build/release/src/app/signalforge` (local build,
ABI-equivalent to the .deb at `build/release/signalforge_1.0.0_amd64.deb`
modulo `CPACK_STRIP_FILES`; same BuildID `3481b8cf...`)
**Companion run**: run 4 (`run4-halt-chart-zero-size.md`) for the chart-render
defect (F4); this run audits paths that do **not** depend on chart rendering.

## Why a non-chart audit

After four successive HALTs at `T3 step 6 — verify decoder → chart` (runs 1–4)
we shifted the operator session to verify every M13 protocol path that does
not bottleneck through the chart QQuickItem. Goal: surface bugs that would
otherwise be masked by the chart fix queue, so the next M14 fix wave can
batch them.

## Phase results (matrix)

| Phase | Subcheck | Verdict | Notes |
|---|---|---|---|
| A. Connection lifecycle | Add Serial | ✓ | dialog/save/list-row OK |
| | Add TCP | ✓ | |
| | Add UDP | ✓ | |
| | Edit | ✓ | baud change persisted across re-open dialog (in-memory) |
| | Remove | ✓ | row gone after confirm |
| | Connect / Disconnect state | ✓ | Idle ↔ Connected transitions visible in column |
| B. Recording | Start / Stop | ✓ | UI controls work; file open/close OK |
| | **Status-bar bytes increment** | ✗ | F7 — counter stays 0 during recording; jumps to final at Stop |
| | File written | ✓ | structurally valid SFREPLAY v1 |
| | sfreplay_inspect parses | △ | parses OK but key metadata empty (F8/F9) |
| C. Replay | File-open dialog | ✓ | `*.sfreplay` filter applies |
| | Mode switch into Replay | ✓ | |
| | Replay toolbar appears | ✓ | Play / Pause / Step ◀ / Step ▶ / speed dropdown |
| | Buttons clickable + state-bar position advances | ✓ | charts white per F4 but positional UI works |
| D. Mode transition | Live → Replay | ✓ | confirmation dialog observed |
| | Replay → Live | ✓ | exit-replay path with 3-option dialog observed |
| E. UI | Add Chart button | ✓ | chart-1..chart-5 all created |
| | Multi-chart layout | ✓ | vertical stack |
| | Signal Selector populates | ✓ | 5+ top-level signals + bit fields |
| | Signal toggle (checkbox) | ✓ | |
| F. Persistence | Connections auto-load | ✗ | **F17 — yaml never written, nothing to load** |
| | Auto-connect on startup | ✗ | blocked by F17 |

## Findings (F5–F18)

### F5 — Connection list lacks right-click context menu

- **Severity**: Minor (UX deviation, functional fallback present)
- **Symptom**: Right-clicking a connection row opens nothing. M9 protocol
  "Test 5 — Edit / remove connection" instructs `Right-click an Idle
  connection → Edit`. No menu appears.
- **Workaround**: Double-click opens the Edit dialog. All Edit/Remove
  functionality available via toolbar/menu/double-click instead.
- **Spec impact**: T5 (M9 §Test 5) wording mismatch but functional pass.
- **Recommended fix**: Add `QTreeWidget::customContextMenuRequested` →
  Edit / Remove / Connect / Disconnect items.

### F6 — Recording silently drops all signals when source was Connected pre-Record

- **Severity**: **Critical** (silent data loss)
- **Symptom**: When a driver is `Connected` *before* the user clicks
  Session → Record, the resulting `.sfreplay` file is structurally valid
  but completely empty:

  ```
  Records: 0 total
  Signal catalog (initial: 0; extensions added: 0)
  Type 1 (Signal Value): 0
  ```

  The status bar shows `Stopped (52 bytes)` — exactly header (36 B) +
  footer (16 B), zero payload.

- **Root cause** (in-source): `session_writer.cpp:46-58` documents that
  the SessionWriter's initial `metadata_.signalCatalog` comes from
  whatever `onSignalsRegistered` events the writer has observed since
  construction, *via TeeSink*. But `main_window.cpp:637-642` runs:

  ```cpp
  sessionWriter_->start(path);                     // ① writes header with empty catalog
  teeSink_->addSink(sessionWriter_.get());         // ② only NOW subscribes
  ```

  TeeSink does not replay history. If the decoder for an existing driver
  has already fired `onSignalsRegistered` to the TeeSink subscribers
  (`signalBufferRegistry_`), the SessionWriter — which is added later —
  will never see it. The writer's `onSignal()` continues to be called for
  in-flight values, but downstream `fileWriter_->enqueue()` evidently
  drops them silently because the recording's catalog index doesn't know
  the signal IDs.

- **Workaround**: Reverse the order — start Record first, then Connect.
  Verified: 24 423 B / 901 records (900 Signal Value + 1 Catalog
  Extension) over 19.82 s span captured cleanly.

- **Spec impact**: M10 protocol §Test 1 (GUI round-trip), §Test 2 (across
  restart), §Test 5 (backpressure) all assume the natural flow of
  Connect → Record → Stop. Under V1 this flow loses 100 % of data.
  T7 / T8 fail; T10 (mid-stream catalog) coincidentally passes only
  because the workaround flow *is* mid-stream registration.

- **Recommended fix**: At `start()` time, walk `registry_` (which is
  passed in but currently unused per `session_writer.cpp:25-29` "retained
  for V1.5+ when the writer may consult the registry directly") and seed
  `metadata_.signalCatalog` from the registered signals. Or have TeeSink
  cache the most recent `onSignalsRegistered(driverId, ...)` per driver
  and replay them to any `addSink()` caller.

### F7 — Status-bar byte counter does not update during recording

- **Severity**: Minor (UX) — but visually misleading
- **Symptom**: While Recording is active and data is flowing, the
  status-bar shows `● Recording: <name> (0 bytes)` indefinitely. Only on
  Session → Stop does it jump to the final byte count.
- **Spec impact**: M10 protocol §Test 1 step 3: *"Verify status bar
  shows '● Recording: <name> (N bytes)' with N updating periodically."*
  The "updating periodically" clause fails.
- **Recommended fix**: Wire SessionWriter byte-count signal to the
  status bar via `Qt::QueuedConnection` on a `QTimer`-driven poll
  (e.g. 250 ms tick).

### F8 — Recording metadata `description` is always empty

- **Severity**: Minor
- **Symptom**: `sfreplay_inspect` shows `Description: ""`. The Record
  dialog (currently a plain `QFileDialog`) does not collect a
  description.
- **Spec impact**: M10 spec §2.1-9 lists `description` as a metadata
  field; M10-done.md §15 confirms it's recorded. V1 ignores it at the
  GUI level.
- **Recommended fix**: Custom Record dialog with a description field,
  or default to filename basename + timestamp.

### F9 — Recording metadata `decoderSchemaId` is always empty

- **Severity**: **Serious** (breaks downstream replay validation)
- **Symptom**: `Schema ID: ""` in inspect output even when the live
  driver has a non-empty `decoderSchemaId`. SessionWriter's `start()`
  signature accepts a `decoderSchemaId` parameter
  (`session_writer.hpp:90`) but `main_window.cpp` does not pass one.
- **Spec impact**: M11 §S8 / §S9 mode-transition path expects
  schema-match validation when re-opening a recording in a different
  session. Without `decoderSchemaId` written to the file header, V1 has
  no way to verify the destination decoder matches the source.
- **Recommended fix**: At record start, look up the *active* connection
  whose driver is producing data and pass its `cfg.decoderSchemaId` (or
  the resolved schema path) into `sessionWriter_->start(path,
  description, decoderSchemaId)`. If multiple drivers are connected,
  choose by some deterministic rule and record it.

### F10 — Initial signal catalog is always empty (Catalog Extension only)

- **Severity**: Minor (M10 protocol nuance only; resolves naturally if F6 lands)
- **Symptom**: With the F6 workaround, every signal lands in
  `Catalog Extension` records, not the initial header catalog. Inspect
  shows `Signal catalog (initial: 0; extensions added: N)`.
- **Spec impact**: M10 protocol §Test 10 ("mid-stream signal
  registration") expects:
  > Initial catalog includes schema_A signals
  > At least one Catalog Extension record present
  > Final catalog includes schema_A + schema_B
  V1 cannot satisfy "initial includes schema_A" because the workaround
  flow forces *all* signals to go through extension. Once F6 is fixed
  and the natural Connect → Record flow works, the initial catalog will
  populate from the cached events and this finding goes away.

### F11 — Session load takes 10+ seconds for a 24 kB / 901-record file

- **Severity**: **Serious** (blocks T13 if the spec implies "snappy
  load"; perf miss regardless)
- **Symptom**: After `File → Open Session...` and selecting the
  24 423 B fixture, the GUI freezes for ≥ 10 s before the Replay
  toolbar appears. Operator reports occasional outright hang.
- **Likely cause** (correlation with F15, see below): the load path
  triggers signal-buffer (re)registration for every signal the file
  references, and each registration churns through the 256 MiB budget
  while logging at ~60 Hz. The UI thread blocks on the registration call
  chain.
- **Spec impact**: M11 §Test 1 "click Play in the replay toolbar" does
  not specify a load-time bound, but UX expectation is < 1 s for a
  24 kB file. 10× over typical bar.
- **Recommended fix**: investigate session-load → signal-buffer chain;
  consider deferred registration / lazy bind, and definitely fix F15
  (see below) before measuring T11.

### F12 — Replay time display uses wall-clock / system time instead of relative-from-zero

- **Severity**: Minor (M11 spec deviation)
- **Symptom**: Replay status bar shows time in absolute UTC / local
  format rather than `0:00 / 0:19.82` relative to the recording start.
- **Spec impact**: M11 §Test 4 (timeline scrubber) and §Test 5 (speed)
  rely on positional `<pos> / <duration>` formatting.
- **Recommended fix**: Format as `mm:ss.fff / mm:ss.fff` using relative
  offsets from `metadata_.recordingStart`.

### F13 — No per-frame inspection table

- **Severity**: Feature gap (likely V1.5+ scope)
- **Symptom**: There is no UI surface to inspect raw bytes / decoded
  fields of an individual frame. Operator notes this as expected for a
  signal-bringup workbench.
- **Spec impact**: Cross-reference `docs/v1.0-spec-list.md §1` to
  confirm whether per-frame view is in V1 frozen surface; if so this is
  a missed feature, otherwise V1.5+ tracking item.

### F14 — No state guard on `File → Open Session` while in Replay mode

- **Severity**: Serious (operator-reported error popup; not a crash)
- **Symptom**: While the GUI is already in Replay mode, the menu item
  `File → Open Session...` remains enabled. Clicking it triggers an
  error / fails to load the new file cleanly.
- **Spec impact**: M11 §S8 mode-transition logic expects the operator to
  use `Exit Replay` first. V1 should disable the menu item or auto-Exit
  Replay before re-loading.
- **Recommended fix**: Disable the action via
  `QAction::setEnabled(replayModeManager_->state() != Replay)` bound to
  state-change signals.

### F15 — `signal_buffer` budget exhaustion + 60 Hz log spam

- **Severity**: **Critical** (bounded-resource exhaustion + observability flood)
- **Symptom**: Multiple log lines per second during normal operation:

  ```
  signal_buffer registration rejected: would exceed budget.
  current=265416300 bytes, requested=13184100 bytes,
  budget=268435456 bytes, driver=session-replay
  ```

  Translation: `budget = 256 MiB`, `per-signal allocation ≈ 12.5 MiB`.
  With 14 signals from a single driver = 175 MiB; two drivers (live UDP
  + session-replay) = 350 MiB → exhaustion. Triggered by both `Connect`
  and `Open Session`.

  Each chart-redraw tick (~60 Hz) re-attempts the registration and logs
  a fresh error line. Hundreds of error lines per minute. No throttling,
  no de-duplication, no user-visible message.

- **Spec impact**: M11 protocol §Pre-flight assumes the operator can
  open a session and have it play. V1 lets the rejection cascade through
  the UI silently; the user only sees the chart not painting (which we
  already have F4 for) and assumes that's the chart bug.

- **Recommended fix** (multiple, partially-orthogonal):
  1. Tune the per-signal buffer size — 12.5 MiB suggests ~1.6 M samples
     × 8 B (double). Reduce default capacity or compute it from visible
     time-window × LOD pixel-count, not a fixed huge number.
  2. Throttle the rejection log (e.g., emit warning once per
     `(driverId, signalId)` pair per minute).
  3. Surface a user-visible error when registration is rejected.
  4. Freed buffers (after Disconnect / Exit Replay) must be released
     back to the budget; verify the destructor path.

### F16 — Process can exit without `SignalForge exiting` log line

- **Severity**: Observability gap (not directly a functional bug; masks others)
- **Symptom**: One observed exit (after Phase E in run 5) had no
  `SignalForge exiting, rc=0` line, no stderr output, and no crashpad
  minidump. Subsequent clean-exit (close via window-X) reproduced the
  log line correctly. Likely correlated with F15-induced abnormal state
  (OOM or unhandled signal during repeated buffer rejection).
- **Spec impact**: V1 has no usable crash diagnostics. The crash
  reporting subsystem prints
  `crash_reporting: backendHandlerPath is empty; crash reporting
  disabled` on every startup — disabled by design in V1, per
  `crash_reporting.cpp` "zero-config" comment, but the deferred
  crashpad backend never gets discovered → no minidumps written
  anywhere. Combined with F16, abnormal exits become un-diagnosable.
- **Recommended fix**: Either flip crash-reporting to a default-on
  backend path in the .deb (postinst can set
  `cfg.backendHandlerPath = /opt/signalforge/bin/crashpad_handler`),
  or remove the misleading "initialized" log line if it's intentionally
  disabled.

### F17 — Connection persistence is completely broken

- **Severity**: **Critical** (M9 frozen surface promise broken)
- **Symptom**: `~/.config/signalforge/connections.yaml` is **never
  written**, regardless of exit path:
  - Clean exit via window-X: log shows `SignalForge exiting, rc=0` —
    yaml absent.
  - Abnormal exit (F16): yaml absent.
  - Multiple connections defined and edited during the session: yaml
    absent.
- **Verification**: After the persist-test session (UDP added + clean
  exit confirmed in log at 00:14:19), `ls ~/.config/signalforge/`
  returned an empty directory.
- **Spec impact**: M9 protocol §Test 5 ("Verify
  `~/.config/signalforge/connections.yaml` reflects the change") is
  unreachable. §Test 6 (Auto-connect on app start) is unreachable
  because nothing is loaded on startup. M14 audit "F.3 Connections
  auto-load" and "F.4 Auto-connect" both fail.
- **Likely cause**: `MainWindow` constructor calls
  `connectionManager_->loadConfigFile(cfgPath)` but no symmetric
  `saveConfigFile` is invoked anywhere — neither on `addConnection /
  editConnection / removeConnection`, nor on application close. The
  ConnectionManager has YAML serialization code at
  `connection_manager.cpp:429-445` (we saw it during F1 diagnosis) but
  there's no caller.
- **Recommended fix**: Hook `saveConfigFile` to either (a) every
  mutation (`addConnection / editConnection / removeConnection /
  setAutoConnect / setSchema`), or (b) `QApplication::aboutToQuit`,
  or both. Verify with a smoke test that adds a connection, exits, and
  asserts the yaml exists with the expected entry.

### F18 — File menu has no Quit; Ctrl+Q is not bound

- **Severity**: Serious (M13 protocol §Test 9 flow can't be initiated)
- **Symptom**: The `File` menu contains only `Open Session...`. There
  is no `Quit` (or `Exit`) item. `Ctrl+Q` does nothing. The only way to
  close the GUI is the window-X button, which works but is the only
  path.
- **Spec impact**: M10 protocol §Test 9 ("Quit-while-recording prompt")
  prescribes "X button or Cmd+W". Cmd+W (Ctrl+W on Linux) is
  presumably also unbound. The X-button path is the only verification
  vector.
- **Note about F17 interaction**: F18 itself does not cause F17 — even
  the X-button path produces a clean `SignalForge exiting, rc=0` and
  `aboutToQuit` should fire normally. Persistence breakage is
  independent. But adding F18's missing menu item without also fixing
  F17 still leaves a broken UX.
- **Recommended fix**: Add `File → Quit` action with `QKeySequence::Quit`
  shortcut, which on X11 maps to `Ctrl+Q`.

## Updated M13 18-test acceptance projection

Combining run-1..4 and this run's findings:

| Test | Status | Blocker |
|---|---|---|
| T1 Serial | ✗ | F4 chart |
| T2 TCP | ✗ | F4 chart |
| T3 UDP | ✗ | F4 chart |
| T4 Replay | ✗ | F4 chart + F11 perf |
| T5 Edit/Remove | ✓ | works (with F5 caveat) |
| T6 Auto-connect | ✗ | F17 persistence |
| T7 Recording GUI round-trip | ✗ | F6 silent drop unless workaround |
| T8 Across-restart replay | ✗ | F17 |
| T9 Quit-while-recording prompt | ? | F18 path unverifiable / F8 dialog absent |
| T10 Mid-stream catalog | ✓ | passes coincidentally because F6 workaround = mid-stream |
| T11 Backpressure (optional) | ✗ | F15 buffer exhaustion |
| T12 Disk-full (optional) | ? | not exercised; needs sudo + tmpfs |
| T13 Replay GUI open | ✗ | F4 + F11 |
| T14 Play/Pause | ✗ | F4 + F12 (no relative time) |
| T15 Step ◀ / ▶ | ✗ | F4 |
| T16 Timeline scrubber | ✗ | F4 |
| T17 Speed combo | ✗ | F4 |
| T18 Live↔Replay dialogs (optional) | △ | dialogs work (D step), but F14 inconsistency + F17 persistence break full path |

**Best-case PASS = T5 + T10 = 2 / 18**, *even if F4 is fixed*.

To reach the 16/18 acceptance bar, the next M14 fix wave needs all of:

1. **F4** (chart geometry/render) — already in progress
2. **F6** (SessionWriter subscriber-order)
3. **F11 + F15** (perf + buffer budget — likely same fix area)
4. **F17** (connection persistence)
5. **F12** (relative-time replay display)
6. **F18** (Quit menu / shortcut)

Plus secondary cleanups: F7 (status-bar bytes), F8/F9 (recording
metadata), F14 (Replay state guard), F5 (right-click menu).

## CI / observability gaps surfaced by this run

- The M14 S1 release-binary smoke test is the right shape but only
  covers chart-host load. It needs extension to:
  - Add a connection, exit cleanly, then assert `connections.yaml`
    exists (catches F17).
  - Connect a UDP source, start recording, drive a packet, stop, and
    assert the resulting `.sfreplay` has at least one Type 1 record
    (catches F6).
  - Sample the runtime log for `signal_buffer registration rejected`
    spam and fail above some threshold (catches F15).

- Crash reporting is permanently disabled in V1 by configuration
  (`backendHandlerPath` empty) — F16 visibility gap. Decision needed:
  flip on, or accept and update the install docs / status messages so
  operators know any abnormal exit will be opaque.

## Captured artefacts

- This file
- Companion HALT reports: `run1-..` through `run4-..` in this directory
- Runtime log: `~/.local/state/signalforge/logs/signalforge.log` —
  search after marker `>>> M13 retest run5 start 2026-05-09T23:17:06+08:00`
- Recording fixture: `/tmp/m14-rec.sfreplay` (24 423 B, 901 records,
  produced via the F6 workaround)
- Stderr capture: `/tmp/m13-verify-logs/run5/session*.log` (empty —
  signalforge prints nothing to stderr)
- Test source feeders (left in `/tmp/`):
  - `m13-udp-feeder.py` (modbus_style frames at 5 Hz)
  - `m13-tcp-server.py` (TCP variant)
  - `m13-serial-feeder.py` (PTY writer)

## Operator action taken

- Run 5 audit halted at end of Phase F. Phases A–F covered as far as
  they could be exercised; remaining un-exercised paths
  (T11 backpressure, T12 disk-full) deferred to a future run after
  F15 + F17 land.
- All operator-reported deviations and the log-derived findings
  documented above as F5–F18.
- Test sources stopped, GUI exited cleanly, environment idle.

---

**Reviewer**: this is the consolidated non-chart audit input to the
M14 GUI Integration Audit Report (`docs/m14-gui-audit-report.md`).
Severity calls and proposed fixes here are operator-side
recommendations; the milestone owner makes the final ship-or-patch
call.
