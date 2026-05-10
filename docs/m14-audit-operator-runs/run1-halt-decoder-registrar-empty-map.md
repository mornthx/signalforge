# M13 V1.0 Hardware Verification — HALT report

**Date**: 2026-05-09 (CST)
**Operator**: shuai (tmcracer@gmail.com)
**Halt at**: T3 (M9 UDP driver), 1st test attempted of 18
**Acceptance bar**: 16/18 PASS (per M13 protocol §3.5 V + §5.2)
**Projected pass count if session continued**: ≤ 5/18 → < 16 → HALT (H4 trigger
per M13 plan §3)

## Trigger

The defect blocks the entire decode pipeline for live drivers (M9). Symptoms
manifested while attempting Test 3 (UDP) but the root cause is shared by every
M9 driver path.

## Symptom

In `signalforge` GUI:
1. Add UDP connection (bind 127.0.0.1:9998, schema
   `examples/schemas/modbus_style.yaml`).
2. Click Connect → State column shows `Connected`.
3. Drive 8-byte modbus_style read_response frames at 5 Hz to the bound port.
4. **Signal Selector remains empty.** No signals register; no chart populates.

Reproduced after:
- Replacing `~/Music/...` with absolute `/home/shuai/Music/...` schema path.
- Restarting `signalforge` with `SF_LOG_LEVEL=debug` (env var has no effect on
  the release build's log level — separate observation).

## Evidence chain

| Layer | Observation | Verdict |
|---|---|---|
| Feeder | ~10 000 frames sent to 127.0.0.1:9998 (`01 03 04 <temp_BE> 00 01 00`) | Data conformant to `modbus_style.yaml` `read_response` layout |
| Schema | `schema_lint examples/schemas/modbus_style.yaml` → `OK: 2 layouts, 10 fields` | YAML well-formed |
| Kernel UDP | `cat /proc/<pid>/net/udp` → `rx_queue 00000000`, `drops 0` | Packets fully delivered to app socket |
| Driver thread | `UdpIO-127.0.0.1` thread alive | I/O thread reads packets out of socket |
| Decoder logs | **Zero `DecoderRegistrar[*]` info/warn lines for the entire session** | Decoder never attached; `pipelineAttached` apparently does not fire end-to-end |
| Source (`src/app/main_window.cpp:64-66`) | `DecoderRegistrar` constructed with `std::unordered_map<QString, QString>{}` (empty) | No driver TYPE has any schema registered |
| Source (`src/connection/connection_manager.cpp`) | `decoderSchemaId` referenced only at lines 379, 380, 443 — YAML load/save only; **no path passes it to `decoderRegistrar_`** | Per-connection schema field is dead code at runtime |
| Source (`src/decode/decoder_registrar.cpp:55-60`) | Empty-map lookup → "no schema configured for driver type 'X'; no decoder attached" | Even if `pipelineAttached` did fire, no decoder would attach |
| Spec (`docs/milestones/M5-decoder-layer.md:567`) | "M9 Connection Manager replaces this with per-connection user selection." | Architectural intent: M9 must wire per-connection schema |
| Self-noted gap (`.claude/M9-done.md:211`) | "decoderSchemaId resolution in the dialog uses a filesystem walk … rather than `DecoderRegistrar::availableSchemas()` (which doesn't exist on M5's frozen registrar)." | The gap was acknowledged at M9 close but not closed |

## Root cause

M5 § 4.6 mandates that M9 replace M5's hard-coded `driverTypeToSchemaPath`
registration with per-connection user selection. **M9 added the GUI surface
(combo, persistence) but did not add the runtime plumbing** that would route
the user's per-connection schema selection into `DecoderRegistrar` at connect
time. Two specific gaps:

1. **`MainWindow` constructs `DecoderRegistrar` once with an empty map** and
   never updates it.
2. **`ConnectionManager`'s connect path does not push `connection.config().decoderSchemaId`
   into the registrar** before the pipeline is attached.

A secondary observation: in the captured runtime log, **no `DecoderRegistrar`
log line of any level fires** for the UDP connection. Per the code, the
"no schema configured" info-level line should fire on every `pipelineAttached`
emission. Its absence suggests that the `pipelineAttached` signal path itself
is also not reaching the registrar slot — possibly a second wiring defect to
investigate when the primary fix is in place.

## Tests blocked

Direct fail (same decoder-attach pathway):
- T1 Serial driver
- T2 TCP driver
- T3 UDP driver
- T7 Session record GUI round-trip (requires Connected source emitting signals)
- T10 Mid-stream catalog extension (requires two live decoders on different schemas)

Chained block (depend on a recorded `.sfreplay` from T7 or signals from T1/2/3):
- T4 Replay driver
- T8 Replay portable across restart
- T13 GUI open + replay
- T14 Play/Pause toggle
- T15 Step ◀ / ▶
- T16 Timeline scrubber
- T17 Speed combo
- T18 Live ↔ Replay confirmation dialogs (also requires a Connected live driver)

Not necessarily blocked (lifecycle-only, no decoder dependency):
- T5 Edit/Remove connection
- T6 Auto-connect on start
- T9 Quit-while-recording prompt (the prompt itself fires regardless of decode,
  but verifying graceful stop relies on a non-empty file footer)
- T11 / T12 (optional)

Best-case pass projection: ≤ 5/18, well under the 16/18 acceptance bar.

## Recommended fix direction (for the reviewer / next session)

Either of the following closes the gap; the second is closer to the M5 § 4.6
intent:

1. **Bootstrap the map at startup**: in `MainWindow` ctor, walk
   `examples/schemas/*.yaml` and build the `driverTypeToSchemaPath` argument
   before constructing `DecoderRegistrar`. Quick fix, but it forces every
   driver of a given type to share one schema (regression vs. per-connection
   selection).

2. **Per-connection registration** (preferred, matches M5 § 4.6): expose a
   non-frozen public method on `DecoderRegistrar` such as
   `registerDriverSchema(driverId, schemaPath)`, then have
   `ConnectionManager::startConnection` (or wherever the pipeline is attached
   for a connection) call it with `cfg.decoderSchemaId` resolved to an absolute
   path before emitting `pipelineAttached`. Verify the second observation
   (whether `pipelineAttached` is reaching the slot) at the same time.

In either case the `decoderSchemaId` field in `ConnectionConfig` must become
load-bearing, and an integration test should drive a UDP packet through a
GUI-configured connection and assert a signal lands in the registry.

## Captured artefacts

- `/tmp/m13-verify-logs/session.log` (info-level run)
- `/tmp/m13-verify-logs/session-debug.log` (`SF_LOG_LEVEL=debug` had no effect
  on output but the file exists for completeness)
- `~/.local/state/signalforge/logs/signalforge.log` (system runtime log;
  search after marker `>>> log marker before restart 2026-05-09T16:45:22+08:00`)

## Operator action taken

- M13 dogfood session halted at first failed test.
- No further tests attempted.
- Result table not filled (only T3 has data, recorded as FAIL with root cause
  above).
- Awaiting fix and reverification per M13 plan §3.

---

Reviewer: please route to whoever owns `src/app/main_window.cpp` and
`src/connection/connection_manager.cpp` for the M9-side fix. Re-run M13
end-to-end after the patch lands.
