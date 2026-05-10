# ADR-008 — DecoderRegistrar additive `setSchemaForDriverType` API

## Status

Accepted (V1.0 release blocker fix at M13 S7, 2026-05-09).

## Context

M5 spec §4.6 explicitly stated:

> M9 Connection Manager replaces this [hard-coded
> `driverTypeToSchemaPath` map] with per-connection user
> selection.

M9 implementation:

- Added the `decoderSchemaId` field to `ConnectionConfig`
  (`src/connection/connection.hpp:82`).
- Implemented the dialog UI for picking a schema id at
  add-connection / edit-connection time
  (`src/connection/connection_dialog.{hpp,cpp}`).
- Persisted `decoderSchemaId` to and loaded from
  `~/.config/signalforge/connections.yaml`.

But M9 did **not** wire the user's selection back to
`DecoderRegistrar` at runtime.

`MainWindow` constructs `DecoderRegistrar` with an **empty**
`driverTypeToSchemaPath` map at startup
(`src/app/main_window.cpp:62-65`); `ConnectionManager` has
no path to update the map when connections are loaded,
added, or modified.

**Consequence**: V1.0 .deb live mode does not decode any
frames. Every `SchemaDecoder` pipeline attaches with no
schema configured. The connection list shows Connected
state, but no signals decode → no chart populates.

The gap was caught in M13 18-test combined hardware
verification (spec §3.5 V release prerequisite). Tests T1
(Serial), T2 (TCP), T3 (UDP) all failed with the same
symptom: connection succeeds, no signals appear in charts.

V1.0 cannot ship without this fix.

## Decision

Add an additive public method to the M5-frozen
`DecoderRegistrar` interface:

```cpp
/// Update the schema path for a given driver type at runtime.
/// Affects pipelines attached AFTER this call; existing
/// decoders for this type are NOT re-keyed.
/// Empty schemaPath removes the mapping (matches "no
/// decoder" semantics from the M5 ctor map).
/// Thread-safe.
void setSchemaForDriverType(const QString& driverType,
                            const QString& schemaPath);
```

The implementation:

- Acquires the existing `decoderMutex_`.
- Updates `driverTypeToSchemaPath_` (the same internal map
  the M5 ctor populated).
- Returns. **No reactive re-keying** of decoders that have
  already attached for that type.

`ConnectionManager` invokes this method:

- At `loadConfigFile` (once per loaded connection)
- At `addConnection` (once)
- At `editConnection` (once, with the new schema)
- At `removeConnection` (once, with empty schemaPath)

`MainWindow` retains the empty-map `DecoderRegistrar`
constructor call. The runtime population is now driven by
`ConnectionManager` events.

## Rationale

Three alternatives were considered.

### Rejected: Option B — reorder construction in MainWindow

Pre-load the connections YAML before constructing
`DecoderRegistrar`, build a static map at startup. Pass to
the existing ctor.

**Rejected because**: this fixes startup-time loading but
does not handle runtime additions / edits — user would
have to restart SignalForge after adding a new connection.
This is a regression vs the M9 dialog UX intent.

### Rejected: Option C — DecoderRegistrar replacement

Re-design schema selection (e.g., per-connection-id keying
instead of per-driver-type keying) to fix the multi-conn-
same-type edge case as well.

**Rejected because**: V1.5+ scope. V1.0 ship is the
priority; the additive method is the smallest possible
change that unblocks live mode.

### Rejected: Option D — bypass DecoderRegistrar

Construct `SchemaDecoder` instances directly in
`MainWindow`'s `pipelineAttached` slot, bypassing
`DecoderRegistrar`.

**Rejected because**: violates M5 §4.6 architectural intent;
duplicates registrar logic; makes the registrar's existing
code path effectively dead. Also forks the test surface.

### Accepted: Option A — additive method

The accepted decision. Smallest change. Preserves all M5
ctor map behavior. Honors M5 §4.6's "M9 replaces this
[at runtime]" intent. Wires through M9's
`decoderSchemaId` field without re-architecting either
M5 or M9.

## Known limitations (V1.0 ship-as-is)

- The map is **per-driver-type**, not per-connection. If
  the user has two `udp` connections with different
  `decoderSchemaId`s, the registrar will know about
  whichever one was added/edited last. Earlier connections
  that already attached keep their original decoder; new
  pipelines (e.g., on reconnect) use the latest schema.
- `removeConnection` sets the schema to empty for that
  driver type. If the user has multiple connections of the
  same type and removes one, the others lose their
  registrar entry. They'll re-attach correctly on next
  re-add or edit, but mid-Connected state may not refresh
  signals after a same-type peer is removed.

These edge cases are V1.0 acceptable because:

- The M9 dialog encourages 1-connection-per-type usage
  (the typical embedded-device-bring-up workflow).
- Multi-connection-same-type users can use the edit
  dialog to refresh the map.
- A V1.5+ redesign is the appropriate scope for full
  per-connection keying.

These limitations are documented in
`docs/release-notes/v1.0.0.md` "Known limitations".

## Consequences

- **M5-frozen `decoder_registrar.hpp` gains one additive
  public method.** This is V1.0's first frozen-`.hpp`
  modification. All other M2-M12 freeze surface remains
  intact.
- The V1.0 freeze record in `docs/v1.0-spec-list.md` §1
  must be updated with the new sha256 for
  `decoder_registrar.hpp`. The other 25 frozen `.hpp`
  sha256s and the SFREPLAY v1 format spec sha256 remain
  unchanged.
- M9 spec §4 / `decoderSchemaId` field semantics are now
  **functionally connected** to runtime decoder behavior.
  M9-done.md §211's documented gap is closed.
- A new integration test
  (`tests/integration/test_v1_live_mode_pipeline.cpp`)
  verifies the wire-up end-to-end: connections.yaml
  load → driver attach → SchemaDecoder produces
  SignalValues.
- This ADR documents the lesson that release-prerequisite
  hardware verification (M13 §3.5 V) is the gate-of-
  last-resort for catching multi-milestone governance gaps.
  V1.5+ governance improvements proposed in the M13
  done.md governance-lesson section.

## Cross-references

- M5 spec §4.6 (deferral statement that was unfulfilled)
- M9 spec §4 (`decoderSchemaId` design)
- M9-done.md §"Inherited concerns" (gap admission)
- M13 18-test verification protocol §M9 Tests 1-3 (gate
  that caught the bug)
- M13 spec §3.5 V (release prerequisite blocking)
- M12 spec §3.5 N (frozen-surface ADR-008 trigger)
- `docs/v1.0-spec-list.md` §1 (sha256 updated for
  `decoder_registrar.hpp` after this ADR's
  implementation)
- `docs/release-notes/v1.0.0.md` "Known limitations"
  (multi-conn-same-type edge case noted)
