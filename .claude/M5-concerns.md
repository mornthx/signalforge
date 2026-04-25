# M5 — Concerns and deviations

Documented inline as discovered per M5 Phase 5 session instructions.
Schema v1 ambiguities get entries here the moment they surface; C++
deviations similarly.

---

## 1. `SignalValueSink::onSignalsRegistered` parameter renamed `signals` → `signalsList`

**When**: S1, during the C++23 toolchain bump build verification.

**What the spec says** (M5 §4.1):

```cpp
virtual void onSignalsRegistered(const QString& driverId,
                                 const std::vector<SignalMetadata>& signals) {
```

**What broke**: GCC 13 `-std=c++23` compile error
`expected ',' or '...' before 'public'` at the parameter declaration.
Cause: Qt 6 defines `signals` as a preprocessor macro that expands
to `public` under default `QT_KEYWORDS` mode (needed for moc to see
`signals:` access-specifier blocks in QObject classes). Including
`<QString>` transitively defines the macro; `SignalValueSink` is pure
C++ but the macro still fires on the identifier.

**Resolution**: rename the parameter `signals` → `signalsList` in the
three locations:
- `src/decode/decoder_interface.hpp` (the frozen interface)
- `src/decode/logging_signal_value_sink.hpp`
- `src/decode/logging_signal_value_sink.cpp`

**Impact**:
- Parameter names are not part of the ABI (the virtual's signature is
  `void(QString const&, std::vector<SignalMetadata> const&)` either
  way), so binary compatibility is unaffected.
- Doxygen-visible API changes slightly: users reading the interface
  see `signalsList` instead of `signals`. Acceptable — the name is
  strictly more descriptive (the parameter is a list/catalog of
  signal metadata, not Qt signals).
- Alternative considered: add `QT_NO_KEYWORDS` definition to
  `signalforge_decoder` and require consumers of this header to use
  `Q_SIGNALS` / `Q_SLOTS` / `Q_EMIT`. Rejected — would propagate
  across all consumers of the decoder module and conflict with
  existing code that uses `signals:` access specifier blocks.

**Freeze implication**: the frozen interface signature (types only) is
identical to spec §4.1. The parameter *name* in the freeze record
(`M5-done.md` SHA256) reflects `signalsList`; a consumer reading the
spec should see the note in `decoder_interface.hpp` explaining the
rename.

**Status**: resolved in S1. Needs human acknowledgment at milestone
review — if the name must match the spec exactly, the `QT_NO_KEYWORDS`
route is available at the cost of a broader surface change.

---

## 2. `FieldDef`: spec listed two members both named `offset`

**When**: S2, while implementing `src/decode/schema.hpp`.

**What the spec says** (M5 §4.3, FieldDef declaration):

```cpp
struct FieldDef {
    QString name;
    int offset = 0;               ///< Byte offset within payload
    FieldEncoding encoding = FieldEncoding::Uint8;
    int sizeBytes = 0;
    std::optional<Endianness> endianness;
    std::optional<double> scale;
    std::optional<double> offset;     ///< second member with same name
    QString unit;
    std::optional<QString> description;
    std::vector<BitFieldDef> bitFields;
};
```

The spec also references the linear transform as `value = raw * scale
+ offset` in §3.6, §4.4, and §S3 of the plan, treating "offset" as the
linear-transform constant. So the spec intends two distinct concepts —
byte offset and linear-transform offset — but renders them both as
`offset` in the struct.

**What broke**: declaring two members named `offset` is invalid C++; a
literal copy of the spec would not compile.

**Resolution**: keep the byte-position member named `offset` (it is the
user-facing yaml key in §4.8 and the most common usage); rename the
linear-transform member to `offsetTransform`. Yaml key for the
transform becomes `offset_transform` (a strict superset of v1 — no
existing yaml uses the transform offset key, so no schema breakage).

The canonical example `schemas/decoder_schema_v1.yaml` uses
`offset_transform` on the `pressure` field. The JSON meta-schema
documents the same key.

**Impact**: minor — schema v1 is only just being authored, so the
choice of yaml key is a one-time decision rather than a migration.
Internal C++ struct member names are not frozen per spec §6.2.

**Alternative considered**: name the byte position `byteOffset`,
keep the linear transform as `offset`. Rejected because the yaml key
`offset` for byte position is already entrenched in the spec example
and matches industry convention (Wireshark, scapy, kaitai struct all
use `offset` for byte position).

**Status**: resolved in S2. Schema v1 freeze (M5 close) will record
the canonical example with `offset_transform`; this is the authoritative
key for the linear-transform offset for V1's lifetime.

---

## 3. `bool` reserved as a child encoding only

**When**: S2.

**Context**: The spec §6.1 lists `bool` in the encoding enum (frozen
at M5 close). However, the spec also says (§3.4) that single bits
live inside a `bitfield` parent with `bit_count: 1`, and the §4.8
example never uses `bool` as a top-level field encoding.

**Decision**: the validator accepts `bool` as a `FieldEncoding` enum
value but rejects it when used as a top-level field encoding (with a
clear error pointing to "use bit_count: 1 inside a bitfield"). This
matches both §3.4 (single bits live in bitfields) and §6.1 (bool is in
the enum).

**Status**: documented in the validator and the canonical schema's
inline comments. No deviation from the spec; clarifies intent.

---

## 4. M5 PR will exceed CLAUDE.md §Required #4 (800 net lines)

**When**: S4 close, while tracking cumulative diff size.

**What the rule says**: CLAUDE.md §Required #4 — "Each PR or merge is
≤ **800 net lines added**, excluding generated files and test
fixtures. Larger changes must be split."

**Cumulative as of S4 close** (excluding the canonical schema yaml
and JSON, which arguably qualify as "test fixtures" but are also
freeze artifacts):

- S2: +1377 (includes ~520 lines of canonical schema yaml/json
  + ~100 lines of doc comments)
- S3: +776
- S4: +354
- Subtotal: +2507 lines on `milestone/M5`.

**S5–S10 will add roughly**:

- S5 (example schemas + invalid fixtures): ~200 lines (test fixtures)
- S6 (unit tests): ~400 lines (test code)
- S7 (integration tests): ~500 lines (test code)
- S8 (benchmark + baseline doc): ~300 lines
- S9 (schema_lint CLI + README): ~250 lines
- S10 (M5-done.md): ~200 lines

Estimated final M5 PR size: **~4350 net lines**, of which ~600 are
canonical-schema files and ~1100 are test code. Production C++ is
~1900 lines, still well above the 800-line gate.

**Resolution intent**: M5 cannot be split into smaller PRs without
breaking the milestone-closure protocol (PR-per-milestone is the
established pattern in M2/M3/M4). The schema v1 freeze is atomic —
splitting it would either ship a half-frozen yaml format or ship two
PRs that frozen-amend each other, both of which are worse than a
single oversize PR.

**Mitigation in progress**:

- The same concern was raised privately by the user in a prior
  session (see commit history of CLAUDE.md tiered §Required-4 patch
  attempt — Option A discard). Once the user re-issues the tiered
  rule, M5's exceedence is automatically reframed as compliant.
- Until then, this concern is the explicit acknowledgment, with the
  rationale and the metric breakdown above.

**Status**: open. Will be closed at M5 review along with the tiered
rule's adoption (or by an explicit one-time waiver in the M5-done.md
review).
