# V1.0 Combined Hardware Verification Protocol

**M13 / V1.0 release prerequisite** per spec §3.5 V + §4.5.

This document combines the three prior milestone hardware
verification protocols into **one 18-test dogfood session**
the operator runs once before V1.0 ship. It quotes each
prior protocol verbatim under section headers so the
operator has everything offline (the .deb installs this
file).

**Acceptance bar**: **16 of 18** tests pass. ≤ 2 failures
accepted for V1.0 if documented in `.claude/M13-done.md §
Manual hardware verification`. < 16 → HALT (H4 trigger
per M13 plan §3); investigate.

---

## Pre-flight

1. Install SignalForge V1.0 via `.deb` per
   `docs/install.md`. Confirm GUI launches.
2. Have ready:
   - a USB-serial adapter (or `socat` for virtual
     loopback)
   - `examples/schemas/temperature_sensor.yaml`,
     `examples/schemas/modbus_style.yaml`
   - a known-good `.sfreplay` fixture (run
     `sfreplay_inspect` on it first to confirm structure)
3. Set `SF_LOG_LEVEL=info` (or `debug` for failures) so
   the runtime log captures evidence:

```bash
export SF_LOG_LEVEL=info
export SF_LOG_DIR=/tmp/m13-verify-logs/
mkdir -p $SF_LOG_DIR
signalforge 2>&1 | tee $SF_LOG_DIR/session.log
```

4. Allow ~60 minutes for the full 18-test pass. Run in one
   sitting if possible.

---

## Result template

For each test below, record one line:

| # | Test | Result | Notes |
|---|---|---|---|
| 1 | M9-1 Serial driver | _PASS / FAIL_ | |
| 2 | M9-2 TCP driver | | |
| ... | | | |

Final: pass count `_/18`. If ≥ 16, V1.0 is GO. If < 16,
HALT and report failures to the milestone reviewer.

---

## Section M9 — Connection manager (6 tests)

Quoted from `docs/m9-hardware-verification.md`. The
prior protocol covered four driver types + edit / remove /
auto-connect / persistence. Each numbered test below maps
directly to a section in the M9 protocol.

### Test 1 — Serial driver (USB-serial or socat)

Source: `m9-hardware-verification.md §Test 1`.

**Setup**:
- Option A: real USB-serial adapter (FT232 / CP2102) +
  target board emitting frames matching
  `examples/schemas/temperature_sensor.yaml`.
- Option B: virtual loopback —
  ```bash
  socat -d -d pty,raw,echo=0 pty,raw,echo=0
  ```
  Connect SignalForge to one `/dev/pts/X`; write hand-rolled
  bytes to the other.

**Steps**:
1. Launch `signalforge`. Open Connections panel
   (View → Connections, or left dock).
2. Click "Add". Select Serial driver. Configure:
   - Device: `/dev/ttyUSB0` (or socat-printed `/dev/pts/X`)
   - Baud: 115200, 8N1
   - Schema: `temperature_sensor.yaml`
   - Display name: "TestSerial"
3. Click Connect.
4. Verify connection state transitions to **Connected**
   (state column).
5. With device producing data, observe signals appear in
   the Signal Selector (left of chart pane).
6. Drag a signal into a chart; verify samples flow.
7. Click Disconnect; state returns to **Idle**.

**Pass**: connection completes; signals decode; chart
renders.

### Test 2 — TCP driver

Source: `m9-hardware-verification.md §Test 2`.

**Setup**: a TCP server emitting frames. Quick
test-server: `python3 -m http.server` on a port ≠ device
port (use `nc -l 9999` to send canned frames).

**Steps**:
1. Connections → Add → TCP. Host
   `127.0.0.1`, port `9999`. Schema `modbus_style.yaml`.
2. Connect; verify Connected.
3. Send canned frames to the listening port. Verify
   signals appear in chart.
4. Kill the listener; verify reconnect attempt logged
   (M9 reconnect-on-failure behaviour).
5. Restart listener; verify reconnect.

**Pass**: connect + flow + reconnect all work.

### Test 3 — UDP driver

Source: `m9-hardware-verification.md §Test 3`.

**Setup**: a UDP sender. Quick:
```bash
echo -ne '\x01\x02\x03\x04\x05\x06\x07\x08' | nc -u -w0 127.0.0.1 9998
```

**Steps**:
1. Add → UDP. Bind `127.0.0.1:9998`. Schema
   `modbus_style.yaml`.
2. Connect; verify Connected.
3. Send the canned datagram (above). Verify decode →
   chart.
4. Repeat 5 times rapid-fire; verify no dropped frames
   (M4 lock-free SPSC queue handles bursts).

**Pass**: UDP receive + decode + chart all work.

### Test 4 — Replay driver

Source: `m9-hardware-verification.md §Test 4`.

**Setup**: a known-good `.sfreplay` fixture (e.g.,
exported from a prior live session via M10 record).

**Steps**:
1. Add → Replay. File: path to fixture. Schema:
   `temperature_sensor.yaml` (must match fixture's recorded
   schema).
2. Connect; verify file loads, state goes Connected.
3. Verify signals appear at recorded timing.
4. Disconnect; reconnect; verify replays from start.

**Pass**: file load + replay timing + reconnect all work.

### Test 5 — Edit / remove connection

Source: `m9-hardware-verification.md §Test 5`.

**Steps**:
1. Right-click an Idle connection → Edit. Change baud
   rate; save. Verify config persisted (close + reopen
   app; setting still there).
2. Right-click → Remove. Confirm dialog. Verify removal
   from list.
3. Verify `~/.config/signalforge/connections.yaml`
   reflects the change.

**Pass**: edit persists; remove persists.

### Test 6 — Auto-connect on app start

Source: `m9-hardware-verification.md §Test 6`.

**Steps**:
1. With at least one connection set to "auto-connect on
   start" (per-connection setting), close + reopen
   SignalForge.
2. Verify the auto-connect connection auto-transitions to
   Connected on startup.

**Pass**: auto-connect works as configured.

---

## Section M10 — Session writer (6 tests)

Quoted from `docs/m10-hardware-verification.md`. Covers
recording lifecycle.

### Test 7 — GUI round-trip

Source: `m10-hardware-verification.md §Test 1`.

**Steps**:
1. With a Connected connection emitting signals, click
   Session menu → Record... (Ctrl+R).
2. Pick a `.sfreplay` filename in QFileDialog.
3. Verify status bar shows "● Recording: <name> (N
   bytes)" with N updating periodically.
4. After ~5 seconds, click Session → Stop recording.
5. Verify status bar shows "Stopped (N bytes)".
6. Run `sfreplay_inspect <file>`; verify it parses, lists
   the signal catalog, and shows N records.

**Pass**: full round-trip records cleanly + inspect
parses.

### Test 8 — Persists across restart

Source: `m10-hardware-verification.md §Test 2`.

**Steps**:
1. Record a session (Test 7).
2. Quit SignalForge.
3. Re-launch. Open Session... the recorded file.
4. Verify replay UX appears + signals dispatch.

**Pass**: file is portable across app restarts.

### Test 9 — Quit-while-recording prompt

Source: `m10-hardware-verification.md §Test 3`.

**Steps**:
1. Start recording.
2. Try to close the window (X button or Cmd+W).
3. Verify a prompt: "Recording in progress. Stop and
   exit?" with Yes / Cancel.
4. Click Cancel: app stays open, recording continues.
5. Click Yes: recording stops cleanly + app exits.

**Pass**: prompt fires + Cancel preserves + Yes stops
gracefully.

### Test 10 — Mid-stream signal registration

Source: `m10-hardware-verification.md §Test 4`.

**Setup**: two connections with different schemas (e.g.,
TCP with schema_A + Serial with schema_B), where
schema_B's signals will register mid-recording.

**Steps**:
1. Connect TCP first (schema_A signals registered).
2. Start recording.
3. After a few seconds, connect Serial (schema_B signals
   register mid-stream).
4. Stop recording.
5. Run `sfreplay_inspect <file>`; verify:
   - Initial catalog includes schema_A signals
   - At least one Catalog Extension record present
   - Final catalog includes schema_A + schema_B

**Pass**: catalog-extension flow works on the wire.

### Test 11 — Backpressure under heavy load (optional)

Source: `m10-hardware-verification.md §Test 5`.

**Setup**: deliberately overload the writer (e.g., a UDP
flood at > 60 k events/sec sustained).

**Steps**:
1. Start recording.
2. Drive the connection at high rate.
3. Watch `signalforge` logs for "session-writer dropped
   event" warnings (C3 backpressure policy).
4. Verify the file is still readable + has a valid
   footer.

**Pass**: drops counted + file still complete (or
documented as truncated).

### Test 12 — Disk-full / IO error (optional)

Source: `m10-hardware-verification.md §Test 6`.

**Setup**: synthetic — record onto a tmpfs sized to
overflow during the run.

**Steps**:
1. `mkdir /tmp/tiny && sudo mount -t tmpfs -o size=10M tmpfs /tmp/tiny`
2. Record to `/tmp/tiny/full.sfreplay`.
3. Drive enough data to exceed 10 MB.
4. Verify the writer transitions to Error state +
   QMessageBox surfaces the error.
5. `sudo umount /tmp/tiny`.

**Pass**: graceful error transition + user-visible
message.

---

## Section M11 — Replay UX (6 tests)

Quoted from `docs/m11-hardware-verification.md`. Covers
playback controls + Live ↔ Replay transitions.

### Test 13 — GUI open + replay

Source: `m11-hardware-verification.md §Test 1`.

**Setup**: a complete `.sfreplay` file from Test 7 / 8.

**Steps**:
1. Launch SignalForge.
2. File → Open Session… (Ctrl+O).
3. Select the `.sfreplay` file.
4. Click Play in the replay toolbar.

**Pass**:
- Replay toolbar appears
- Status bar shows `Replay: <filename> | <pos> /
  <duration>`
- Charts populate as records dispatch
- State transitions to Ended when file finishes

### Test 14 — Play / Pause toggle

Source: `m11-hardware-verification.md §Test 2`.

**Steps**:
1. Open a session as in Test 13.
2. Click Play.
3. While dispatching, click Pause.
4. Click Play again.

**Pass**:
- Pause halts dispatch immediately
- Status bar's record count freezes during pause
- Resume continues from the same position
- Total dispatched count after Ended equals
  `totalRecords`

### Test 15 — Step ◀ / ▶

Source: `m11-hardware-verification.md §Test 3`.

**Steps**:
1. Open a session.
2. Click `Step ▶` (forward) several times.
3. Click `◀ Step` (backward) several times.

**Pass**:
- Each `Step ▶` dispatches one record (record index
  increments by 1)
- `◀ Step` replays from start to (current - 1) — this is
  V1's documented O(N) backward step

### Test 16 — Timeline scrubber

Source: `m11-hardware-verification.md §Test 4`.

**Steps**:
1. Open a session.
2. Drag the scrubber to ~25 % / ~50 % / ~75 %.

**Pass**:
- Each drag-drop seeks; status bar updates
- Charts re-render after each seek
- No app crash or hang

### Test 17 — Speed combo

Source: `m11-hardware-verification.md §Test 5`.

**Steps**:
1. Open a session.
2. Click Play.
3. Switch speed: 1× → 2× → 5× → 10× → 0.5× → 1×.

**Pass**:
- Speed change applies on next dispatched record
- 10× clearly faster than 1×; 0.5× clearly slower
- No crash / glitches across transitions

### Test 18 — Live ↔ Replay confirmation dialogs (optional)

Source: `m11-hardware-verification.md §Test 6`.

**Setup**: at least one Connected M9 connection.

**Steps**:
1. Connect a driver (e.g., UDP).
2. File → Open Session…
3. Confirmation dialog: "Pause N active connection(s)?"
   - Click Yes → pause + replay begins (PASS for Yes)
4. Click Exit Replay.
5. 3-option dialog: Resume / Stay Disconnected / Cancel.
   - Each branch verified: PASS

**Pass**: full C3 / S8 / S9 mode-transition path works.

---

## Recording results

After completing all 18 tests, fill in the result table at
the top of this document. Total pass count of ≥ 16 is the
V1.0 release acceptance per spec §3.5 V + §5.2.

If pass count < 16, **HALT** the V1.0 ship sequence. Report
specific failures + reproduction logs from
`/tmp/m13-verify-logs/` to the milestone reviewer.

If pass count is 16 or 17 (i.e., 1-2 failures), document
each failure in `.claude/M13-done.md §Manual hardware
verification` with:
- Test number + section
- What was expected
- What actually happened
- Logs / screenshots
- Disposition (V1.0 known limitation, V1.0.1 patch
  candidate, V1.5+ feature, etc.)

---

## Cross-reference

Source protocols (these continue to live in the source tree
unchanged for V1.0):

- M9: `docs/m9-hardware-verification.md`
- M10: `docs/m10-hardware-verification.md`
- M11: `docs/m11-hardware-verification.md`

After V1.0 ship, these prior protocols may continue to be
used for individual driver / module verification; the
combined M13 protocol exists specifically for the V1.0
release-acceptance dogfood session.
