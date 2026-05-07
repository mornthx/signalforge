# M10 — Progress log

Per CLAUDE.md §Required #2 + plan §0, each subtask logs start +
close entries with build / test / format counts and any
deviations.

---

## Pre-S0 — M10 understanding + plan (completed)

- Start: 2026-05-08T00:00Z
- Close: 2026-05-08T00:30Z
- Commit: `5f8ad55` "chore: record M10 understanding and plan"
- CI: run pending — push CI fires on every commit per CLAUDE.md
  §Required #2.
- Deliverables: `.claude/M10-understanding.md` (286 lines, 3
  concerns C1 / C2 / C3 surfaced); `.claude/M10-plan.md` (452
  lines, S0-S11 sequenced).

---

## S0 — ADR-007 + M10-concerns.md (completed)

- Start: 2026-05-08T00:30Z

### Deliverables

- `docs/architecture/decisions/ADR-007-sfreplay-v1-format-pivot.md`:
  Accepted ADR resolving C1 (round-trip path) + C2 (arch.md §G
  divergence). 5-point rationale; explicit consequences section;
  spec amendments table; arch.md acknowledged out-of-date pending
  next arch refresh.
- `.claude/M10-concerns.md`: C1 / C2 / C3 with full reconciliation
  detail. C3 backpressure refinement 4-point policy spelled out
  for S5 implementation.

### Phase 4 decisions captured

- C1: **β** (new `SessionReader` at `src/session/session_reader.{hpp,cpp}`)
- C2: ADR-007 V1 format pivot accepted
- C3: 4-point queue policy approved

### Build / test counts

- Docs-only commit. CLAUDE.md §Required #2 exception applies
  (build graph unaffected). clang-format: not applicable.

### Deviations from plan

- Plan §S0 anticipated authoring ADR-007 with the structure
  given in `M10-understanding.md §5.C2` + plan §S0. The Phase 4
  approval message specified additional ADR sections (Status,
  Context, Decision, Rationale, Consequences, Cross-references)
  and 5 specific rationale points. CC followed the Phase 4
  spec exactly.
- Spec text amendments are recorded in ADR-007 §Consequences
  table (per Phase 4 instruction "CC does NOT modify the M10
  spec file; ADR-007 is the canonical record"). The spec file
  itself is unchanged.

S0 commit: `fc985e5` "docs: M10 S0 — ADR-007 (SFREPLAY v1
format pivot) + concerns".

---

## S1 — Module scaffolding + freeze-surface headers (completed)

- Start: 2026-05-08T00:50Z

### Deliverables

- `src/session/CMakeLists.txt`: `signalforge_session` static
  library now builds the real M10 sources. PUBLIC links:
  `Qt6::Core`, `signalforge_buffer`, `signalforge_decoder`.
  PRIVATE: `signalforge_observability`. AUTOMOC ON. The M0
  `placeholder.{cpp,hpp}` bootstrap files are `git rm`-ed.
- `src/session/session_metadata.hpp` (frozen at M10 close):
  `RecordingState` enum (Idle / Recording / Error) +
  `SessionMetadata` struct (`recordedAt`, `recordingStart`,
  `recordingEnd`, `description`, `decoderSchemaId`,
  `signalCatalog`). Per spec §3.5 the struct deliberately
  omits driver type / display name (notes the §2.1-4 wording
  superseded by §3.5 in a header comment).
- `src/session/session_writer.hpp` (frozen at M10 close):
  `SessionWriter : QObject, SignalValueSink`. Public API per
  spec §4.2 — `start()` / `stop()` / `isRecording()` /
  `state()` / `currentFilePath()` / `metadata()` / 3 atomic
  counters. Qt signals: `recordingStarted` / `recordingStopped`
  / `errorOccurred` / `flushed`. `Q_DISABLE_COPY_MOVE`.
  Doxygen on every public declaration; the class-level
  comment cross-references ADR-007 + the C3 4-point
  backpressure policy.
- `src/session/session_file_writer.hpp` (NOT frozen — internal
  worker class per spec §6.2): `WriteSignalEvent` /
  `CatalogExtensionEvent` / `StopEvent` queue events +
  `SessionEvent = std::variant<...>`; `SessionFileWriter`
  worker class with `openFile()` / `enqueue()` /
  `processQueue()` / counters. Internal constants for queue
  capacity (10000), flush interval (1 s), non-droppable block
  timeout (10 ms) are in the header so the unit tests can
  reference them later (S5).
- `src/session/session_writer.cpp` + `session_file_writer.cpp`:
  empty-stub implementations that compile. Each method body
  comments the future S subtask that fills it in (S3 lifecycle,
  S4 encoder, S5 backpressure).

### Build / test counts

- Debug + Release + debug-asan all build clean (only
  `signalforge_session` rebuilt; no other modules touched).
- ctest: Debug 506/506 pass; Release 506/506 pass. (Unchanged
  from M9 close — S1 adds no test code yet.)
- `clang-format --dry-run -Werror` clean on all 5 new / changed
  session files (one violation found by initial dry-run, fixed
  via `clang-format -i` and re-verified).

### Deviations from plan

- Plan §S1 anticipated authoring placeholder `.cpp` stubs
  alongside the freeze-headers. M0 had already laid down a
  `placeholder.{cpp,hpp}` skeleton in `src/session/` that
  would have shadowed the real M10 sources. Decision:
  `git rm` the M0 placeholders rather than carrying them as
  "removed code" comments (per CLAUDE.md §Anti-patterns). No
  net source-tree footprint cost — the placeholder was a 35
  byte `.cpp` and 95 byte `.hpp`, neither referenced
  anywhere outside its own CMakeLists.txt.

S1 commit: `5f3bc6b` "session: scaffold M10 module + freeze-surface
headers (S1)". Pushed after S0 CI green (run 25515271059 ✓).
S1 CI: run 25515817537 (in_progress at S2 close).

---

## S2 — Canonical SFREPLAY v1 format spec (completed)

- Start: 2026-05-08T01:00Z

### Deliverables

- `docs/format/sfreplay-v1.md` (495 lines): canonical, frozen-at-
  M10-close binary format spec. Sections: scope (§1), endianness
  + integer encoding + time (§2), file layout (§3), header (§4
  with fixed prefix + variable section + worked example), signal
  catalog entry (§5 with type table), records (§6 with all 4
  record types: Signal Value, Catalog Extension, Marker,
  Heartbeat — and droppability annotated for S5 backpressure
  cross-ref), footer (§7 with exact byte sequence for the
  truncated-magic compatibility note), forward-compat rules (§8),
  truncation handling (§9), reader conformance (§10), writer
  conformance (§11), byte-level walkthrough (§12), reference
  impls + acknowledgments (§13-14).
- sha256: `20cae91f3f8702538b1d79c719673af9b815e542816b52b867ffad0a87d59c92`
  → recorded for the M10-done.md freeze record (S11).

### Build / test counts

- Docs-only commit; CLAUDE.md §Required #2 exception applies.
- No build, no ctest, no clang-format implication.

### Deviations from plan

- Plan §S2 anticipated 5 h of authoring effort; took ~30 min via
  in-context drafting from spec §4.1. The "no TBD anywhere"
  HALT trigger from plan §S2 is met — every field has size,
  type, and meaning specified explicitly. The byte-level
  walkthrough §12 is illustrative only (re-compute from real
  writer output); a foot-note flags this so implementers don't
  copy literal numbers.
- Spec author footnote on the footer magic: spelling
  "REPLAYEOF" is 9 ASCII chars but the file format only stores
  the first 8 bytes (`R E P L A Y E O`). The format spec
  documents this explicitly so readers don't accidentally
  search for a 9-byte sequence. Captured in §7 + §7.1 with
  the exact hex bytes.

S2 commit: pending push (gated by S1 CI green).

