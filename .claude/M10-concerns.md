# M10 — Concerns (recorded ahead of S1)

This file records the three concerns surfaced at M10 read-time
(Phase 3 → Phase 4) and their resolutions accepted at Phase 4
review. All three are addressed in S0 via ADR-007 + this file;
no HALT triggers fired during planning.

Per CLAUDE.md §Ambiguity handling, deviations are recorded here,
executed as the spec intends (with the Phase-4-accepted
refinements), and re-evaluated at milestone review.

---

## C1 — M9 ReplayDriver round-trip is structurally impossible

**Spec text** (M10 §1, §3.6, §7 trigger #2, §8.3 first item)
asserts the M9 `ReplayDriver` reads files written by M10.

**Reality** (verified on `milestone/M10` at HEAD `16f31fd`,
read `src/drivers/replay_driver.cpp` lines 171-225):

| Layer | M9 reader (raw frames) | M10 §4.1 writer (signal values) |
|---|---|---|
| Header | Fixed 16 B | Variable; magic + version + headerLen + recordedAt + descLen / desc / schemaIdLen / schemaId + signalCount + catalog |
| Record header | 12 B (`u64 nanosOffset` + `u32 payloadLen`) | 8 B (`u32 recordType` + `u32 payloadLen`) |
| Record content | Raw frame bytes; replayed as `RawFrame` for downstream re-decoding | Decoded `SignalValue`s; already-decoded |
| Output | `DriverInterface::frameOut(RawFrame)` — **M2-frozen** | Would need `SignalValueSink::onSignal(SignalValue)` |

`DriverInterface::frameOut` is M2-frozen; a driver cannot emit
`SignalValue`s without breaking M2. Thus M10's spec §8.3 first
item is impossible to honor literally.

**Resolution** (Phase 4 accepted **interpretation β**):

Add a new `signalforge::session::SessionReader` class
(`src/session/session_reader.{hpp,cpp}`) in M10 S6. It consumes
`.sfreplay` v1 files and pushes `SignalValue`s directly to a
`SignalValueSink`. Round-trip integration testing (M10 S7) goes
through `SessionReader`, not M9 `ReplayDriver`.

The M9 `ReplayDriver` retains its current scope — raw-frame
log replayer for M3 fixtures and future raw-frame recording
workflows (V1.5+).

Spec §8.3 first item, §7 trigger #2, §3.6 first paragraph, and
§1 closing paragraph are amended in **ADR-007** consequences
section. CC does not edit the merged M10 spec file (frozen at
PR #17 merge); ADR-007 is the canonical record of the
amendments.

**Status**: ✅ resolved. Implementation lands in S6 per plan.

---

## C2 — `architecture.md §G` specifies a different session format

**Arch text** (`docs/architecture/architecture.md §G` 273-396,
specifically §385-386):

> Session files: two-file format
>   `sessions/*.sfr` — Custom binary chunk; raw frames and
>     messages; magic + version in header
>   `sessions/*.sfi` — Custom binary index; time index and
>     bookmarks; magic + version in header
>
> Domain types: SessionMetadata, SessionChunk, ReplayIndex,
> Bookmark, SessionFormatVersion.

**M10 spec** (§4.1) specifies a single `.sfreplay` file with
signal-level records and **no separate index**. CLAUDE.md HALT
trigger #7 (contradiction between milestone spec and
`architecture.md`) fires literally.

**Resolution**: per CLAUDE.md §Disagreement handling and the
established M5 / M6 / M9 pattern of resolving spec / arch drift
via parallel-track ADR, this ADR-007 records the V1 single-file
signal-level decision. arch.md §G is acknowledged
**out-of-date pending the next arch refresh**; CC does **not**
unilaterally edit `architecture.md` (CLAUDE.md §Forbidden #2).

The two-file `.sfr` + `.sfi` design is **deferred to V2**. ADR-007
§Consequences spells out the rationale (5 specific points: file
sizes, signal-level direct rendering, single-file UX, M10 §3.5
alignment, V2 evolution).

**Status**: ✅ resolved via ADR-007 (Accepted, this commit).
Next arch refresh by the human will reconcile §G text.

---

## C3 — Backpressure refinement for non-droppable queue head

**Spec text** (M10 §4.6) sample code only checks for
`WriteSignalEvent` at queue head before dropping. Catalog
Extension and Stop sentinel events are non-droppable —
losing one corrupts the file (records reference signal
indices that match the file's catalog).

**Resolution** (Phase 4 accepted refinement):

`SessionFileWriter::enqueue` policy under queue full:

1. **Droppable event arrives, queue full** → drop the new
   event (FIFO retention of recent writes).
2. **Non-droppable event arrives, queue full** → drop the
   **oldest droppable** event in the queue first.
3. **Queue full of non-droppable events** (vanishingly
   rare in practice — would require thousands of mid-record
   signal registrations) → block on enqueue with **10 ms**
   timeout.
4. **10 ms timeout exceeded** → log `SF_LOG_ERROR`, return
   `false` from `enqueue`, transition `RecordingState` to
   `Error`. Recording is halted; partial file remains
   valid up to the last flush.

`session_writer_dropped_events_total` increments per drop
(both droppable-overflow and oldest-droppable-eviction
paths).

This is a **refinement** of §4.6, not a contradiction —
§4.6's intent ("drop the new event instead" if queue head
is non-droppable) is preserved at point 1; points 2-4
fill in the unspecified behavior for the rare cases.

**Status**: ✅ resolved. Implementation in S5.

---

## Summary table

| # | Concern | Phase 4 decision | Lands in |
|---|---|---|---|
| C1 | Round-trip impossible via M9 ReplayDriver | β: new SessionReader at `src/session/session_reader.{hpp,cpp}` | S6 (impl) + S7 (round-trip test) |
| C2 | arch.md §G specifies two-file format | ADR-007 §Consequences: V1 single-file; .sfr+.sfi V2-deferred | S0 (this commit) — done |
| C3 | §4.6 backpressure ambiguous on non-droppable head | 4-point refinement; non-droppable blocks 10 ms then `Error` | S5 (queue impl + tests) |

No HALT triggers fired during M10 read or planning. ADR-007 is
the authoritative cross-reference for §G + §4.1 reconciliation.
