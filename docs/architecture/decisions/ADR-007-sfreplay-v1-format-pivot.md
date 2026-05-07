# ADR-007 — SFREPLAY v1 Format Pivot (Single-File Signal-Level Recording)

**Status**: Accepted
**Date**: 2026-05-08
**Context**: M10 spec §4.1 vs `docs/architecture/architecture.md §G` divergence; M9 ReplayDriver round-trip impossibility (M10-concerns.md C1 / C2)

---

## Decision

V1 recording uses the M10 spec §4.1 single-file `.sfreplay` format
with **signal-level records**. The two-file `.sfr` (raw frame
chunks) + `.sfi` (binary index) design described in
`architecture.md §G` and §385 is **deferred to V2**.

Round-trip integration testing for the new format is the
responsibility of a **new** `signalforge::session::SessionReader`
class (M10 deliverable, `src/session/session_reader.{hpp,cpp}`),
not the M9 `ReplayDriver`. M9 ReplayDriver retains its current
scope as a **raw-frame-log** replayer for M3 / V1 fixtures and
future raw-frame recordings (V1.5+ scenarios).

The implementation interpretation chosen at Phase 4 review is
**β** (new SessionReader) per `M10-understanding.md §5.C1` and
`M10-plan.md §S6`. **α** (extending M9 ReplayDriver to dispatch
by `formatVersion`) was rejected — see Rationale points 4 and 5
below.

---

## Context

The M10 spec was authored with the assumption that "M9 ReplayDriver
already consumes the SFREPLAY format" (spec §1, §3.6, §7 trigger #2,
§8.3 first item). State observation on `milestone/M10` at HEAD
`16f31fd` showed this is structurally impossible to honor as
written:

| Layer | M9 reader (`replay_driver.cpp:171-225`) | M10 §4.1 writer |
|---|---|---|
| Header | Fixed 16 bytes (magic + 8 unparsed) | Variable (magic + version + headerLen + recordedAt + descLen / desc / schemaIdLen / schemaId + signalCount + signal catalog) |
| Record header | 12 B (`u64 nanosOffset` + `u32 payloadLen`) | 8 B (`u32 recordType` + `u32 payloadLen`) |
| Record content | Raw frame bytes (replayed as `RawFrame` for downstream re-decoding) | Decoded `SignalValue` (`{u32 signalIdx, i64 timestampNs, typed value}`) |
| Output | `DriverInterface::frameOut(RawFrame)` — **M2-frozen** | Would need `SignalValueSink::onSignal(SignalValue)` |

`DriverInterface::frameOut` is part of M2's frozen freeze surface
(per `M2-done.md`); a driver cannot emit `SignalValue`s without
breaking that contract.

Separately, `architecture.md §G` (lines 273-396) describes
recording as a two-file format (`.sfr` + `.sfi`) with
`SessionMetadata` / `SessionChunk` / `ReplayIndex` /
`Bookmark` / `SessionFormatVersion` as the layer's domain types.
M10 spec specifies a single-file `.sfreplay` with no separate
index. CLAUDE.md HALT trigger #7 ("contradiction between
milestone spec and `architecture.md`") fires here.

Per CLAUDE.md §Disagreement handling, CC records the concern and
**executes the spec as written**. This ADR is the authoritative
record of the V1 format choice; `architecture.md §G` is
acknowledged out-of-date and will be reconciled at the next arch
refresh.

---

## Rationale

Five points support the V1 single-file signal-level format over
the two-file raw-frame format described in `architecture.md §G`.

1. **File sizes are smaller for the realistic V1 workload**.
   60 signals × 1 kHz × 10 s = 600 000 events. Signal-level
   records: ~30 MB at typical per-record sizes (8 B record
   header + ~14 B per Double payload). Raw-frame records would
   carry the full driver payload (which after M5 decode produces
   N signals); per-frame size is implementation-defined and
   typically 5-20× larger when the upstream driver emits dense
   frames. For session storage, recording the *decoded* output
   is closer to the user's mental model and disk-efficient.

2. **Replay → chart is direct under signal-level recording**.
   The M11 Replay UX needs `SignalValue` events to push into the
   M6 buffers / M8 chart. Signal-level recording lets
   `SessionReader` push directly to a `SignalValueSink` without
   a re-decode step. Raw-frame recording would require the
   replay path to re-instantiate the same M5 decoder used at
   record time — coupling that the spec explicitly forbids
   (M10 §3.5: no decoder config in file beyond the schema ID
   reference).

3. **Single-file UX is simpler**. Users export, share, and
   archive *one* file. Two-file formats (`.sfr` + `.sfi`) require
   keeping the pair together; missing or misnamed indexes are
   a known operational hazard. The footer-based "file complete"
   signal in §4.1 plus the `Heartbeat` record type substitute
   for the index's truncation-detection role at this milestone's
   data sizes.

4. **α (extend M9 ReplayDriver) contradicts M10 §3.5**. To make
   the M9 reader work with the new format under interpretation α,
   the file would need to carry decoder schema *content* (not
   just a reference), so the runtime can re-decode the synthetic
   frames. M10 §3.5 explicitly excludes connection / decoder
   config from the file. α is therefore not a viable
   interpretation of the spec as a whole — the spec internally
   agrees with β.

5. **β preserves M9 ReplayDriver value for future raw-frame
   workflows**. Raw-frame recording is genuinely useful for
   V1.5+ scenarios: capturing a driver's wire output verbatim
   (e.g., "I want to record the bytes leaving this serial port,
   not the decoded interpretation"). M9 ReplayDriver already
   handles this — leaving it untouched preserves the option for
   V1.5+ to add raw-frame recording without revisiting the
   driver. Under α, the dispatch-by-`formatVersion` branch
   would entangle two unrelated concerns in one class.

---

## Consequences

### What this ADR locks for V1

1. **File format**: single `.sfreplay` per recording session.
   Magic `"SFREPLAY"`, `formatVersion=1`, layout per M10 spec
   §4.1 (canonical spec is authored at
   `docs/format/sfreplay-v1.md` in M10 S2; sha256 → M10-done.md
   freeze record).

2. **Round-trip path**: `SessionWriter` (M10) → file →
   `SessionReader` (M10, new) → `SignalValueSink`. The
   M9 `ReplayDriver` is **not** in this path.

3. **M9 ReplayDriver scope**: unchanged from M9 close — reads
   the legacy 16-byte-header + frame-record format produced by
   M3 fixtures. Continues to support `loop`, `playbackSpeed`,
   pause/resume per the M9 freeze.

4. **M11 Replay UX** consumes `.sfreplay` files via
   `SessionReader`. The M11 `ReplayDriver` UI surface (timeline,
   pause, jump-to-time) is unchanged in shape; only the data
   source plugs into `SessionReader` instead of the M9 driver
   when the file is `.sfreplay`. M11 is responsible for routing
   the right reader based on file extension or magic-byte
   detection.

### Spec text amendments (M10 spec wording corrected here, not
in the spec file — the spec file is frozen at PR #17 merge)

The following M10 spec passages should be read as if amended per
this ADR. CC does not modify the merged M10 spec file (per
CLAUDE.md §Forbidden #2 + the spec freeze at PR #17 merge); this
ADR is the canonical record.

- **§1 closing paragraph**: "M11 Replay" is to be read as
  "M11 Replay (via the new `SessionReader`)".
- **§3.6 first paragraph**: "M9 ReplayDriver reads SFREPLAY
  format" is to be read as "M9 ReplayDriver reads raw-frame log
  files (separate format from V1 SFREPLAY); M10 introduces
  `SessionReader` for SFREPLAY v1 signal-level files".
- **§7 HALT trigger #2**: "M9 ReplayDriver cannot read
  M10-written file" is to be read as "`SessionReader` cannot
  read M10-written file".
- **§8.3 first acceptance item**: "M9 ReplayDriver round-trip"
  is to be read as "`SessionReader` round-trip".

### `architecture.md §G` is acknowledged out-of-date

The two-file `.sfr` + `.sfi` design described in
`architecture.md §385` and the `SessionMetadata` /
`SessionChunk` / `ReplayIndex` / `Bookmark` types listed in §G
predate the V1 scope decisions. CLAUDE.md §Forbidden #2
prohibits CC from modifying `docs/architecture/**` (other than
adding new ADRs, which is the established pattern via ADR-001 …
ADR-005). This ADR is therefore the authoritative record of the
V1 session format. At the next arch refresh, `architecture.md §G`
should be updated to reference this ADR and either:

- Mark the two-file format as "V2 deferred"; or
- Replace §G's format description with the SFREPLAY v1 single-
  file format and move `.sfr` / `.sfi` into a "future work"
  section.

CC will not unilaterally make that arch.md edit. The next ADR
or arch refresh is authored by the human.

### Backpressure refinement (M10-concerns.md C3)

Independently of the format pivot, M10 spec §4.6's backpressure
sample code is ambiguous when the queue head is non-droppable
(Catalog Extension or Stop sentinel). The Phase 4 approval
refines this:

1. Droppable event arrives, queue full → drop the new event
   (FIFO retention of recent writes).
2. Non-droppable event arrives, queue full → drop the
   **oldest droppable event** in the queue first.
3. Queue full of non-droppable events (vanishingly rare) →
   block on enqueue with 10 ms timeout.
4. 10 ms timeout exceeded → log ERROR, return false from
   `enqueue`, transition `RecordingState` to `Error`.

`SessionFileWriter::enqueue` implements this in M10 S5. This is
a **refinement** of §4.6, not a contradiction — the spec's
intent ("non-droppable head means drop the new event instead")
is preserved at point 1; points 2-4 fill in the unspecified
behavior for the rare cases the §4.6 sample code didn't cover.

### Cross-references

- M10 spec: `docs/milestones/M10-session-writer.md` §1 / §3.6 /
  §4.1 / §4.6 / §7 trigger #2 / §8.3
- `M10-concerns.md`: C1 (round-trip impossibility), C2 (arch.md
  §G divergence), C3 (backpressure on non-droppable head)
- `M10-understanding.md` §5: full divergence analysis
- `M10-plan.md` §S0: this ADR is S0's deliverable; §S5 / §S6
  branch on this decision
- M9 `ReplayDriver`: `src/drivers/replay_driver.{hpp,cpp}` —
  retained scope
- New M10 `SessionReader`: `src/session/session_reader.{hpp,cpp}`
  — to be created in S6
- `architecture.md §G` (273-396, 385-396): out-of-date pending
  arch refresh
- ADR-005 (`docs/architecture/decisions/ADR-005-signal-buffer-publish-cadence.md`):
  precedent for "spec / arch divergence resolved via ADR" at
  M6 close
- M9 `M9-concerns.md` C1 (driver-config namespace): precedent
  for "spec text vs implementation truth" reconciliation at
  M9 close

---

## Acceptance for this ADR

This ADR is accepted when:

- Committed on `milestone/M10` as part of M10 S0.
- M10-concerns.md cross-references this ADR for C1 / C2 / C3
  resolution.
- M10-done.md (at M10 close, S11) lists this ADR in its
  "Deviations and concerns" section.

No further approval needed — Phase 4 review at the
`milestone/M10` planning checkpoint (2026-05-08) is the
authoritative human approval, recorded in chat as
"approved, execute M10".
