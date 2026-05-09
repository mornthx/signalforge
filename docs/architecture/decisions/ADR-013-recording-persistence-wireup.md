# ADR-013 — Recording catalog + connection persistence wire-up

## Status

Accepted (V1.0 release blocker fix at M14 S4 Wave 2,
2026-05-10; follows ADR-008 / ADR-009 / ADR-010 / ADR-011 in
the V1 GUI integration audit governance pattern).

## Context

The M14 S3 operator audit (`docs/m14-audit-operator-runs/run5-non-chart-audit.md`)
surfaced two unrelated-symptom-but-same-pattern Critical
findings, both of which are wire-up gaps between two
M2-M12-frozen subsystems that no per-module test exercised
end-to-end.

### F6 — Recording silently drops all signals when source was Connected pre-Record

When a driver is `Connected` *before* the user clicks Session
→ Record, the resulting `.sfreplay` file is structurally
valid (header + footer ~ 52 B) but has zero Type-1 (Signal
Value) records. 100% data loss, no error reported.

`SessionWriter` subscribes to `TeeSink` (per ADR-007 fan-out)
to receive `onSignalsRegistered` events that populate
`metadata_.signalCatalog` for the file's initial catalog.
`SessionWriter::onSignalsRegistered` is documented to always
cache (whether recording or not), and `SessionWriter::onSignal`
early-returns when not recording.

The natural intent is: SessionWriter is *always* subscribed
to TeeSink, and recording state just gates whether onSignal
events become file events. But `MainWindow::onRecordToggle`
subscribes SessionWriter only at start time:

```cpp
sessionWriter_->start(path);                     // ① writes header
                                                  //   with empty catalog
teeSink_->addSink(sessionWriter_.get());          // ② only NOW subscribes
```

TeeSink doesn't replay history. If the SchemaDecoder for the
existing connection has already fired `onSignalsRegistered`
to TeeSink subscribers (chiefly `SignalBufferRegistry`), the
SessionWriter — which is subscribed second — will never see
it. The writer's `onSignal()` continues to be called for
in-flight values, but `fileWriter_->enqueue()` rejects them
because the catalog index doesn't know the signal IDs →
silent drop.

### F17 — Connection persistence is completely broken

`~/.config/signalforge/connections.yaml` is **never written**,
regardless of exit path. `ConnectionManager::addConnection`,
`editConnection`, `removeConnection` all call
`autoSave()`, which calls `saveConfigFile(configPath_)` IF
`configPath_` is non-empty. But on first launch the YAML
file doesn't exist, so:

```cpp
bool ConnectionManager::loadConfigFile(const QString& path) {
    std::ifstream in(path.toStdString());
    if (!in.is_open()) {
        SF_LOG_INFO("…file not found at {}", path.toStdString());
        return false;                  // configPath_ left empty
    }
    // ... only on success path:
    configPath_ = path;                // ← only set here
    return true;
}
```

`configPath_` stays empty forever. All subsequent
`autoSave()` calls return early. M9 §Test 5 (verify
`connections.yaml` reflects mutations) and §Test 6
(auto-connect from yaml on app start) are completely
unreachable.

### Pattern

Both findings follow the same multi-milestone governance
gap that ADR-008 / ADR-009 / ADR-010 / ADR-011 documented:

- M9 spec: Connection lifecycle and YAML persistence
  contracts. Per-module unit tests verified
  `loadConfigFile` and `saveConfigFile` round-trips, but
  the *caller-driven autosave* path was never end-to-end
  tested with a missing file.
- M10 spec: Recording lifecycle. Per-module unit tests
  verified SessionWriter subscriber semantics in
  isolation, but the MainWindow Connect → Record flow
  (where onSignalsRegistered fires *before* the writer
  subscribes) was never end-to-end tested.
- M14 §3.2 audit caught both because it ran the GUI through
  the full operator path.

## Decision

Two minimal fixes, both in non-frozen files.

### F6 fix — `MainWindow` always-subscribes SessionWriter to TeeSink

```cpp
// src/app/main_window.cpp — MainWindow ctor (after sessionWriter_
// construction):
teeSink_->addSink(sessionWriter_.get());
```

`onRecordToggle` no longer manages addSink / removeSink. The
SessionWriter stays subscribed for the MainWindow lifetime;
recording state still gates whether `onSignal` events become
file writes (existing early-return in
`SessionWriter::onSignal`). `onSignalsRegistered` always
caches into `metadata_.signalCatalog` (existing behavior),
so the natural Connect → Record flow now seeds the catalog
correctly.

### F17 fix — `ConnectionManager::loadConfigFile` sets configPath_ unconditionally

```cpp
// src/connection/connection_manager.cpp:
bool ConnectionManager::loadConfigFile(const QString& path) {
    // Bootstrap the autosave target even on first-launch /
    // missing-file / parse-error paths, so subsequent
    // mutations write to the right file.
    configPath_ = path;
    // … existing reset-to-empty + early-return paths
}
```

One-line fix. All early-return paths now leave `configPath_`
populated; subsequent `addConnection` / `editConnection` /
`removeConnection` mutations write to disk through the
already-existing `autoSave()` plumbing.

### Defense-in-depth — `MainWindow::aboutToQuit` save

`MainWindow` connects `QApplication::aboutToQuit` to a
saving lambda, calling `connectionManager_->saveConfigFile(
ConnectionManager::defaultConfigPath())`. Belt-and-suspenders
in case any future mutation path bypasses `autoSave()` or
the user crashes before `editConnection()` returns.

This keeps **all changes in non-frozen files**:

- `src/app/main_window.cpp` — not frozen.
- `src/connection/connection_manager.cpp` — not frozen
  (`.hpp` is frozen at M9; only the .cpp implementation
  detail is touched).
- `src/session/session_writer.{hpp,cpp}` — not modified
  (existing `onSignal` / `onSignalsRegistered` semantics
  are sufficient).

## Rationale

### F6 alternative considered: seed metadata_.signalCatalog from registry at start()

Audit recommendation §F6 suggested an alternative: at
`SessionWriter::start()`, walk the registry pointer (which
the writer has retained but doesn't use) and seed the catalog
from registered signals.

**Rejected because**:

- Modifies SessionWriter's behavior in a way that overlaps
  with the always-cache path (duplicate catalog entries
  if the writer is also subscribed normally).
- Couples SessionWriter to the buffer registry's signal-
  enumeration API, which is incidental at V1.
- The always-subscribe approach matches SessionWriter's
  documented design (sink overrides cache always; state
  gates onSignal). The bug was MainWindow not honoring
  that contract.

### F17 alternative considered: hooks per-mutation in MainWindow

Subscribe `MainWindow` to `connectionAdded` /
`connectionRemoved` / `connectionStateChanged` signals and
call `saveConfigFile` from each handler.

**Rejected because**:

- ConnectionManager already has `autoSave()` plumbing
  internally; bypassing it duplicates logic.
- The actual bug is one missing assignment (`configPath_ =
  path` on the early-return path); fixing it at the
  source is simpler and removes the "why is autoSave a
  no-op" mystery.

The defense-in-depth `aboutToQuit` save is added because
it's nearly free and handles any edge case where the
mutation path is bypassed (rare but possible for future
features).

## Consequences

- **F6 resolved**: Connect → Record now produces a file with
  Type-1 records. M10 §Test 1 / §Test 2 / §Test 5 acceptance
  paths unblocked.
- **F17 resolved**: `connections.yaml` is written on every
  mutation + on app exit. M9 §Test 5 / §Test 6 (auto-connect)
  unblocked.
- **No frozen-`.hpp` modification.** M2-M13 freeze surface +
  ADR-008/009/010/011 history intact. Frozen-surface counter
  unchanged at 0/2 (well under HALT #5 threshold).
- **18-test acceptance projection** (per
  `docs/m14-gui-audit-report.md`): T6 / T7 / T8 advance from
  ✗ to ✓ after operator validation; running tally moves from
  8/18 (post-Wave-1) to 11/18 (post-Wave-2).

## V1.0 governance lessons (combined with ADR-008…012)

The same multi-milestone gap pattern recurred five times in
the M13/M14 release-prereq cycle. ADR-013 covers two of them
(F6 + F17). All five share:

1. Per-module unit tests verify single-subsystem semantics
   in isolation.
2. End-to-end GUI integration was never wired into a CI
   gate until M14 S1.
3. Operator dogfood (the 18-test HW verification) is the
   last-line gate that catches the wire-up gaps.

The M14 S1 CI smoke test now catches scene-graph + qrc +
chart-sizing regressions automatically. The audit
recommendation calls for two more smoke extensions:

- **Persistence smoke** (post-F17 fix): adds a connection,
  exits cleanly, asserts `connections.yaml` exists with the
  expected entry.
- **Recording smoke** (post-F6 fix): connects a UDP source,
  starts recording, drives frames, stops, asserts the
  resulting `.sfreplay` has ≥ 1 Type-1 record.

Both deferred to a follow-up commit so the operator can
validate F6/F17 fixes empirically first; the smoke
extensions then lock in the regression coverage.

## Cross-references

- ADR-008 / ADR-009 / ADR-010 / ADR-011 / ADR-012 (V1
  release-prereq governance pattern; ADR-012 reserved for
  any production fix needed if F4 had turned out
  architectural — Wave 1 closed Path α with no ADR-012)
- M9 spec / M10 spec / M11 spec
- `docs/m14-audit-operator-runs/run5-non-chart-audit.md` §F6
  and §F17 (operator findings)
- `docs/m14-gui-audit-report.md` (consolidated audit report;
  18-test projection table)
- `.claude/M14-progress.md` §"Audit findings" (running
  state + counters)
