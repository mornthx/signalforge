# M11 — Manual hardware verification protocol

This document specifies the **6 manual tests** the operator runs
against M11 Replay UX before declaring V1.0 ready. The
automated test suite (585 ctest cases at M11 close) covers
correctness; this protocol verifies the user-facing UX.

| ID | Test | Pass criterion | Optional |
|---|---|---|---|
| 1 | GUI open + replay | Open a `.sfreplay` file, charts populate as records dispatch | no |
| 2 | Play / Pause toggle | Click Play; charts update; click Pause; charts freeze | no |
| 3 | Step ◀/▶ | Step forward dispatches one record; step backward replays | no |
| 4 | Timeline scrubber | Drag scrubber; charts re-render at the new position | no |
| 5 | Speed combo | Switch among 0.5× / 1× / 2× / 5× / 10×; visible speed-up | no |
| 6 | Live ↔ Replay confirmation dialogs | Connect a driver, then Open Session… → dialog asks to pause; on Exit Replay → dialog offers resume / stay disconnected / cancel | yes |

Run on the same hardware target as M10's hardware verification
(M10 spec §8.7).

## Detailed protocol

### Test 1 — GUI open + replay

**Setup**: a complete `.sfreplay` file from prior recording (M10
Test 1 fixture, or a fresh one).

**Steps**:
1. Launch SignalForge.
2. **File → Open Session…** (Ctrl+O).
3. Select the `.sfreplay` file.
4. Click Play in the replay toolbar.

**Pass criterion**:
- The replay toolbar appears.
- Status bar shows `Replay: <filename> | <pos> / <duration>`.
- Charts populate as records dispatch.
- The state transitions to Ended when the file finishes.

### Test 2 — Play / Pause toggle

**Steps**:
1. Open a session as in Test 1.
2. Click Play.
3. While dispatching, click Pause.
4. Click Play again.

**Pass criterion**:
- Pause halts dispatch immediately.
- Status bar's record count freezes during pause.
- Resume continues from the same position.
- Total dispatched count after Ended equals the file's
  `totalRecords`.

### Test 3 — Step ◀/▶

**Steps**:
1. Open a session.
2. Click `Step ▶` (forward) several times.
3. Click `◀ Step` (backward) several times.

**Pass criterion**:
- Each `Step ▶` dispatches exactly one record (record index
  increments by 1; charts show one new sample per signal).
- `◀ Step` replays from the start to (current - 1) — observable
  as a transient "rewind to start, fast-forward to here"
  behaviour. This is V1's documented O(N) backward step.

### Test 4 — Timeline scrubber

**Steps**:
1. Open a session.
2. Drag the scrubber slider to ~25%, ~50%, ~75%.

**Pass criterion**:
- Each drag-drop action seeks; status bar position updates
  to the corresponding timestamp.
- Charts re-render after each seek.
- No app crash or hang for any seek position (including
  drag-to-end and drag-to-start).

### Test 5 — Speed combo

**Steps**:
1. Open a session.
2. Click Play.
3. Switch speed: 1× → 2× → 5× → 10× → 0.5× → 1×.

**Pass criterion**:
- Speed change applies on the next dispatched record.
- 10× clearly faster than 1×; 0.5× clearly slower.
- No crash or audio/visual glitches across speed transitions.

### Test 6 — Live ↔ Replay confirmation dialogs

**Setup**: at least one M9 connection currently `Connected`.

**Steps (Enter Replay path)**:
1. Connect a driver (e.g., a UDP echo via `nc`).
2. Confirm it shows Connected in the connection list.
3. **File → Open Session…** to open a `.sfreplay`.
4. Confirmation dialog appears: "Pause N active connection(s)?"
   - Click No → no transition, no disconnect. Connection stays
     Connected. (Pass for the No path.)
   - Click Yes → connection moves to Idle, replay begins. (Pass
     for the Yes path.)

**Steps (Exit Replay path)**:
1. While in Replay, click `Exit Replay` in the replay toolbar.
2. Three-option dialog: Resume / Stay Disconnected / Cancel.
   - Cancel → no transition; stays in Replay. (Pass.)
   - Stay Disconnected → returns to Live; previously-paused
     connections remain disconnected. (Pass.)
   - Resume → returns to Live; previously-paused connections
     reconnect (state goes back to Connected within ~1 s).
     (Pass.)

This test covers the full C3 / S8 / S9 mode-transition path.
Optional because not every operator fixture has a live driver
to pair with.

## Pass-rate goal

5 of 6 tests pass (Test 6 optional). Failures should be
recorded in `.claude/M11-concerns.md` for V1.5+ remediation.

## Reporting

Operator records results in M11-done.md §"Manual hardware
verification" table, mirroring the M10 protocol's reporting
shape. Test 6 outcomes for each of the three exit branches
should be recorded individually if exercised.

## Cross-reference

- M9 protocol: `docs/m9-hardware-verification.md` (6 tests)
- M10 protocol: `docs/m10-hardware-verification.md` (6 tests)
- M11 protocol: this file (6 tests)

V1.0 hardware verification = combined 18 tests across the
three protocols, executed in a single dogfooding session by
the operator.
