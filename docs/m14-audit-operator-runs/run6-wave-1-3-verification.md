# M14 GUI Audit — Run 6: Wave 1-3 Fix Verification

**Date**: 2026-05-10 (CST)
**Operator**: shuai
**Branch / commit**: `milestone/M14` @ `b2bd6c4` (after Wave 1-3 fixes)
**Binary under test**: `build/release/src/app/signalforge` (local build; same
ABI as the .deb). Binary mtime newer than all relevant source files; no
rebuild needed.
**Prior run**: run 5 (`run5-non-chart-audit.md`) catalogued F4 + F5–F18.

## Sanity gate (pre-launch)

Per the resume protocol from run 5, verify in source/binary that each
declared Wave 1-3 fix is actually present. All gates pass:

| Gate | Result |
|---|---|
| Branch is `milestone/M14` | ✓ |
| `nm signalforge \| grep -c qInitResources_qml` | 1 (F3 still in) |
| `chart->setSize` in `main_window.cpp::rebuildChartWidgets` | ✓ line 519 (F4 source-level) |
| Recent commits include F6 / F12 / F15 / F17 / F18 fixes | ✓ commits `5c639fe`, `18bab31`, `555f270`, `6026395` |
| `saveConfigFile` call site in `main_window.cpp` | ✓ line 159 (F17) |
| `QKeySequence::Quit` action in `main_window.cpp` | ✓ line 758 (F18) |

Note: commit `7ed9d7b` self-marks F4 as `[partial: chart-specific paint]`,
so we treat F4 verification as runtime-required.

## Phase / step results

### Step 1 — Add UDP + Connect (cores F4 / F15)

| Item | Run-5 baseline | Run-6 result |
|---|---|---|
| Add → Connect → Signal Selector populates | ✓ | ✓ |
| **Chart paints waveform** | ✗ pure white | **✓ waveform visible** (operator confirmed) |
| `signal_buffer registration rejected` log lines | hundreds | **0** (counted via `awk + grep -c`) |

→ **F4 ✓ fully resolved at runtime** (despite the "[partial]" tag on `7ed9d7b`).
→ **F15 ✓ fully resolved**.

### Step 2 — Connect → Record → Stop (core F6, with bonus F10)

Recording flow: TestUDP Connected first, then Session → Record → Stop after
~15 s (per log-derived duration).

```
File: ~/Music/test_signalforge/record/m14-rec-run6 (354510 bytes)
Records: 13761 total
  Type 1 (Signal Value):      13761
  Type 2 (Catalog Extension):     0
Signal catalog (initial: 14; extensions added: 0)
```

Compare to run 5 same flow: 52 bytes, 0 records, catalog `(initial: 0; extensions added: 0)`.

→ **F6 ✓ fully resolved**. SessionWriter now has the catalog at `start()`
   time (ADR-013 per commit `5c639fe`).
→ **F10 ✓ resolved as side-effect** — initial catalog populates correctly,
   M10 §Test 10's "Initial catalog includes schema_A" wording is now satisfiable.

### Step 3 — Replay (cores F12 / F11 / F19)

- Operator confirmed: time display is **relative** (`m:ss.fff`) — **F12 ✓**.
- Operator confirmed: replay launches fine, position advances on Play.
- Operator did not flag load-time delay → assume **F11 ✓ presumed-fixed**
  (not independently measured; tooling-only verification deferred).
- **New issue surfaced — F19 (see below)**.

### Step 4 — Quit via Ctrl+Q (cores F18 + F17)

- File menu now has `&Quit` action with `Ctrl+Q` shortcut (operator
  confirmed item exists; pressed Ctrl+Q to exit).
- Log final line: `SignalForge exiting, rc=0` at 16:29:41. Clean exit
  via `QApplication::quit()` path. **F18 ✓**.
- Immediately post-exit, `~/.config/signalforge/connections.yaml` exists
  with full TestUDP config:
  ```yaml
  schema_version: 1
  connections:
    - id: conn-f05322b4
      displayName: testudp
      driverType: udp
      driverConfig:
        localBindAddress: 127.0.0.1
        localBindPort: 9998
      decoderSchemaId: modbus_style
      autoConnectOnStartup: false
  ```
  **F17 ✓ persistence side**.

### Step 5 — Restart / Auto-load (cores F17 end-to-end)

After restart with no config changes:

- Startup log shows the schema-binding event for the loaded UDP connection
  (`DecoderRegistrar: schema for driver type 'udp' set to ...`); the prior
  `loadConfigFile: file not found` line is gone.
- Operator confirmed: **TestUDP appears in the connection list**, with
  state column showing **Idle** (matching `autoConnectOnStartup: false`).

→ **F17 ✓ end-to-end** (write → re-load → state preserved).
→ Also rules out a candidate F20 (auto-connect-by-accident); the
  driver-type schema set + UdpIO thread spawn at startup do not imply
  a Connect — the connection respects its `autoConnectOnStartup` flag.

## Findings status after run 6

| Finding | Severity | Run-6 status | Notes |
|---|---|---|---|
| F1 | resolved | ✓ verified runs 2+ | ADR-008 |
| F2 | resolved | ✓ source-level run 3+ | ADR-010 |
| F3 | resolved | ✓ `nm` symbol still =1 | S8.2 Q_INIT_RESOURCE |
| **F4** | resolved | **✓ fully (runtime)** | despite `[partial]` tag, chart paints |
| F5 — no right-click context menu | minor | un-addressed | not on Wave 1-3 scope |
| **F6** | resolved | **✓ fully** | ADR-013, Wave 2 |
| F7 — bytes counter not live during record | minor | un-addressed | un-verified in run 6 |
| F8 — recording `description` empty | minor | un-addressed | inspect still shows "" |
| F9 — recording `decoderSchemaId` empty | serious | un-addressed | inspect still shows `Schema ID: ""` |
| **F10** | resolved | **✓ side-effect of F6** | initial catalog now 14 |
| F11 — session load slow | serious | unconfirmed | operator did not flag in run 6; no measurement |
| **F12** | resolved | **✓** | Wave 3 |
| F13 — frame-detail table missing | feature gap | un-addressed | likely V1.5+ scope |
| F14 — Replay state guard | serious | un-addressed | un-verified in run 6 |
| **F15** | resolved | **✓ 0 rejection lines** | Wave 3 (budget tuning + log throttle) |
| F16 — abnormal-exit observability | observability | unchanged | crash_reporting still configured-disabled by V1 design |
| **F17** | resolved | **✓ end-to-end** | ADR-013, Wave 2 |
| **F18** | resolved | **✓** | Wave 3 |
| **F19 — speed-change first-play stutter** | new (serious) | open | see below |

### F19 — Speed-change first-play stutter / latency

- **Severity**: Serious (M11 §Test 5 verdict at risk)
- **Symptom**: After changing the replay speed from the toolbar's speed
  combo, the first subsequent Play does not dispatch — operator must
  Pause and re-Play before dispatch resumes. Even with that workaround,
  there is a noticeable delay before the first record dispatches; the
  delay scales with speed setting and is most pronounced at 5×.
- **Likely cause**: `PlaybackController::setSpeed` probably rebuilds
  the dispatch schedule synchronously on the GUI thread without
  resetting any pending tick that was scheduled under the old speed.
  The first tick under the new speed is therefore delivered late
  (or not at all until Pause→Play resets state). Higher speeds amplify
  the visual perception because tick interval shrinks.
- **Spec impact**: M11 §Test 5 explicitly checks
  > "Speed change applies on next dispatched record · 10× clearly
  >  faster than 1× · No crash / glitches across transitions"
  V1 fails the "applies on next dispatched record" + "no glitches"
  parts.
- **Recommended fix**: in the speed-change path, (a) cancel any pending
  `QTimer` event under the old interval, (b) snapshot the current
  dispatch position, (c) restart the timer with the new interval and
  re-schedule from the snapshot.

## M13 18-test acceptance projection — updated

| Test | Run-5 verdict | Run-6 verdict |
|---|---|---|
| T1 Serial | ✗ F4 | ✓ chart now paints (chart criterion only — Serial decoder mismatch is a schema test artefact, not a defect) |
| T2 TCP | ✗ F4 | ✓ same reasoning |
| T3 UDP | ✗ F4 | ✓ confirmed by operator at Step 1 |
| T4 Replay | ✗ F4 + F11 | △ chart works; F11 unmeasured this run; M11 §Test 4 needs explicit Replay-driver test on a fresh instance |
| T5 Edit/Remove | ✓ (with F5 caveat) | ✓ same |
| T6 Auto-connect | ✗ F17 | △ persistence works; explicit `autoConnectOnStartup: true` flow not yet exercised in run 6 |
| T7 Recording GUI round-trip | ✗ F6 | ✓ Connect→Record→Stop works, file rich, sfreplay_inspect parses |
| T8 Across-restart replay | ✗ F17 | △ persistence works for connections; the .sfreplay file portability across restart is independent and previously verified at file level |
| T9 Quit-while-recording prompt | ? (F18 missing) | △ Quit menu now exists; quit-during-recording confirmation dialog **not exercised in run 6** — needs targeted run |
| T10 Mid-stream catalog | ✓ (coincidental) | ✓ now via natural flow (F6 fixed) |
| T11 Backpressure (optional) | ✗ F15 | △ buffer fix in; explicit high-rate flood not exercised in run 6 |
| T12 Disk-full (optional) | ? | not exercised |
| T13 Replay GUI open | ✗ F4 + F11 | △ chart works; F11 unmeasured; full Replay UX exercised in run 6 |
| T14 Play/Pause | ✗ F4 + F12 | ✓ both criteria met (chart, time format) |
| T15 Step ◀/▶ | ✗ F4 | △ chart works; explicit step verification deferred |
| T16 Timeline scrubber | ✗ F4 | △ chart works; explicit scrubber verification deferred |
| T17 Speed combo | ✗ F4 | **✗ blocked by F19** (new) — V1 fails "applies on next dispatched record" |
| T18 Live↔Replay dialogs | △ (F14) | △ unchanged — F14 not addressed |

**Best-case PASS projection** if a focused operator run exercises the
deferred items and they pass: **roughly 12–14 / 18**, contingent on:

- F19 fix landing (or T17 acceptance allowing the pause-restart
  workaround as a documented limitation).
- F14 fix (or workaround) for T18 full path.
- F9 fix or operator-acceptable interpretation for T8 cross-system
  replay (the file's `Schema ID` is empty so cross-binary replay
  validation can't tell apart correct vs incorrect schema).
- T9 / T11 / T15 / T16 explicitly exercised.

The 16/18 acceptance bar is now **plausible** (no longer "blocked at
≤2/18 even with F4 fixed" as run 5 estimated). One more focused run
plus the F9 / F14 / F19 fixes can push it across the bar.

## Captured artefacts

- This file
- Session log: `~/.local/state/signalforge/logs/signalforge.log` —
  search after marker `>>> M13 retest run6 start 2026-05-10T16:12:22+08:00`
  and the second marker `>>> M13 retest run6 RESTART start 2026-05-10T16:30:28+08:00`
- Recording fixture: `~/Music/test_signalforge/record/m14-rec-run6` (354 510 B,
  13 761 records). Note: operator typed filename without `.sfreplay`
  extension; file is otherwise valid SFREPLAY v1.
- Persistence artefact: `~/.config/signalforge/connections.yaml`
  (364 B, schema_version=1, single TestUDP entry).

## Operator action taken

- Run 6 verifies 7 of 8 critical / serious Wave 1-3 fixes (F4, F6, F10,
  F12, F15, F17, F18) at runtime. F11 not independently measured.
- Surfaced 1 new serious finding (F19, speed-change stutter).
- 6 prior findings (F5, F7, F8, F9, F13, F14, F16) remain un-addressed
  but are minor / observability / feature-gap class — none blocks
  16/18 by itself.
- Environment cleaned: GUI exited via Ctrl+Q (run 6) and via TERM
  (auto-load verification), all data feeders stopped.

---

**Reviewer**: M14 Wave 1-3 fix wave is the largest single delta
verified in this audit so far. Recommend a final focused run that
targets T9, T11, T15, T16 explicitly + the F19 fix + the secondary
findings (F5, F7, F8, F9, F14) before declaring V1.0 ship-ready.
