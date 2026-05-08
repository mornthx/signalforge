# M10 — Understanding (Session Writer)

This document is CC's reading of `docs/milestones/M10-session-writer.md`,
read at session start on milestone/M10 (HEAD = `16f31fd`, M10 spec from
PR #17 merged at `83c409c`). It mirrors the M9-understanding.md
structure: spec restate, intent, dependencies, freeze surface,
acceptance, and concerns recorded ahead of S1.

CLAUDE.md is the binding contract; where this document and the spec
disagree, the spec (and CLAUDE.md) win. This file exists for the
human's Phase 4 review.

---

## 1. What the spec asks for

M10 makes recording sessions persistent on disk in a frozen V1 file
format. The user-visible workflow after M10 closes is:

1. Connect device (M9)
2. Click **Record** in the toolbar (M10) → save-file dialog →
   `SessionWriter::start(path)` opens an `.sfreplay` file
3. Signals flow into charts (M8) **and** are written to disk
4. Click **Stop** (or app quit) → file closed gracefully
5. (Future M11) Re-open the file and replay it through the chart UI

The freeze surface at M10 close is:

- `SessionWriter` (`src/session/session_writer.hpp`) — public API
- `SessionMetadata` (`src/session/session_metadata.hpp`) — frozen
  in-memory representation of the file header
- `docs/format/sfreplay-v1.md` — canonical V1 binary format spec

V1 promise: any V2 reader must continue to parse V1 files; any V1
reader must skip-over additive V2 extensions without crashing.

## 2. Locked design decisions (spec §3)

| # | Decision | What it means for M10 |
|---|---|---|
| 3.1 | Custom binary format with `"SFREPLAY"` magic + `formatVersion=1` | Not HDF5 / Parquet / Arrow. No external deps. |
| 3.2 | Independent worker thread for disk I/O | `QThread` + `moveToThread`; bounded queue; main thread never blocks on disk |
| 3.3 | User-initiated recording only | Toolbar Record button; no auto-on-connect in V1 |
| 3.4 | All signals recorded (no per-signal filter) | `SessionWriter` registers as a `SignalValueSink` on the M6 registry; new mid-stream signals appended via Catalog Extension records |
| 3.5 | Signals + decoder schema metadata in header; **no connection config** | Replay can re-decode but can't reconnect to the same physical device |
| 3.6 | Format itself is frozen at M10 close (not M9 close) | M9 had it informally; M10 makes the spec a contract |
| 3.7 | No soft-HALT (inherits M2-M9 strictness) | All 7 M10-specific HALT triggers are hard |
| 3.8 | Metric naming under `session_writer_*` prefix | Counters/gauges per spec §3.8 |

## 3. Module landscape inherited from M2-M9

The session writer composes existing frozen surfaces. Reading order
for arch / freeze record:

- **M2** `src/drivers/driver_interface.hpp` — `DriverInterface`
  contract is **frozen**. M10 does NOT touch it. (M9 just used it.)
- **M5** `src/decode/decoder_interface.hpp` — `SignalValue`
  (`std::variant<bool, std::int64_t, double, QString>`),
  `SignalMetadata` (`id`, `name`, `unit`, `type`, optional
  `description` / `scale` / `offset`), and `SignalValueSink` (3 virtuals:
  `onSignal`, `onSignalsRegistered`, `onSignalsUnregistered`) are M5's
  freeze. The spec writes `SessionWriter : public ... SignalValueSink`,
  which is the canonical hook to receive signal values.

  **Namespace pin**: `signalforge::decoder` (not `decode`, despite the
  directory `src/decode/`). The M10 spec example code matches.
- **M6** `src/buffer/signal_buffer_registry.hpp` — `SignalBufferRegistry`
  is the registry that fans out to all sinks. `SessionWriter` will
  register itself as one of those sinks. The registry's freeze is
  M6's; M10 reuses, never extends.
- **M9** `src/drivers/replay_driver.{hpp,cpp}` — already consumes a
  16-byte SFREPLAY header + a frame-stream record format. **Not
  frozen** per M9-done.md (M9-concerns.md C2 explicitly notes M9's
  freeze list is `connection.hpp` + `connection_manager.hpp` only).

## 4. The freeze surface I will produce in M10

Per spec §6.1:

- `src/session/session_writer.hpp` (frozen at M10 close)
- `src/session/session_metadata.hpp` (frozen at M10 close)
- `docs/format/sfreplay-v1.md` (frozen at M10 close — V1 binary
  format spec, the milestone's first frozen file format)

Per spec §6.2 the following are explicitly NOT frozen and stay
internal:

- `SessionFileWriter` worker class
- Queue capacity / flush interval defaults
- `sfreplay_inspect` CLI internals

## 5. Concerns recorded ahead of S1

Three concerns surfaced during state observation. **C1 is a hard
contradiction** that blocks S0 of the plan; CC will not start writer
code until the human resolves it at Phase 4 review (or instructs CC
to proceed with one of the two interpretations sketched).

### C1 — Round-trip through M9 ReplayDriver is structurally impossible (HALT trigger candidate)

M10 spec §1, §3.6, §7 trigger #2, and §8.3 first item all assert that
**M9's `ReplayDriver` reads files written by M10**. Verified on
`milestone/M10` at HEAD `16f31fd`, this is **not implementable as
written**:

| Layer | What M9 reads (`replay_driver.cpp:171-225`) | What M10 §4.1 writes |
|---|---|---|
| Header | Fixed **16 bytes**: `"SFREPLAY"` (8) + 8 unparsed bytes | **Variable**: magic (8) + formatVersion (4) + headerLen (4) + recordedAt (8) + descLen / desc / schemaIdLen / schemaId / signalCount + signal catalog |
| Record header | **12 bytes**: `u64 nanosOffset` + `u32 payloadLen` | **8 bytes**: `u32 recordType` + `u32 payloadLen` |
| Record content | **Raw frame bytes** (`RawFrame.payload`); replayed as frames into the pipeline (decoded again at runtime) | **Decoded signal values** (`{u32 signalIdx, i64 timestampNs, typed value}`); already-decoded, no re-decoding step |
| Output channel | `DriverInterface::frameOut(RawFrame)` — frames | Would need to fan out as `SignalValueSink::onSignal(SignalValue)` — signals |

`DriverInterface::frameOut` is **M2-frozen** (per `M2-done.md`). A
driver cannot emit `SignalValue`s without breaking that contract. So
even if `ReplayDriver` is updated to parse the new header, it cannot
produce signal values to the rest of the system without violating M2.

Two implementations, both consistent with the spec **as written**:

- **Interpretation α — extend `ReplayDriver` to dispatch by header**:
  if first 16 bytes match `"SFREPLAY"` + `formatVersion >= 1`, read
  the new variable header and the typed record stream; emit per-record
  synthetic `RawFrame`s whose payload is the raw record bytes (so
  pipeline downstream re-decodes via M5).
  - Pro: round-trip via M9 ReplayDriver works → spec §8.3 first item
    literally satisfied.
  - Con: requires re-encoding the typed records back into something
    that walks through M3 driver → M4 pipeline → M5 decoder → M6
    registry. The format §4.1 records are already decoded
    `SignalValue`s. Reconstructing a frame round-trip from a decoded
    signal is wasteful and fragile (and may not produce identical
    byte-level frames if the source decoder isn't deterministic).
  - This interpretation conflicts with M10 §3.5 which **explicitly
    excludes** connection / decoder config from the file. Without
    a decoder, the synthetic-frame round-trip fails.
- **Interpretation β — add a new `src/session/session_reader.hpp`
  reader that emits `SignalValue`s directly via `SignalValueSink`**:
  M9 `ReplayDriver` stays a frame-replayer for raw-frame fixtures;
  M10's `SessionReader` consumes the new format and pushes
  `SignalValue`s straight into a `SignalValueSink` (e.g., a
  `SignalBufferRegistry` for replay). Round-trip is M10-write →
  `SessionReader`-read → `SignalValueSink`.
  - Pro: clean separation; no M2 / M9 modifications; matches §3.5
    (no connection / decoder config in file → re-decode not needed).
  - Con: spec §8.3 first item, §7 trigger #2, and §1 / §3.6's "M9's
    ReplayDriver" wording are not literally honored. The spec author
    appears to have conflated "the file format M9 named" with "the
    component M9 ships". The two are not the same: M9 named the
    format ("SFREPLAY" magic + frame stream); M10 specifies a
    different, signal-level record format under the same magic.

CC's recommended interpretation: **β**, with `SessionReader` added as
an additive deliverable in `src/session/`. CC reads §3.6 ("Format
spec moves into M10 — M9 had it as code") as evidence the spec author
intended the format itself to evolve from M9's frame stream into
M10's signal-level stream, with M9's reader staying a raw-frame tool
(useful for M3 fixtures + future raw-frame recordings) and a new
M10 reader handling signal-level files. Under β, "M9 ReplayDriver
round-trip" in §8.3 / §7 trigger #2 should be re-read as "M10
round-trip" (write via writer, read via the new reader).

The plan's S0 is to capture this resolution in an ADR
(`docs/architecture/decisions/ADR-007-sfreplay-v1-format-pivot.md`)
and revise spec §7 trigger #2 / §8.3 first item via the
"additive extension without HALT" carve-out in CLAUDE.md
§Ambiguity (the carve-out is for CC's own reports, but the
parallel-track ADR pattern is established by M6 ADR-005 +
M9 C2). Phase 4 review is where the human approves α / β /
revise-spec.

### C2 — `arch.md §G` describes a different session format

`docs/architecture/architecture.md` §385-396 describes session files
as **two-file**: `.sfr` (raw frame chunks) + `.sfi` (binary index).
`SessionMetadata`, `SessionChunk`, `ReplayIndex`, `Bookmark`, and
`SessionFormatVersion` are listed as the layer's domain types.

M10 spec specifies **one-file** `.sfreplay` with signal-level records
and **no separate index**. CLAUDE.md §Forbidden #2 prohibits modifying
`docs/architecture/**` from CC. The contradiction is real.

Resolution: per CLAUDE.md HALT trigger #7 ("contradiction between
the milestone spec and `architecture.md`"), the strict reading is
HALT. The pragmatic reading — used previously for arch / spec drift
in M5 (yaml schema vs §G) and M9 (`*Config` namespace) — is to
follow the milestone spec, document the deviation in M10-concerns.md
+ M10-done.md, and let the human reconcile arch.md at next refresh.
CC will follow the pragmatic path **with explicit human approval at
Phase 4**. If denied, CC HALTs.

The §G "ReplayIndex" + ".sfi" companion-file question is deferred to
V1.5 (the spec §2.2 says "no automatic rollover" / "no index"). That
is consistent with the spec; arch.md is the one out-of-date.

### C3 — Spec §4.1 "catalog extension" record vs the bounded queue

Spec §4.1 record type 2 (Catalog Extension) is written when a new
signal registers mid-recording. Spec §3.4 + spec §2.1-10 say the
queue is bounded at 10 000 events and on overflow CC drops *oldest*
events. But Catalog Extension events are **not droppable** — losing
one corrupts the file (records reference signal indices into the
catalog). Spec §4.6 alludes to this: "drop the new event instead" if
queue head is non-droppable, but the wording is ambiguous and the
sample code in §4.6 only checks for `WriteSignalEvent` at the head.

Resolution: track Catalog Extension and Stop sentinels as
**non-droppable** in queue accounting; on overflow, drop the
**oldest droppable WriteSignalEvent**. If queue is full of
non-droppable events (vanishingly rare), the writer must **block
briefly** on the queue to avoid corrupting the file — this is the
*one* place worker-thread coordination is allowed to back-pressure
the main thread, and only for queue full of catalog-mutation events
(not signal data). This is a refinement of §4.6, not a contradiction;
captured as a concern for traceability. Plan §S5 (queue + backpressure)
will spell out the policy.

## 6. What CC will produce in M10 (mapped to plan subtasks)

Detail in `M10-plan.md`. Summary of deliverables, in order:

1. **S0** — ADR + concerns + format-resolution checkpoint (C1 + C2)
2. **S1** — Module scaffolding (`src/session/CMakeLists.txt`, freeze
   headers `session_writer.hpp` + `session_metadata.hpp`, namespace
   skeleton)
3. **S2** — `docs/format/sfreplay-v1.md` canonical format spec
   (sha256 → freeze record)
4. **S3** — `SessionWriter` class + worker `SessionFileWriter`
   skeleton + `QThread` lifecycle
5. **S4** — `SessionFileWriter::openFile` + header / catalog /
   record encoding + footer
6. **S5** — Queue + backpressure (incl. C3 non-droppable handling)
7. **S6** — `SessionReader` (under interpretation β) **OR**
   `ReplayDriver` extension (under α). Gated on Phase 4 decision.
8. **S7** — Round-trip integration test (writer → reader →
   `SignalValueSink` → comparison) + 7 integration tests per spec §2.1-12
9. **S8** — `tools/sfreplay_inspect/` CLI tool
10. **S9** — `MainWindow` Record toolbar + status-bar integration
11. **S10** — `bench_session_writer` benchmark + 30-min memory soak
   (mirrors M8 / M9 S5s pattern)
12. **S11** — `.claude/M10-done.md` + freeze record (sha256 of 3
    frozen files) + manual verification protocol

## 7. Freeze record CC will deliver

Per spec §6.3, M10-done.md will sha256:

- `src/session/session_writer.hpp`
- `src/session/session_metadata.hpp`
- `docs/format/sfreplay-v1.md`

Plus the standard M10-done.md sections: deliverables checklist, PR
state, test counts, performance gates, hand-off to M11/M12/M13,
HALT-trigger disposition table, deferred-to-V1.5+ list.

## 8. Acceptance read-back (spec §8 → CC's interpretation)

| Spec gate | CC interpretation |
|---|---|
| §8.1 build / test | Debug + Release + debug-asan all green; coverage ≥ 80 % on session module; ctest passes on milestone/M10 head |
| §8.2 perf — 60k events/sec sustained | bench_session_writer at 60 sig × 1 kHz × 10 s; HALT if < 30 k/sec or main-thread block > 5 ms |
| §8.2 perf — 30-min memory growth < 10 % | reuse M9 S5s soak harness pattern; new bench_session_writer adds memory snapshots |
| §8.3 round-trip | Under β: M10 writer → `SessionReader` → `SignalValueSink` → bit-equal value sequence. Under α: M10 writer → ReplayDriver → frame-equal stream. **Phase 4 picks.** |
| §8.3 truncated file | Footer-less file readable up to last complete record; tested explicitly |
| §8.4 lifecycle | Start / stop / start cycles green; worker thread joined before destructor; disk-full simulation logs ERROR + leaves file valid up to last flush |
| §8.5 thread safety | ASan clean (Debug + debug-asan); TSan clean if local host permits, else CI-only (per the host-ASan-preload memory note) |
| §8.6 freeze record | M10-done.md §Freezes lists 3 sha256s + no M2-M9 frozen-file modifications |
| §8.7 hand-off | M11 (Replay UX): SFREPLAY v1 spec + sfreplay_inspect tool; M12 (Performance): bench_session_writer baseline; M13 (Packaging): docs/format/ ships in install bundle |

## 9. Inherited concerns from M9

Per the user's Phase 3 continuation message, two M9 follow-ups remain
and are **explicitly not** M10's responsibility:

- M8-done.md §8.2 "Pending: 1-hour soak" checkbox flip (M9 S5s
  resolved the soak, but the M9 session permission rule blocked
  editing the prior milestone done file). Human-driven follow-up,
  noted in M9 C3.
- M9 §8.3 manual hardware verification (6-test protocol in
  `docs/m9-hardware-verification.md`). Human runs against real /
  mock hardware after M9 merge.

M10 carries no inherited soak / perf concerns from M9.

## 10. Risks tracked into the plan

- **Risk A — C1 not resolved at Phase 4**: M10 cannot start writer
  code without the format / round-trip resolution. Plan §S0 is the
  blocker. Mitigation: surface C1 as the **first** Phase 4 review
  item; if the human picks α, plan §S6 swaps to ReplayDriver
  extension; if β, plan §S6 produces `SessionReader`.
- **Risk B — Worker thread vs main thread synchronization bugs**:
  Qt's `moveToThread` + `QQueuedConnection` is well-trodden, but
  10 000-deep queue + atomic counters + clean shutdown across
  start/stop cycles is the kind of code where ASan / TSan find
  bugs M9 didn't cover. Mitigation: dedicate one S subtask to
  threading tests; gate by ASan-clean (and TSan if local host
  permits — per the existing host-ASan-preload memory note).
- **Risk C — Disk-full / IO error**: spec §2.1-7 + §8.4 ask for
  graceful degradation. Mitigation: explicit `tests/integration/
  test_session_writer_disk_full.cpp` + simulated `QFile::write`
  failure injection.
- **Risk D — Format-spec authoring errors**: §4.1 has detailed
  byte-level definitions; transcription mistakes in the canonical
  spec doc become permanent V1 contracts. Mitigation: S2 (spec
  authoring) + S4 (encoder) + S6 (decoder) cross-validate via
  round-trip. The `sfreplay_inspect` tool (S8) is a third
  independent parser that verifies the spec is implementable.

---

End of M10 understanding. Plan is in `M10-plan.md`. Phase 4 review
gate: human approval at this checkpoint.
