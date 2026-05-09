# M14 GUI Integration Audit Report

| Field | Value |
|---|---|
| Milestone | M14 (V1.0 GUI integration audit) |
| Status | **draft — operator pass(es) pending** |
| Authoritative spec | `docs/milestones/M14-gui-audit.md` §3.2, §4.2 |
| Companion doc | `docs/v1.0-spec-list.md` §1 (frozen surface) |
| Related ADRs | ADR-008, ADR-009, ADR-010, ADR-011 (run-1→run-4 history) |
| Smoke test | `tests/integration/gui/release_binary_smoke.sh` (M14 S1) |

This report is the audit deliverable per spec §2.1 #2. The
skeleton is committed at S0/S2 close so the operator (and any
later reader) sees the full path matrix in advance. Each path
table-row gets filled in during operator dogfood passes per
spec §4.2:

- **Operator** column — does the path work in a real X11 GUI
  session against the released `signalforge` binary?
- **CI smoke** column — does the M14 S1 smoke test (or a
  smoke extension) confirm the same?
- **Status** — ✓ working / ⚠ working with caveat / ✗ broken.
- **Severity** — only set when Status is ✗ or ⚠:
  - **Critical** — blocks V1.0 ship.
  - **Serious** — major UX impact; V1.0.1 patch acceptable.
  - **Minor** — cosmetic; V1.5+ acceptable.
- **Proposed fix** — short note. If "TBD", S4 will diagnose +
  decide. If "ADR-XYZ", indicates a likely architectural
  fix that needs an ADR.

Statuses MUST be honest: Scenario decision (M14.5 X) depends
on this report. CC will not pre-commit to Scenario A; per
M14-progress §"Scenario decision discipline (C5 reminder)".

Open findings already known at S2 close are listed under
**Pre-audit findings** below — they propagate into the path
table as the operator confirms or rejects them.

---

## Pre-audit findings (carried over from S1+S2 development)

These are bugs surfaced by smoke-test build-out, before the
formal operator pass began. They feed the path table; some
may resolve during the audit if they turn out to be
xvfb/offscreen-only artifacts.

| ID | Symptom | Origin | Status | Severity | Proposed fix |
|---|---|---|---|---|---|
| F1 | Chart QQuickItem sized 0×0 (chart pane blank in operator GUI) | Run-4 dogfood / S1 smoke | **RESOLVED** in S2 | n/a | ADR-011 — landed in `4038191` |
| F2 | Segfault during shutdown after `--exit-after-dump` | S1 smoke harness | open | TBD | TBD; may be offscreen-platform Qt issue, may be SessionWriter teardown order |
| F3 | `UdpDriver destroyed in non-Idle state` warning on shutdown | S1 smoke harness | open | TBD | TBD; connection-lifecycle teardown contract |
| F4 | Chart paints no visible content even with correct geometry (size=661×720, signals=1, redraws=129, but framebuffer is all-white) | S2 smoke harness | open | **TBD — operator real-X11 confirmation pending** | TBD; may need ADR-012 if architectural |

If F4 is xvfb-only (operator says "chart works in my real GUI"),
it becomes a smoke-test-fixture issue, severity drops to
Serious-or-below, V1.0.1 candidate. If F4 reproduces in real
X11, it likely shares root cause with run-4 / ADR-011 and gets
a real architectural fix.

---

## Path matrix

### Live mode chain (spec §3.2 1/6)

The chain that decoded signals traverse from a connected
driver to a user-visible chart:

```
ConnectionDialog → save to YAML
ConnectionManager → load from YAML
Connection state machine → driver attach
PipelineManager.attach → DecoderRegistrar.pipelineAttached  ← ADR-008/009 wires
DecoderRegistrar → SchemaDecoder construction
SchemaDecoder → SignalValueSink (TeeSink)
TeeSink → SignalBufferRegistry + SessionWriter
SignalBufferRegistry → ChartManager
ChartManager → Chart QQuickItem rendering             ← ADR-011 fixes geometry
Chart redraw timer → scene-graph paint nodes          ← F4 candidate
QQuickItem → QQuickWidget host scene                  ← ADR-010 fixes hosting
QQuickWidget → user-visible pane
```

| # | Path | Expected behavior | Operator | CI smoke | Status | Severity | Proposed fix |
|---|---|---|---|---|---|---|---|
| LM-1 | ConnectionDialog → "Add" creates a Connection in `Idle` state | Dialog accept → connection appears in left panel; state widget shows "Idle" | | | | | |
| LM-2 | ConnectionDialog → save persists to YAML | Yaml file at `~/.config/signalforge/connections.yaml` updates with the new connection | | | | | |
| LM-3 | ConnectionManager loads YAML on startup | Restart app; previously-added connection appears in left panel | | | | | |
| LM-4 | Connection.connectDriver() transitions Idle→Connecting→Connected | Click "Connect"; status widget walks Connecting → Connected | | | | | |
| LM-5 | PipelineManager.attach fires on Connected (ADR-009) | Log line `pipelineAttached signal fires` for the driver id | | | | | |
| LM-6 | DecoderRegistrar.onPipelineAttached constructs SchemaDecoder (ADR-008) | Log line `DecoderRegistrar[...]: decoder attached using schema` | | | | | |
| LM-7 | SchemaDecoder produces SignalValues to TeeSink | SignalBufferRegistry.signalIds() includes the schema's fields | | | | | |
| LM-8 | TeeSink fans out to BufferRegistry + (optionally) SessionWriter | Both observers see the same value stream | | | | | |
| LM-9 | SignalBufferRegistry stores recent samples for chart consumption | `signalForId(...).recentSamples()` returns >0 entries when frames flowing | | | | | |
| LM-10 | ChartManager → Chart QQuickItem rendering | Chart `setSize(...)` applied; `redraws` counter increments at 30 Hz | | | | | |
| LM-11 | Chart redraw timer → scene-graph paint nodes | Chart's `updatePaintNode` produces non-trivial QSGNode tree | | F4 ✗ | | | |
| LM-12 | QQuickWidget shows the chart visibly to the user | User sees lines drawn on the chart pane | | F4 ✗ | | | |

### Recording chain (spec §3.2 2/6)

| # | Path | Expected behavior | Operator | CI smoke | Status | Severity | Proposed fix |
|---|---|---|---|---|---|---|---|
| RC-1 | File → Record menu → SessionWriter.start | Click Record; pick path; status bar switches to "● Recording" | | | | | |
| RC-2 | SessionWriter → SessionFileWriter worker thread | File at chosen path grows over time | | | | | |
| RC-3 | TeeSink delivery to writer | While recording, decoded signals also reach the SFREPLAY file | | | | | |
| RC-4 | Status bar bytes counter updates during recording | Counter increases at the right magnitude (~kbps × signals) | | | | | |
| RC-5 | File → Stop Record → SessionWriter.stop | Click Stop; status bar reverts to "Idle"; file finalized | | | | | |
| RC-6 | File integrity (post-stop SFREPLAY validates) | `sfreplay_inspect <file>` reports schema_version=1, header OK, frame count > 0 | | | | | |

### Replay chain (spec §3.2 3/6)

| # | Path | Expected behavior | Operator | CI smoke | Status | Severity | Proposed fix |
|---|---|---|---|---|---|---|---|
| RP-1 | File → Open Session menu → SessionReader | Open dialog → pick `.sfreplay` → app enters Replay mode | | | | | |
| RP-2 | SessionReader → SessionPlayer → PlaybackController | Replay-mode toolbar appears (Play/Pause/Step/Seek/Speed) | | | | | |
| RP-3 | PlaybackController → MainWindow replay UI mode switch | Live-mode controls disabled; replay status label shows file name | | | | | |
| RP-4 | Replay toolbar (Play/Pause/Step/Seek/Speed) all functional | Each action moves position, slider, or speed as expected | | | | | |
| RP-5 | ChartManager re-renders from replay data | Chart shows the recorded waveform when scrubbing | | | | | |
| RP-6 | Status bar updates current position | Position label shows `t / duration ns | record / total` | | | | | |

### Mode transitions (spec §3.2 4/6)

| # | Path | Expected behavior | Operator | CI smoke | Status | Severity | Proposed fix |
|---|---|---|---|---|---|---|---|
| MT-1 | Live → Replay (M11 ReplayModeManager) | Confirm dialog if any active connection; on accept, connections pause | | | | | |
| MT-2 | Connection auto-disconnect on Replay enter | Active connections move to Idle (or paused) before Replay loads | | | | | |
| MT-3 | Replay → Live (Exit Replay) | Confirm dialog asks "Resume previously-paused connections?" | | | | | |
| MT-4 | Connection auto-reconnect on Replay exit (decision dialog) | "Yes" reconnects paused connections; "No" leaves them Idle | | | | | |

### Persistence (spec §3.2 5/6)

| # | Path | Expected behavior | Operator | CI smoke | Status | Severity | Proposed fix |
|---|---|---|---|---|---|---|---|
| PE-1 | Quit app → connection list saved to YAML | Last-known connection list written to `~/.config/signalforge/connections.yaml` on close | | | | | |
| PE-2 | Restart → connections auto-loaded | Previously-saved connections reappear in left panel after relaunch | | | | | |
| PE-3 | `autoConnectOnStartup` setting honored on restart | Connections marked `autoConnectOnStartup: true` reach Connected without user click. **Note**: V1 spec says forward-compatibility only (M9.2); confirm whether the field actually triggers anything | | | | | |

### UI elements (spec §3.2 6/6)

| # | Path | Expected behavior | Operator | CI smoke | Status | Severity | Proposed fix |
|---|---|---|---|---|---|---|---|
| UI-1 | Connections menu (Add / Connect All / Disconnect All) | Each menu item triggers the corresponding action; keyboard shortcut works (Ctrl+M for Add) | | | | | |
| UI-2 | Session menu (Record / Stop) | Ctrl+R toggles Record dialog → recording toolbar | | | | | |
| UI-3 | File menu (Open Session) | Ctrl+O opens replay file picker | | | | | |
| UI-4 | Chart toolbar (Live toggle / Time presets / Add Chart) | All buttons responsive; live toggle visibly pauses the time axis | | | | | |
| UI-5 | Replay toolbar (Play/Pause/Step±/Seek/Speed/Exit) visible only in Replay mode | Toolbar hidden in Live; visible in Replay; hidden again on Exit | | | | | |
| UI-6 | ConnectionListWidget (left dock) | Shows all connections; click → edit; right-click → context menu (if any) | | | | | |
| UI-7 | ConnectionStatusWidget (status bar) | Click → raises connections dock; reflects aggregate connection state | | | | | |
| UI-8 | Status bar permanent widgets (FPS / Dropped / Throttled / Recording / Replay / Connections) | Each widget updates at the documented cadence | | | | | |
| UI-9 | Multi-chart: Add Chart button creates a second chart | Two charts coexist, each with independent signal selection | | | | | |
| UI-10 | Multi-chart: removal | **Note**: spec §3.2 + release-notes flags missing UI; confirm there is no chart-removal control. V1.5+ deferral | | | | | |
| UI-11 | SignalSelector tree population | Tree shows all `SignalBufferRegistry::signalIds()` signals; refreshes on connection state change | | | | | |
| UI-12 | SignalSelector toggle on/off | Click signal in tree → adds to active chart; click again → removes | | | | | |
| UI-13 | SignalSelector multi-signal toggle | Multiple signals across drivers can be added to one chart simultaneously | | | | | |
| UI-14 | Settings dialog (if any) | **Note**: V1 has no settings dialog per spec; confirm | | | | | |

---

## Audit pass log

(Each operator pass appends a log entry below; CC consumes
the entries when filling in path-table cells and when
authoring S4 fix commits.)

### Pass 1 — TBD (pending operator)

(no entries yet)

---

## Severity tally (refreshed at S3 close)

(blank — refreshed by CC after each operator pass)

| Severity | Count | Paths |
|---|---:|---|
| ✓ Working | 0 | — |
| ⚠ Caveat | 0 | — |
| ✗ Critical | 0 | — |
| ✗ Serious | 0 | — |
| ✗ Minor | 0 | — |

The S5 V1.0 scope re-evaluation reads from this tally:

- Critical = 0 → Scenario A path likely
- Critical ≤ N (small) and all fixable in M14 → Scenario A
- Critical > 10 (M14 HALT #3) or Critical not fixable
  (M14 HALT #1) → Scenario B/C decision
- HALT #5 (>2 frozen-`.hpp` mods) → Scenario B/C
- HALT #4 (post-fix 18-test < 12/18) → Scenario B/C

---

## Cross-references

- Spec: `docs/milestones/M14-gui-audit.md`
- Plan: `.claude/M14-plan.md`
- Concerns: `.claude/M14-concerns.md`
- Progress: `.claude/M14-progress.md` (live findings + counters)
- Smoke harness: `tests/integration/gui/release_binary_smoke.sh`
- Run-1→run-4 history: `docs/architecture/decisions/ADR-010` §"Implementation lesson"
