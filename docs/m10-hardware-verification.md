# M10 — Manual hardware verification protocol

This protocol mirrors `docs/m9-hardware-verification.md` and
exercises the M10 session-recording path end-to-end against
real (or socat-loopback) hardware.

The CC session that closed M10 cannot run these tests
directly (no hardware in CC's runtime); the operator runs
them post-merge and records pass/fail in `.claude/M10-done.md`
§Manual hardware verification.

## Prerequisites

- A working SignalForge build: `build/release/src/app/signalforge`.
- One of:
  - A real serial device or a `socat`-loopback pair
  - A TCP echo server (or `socat TCP-LISTEN:9090,reuseaddr,fork EXEC:/bin/cat`)
  - A UDP sender (or `socat - UDP-DATAGRAM:127.0.0.1:7000,bind=:7001`)
  - An existing `.sfreplay` session file (use the file from
    `tests/integration/test_session_full_stack_round_trip` or
    write one via the M10 build itself)
- The `sfreplay_inspect` CLI:
  `build/release/tools/sfreplay_inspect/sfreplay_inspect`
- Disk free space ≥ 100 MB in `~/.config/signalforge/` and
  in the `/tmp` recording directory.

## Test 1 — Record + replay round-trip via the GUI

**Goal**: confirm the M10 Session menu / Record action +
SessionWriter + on-disk file + SessionReader path works
end-to-end.

1. Launch `signalforge`.
2. **Connections** → **Add…**, configure a serial / TCP /
   UDP / replay connection that produces signals (use a
   real device, an existing `.sfreplay` fixture, or a
   socat-loopback pair).
3. Click **Connect** and confirm signals appear in the
   chart subsystem (M8).
4. **Session** → **Record…** (Ctrl+R). Choose
   `~/recordings/m10-test.sfreplay` as the path.
5. Verify the status bar reads
   `● Recording: m10-test.sfreplay (N bytes)` and the
   byte count grows over time.
6. Wait ≥ 30 s for non-trivial data accumulation.
7. **Session** → **Stop recording** (the action toggles).
   Status bar reads `Stopped (M bytes)`.
8. Run `sfreplay_inspect ~/recordings/m10-test.sfreplay`
   and verify:
   - `format_version=1`
   - signal catalog matches the live signals from step 3
   - `record_histogram.1_signal_value` matches the
     recorded duration × 1 kHz × signal count
   - `footer_present=true`, `file_complete=true`

**Pass criteria**: round-trip cleanly; no Qt errors in
log; SessionWriter `errorOccurred` not emitted.

## Test 2 — Recording file persists across app restart

**Goal**: confirm the file remains valid + readable after
a clean shutdown.

1. Repeat Test 1 steps 1-7.
2. Quit `signalforge` (File → Quit, or Ctrl+Q).
3. Re-launch `signalforge`.
4. Run `sfreplay_inspect <path>` again. Result must be
   identical to step 8 of Test 1.

**Pass criteria**: file is bit-identical; `sfreplay_inspect`
reports the same record histogram + complete footer.

## Test 3 — Quit-while-recording prompt

**Goal**: confirm `closeEvent` honors the "stop recording
first?" prompt.

1. Launch `signalforge`, start a recording (Test 1 steps
   1-5).
2. While recording is active, attempt to close the
   application (window close button or Alt+F4).
3. Verify the modal dialog "A session is currently being
   recorded. Stop recording and exit?" appears.
4. Click **Cancel** → window stays open, recording
   continues.
5. Repeat from step 1, but click **Yes** at the prompt.
6. Verify the application closes cleanly and the file is
   still valid (run `sfreplay_inspect`).

**Pass criteria**: prompt fires; Cancel preserves
recording; Yes stops + closes; file remains complete with
footer.

## Test 4 — Recording with mid-stream signal registration

**Goal**: confirm Catalog Extension records are correctly
emitted when a new signal registers mid-recording.

1. Launch `signalforge`. Connect to a device that publishes
   only some of its signals initially (e.g., a serial
   device whose decoder schema reveals signals on different
   frame types).
2. Start recording (Test 1 step 4-5).
3. Trigger the device to emit a frame that registers a
   *new* signal (not yet in the registry). Confirm the
   chart subsystem displays the new signal.
4. Stop recording.
5. Run `sfreplay_inspect <path>`. Verify:
   - `record_histogram.2_catalog_extension >= 1`
   - `catalog_extension_added` ≥ 1
   - All signals (initial + late) appear in the JSON
     `signals` array of the inspector's output (after
     replaying via `SessionReader` — the inspector itself
     only shows the initial catalog by design).

**Pass criteria**: at least one Catalog Extension record
landed; round-trip via `SessionReader` recovers all
signals (initial + late).

## Test 5 — Backpressure under heavy load

**Goal**: confirm the C3 backpressure policy under stress.
Optional / advanced — only if the operator has a
high-rate (≥ 60 kHz) hardware source.

1. Launch `signalforge`. Connect to a high-rate source
   (e.g., synthetic socat that emits frames at 100+ kHz).
2. Start recording.
3. Watch the SessionWriter's `dropped_events_total` log
   message (`SF_LOG_WARN`). Drops are expected if the
   producer rate exceeds the writer's queue capacity (10 k
   events) divided by the worker's drain rate.
4. Stop recording. Confirm the file is still parseable
   via `sfreplay_inspect` (footer present).

**Pass criteria**: drops are logged via SF_LOG_WARN with
the metric name `session_writer_dropped_events_total`;
the file remains valid (footer + catalog intact); no
crash or main-thread block.

## Test 6 — Disk-full / IO error path

**Goal**: confirm graceful degradation when the recording
target disk fills up.

1. Locate or create a small filesystem (e.g.,
   `truncate -s 32M /tmp/small.img && mkfs.ext4 /tmp/small.img &&
   mount /tmp/small.img /mnt/small`).
2. Launch `signalforge`. Connect to a moderate-rate
   source.
3. Start recording with the path inside the small fs.
4. Wait until the fs fills.
5. Verify the status bar reads `Recording error` and a
   `QMessageBox` surfaces with the disk-full message.
6. Verify `signalforge` does not crash; the partial file
   on disk is parseable up to the last flush via
   `sfreplay_inspect` (footer absent → `file_complete=false`).

**Pass criteria**: disk-full triggers
`SessionWriter::errorOccurred` (status label updates,
message box appears); writer detaches from TeeSink;
partial file is readable up to last flushed record.

## Recording the results

After running the tests, record the result in
`.claude/M10-done.md` § Manual hardware verification:

```
| Test | Result | Notes |
|---|---|---|
| 1. GUI round-trip | ✅ / ⚠️ / ❌ | <one-line note> |
| 2. Persists across restart | ✅ / ⚠️ / ❌ | |
| 3. Quit-while-recording prompt | ✅ / ⚠️ / ❌ | |
| 4. Mid-stream signal registration | ✅ / ⚠️ / ❌ | |
| 5. Backpressure under heavy load | ✅ / ⚠️ / ❌ | (optional) |
| 6. Disk-full / IO error | ✅ / ⚠️ / ❌ | |
```

Pass rate goal: **≥ 5/6** (Test 5 is optional).
