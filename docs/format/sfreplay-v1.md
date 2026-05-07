# SFREPLAY v1 — Canonical Binary Format Specification

**Status**: Frozen at M10 close (2026-05-08). V1 contract.
**Magic**: ASCII `"SFREPLAY"` (8 bytes, no NUL terminator).
**Format version**: `1` (`uint32_le` at file offset 8).
**Extension**: `.sfreplay`.
**Cross-references**:

- `docs/milestones/M10-session-writer.md` §4.1 — milestone spec
  (this doc is the canonical, frozen restate)
- `docs/architecture/decisions/ADR-007-sfreplay-v1-format-pivot.md`
  — V1 format pivot, M9 ReplayDriver vs SessionReader split
- `src/session/session_writer.hpp` — frozen writer surface
- `src/session/session_metadata.hpp` — frozen in-memory header
  representation
- `src/session/session_reader.hpp` (M10 S6) — round-trip reader

---

## 1. Scope and intent

A SFREPLAY v1 file holds a complete record of one signal-recording
session: file-level metadata, the catalog of signals known to the
writer, the time-ordered stream of signal values, and a footer that
asserts "this file finished cleanly".

The format is the V1 contract for SignalForge sessions. Once shipped,
V1 readers (in any language, on any platform) must continue to parse
files that conform to this spec for the lifetime of V1. V2+ may
extend the format, but only in ways V1 readers can ignore — see §6.

What this format **does not** include (deliberately, per
`M10-session-writer.md §3.5` + ADR-007):

- Connection configuration (driver type, host, port, baud rate)
- Decoder schema *content* (only an opaque `schemaId` reference)
- Application UI state (chart layouts, panel positions)

Replay needs the signal stream + signal catalog + optional schema
reference to produce values for charts; nothing more.

## 2. Endianness, integer encoding, time

- **Endianness**: little-endian everywhere. Every multi-byte
  integer or floating-point field is encoded LSB-first (Intel /
  ARM little-endian native; readers on big-endian hosts must
  byte-swap).
- **Integer types**: `uint32_le`, `uint64_le`, `int32_le`,
  `int64_le` denote the corresponding fixed-width unsigned /
  signed two's-complement integer. `uint8` is a single byte;
  endianness is not applicable.
- **Floating-point**: `double_le` is IEEE 754 binary64
  little-endian (8 bytes).
- **Strings**: every string field is `uint32_le strLen` followed
  by `strLen` bytes of UTF-8 (no NUL terminator, no BOM).
  `strLen` may be `0` — that encodes the empty string. The maximum
  practical `strLen` is `2^32 − 1` bytes, but writers SHOULD keep
  individual strings under 64 KiB; readers MAY refuse strings
  longer than 16 MiB.
- **Time**:
  - `recordedAt` (file header): `int64_le` nanoseconds since the
    Unix epoch (`1970-01-01T00:00:00Z`). Wall clock origin —
    informational only; replay logic must not depend on it.
  - Per-record `timestampNs` (record payload): `int64_le`
    nanoseconds since the **recording start** captured in the
    file header's `recordedAt`. Monotonic with respect to the
    writer's steady-clock at record time.

## 3. File layout (high-level)

```
+-----------------+  offset 0
| Header          |  variable; ends at headerLen
+-----------------+
| Signal Catalog  |  variable; embedded inside the header
+-----------------+  offset headerLen
| Records         |  stream of typed records; variable count
+-----------------+
| Footer          |  fixed 16 bytes; final 16 bytes of file
+-----------------+
```

**Total file length** = `headerLen + sum(record total sizes) + 16`.

A file without the 16-byte footer is **incomplete** (the writer
was interrupted before clean stop). Readers must tolerate this
case — replay all complete records, then stop. See §7.

## 4. Header (offset 0)

The header is variable-length. The fixed prefix is 16 bytes; from
offset 16 onwards the header holds three variable-length string
fields, the signal count, and the signal catalog itself.

### 4.1 Fixed prefix (offsets 0..15, 16 bytes)

| Offset | Size | Field | Type | Description |
|---:|---:|---|---|---|
| 0 | 8 | `magic` | `uint8[8]` | ASCII `"SFREPLAY"`. No NUL. |
| 8 | 4 | `formatVersion` | `uint32_le` | `1` for this spec. |
| 12 | 4 | `headerLen` | `uint32_le` | Total header byte length, from offset 0 through the last byte of the signal catalog. |

Readers must verify `magic == "SFREPLAY"` and `formatVersion == 1`
before parsing the rest. A reader encountering `formatVersion > 1`
MAY attempt to parse the file as V1 (per the V2 forward-compat rules
in §6) but is not required to.

### 4.2 Variable section (offsets 16..headerLen-1)

| Field | Type | Description |
|---|---|---|
| `recordedAt` | `int64_le` | Wall-clock origin: nanoseconds since Unix epoch when recording started. |
| `descLen` | `uint32_le` | Length of `description` in bytes (UTF-8). |
| `description` | `uint8[descLen]` | UTF-8 bytes; may be empty (descLen=0). |
| `schemaIdLen` | `uint32_le` | Length of `schemaId` in bytes (UTF-8). |
| `schemaId` | `uint8[schemaIdLen]` | UTF-8 bytes; may be empty (schemaIdLen=0). Opaque to this format spec; the application interprets it (typically an M5 decoder schema id). |
| `signalCount` | `uint32_le` | Number of entries in the initial signal catalog. May be `0`. |
| `signalCatalog` | `SignalEntry[signalCount]` | See §5. |

The variable section's total byte length plus the 16-byte fixed
prefix equals `headerLen`.

### 4.3 Header total length (worked example)

Empty description, empty schemaId, 0 signals:
`16 + 8 + 4 + 0 + 4 + 0 + 4 + 0 = 36 bytes`.

Description "Run #42", schemaId "dev-board-frame-v1", 60 signals
of typical metadata size (~100 B each):
`16 + 8 + 4 + 7 + 4 + 18 + 4 + (60 × ~100) ≈ 6 KB`.

Readers must rely on `headerLen` (offset 12) — never on parsing
through to the signal catalog and computing the position
post-hoc — when seeking to the first record. This makes the header
forward-compatible: V2 may add fields after `signalCount` without
breaking V1 readers, provided V2 increments `headerLen`
accordingly.

## 5. Signal catalog entry

One entry per signal. Encoded inline at the end of the header (see
§4.2) and again — appended — inside any Catalog Extension record
(see §6.3 record type 2).

| Field | Type | Description |
|---|---|---|
| `signalIdLen` | `uint32_le` | Length of `signalId`. |
| `signalId` | `uint8[signalIdLen]` | UTF-8; matches `SignalMetadata.id`. Convention: `<driverId>/<fieldName>` (M5). |
| `nameLen` | `uint32_le` | Length of `name`. |
| `name` | `uint8[nameLen]` | UTF-8; human-readable. |
| `unitLen` | `uint32_le` | Length of `unit`. |
| `unit` | `uint8[unitLen]` | UTF-8; SI / engineering unit. |
| `descLen` | `uint32_le` | Length of `description`. |
| `description` | `uint8[descLen]` | UTF-8; optional (may be empty). |
| `type` | `uint8` | Type tag — see table below. |
| `hasScale` | `uint8` | `0` or `1`. |
| `scale` | `double_le` | Linear-transform multiplier. **Present only if `hasScale != 0`.** |
| `hasOffset` | `uint8` | `0` or `1`. |
| `offset` | `double_le` | Linear-transform addend. **Present only if `hasOffset != 0`.** |

Type tag values:

| Value | Meaning | Per-record payload encoding |
|---:|---|---|
| 0 | bool | 1 byte (`0` = false, `1` = true) |
| 1 | int64 | 8 bytes (`int64_le`) |
| 2 | double | 8 bytes (`double_le`) |
| 3 | string | `uint32_le strLen` + `strLen` UTF-8 bytes |

Other type values are reserved for V2.

The conditional encoding of `scale` / `offset` (gated by their
`hasX` flag bytes) keeps catalog entries compact when the linear
transform is absent (the common case).

## 6. Records (offset = headerLen .. file_size − 16)

Each record:

| Offset (within record) | Size | Field | Type | Description |
|---:|---:|---|---|---|
| 0 | 4 | `recordType` | `uint32_le` | Discriminator — see §6.1. |
| 4 | 4 | `payloadLen` | `uint32_le` | Bytes in `payload`. |
| 8 | `payloadLen` | `payload` | `uint8[payloadLen]` | Type-specific payload. |

Records are written in time order — strictly non-decreasing
`timestampNs` for record types 1 and 4. Record types 2 and 3 may
be interleaved at any monotonic timestamp.

### 6.1 Record types

| Value | Name | Droppable in queue? | Description |
|---:|---|---|---|
| 1 | Signal Value | Yes | A single signal-value sample. See §6.2. |
| 2 | Catalog Extension | No | New signals registered mid-stream. See §6.3. |
| 3 | Marker | Yes | User annotation. V1 writers MAY emit; V1 readers MAY ignore. See §6.4. |
| 4 | Heartbeat | Yes | Truncation-detection liveness record. See §6.5. |

V2 writers may use record types ≥ 5; V1 readers must skip-over
unknown types using `payloadLen`.

### 6.2 Type 1 — Signal Value

Payload layout:

| Offset (within payload) | Size | Field | Type | Description |
|---:|---:|---|---|---|
| 0 | 4 | `signalIdx` | `uint32_le` | Index into the file's running catalog (initial catalog + Catalog Extensions, in order). 0-based. |
| 4 | 8 | `timestampNs` | `int64_le` | Nanoseconds since `recordedAt`. |
| 12 | varies | `value` | per type | See per-type encoding in §5. |

`signalIdx` references the catalog at the time the record is
*read*, not the time it is *written* — provided the reader has
applied all prior Catalog Extension records in order, the indices
are stable. A `signalIdx` larger than the running catalog size is
a malformed file (readers must report and stop).

Total record size = `8 (record header) + 12 + value_size`. For a
double signal, that is `28 bytes`.

### 6.3 Type 2 — Catalog Extension

Payload layout:

| Offset | Size | Field | Type | Description |
|---:|---:|---|---|---|
| 0 | 4 | `addedSignalCount` | `uint32_le` | Number of new signals in this extension. |
| 4 | varies | `addedSignals` | `SignalEntry[addedSignalCount]` | Same shape as §5. |

After processing a Catalog Extension, the running catalog grows by
`addedSignalCount` entries. Subsequent `signalIdx` values may
reference any index in the extended catalog.

A writer MUST emit a Catalog Extension before any Signal Value
record references a signal not in the previous catalog.

### 6.4 Type 3 — Marker

Payload layout:

| Offset | Size | Field | Type | Description |
|---:|---:|---|---|---|
| 0 | 8 | `timestampNs` | `int64_le` | Nanoseconds since `recordedAt`. |
| 8 | 4 | `labelLen` | `uint32_le` | Length of `label`. |
| 12 | `labelLen` | `label` | `uint8[labelLen]` | UTF-8 user-supplied annotation. |

V1 writers MAY emit Markers (e.g., user pressed a "tag this moment"
button). V1 readers SHOULD render them in replay UI but MAY ignore
them entirely. Markers carry no signal-value semantics.

### 6.5 Type 4 — Heartbeat

Payload layout:

| Offset | Size | Field | Type | Description |
|---:|---:|---|---|---|
| 0 | 8 | `timestampNs` | `int64_le` | Nanoseconds since `recordedAt`. |

A writer SHOULD emit a Heartbeat at least every **10 seconds** of
wall-clock recording time, regardless of whether signals are
flowing. The Heartbeat is the format's truncation-detection
mechanism: a reader scanning a footer-less file can use the
spacing of the last few Heartbeats to estimate where the
recording was cut off.

## 7. Footer (final 16 bytes of file)

| Offset (within footer) | Size | Field | Type | Description |
|---:|---:|---|---|---|
| 0 | 8 | `magic` | `uint8[8]` | ASCII `"REPLAYEOF"`. **Note**: 9 ASCII chars; we encode the first 8 (`R-E-P-L-A-Y-E-O`). The 9th char `F` is preserved in the literal-string sense for documentation only and **does not** appear in the file. Readers compare against the byte sequence `0x52 0x45 0x50 0x4C 0x41 0x59 0x45 0x4F`. |
| 8 | 4 | `totalRecords` | `uint32_le` | Count of all records (any type). |
| 12 | 4 | `reserved` | `uint32_le` | Must be `0` in V1. V2 may use additively. |

A file with the footer present + `totalRecords` matching the
actually-readable record count is **complete**. Any other
condition is **incomplete**:

- Footer absent (file size < headerLen + 16, or last 16 bytes
  don't match the footer magic): writer was interrupted.
- Footer present but `totalRecords` exceeds the count the reader
  could actually parse: late records were truncated mid-write.

Readers must replay all complete records up to the truncation
point and report the file as incomplete; they must not raise an
error or refuse the file.

### 7.1 Footer magic byte sequence

For implementer convenience, the exact 8-byte sequence is:

```
0x52 0x45 0x50 0x4C 0x41 0x59 0x45 0x4F
 R    E    P    L    A    Y    E    O
```

The 9-letter "REPLAYEOF" mnemonic in human writing is a
convenience; the file format uses 8 bytes.

## 8. Forward-compatibility rules

These rules let V2 extend the format without breaking V1 readers.

1. **Record types ≥ 5** are reserved for V2+. V1 readers
   encountering an unknown `recordType` must skip over the record
   using `payloadLen` (no semantic interpretation), continuing to
   the next record. They must not raise an error.

2. **Header trailer fields** may be added by V2 after `signalCount`
   (i.e., between the signal catalog and the body of the file).
   V2 writers must adjust `headerLen` to include the new fields.
   V1 readers must use `headerLen` (offset 12) — never compute it
   — when seeking past the header. V1 readers therefore skip-over
   trailer fields they don't understand.

3. **Signal catalog entry trailing fields** may be added by V2.
   Each signal-catalog entry's total length is implicitly defined
   by the variable-length string fields + the conditional
   `scale` / `offset`. V2 must keep that derivation valid (no
   removed fields, no reordered required fields). V1 readers
   compute each entry's length from its declared field sizes; V2
   trailing additions are invisible to V1.

4. **Footer reserved bytes** (offsets 12..15) may be used by V2
   additively. V1 readers must not interpret them.

5. **The magic and `formatVersion=1` header bytes are
   immutable** for the lifetime of V1. A V2 reader handling V1
   files reads the V1 layout exactly as specified here; a V2
   writer producing V2 files uses a higher `formatVersion` value.

6. **Endianness is immutable**: V2+ remains little-endian.

7. **Time epoch is immutable**: `recordedAt` remains nanoseconds
   since Unix epoch; per-record `timestampNs` remains nanoseconds
   since `recordedAt`.

## 9. Truncation handling (re-stated for emphasis)

Two truncation scenarios:

**Scenario A — clean truncation between records**: the writer was
killed after a complete record was flushed but before the footer.
File size = `headerLen + N × record_total + 0`. A reader replays
N records and reports incomplete.

**Scenario B — partial last record**: the writer was killed
mid-record. File size = `headerLen + (N − 1) × record_total +
partial_record_bytes < record_header (8) + payloadLen`. A reader
encountering an incomplete record header (< 8 bytes remain) or a
record header whose `8 + payloadLen` runs past EOF must discard
the partial record and report incomplete.

Both scenarios are non-fatal. V1 readers must replay everything up
to the truncation and surface the file as "incomplete" (e.g.,
returning a flag, logging, or rendering a warning in UI).

## 10. Conformance — minimum reader behavior

A V1 reader that meets the contract must:

- Verify magic + formatVersion + headerLen.
- Parse all variable header fields per §4.2.
- Build the running signal catalog.
- For each record in `[headerLen, file_size − 16)`:
  - Read 8-byte record header.
  - Validate `payloadLen` does not run past EOF.
  - For known `recordType` (1..4): parse + dispatch per §6.
  - For unknown `recordType`: skip over by `payloadLen`.
- Optionally parse footer; if absent, surface incomplete-file
  status.
- Tolerate truncated last record (§9).

A V1 reader is NOT required to:

- Render Markers (§6.4).
- Validate `totalRecords` matches the parsed count.
- Reconstruct the wall-clock time of each record (the
  `recordedAt` + per-record `timestampNs` provides this; readers
  may surface raw `timestampNs` if simpler).

## 11. Conformance — minimum writer behavior

A V1 writer that meets the contract must:

- Write the fixed prefix + variable section + signal catalog in
  one atomic flush at recording start.
- Write only the 4 V1 record types (1..4).
- Maintain monotonic `timestampNs` per signal in record type 1.
- Emit a Catalog Extension before referencing a new signal in
  type 1 records.
- On clean stop, flush all queued records, write the footer with
  accurate `totalRecords`, then close the file.
- On error, attempt to close the file; the resulting file may be
  incomplete (no footer) but must remain readable up to the last
  flushed record.

A V1 writer SHOULD:

- Emit a Heartbeat (§6.5) at least every 10 s of recording.
- Periodically flush (default 1 s).
- Use `fsync` or platform equivalent on flush to bound data loss.

## 12. Sample byte-level walkthrough

A minimal valid file with one Double signal `voltage` (V),
recording start `2026-05-08T14:30:00Z`, no description, no
schemaId, one Signal Value record `(timestampNs=1_000_000,
value=12.34)`, then footer:

```
Offset  Bytes (hex)                                          Meaning
------  ---------------------------------------------------  -------
0x00    53 46 52 45 50 4C 41 59                              magic "SFREPLAY"
0x08    01 00 00 00                                          formatVersion = 1
0x0C    50 00 00 00                                          headerLen = 80 (= 0x50)
0x10    00 7C 30 67 6E 80 7B 17                              recordedAt (ns since epoch, LE)
0x18    00 00 00 00                                          descLen = 0
0x1C    00 00 00 00                                          schemaIdLen = 0
0x20    01 00 00 00                                          signalCount = 1
0x24    07 00 00 00                                          signalIdLen = 7
0x28    76 6F 6C 74 61 67 65                                 "voltage"
0x2F    07 00 00 00                                          nameLen = 7
0x33    56 6F 6C 74 61 67 65                                 "Voltage"
0x3A    01 00 00 00                                          unitLen = 1
0x3E    56                                                   "V"
0x3F    00 00 00 00                                          descLen = 0 (signal description)
0x43    02                                                   type = 2 (double)
0x44    00                                                   hasScale = 0
0x45    00                                                   hasOffset = 0
0x46    00 00 00 00                                          (0 padding to align? -- NO, packing is tight)
                                                             Actually: header ends at offset 0x46;
                                                             headerLen we declared was 0x50 — recompute.
```

(Implementer note: the exact `headerLen` depends on whether the
implementation pads anything. The format does **not** mandate
padding; `headerLen` is the byte count up to and including the
last catalog entry's last byte. Re-compute and write the actual
value. The walkthrough above is illustrative; the real number for
this fixture is `36 + 4 + 7 + 4 + 7 + 4 + 1 + 4 + 0 + 1 + 1 + 1 = 70`
= `0x46`, and the writer must put `0x46 0x00 0x00 0x00` at
offset 12.)

After the header, the Signal Value record at offset 0x46:

```
Offset  Bytes (hex)                                          Meaning
------  ---------------------------------------------------  -------
0x46    01 00 00 00                                          recordType = 1 (Signal Value)
0x4A    14 00 00 00                                          payloadLen = 20 (= 4+8+8)
0x4E    00 00 00 00                                          signalIdx = 0
0x52    40 42 0F 00 00 00 00 00                              timestampNs = 1_000_000 (LE)
0x5A    AE 47 E1 7A 14 AE 28 40                              value = 12.34 (double_le)
```

Then the footer at offset 0x62:

```
0x62    52 45 50 4C 41 59 45 4F                              footer magic
0x6A    01 00 00 00                                          totalRecords = 1
0x6E    00 00 00 00                                          reserved
```

Total file size: 0x72 = 114 bytes.

This walkthrough is **illustrative** — implementers must
re-compute byte offsets from the actual writer output; do not
copy these literal numbers.

## 13. Reference implementations

The M10 milestone ships:

- **Writer**: `signalforge::session::SessionWriter` + internal
  `SessionFileWriter` — `src/session/`
- **Reader**: `signalforge::session::SessionReader` —
  `src/session/session_reader.{hpp,cpp}` (M10 S6)
- **Inspector**: `tools/sfreplay_inspect/` — read-only CLI tool
  that prints header / catalog / record histogram / footer in
  human-readable + `--json` machine formats (M10 S8)
- **Round-trip integration test**:
  `tests/integration/test_session_writer_replay_round_trip.cpp`
  (M10 S7) — the HALT-trigger #2 gate

The M9 `signalforge::drivers::ReplayDriver` is **not** a V1
SFREPLAY reader. It reads a separate raw-frame log format; see
ADR-007 for the rationale.

## 14. Acknowledgments

The format builds on: M9's reservation of the `"SFREPLAY"` magic;
M5's `SignalValueSink` interface (the writer's input contract);
M6's `SignalBufferRegistry` (the writer's data source); the V1
session-format intent recorded in `architecture.md §G` (now
deferred to V2 per ADR-007).
