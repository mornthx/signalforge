# M14 GUI Integration Audit Report

| Field | Value |
|---|---|
| Milestone | M14 (V1.0 GUI integration audit) |
| Status | **operator pass complete (run 5); 14 findings F5–F18** |
| Authoritative spec | `docs/milestones/M14-gui-audit.md` §3.2, §4.2 |
| Operator audit input | `docs/m14-audit-operator-runs/run5-non-chart-audit.md` (415 lines) |
| Companion HALTs | run1-…/run2-…/run3-…/run4-… in same directory (the F1–F4 history) |
| Branch / commit at audit | `milestone/M14` @ `4038191` (S2 chart-geometry fix) |
| Related ADRs | ADR-008, ADR-009, ADR-010, ADR-011 |
| Smoke test | `tests/integration/gui/release_binary_smoke.sh` (M14 S1) |

This report is the audit deliverable per spec §2.1 #2. Operator
ran the V1 GUI through every M13 18-test path that does not
bottleneck on the F4 chart-render bug, surfacing **14 new
findings F5–F18**. Combined with the F1–F4 history (M13 runs
1–4 + M14 S2 chart-sizing fix), the audit results inform the
S5 V1.0 scope re-evaluation.

---

## Scenario decision (S5 preview)

**Scenario A — V1.0 full ship after Wave 1+2+3 fixes.**

Rationale:
- V1.0 GUI core is architecturally sound. Operator audit
  Phases A–E pass cleanly modulo individual wire-up gaps:
  Connection lifecycle ✓, Recording IO ✓ (with F6 workaround),
  Replay file open + mode switch ✓, Multi-chart UI ✓.
- The 4 Critical findings (F4 / F6 / F15 / F17) are wire-up +
  resource-budget gaps, not architectural rewrites. All have
  clear fix paths.
- Frozen-surface impact preliminarily 0–2 modifications, well
  under HALT #5 (>2).
- Critical count = 4, well under HALT #3 (>10).
- HALT #1 not triggered: no Critical bug is "not fixable in M14".

S5 will ratify the decision once Waves 1–3 complete and the
S6 18-test re-run reaches ≥ 16/18.

---

## Pre-audit findings (carried over from S1+S2 development)

| ID | Symptom | Origin | Status | Severity | Disposition |
|---|---|---|---|---|---|
| F1 | Chart QQuickItem sized 0×0 (run-4) | run4 dogfood | **RESOLVED** | n/a | ADR-011 / S2 commit `4038191` |
| F2 | Segfault during shutdown after `--exit-after-dump` | S1 smoke harness | open | Minor | Smoke-only / V1.0.1 |
| F3 | `UdpDriver destroyed in non-Idle state` warning | S1 smoke harness | open | Minor | Smoke-only / V1.0.1 |
| F4 | Chart paints no visible content (smoke under xvfb) | S2 smoke + run5 audit | **RESOLVED** (Wave 1, Path α) | n/a | ADR-011 (S2 chart sizing) + smoke harness sets `SF_F4_DIAG=1` so the orange `QSGSimpleRectNode` rasterizes under software-RHI (production users do not set this var; behavior unchanged). Operator real-X11 dogfood confirmed production chart line painting works |

---

## Operator-pass findings (run 5)

| ID | Symptom | Severity | Disposition |
|---|---|---|---|
| **F4** | Chart paints no visible content despite size=661×720 | **Critical** | **RESOLVED** (Wave 1, Path α): operator real-X11 confirmed production chart paints; smoke under xvfb-run+software-RHI doesn't reliably rasterize 1-px line strips, so harness sets `SF_F4_DIAG=1` to use the env-gated orange `QSGSimpleRectNode` as Tier A canary |
| F5 | Connection list lacks right-click context menu (double-click works) | Minor | V1.0.1 |
| **F6** | Recording silently drops all signals when source was Connected pre-Record | **Critical** | Wave 2 (with F17) |
| F7 | Status-bar byte counter does not update during recording | Minor | V1.0.1 |
| F8 | Recording metadata `description` always empty | Minor | V1.0.1 |
| F9 | Recording metadata `decoderSchemaId` always empty | Serious | V1.0.1 (operator promoted to Critical-adjacent; M14 milestone-owner kept as Minor for ship; downstream replay-validation deferred to V1.0.1) |
| F10 | Initial signal catalog always empty (auto-resolves with F6) | Minor | resolved by F6 fix |
| F11 | Replay session-load 10× UI bound | Serious | Wave 3 (likely shares root cause with F15) |
| F12 | Replay time display uses wall-clock instead of relative-from-zero | Minor | Wave 3 |
| F13 | No per-frame inspection table | Feature gap | V1.5+ |
| F14 | No state guard on `File → Open Session` while in Replay mode | Serious | V1.0.1 |
| **F15** | `signal_buffer` budget exhaustion + 60 Hz log spam | **Critical** | Wave 3 |
| F16 | Process can exit without `SignalForge exiting` log line | Observability gap | V1.0.1 |
| **F17** | `connections.yaml` is never written; persistence completely broken | **Critical** | Wave 2 (with F6) |
| F18 | File menu has no Quit; Ctrl+Q is not bound | Serious | Wave 3 |

**Critical: 4 (F4, F6, F15, F17)** — well under M14 HALT #3 (>10).

---

## Severity tally (post operator pass)

| Severity | Count | Findings |
|---|---:|---|
| ✓ Resolved | 2 | F1, F4 |
| **Critical** | 3 | F6, F15, F17 |
| Serious | 3 | F11, F14, F18 |
| Minor | 7 | F5, F7, F8, F9, F12, F13, F16 |
| **Total open** | 13 | (F2/F3 deferred, F10 auto-resolves, F13 V1.5+) |

---

## Path matrix — operator pass results

### Phase A — Connection lifecycle (all ✓)

| # | Path | Status |
|---|---|---|
| LM-1..LM-4 | Add Serial / TCP / UDP, Edit, Remove, Connect/Disconnect | ✓ |

### Phase B — Recording

| # | Path | Status | Finding |
|---|---|---|---|
| RC-1 | Start / Stop UI controls work | ✓ | |
| RC-2 | File written, structurally valid SFREPLAY v1 | ✓ | |
| RC-3 | TeeSink delivery to writer (Connect → Record path) | ✗ | **F6** Critical |
| RC-4 | Status-bar bytes counter updates during recording | ✗ | F7 Minor |
| RC-5 | sfreplay_inspect parses + key metadata populated | △ | F8 / F9 Minor |

### Phase C — Replay

| # | Path | Status | Finding |
|---|---|---|---|
| RP-1 | File-open dialog (`*.sfreplay` filter) | ✓ | |
| RP-2 | Mode switch into Replay | ✓ | |
| RP-3 | Replay toolbar appears (Play/Pause/Step/Speed/Seek) | ✓ | |
| RP-4 | Buttons clickable + position advances | ✓ | (chart still blank per F4) |
| RP-5 | Session load completes within UX bound (< 1 s for 24 kB file) | ✗ | **F11** Serious |
| RP-6 | Time display uses relative-from-zero format | ✗ | F12 Minor |

### Phase D — Mode transitions (all ✓)

| # | Path | Status |
|---|---|---|
| MT-1 | Live → Replay confirmation dialog | ✓ |
| MT-2 | Connection auto-disconnect on Replay enter | ✓ |
| MT-3 | Replay → Live (Exit Replay) with 3-option dialog | ✓ |
| MT-4 | Auto-reconnect / Stay-Idle decision | ✓ |

### Phase E — UI elements

| # | Path | Status | Finding |
|---|---|---|---|
| UI-1 | Add Chart button (multi-chart 5+ instances) | ✓ | |
| UI-2 | Multi-chart vertical stack layout | ✓ | |
| UI-3 | Signal Selector populates with 5+ signals + bit-fields | ✓ | |
| UI-4 | Signal toggle (checkbox) | ✓ | |
| UI-5 | Connection list right-click context menu | ✗ | F5 Minor |
| UI-6 | File → Quit menu / Ctrl+Q binding | ✗ | **F18** Serious |
| UI-7 | File → Open Session disabled while in Replay | ✗ | F14 Serious |

### Phase F — Persistence (all blocked by F17)

| # | Path | Status | Finding |
|---|---|---|---|
| PE-1 | Quit app → connections.yaml saved | ✗ | **F17** Critical |
| PE-2 | Restart → connections auto-loaded | ✗ | blocked by F17 |
| PE-3 | `autoConnectOnStartup` honored | ✗ | blocked by F17 |

---

## Wave fix sequencing

Per M14.3 P (one bug = one commit) plus per-bug operator dogfood
(per M14-concerns C2 daily ping-pong), fixes batch into 3
waves so operator validates each set together:

### Wave 1 — F4 (chart QSGNode/scene-graph diagnosis)

- Architectural / root cause unknown after S2 chart-geometry fix
- Per-fix operator dogfood (do NOT batch with others)
- May need ADR-012 if frozen-`.hpp` modification required
- This wave: diagnostic instrumentation first, then fix
- Verify by smoke Tier A + operator real-X11 chart paints

### Wave 2 — F6 + F17 (wire-up batch)

- F6: SessionWriter subscriber order
- F17: ConnectionManager.saveConfigFile never called
- Both are wire-up gaps in `main_window.cpp` /
  `connection_manager.cpp`; same governance pattern as
  ADR-009 / ADR-010
- Single ADR-013 documenting both wire-up gaps + V1
  governance lesson
- One operator dogfood validates the batch

### Wave 3 — F11 + F15 + F12 + F18 (buffer/perf/UX batch)

- F15 has 4 sub-fixes per audit recommendation:
  1. Tune per-signal buffer size (12.5 MiB / signal → smaller, derived from time-window × LOD)
  2. Throttle rejection log (once per `(driverId, signalId)` per minute)
  3. Surface user-visible error
  4. Verify destructor releases budget back
- F11 may share root cause with F15
- F12 (relative time formatter) and F18 (Quit menu / Ctrl+Q) are independent UX
- ADR-014 if F15 fix is architectural; F12/F18 likely no ADR
- One operator dogfood validates this batch

### Deferred to V1.0.1 / V1.5+

V1.0.1 patch candidates (not blocking ship):
- F5 (right-click context menu)
- F7 (status-bar bytes live update)
- F8 / F9 (recording metadata description / decoderSchemaId)
- F14 (Replay-mode Open Session guard)
- F16 (crash reporting backend path / disabled-message clarity)

V1.5+ feature:
- F13 (per-frame inspection table)

---

## Updated M13 18-test acceptance projection (operator-derived)

Combining run-1..4 + run-5:

| Test | Current | After Wave 1 (F4) | After Wave 2 (F6+F17) | After Wave 3 (F11+F15+F12+F18) |
|---|---|---|---|---|
| T1 Serial | ✗ F4 | ✓ | ✓ | ✓ |
| T2 TCP | ✗ F4 | ✓ | ✓ | ✓ |
| T3 UDP | ✗ F4 | ✓ | ✓ | ✓ |
| T4 Replay | ✗ F4+F11 | ✗ F11 | ✗ F11 | ✓ |
| T5 Edit/Remove | ✓ | ✓ | ✓ | ✓ |
| T6 Auto-connect | ✗ F17 | ✗ F17 | ✓ | ✓ |
| T7 Recording GUI | ✗ F6 | ✗ F6 | ✓ | ✓ |
| T8 Across-restart | ✗ F17 | ✗ F17 | ✓ | ✓ |
| T9 Quit-while-recording | ? F18 | ? F18 | ? F18 | ✓ |
| T10 Mid-stream catalog | ✓ (workaround) | ✓ | ✓ | ✓ |
| T11 Backpressure (opt) | ✗ F15 | ✗ F15 | ✗ F15 | ✓ |
| T12 Disk-full (opt) | ? | ? | ? | ? (operator) |
| T13 Replay GUI open | ✗ F4+F11 | ✗ F11 | ✗ F11 | ✓ |
| T14 Play/Pause | ✗ F4+F12 | ✗ F12 | ✗ F12 | ✓ |
| T15 Step ◀/▶ | ✗ F4 | ✓ | ✓ | ✓ |
| T16 Timeline scrubber | ✗ F4 | ✓ | ✓ | ✓ |
| T17 Speed combo | ✗ F4 | ✓ | ✓ | ✓ |
| T18 Live↔Replay (opt) | △ F14+F17 | △ F14+F17 | △ F14 | △ F14 (V1.0.1) |
| **Best-case PASS** | **2/18** | **8/18** | **11/18** | **16/18** |

Reaching 16/18 acceptance bar requires all three waves. T18 stays △ post-M14 because F14 is V1.0.1.

---

## Smoke-test extensions surfaced by audit

The M14 S1 smoke covers chart-host + chart-pixel + log-grep. Audit suggests three additions (deferred until each underlying fix lands so smoke can verify regression):

- **Persistence smoke** (after Wave 2 / F17): add a connection, exit cleanly, assert `connections.yaml` exists with the expected entry.
- **Recording smoke** (after Wave 2 / F6): connect a UDP source, start recording, drive a packet, stop, assert resulting `.sfreplay` has ≥ 1 Type-1 record.
- **Buffer-pressure smoke** (after Wave 3 / F15): sample log for `signal_buffer registration rejected` rate; fail above threshold (e.g., > 5 occurrences in test window).

Each extension is an additional ctest test under `tests/integration/gui/` per M14-concerns C4. None added in this audit-report commit; they land in their respective wave commits.

---

## Cross-references

- Spec: `docs/milestones/M14-gui-audit.md`
- Plan: `.claude/M14-plan.md`
- Concerns: `.claude/M14-concerns.md`
- Progress: `.claude/M14-progress.md` (live state + counters)
- Operator pass: `docs/m14-audit-operator-runs/run5-non-chart-audit.md`
- Smoke harness: `tests/integration/gui/release_binary_smoke.sh`
- Run-1→run-4 history: `docs/architecture/decisions/ADR-010` §"Implementation lesson"
