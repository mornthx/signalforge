# M11 — Concerns

Six concerns surfaced during Phase 4 understanding. Each carries an
implementation-level resolution; **none currently requires
ADR-008**. The C1 resolution drives the most work in Phase 5
(SessionReader streaming extension at S2).

If S10 perf measurement breaks the H4 watermark
(seek > 500 ms on a 600 k-record file), this file is amended with
an ADR-008 stub at that point per plan §S0 conditional path.

---

## C1 — M10 `SessionReader` API gap (load-bearing)

### Statement

The M11 spec §4.4 worker-loop sketch and §4.5 seek sketch reference
the following methods on `signalforge::session::SessionReader`:

- `reader_->readNext()` — returns an opaque per-record struct with
  `timestampNs`, `signalId`, `value` fields.
- `reader_->seekToTimestamp(int64_t ns)` — re-positions to a
  target timestamp.
- `reader_->currentRecordIndex()` — returns the current record
  ordinal.

**None of these exist** in the actual M10 surface
(`src/session/session_reader.hpp` at the M10 close commit). The
real API is:

```cpp
[[nodiscard]] bool open(const QString& filePath);
void close();
[[nodiscard]] bool isOpen() const noexcept;
[[nodiscard]] SessionMetadata metadata() const;
[[nodiscard]] bool replayAll(SignalValueSink& sink);  // fire-and-forget
[[nodiscard]] std::size_t recordsRead() const noexcept;
[[nodiscard]] bool fileComplete() const noexcept;
```

The spec acknowledges the gap implicitly in §4.5 ("M10
SessionReader may need a `seekToTimestamp(ns)` method") and
explicitly in §9 Notes #1 ("you can either: 1. Add it to M10's
frozen API → file as M11 concern + ADR; 2. Build on top via
sequential read → no M10 change").

### Critical authority context (verified at plan time)

`session_reader.hpp` lines 33-36 read verbatim:

> Not part of the M10 freeze surface (per spec §6.2 only the
> writer-side plus the format spec are frozen). M11 may extend the
> public API additively.

`.claude/M10-done.md` §Freezes lists exactly three frozen items:

| File | Frozen at M10 close |
|---|---|
| `docs/format/sfreplay-v1.md` | yes |
| `src/session/session_writer.hpp` | yes |
| `src/session/session_metadata.hpp` | yes |

`SessionReader` is **not** on this list. M10's spec §6.2 explicitly
anticipated M11's needs.

### Resolutions considered

#### α — Extend SessionReader additively (default)

Add the streaming + seek surface to `SessionReader` directly.
Because the class is *outside* M10's freeze, this is **not** an
ADR-requiring change. Concretely:

- New public struct:
  ```cpp
  struct ReplayRecord {
      std::int64_t timestampNs;
      QString      signalId;
      signalforge::decoder::SignalValue value;
  };
  ```
- New public methods:
  ```cpp
  [[nodiscard]] bool readNextRecord(ReplayRecord& out);
  [[nodiscard]] bool seekToTimestamp(std::int64_t targetNs);
  [[nodiscard]] std::size_t currentRecordIndex() const noexcept;
  [[nodiscard]] std::int64_t currentTimestampNs() const noexcept;
  [[nodiscard]] bool atEnd() const noexcept;
  void bindCatalogSink(signalforge::decoder::SignalValueSink* sink);
  ```
- `replayAll(sink)` is reimplemented as a thin loop over
  `readNextRecord` + the bound catalog-sink fan-out (so M10's
  round-trip test in `tests/unit/session/session_reader_test.cpp`
  passes unchanged — automatic regression detector).
- `seekToTimestamp` performs sequential scan from the current
  position forward; for backward seek it re-opens the file and
  scans from the post-header offset. Catalog-extension records
  encountered during the scan are dispatched to the bound catalog
  sink so listeners see catalog state at the seek target.

Code cost: ~400 LOC additive on `session_reader.{hpp,cpp}`. M10
freeze is preserved.

#### β — Parallel reader in `src/replay/`

Re-implement SFREPLAY v1 parsing inside a new
`signalforge::replay::ReplayFileReader` class. SessionReader stays
untouched. Adds ~600 LOC of parsing duplicated against
`docs/format/sfreplay-v1.md`. High drift risk; loses the
`sfreplay_inspect` / SessionReader / future readers single-source
discipline.

### Decision

**Adopt α** at S2 implementation time. β is reserved as a fallback
only if α perf escapes the H4 < 500 ms seek gate at S10 *and*
indexed-seek (an additional α extension) also fails. β is **not**
the planned path.

### S10 escalation path

If H4 fires at S10, append an ADR-008 stub to this file proposing
indexed seek as an M10 hotfix (parallel pattern to M6 ADR-005's
post-freeze indexed-seek extension). The ADR would freeze a new
`SessionWriter` field — the index footer — and would be mergeable
to V1 via ADR amendment. Not anticipated; flagged for completeness.

---

## C2 — Spec include-path typo

### Statement

Spec §4.2 shows:

```cpp
#include "decoder/decoder_interface.hpp"  // M5 frozen for SignalValueSink
```

The actual path under `src/` is `decode/decoder_interface.hpp`
(directory is **decode**, singular). Sibling includes in M9 / M10
already use the singular form (e.g.,
`src/session/session_reader.hpp:4` uses `"decode/decoder_interface.hpp"`).

### Resolution

Implementation-level correction in S1 freeze headers; spec is **not**
amended (this is a sketch artefact, not a spec contract).

### Authority

CLAUDE.md §Disagreement permits "execute the spec as written" in
spirit while correcting trivially incorrect identifier text;
`.clang-tidy` would fail on a non-existent include path, so the
spec-literal version cannot compile. Documented here for the
record.

---

## C3 — M9 `ConnectionManager` lacks pause/resume

### Statement

Spec §3.4 / §4.8 describe "pause M9 connections" semantics for
Live → Replay transitions. M9's actual public surface
(`src/connection/connection_manager.hpp`, frozen at M9 close):

- `[[nodiscard]] bool connectConnection(const QString& id);`
- `[[nodiscard]] bool disconnectConnection(const QString& id);`
- `void connectAll();`
- `void disconnectAll();`
- `[[nodiscard]] QStringList connectionIds() const;`
- `[[nodiscard]] Connection* connection(const QString& id) const;`

There is **no** `pauseConnection` / `pauseAll` / `resumeAll`. Adding
one would modify a frozen interface (M9 close — see M9-done.md
§Freezes) and trigger H1.

### Resolution

`ReplayModeManager` synthesises pause/resume:

- `enterReplay()`:
  1. iterate `connectionIds()`
  2. for each whose `state() == Connection::State::Connected`,
     append id to `previouslyConnectedIds_`
  3. for each previously-connected id, call
     `disconnectConnection(id)`
  4. emit `connectionsPaused()`
- `exitReplay()` (Resume branch only):
  1. iterate stored `previouslyConnectedIds_`
  2. for each, call `connectConnection(id)`
  3. emit `connectionsRestored()`

This is **functionally equivalent** to pause/resume from the
user's point of view: the connection's runtime state machine is
the same as if the user had clicked Disconnect / Connect manually.
No M9 change; H1 stays clear.

### V1.5+ note

If real-world UX shows the connect-from-disconnected cycle is too
slow (e.g., re-handshake costs visible in the Live ↔ Replay
transition latency gate), a true pause API on M9 is V1.5+ work.
Out of M11 scope.

---

## C4 — `std::this_thread::sleep_for` precision risk at 10×

### Statement

Spec §4.4 worker loop uses:

```cpp
std::this_thread::sleep_for(scaledDelta);
```

On Linux this maps to `nanosleep` which has typical scheduler
granularity ~50 µs (worse under load — CFS time-slice quanta).
At 1× speed with records spaced ≥ 1 ms apart, drift is well within
the spec §5.1 < 5 % gate. At 10× speed where records arrive
sub-100 µs apart in the file, drift may become visible (10× gate
is "10s file completes in 1s ± 10 %").

### Resolution

Two-stage policy at S5 / S10:

1. **Stage A — `sleep_for` baseline**: implement S4 + S5 worker
   with `sleep_for`. Bench at S10 measures 1× drift over 10 s, 10×
   completion time, 3-run variance.
2. **Stage B — `QTimer` fallback** (only if Stage A misses §5.1):
   replace the worker loop with single-shot `QTimer` events
   armed against an absolute deadline (`std::chrono::steady_clock::
   now() + remainingScaledDelta`). Mirrors the M10 S10 pattern
   where `sleep_until` beat `sleep_for` for the 60 k-events/sec
   pacing.

If Stage B also misses, H2 fires.

### Note on `sleep_until`

A simpler alternative to QTimer is `std::this_thread::sleep_until`
with an absolute deadline. M10 S10 used this successfully for
60 k-events/sec pacing. M11 may adopt this **before** falling back
to QTimer if Stage A misses; it is one line of code and stays in
the worker thread. Doc-only addition to the plan; recorded here
for transparency.

---

## C5 — `positionChanged` rate-limit implementation

### Statement

Spec §2.1-10 calls for `positionChanged` emission "at most every
30 Hz (rate-limited)". Naive emit-per-record would flood the main
event loop at 60 k events/sec.

### Resolution

Worker-side throttle via `QDeadlineTimer cooldown_(33ms)`:

- Worker holds a `lastEmitTime_` timestamp and a
  `pendingPosition_` (timestampNs + recordIndex) state.
- On every dispatched record, update `pendingPosition_`.
- If `now() - lastEmitTime_ >= 33ms`, emit
  `positionChanged(pendingPosition_)` via queued connection and
  reset `lastEmitTime_`. Otherwise skip.
- On worker shutdown, flush the last `pendingPosition_` so the UI
  sees the final position.

This guarantees the UI never falls behind by > 1 frame and never
emits faster than 30 Hz. No timer object required.

---

## C6 — Cross-mode gating with M10 SessionWriter

### Statement

Spec §9 Notes #4: "Mode transition during recording is undefined.
If user is recording (M10 SessionWriter active) and tries to enter
Replay, V1 should disable the menu item or show error."

The symmetric case (entering recording while in Replay) is not
explicitly covered, but obviously also undefined.

### Resolution

MainWindow gates two actions:

| Action | Disabled when |
|---|---|
| **File → Open Session…** (Ctrl+O) | `sessionWriter_->state() == RecordingState::Recording` |
| **Session → Record…** (Ctrl+R) | `replayMode_->currentMode() == AppMode::Replay` |

State changes that affect either gate connect to the corresponding
QAction's `setEnabled` slot:

- `SessionWriter::recordingStarted` → disable Open Session
- `SessionWriter::recordingStopped` → re-enable Open Session
- `ReplayModeManager::modeChanged(Replay)` → disable Record
- `ReplayModeManager::modeChanged(Live)` → re-enable Record

Implementation in S9. No spec deviation.

---

## Summary

| ID | Resolution path | ADR? | Ships in subtask |
|---|---|---|---|
| C1 | α — additive SessionReader extension | No (default) | S2 |
| C2 | implementation-level path correction | No | S1 |
| C3 | ReplayModeManager synthesises pause/resume | No | S8 |
| C4 | two-stage sleep_for → QTimer fallback | No | S5 / S10 |
| C5 | 33 ms cooldown in worker | No | S5 |
| C6 | MainWindow QAction gating | No | S9 |

**No ADR-008 expected for V1.** This file is the canonical record;
M11-done.md will reference it from §Deviations.
