# M10 — Plan (Session Writer)

This plan turns `docs/milestones/M10-session-writer.md` (read on
`milestone/M10` at HEAD `16f31fd`) into a sequenced, gated set of
subtasks. It is companion to `.claude/M10-understanding.md`, which
records the spec restate + 3 concerns (C1 / C2 / C3) raised at
read-time.

The plan deliberately gates **all writer-code subtasks** behind S0,
which captures Phase 4's resolution of the format / round-trip
contradiction (C1) + the arch-divergence reconciliation (C2). CC
will not begin S1 onward until S0 lands an approved direction.

## 0. Sequencing diagram

```
S0 (ADR + concerns)
  │
  ├─→ S1 (scaffolding)
  │     │
  │     ├─→ S2 (format spec doc) ─┐
  │     │                          │
  │     ├─→ S3 (writer skeleton)   │
  │     │      │                   │
  │     │      ├─→ S4 (encoder) ───┤
  │     │      │      │            │
  │     │      ├─→ S5 (queue / backpressure)
  │     │      │      │
  │     │      └─→ S6 (reader: α or β per S0)
  │     │             │
  │     │             └─→ S7 (round-trip + 7 integration tests)
  │     │                    │
  │     │                    ├─→ S8 (sfreplay_inspect CLI)
  │     │                    │
  │     │                    ├─→ S9 (MainWindow Record toolbar)
  │     │                    │
  │     │                    └─→ S10 (bench + 30-min memory soak)
  │     │                           │
  │     │                           └─→ S11 (M10-done.md + freeze record)
```

Estimated effort: 5–7 person-days per spec; CC plans the same
~50 hour budget M9 used. Slack of ~6 h reserved for S0 scope
(ADR authoring + spec amendments) and S2 (format spec writing
takes longer than mechanical impl when correctness is V1-permanent).

## 1. Subtask ledger

| # | Subtask | Deps | Effort | Commit | Notes |
|---|---|---|---|---|---|
| S0 | C1 / C2 resolution: ADR-007 (format pivot to signal-level), spec § 7 trigger #2 / § 8.3 first-item amendment, M10-concerns.md authoring | (none) | 4 h | Yes | Phase 4 hard gate. CC drafts ADR + amendments → human approves α or β at Phase 4 review. CC then proceeds. **No code subtask starts until S0 is committed and Phase 4 passes.** |
| S1 | Module scaffolding: `src/session/CMakeLists.txt` + freeze-surface headers (`session_writer.hpp` + `session_metadata.hpp`) + namespace skeleton | S0 | 3 h | Yes | `signalforge::session` namespace. Headers carry only declarations + Doxygen; `.cpp` is empty stubs. Verifies CMake + linkage end-to-end before any logic lands. |
| S2 | `docs/format/sfreplay-v1.md` canonical V1 binary format spec | S0 | 5 h | Yes | The first frozen file format spec in V1. Header / catalog / record-type / footer / forward-compat rules, byte-by-byte. Produces sha256 → S11 freeze record. Cross-validated against M9 ReplayDriver behavior and the chosen reader (α / β) from S0. |
| S3 | `SessionWriter` class + `SessionFileWriter` skeleton + worker `QThread` lifecycle | S1 | 5 h | Yes | API per spec § 4.2. `start()` / `stop()` / `isRecording()` + 4 Qt signals. Worker thread per spec § 4.4 pattern. Lifecycle unit tests (`session_writer_lifecycle_test.cpp`): start / stop, multiple cycles, destructor joins worker. |
| S4 | `SessionFileWriter::openFile` + header / catalog / record encoding + footer + flush | S2 + S3 | 6 h | Yes | Implements the format from S2. `appendLe` helpers; per-type `SignalValue` encoding (bool=1B, int64=8B, double=8B, string=4B+UTF-8). Periodic 1 s flush. `flushAndClose()` writes footer + `fsync`. Encoder unit tests verify byte-level output for every record type. |
| S5 | Queue + backpressure (per spec § 4.6 + concerns.md C3) | S3 | 4 h | Yes | Bounded `QQueue<SessionEvent>` capacity 10 000. Drop-oldest *droppable* event on overflow (signal events). Catalog Extension + StopEvent are non-droppable. Counter `session_writer_dropped_events_total` increments per drop. Test: queue overflow + recovery; non-droppable preserved. |
| S6 | Reader implementation (α or β per S0) | S2 + S4 | 5 h | Yes | **Under α**: extend `replay_driver.cpp` to dispatch by `formatVersion`; new format → emit synthetic frames. **Under β**: new `src/session/session_reader.hpp` that consumes V1 file and pushes `SignalValue` to a `SignalValueSink`. Either path: sha256 of any modified or new frozen-surface file recorded in S11. |
| S7 | Round-trip integration test + 7 named integration tests per spec § 2.1-12 | S4 + S6 | 6 h | Yes | `tests/integration/test_session_writer_replay_round_trip.cpp` is the headline test (HALT-trigger #2 gate). Plus the 6 spec § 2.1-12 tests: basic_lifecycle, metadata, disk_full, concurrent_access, threading, long_session. Disk-full uses an injected `QIODevice` fault. |
| S8 | `tools/sfreplay_inspect/` CLI tool | S2 + S4 | 4 h | Yes | Mirrors `tools/schema_lint/` (M5) and `tools/expr_lint/` (M7) shape. CMake target with `Qt6::Core` only; reads file, prints header / catalog / record histogram / footer; `--json` output flag. Adds a fixture-based test that round-trips writer-output → inspector-parsed-output. |
| S9 | MainWindow Record toolbar + status bar | S3 | 4 h | Yes | `Record` toolbar action toggles `SessionWriter::start/stop`. `QFileDialog::getSaveFileName` for path. Status bar shows file-size on `flushed` signal. App-quit while recording → graceful stop (modal "Stop recording first?" prompt or auto-stop, M10.3 P decision). |
| S10 | `bench_session_writer` + 30-min memory soak | S5 + S6 + S9 | 4 h impl + 0.5 h soak run | Yes | At `tests/benchmark/bench_session_writer.cpp`. 60 sig × 1 kHz × 10 s sustained recording; bench reports queue-depth p99, main-thread-block max, disk write throughput. 30-min soak memory growth gated < 10 % per spec § 5.4 + § 8.2. Results to `tests/benchmark/results/M10-baseline.md`. |
| S11 | `.claude/M10-done.md` + freeze record + PR + manual hardware verification protocol | S2 + everything else | 4 h | Yes | Mirrors M9 S11. sha256 of 3 frozen files. PR # / merge SHA placeholders. Hardware verification protocol authored alongside (analogous to `docs/m9-hardware-verification.md`); execution deferred to operator. |

**Total estimated effort**: ~50 h (within spec's 5–7 person-day
budget). S0 + S2 + S6 are the highest-risk items because they
encode V1-permanent decisions; CC reserves slack there.

## 2. Subtask details

### S0 — C1 / C2 resolution + ADR

**Deliverables**:

- `docs/architecture/decisions/ADR-007-sfreplay-v1-format-pivot.md`
  proposing: "Session file V1 records signal-level events under
  the SFREPLAY magic that M9 ReplayDriver introduced; round-trip
  testing uses [α: extended ReplayDriver / β: new SessionReader];
  arch.md §G's `.sfr` + `.sfi` two-file plan is **deferred to V2**
  and superseded for V1 by this single-file `.sfreplay` format."
  ADR is authored by CC for human approval at Phase 4.
- `.claude/M10-concerns.md` recording C1 (round-trip
  impossibility), C2 (arch §G divergence), and C3 (catalog
  extension non-droppable in queue).
- Human approval at Phase 4 of α vs β. Plan branches at S6.

**HALT triggers**: if Phase 4 review denies the format pivot
(both α and β), CC HALTs and writes
`.claude/halt/HALT-<UTC>-m10-format-blocked.md`.

**Build / test gates**: docs / ADR only — no compile or test
implications.

### S1 — Scaffolding + freeze-surface headers

**Deliverables**:

- `src/session/CMakeLists.txt`: `signalforge_session` static lib.
  PUBLIC: `Qt6::Core`, `signalforge_buffer`, `signalforge_decoder`.
  PRIVATE: `signalforge_observability`. AUTOMOC ON. No new external
  deps.
- `src/session/session_writer.hpp`: per spec § 4.2. Class
  `SessionWriter : QObject, signalforge::decoder::SignalValueSink`.
  Public: `start` / `stop` / `isRecording` / `state` / counters.
  Signals: `recordingStarted` / `recordingStopped` / `errorOccurred`
  / `flushed`. `Q_DISABLE_COPY_MOVE`.
- `src/session/session_metadata.hpp`: `SessionMetadata` struct +
  `RecordingState` enum.
- `src/session/{session_writer, session_file_writer}.cpp`: empty
  stubs that compile.
- `src/CMakeLists.txt`: add `add_subdirectory(session)` after
  `connection`.

**Build / test gates**: Debug + Release build clean. clang-format
clean. No new ctest (placeholder lib only).

### S2 — Canonical SFREPLAY v1 format spec

**Deliverables**:

- `docs/format/sfreplay-v1.md` byte-by-byte spec per M10 § 4.1
  with these explicit additions (under S0's ADR-007):
  - **Format-version detection rules**: how V1 readers MUST fail
    closed when first 8 bytes are not `"SFREPLAY"`; how V2+ readers
    SHOULD fall through to V1 parsing when `formatVersion == 1`.
  - **Endianness** (LE everywhere) made explicit at the top.
  - **Field-by-field tables** (header, catalog entry, record
    header + per-type payload, footer) with offsets, sizes,
    encodings, and example bytes.
  - **Forward-compatibility rules**: V2 may extend record types
    > 4 (V1 readers skip via `payloadLen`); V2 may extend header
    after `signalCount` (V1 readers stop at declared `headerLen`);
    V2 may add per-signal-catalog-entry trailing fields (V1
    readers ignore based on declared per-entry byte length).
  - **Truncation handling**: missing footer means "incomplete";
    parser must read records until `payloadLen`-driven EOF or a
    record header that runs past file end; partial last record is
    discarded silently.
- The spec's sha256 lands in S11 freeze record.

**HALT triggers**: if the format spec admits any "TBD" or
ambiguous field, CC HALTs (per M10 spec § 9 first bullet:
"format spec must be **complete**").

**Build / test gates**: docs only. clang-format N/A.

### S3 — `SessionWriter` + worker thread skeleton

**Deliverables**:

- `SessionWriter::SessionWriter(SignalBufferRegistry&)` constructor
  registers self as a `SignalValueSink` on the registry.
- `start(filePath, description, schemaId)` opens worker `QThread`,
  moves a fresh `SessionFileWriter` onto it, opens the file
  synchronously on the worker via `QMetaObject::invokeMethod` with
  `Qt::BlockingQueuedConnection` so failures surface synchronously
  to the caller. Returns false on failure.
- `stop()` enqueues `StopEvent`, calls `workerThread_->quit()` +
  `wait()`, returns total bytes written. Thread is joined before
  the destructor returns (per spec § 4.4 + § 8.4).
- `SignalValueSink::onSignal` / `onSignalsRegistered` /
  `onSignalsUnregistered` overrides forward events into the
  worker's queue.
- `tests/unit/session/session_writer_lifecycle_test.cpp`: 6 cases
  — start / stop / multiple cycles / start-without-stop is
  rejected / state correctness / destructor joins worker.

**Build / test gates**: Debug + Release ctest pass; debug-asan
clean. No data-race in TSan (best-effort if host permits, else
CI-only).

### S4 — Encoder + flush + footer

**Deliverables**:

- `SessionFileWriter::openFile` writes header + initial catalog
  per S2.
- `processQueue` event loop: pop `SessionEvent`s, dispatch by
  `std::variant` alternative, encode + write.
- `writeSignalRecord` per spec § 4.5. `writeCatalogExtension`,
  `writeMarker`, `writeHeartbeat` (heartbeat written every 10 s
  per § 4.1 type 4).
- Periodic 1 s flush via worker-side `QTimer` (created on the
  worker thread so its parent affinity is right). `fsync` on
  flush + on close per spec § 5.2 (file-integrity gate).
- `tests/unit/session/session_writer_encoding_test.cpp`: byte-level
  output for every record type. Includes binary `payload` test
  (NUL / CRLF / 0xFF) for the M9 round-trip pattern.

**Build / test gates**: Debug + Release + debug-asan all clean;
new ctest count rises by ~10 cases.

### S5 — Queue + backpressure (incl. C3)

**Deliverables**:

- `SessionFileWriter::enqueue` per spec § 4.6 with the C3 refinement:
  - Detect non-droppable head (`StopEvent` /
    `CatalogExtensionEvent`) before dropping;
  - On queue full of non-droppable events (rare): block with a
    short timeout (10 ms) before retrying; if still full,
    enqueue a synthetic disk-error event and transition to
    `RecordingState::Error`.
- `session_writer_dropped_events_total` counter incremented per
  drop; observable via `SF_LOG_WARN` (mirrors M9 auto-connect
  metric pattern: log-now, registry-wire-up V1.5+).
- `tests/unit/session/session_writer_backpressure_test.cpp`: queue
  overflow with droppable head, queue overflow with non-droppable
  head, recovery after disk catches up.

**Build / test gates**: Debug + Release + debug-asan all clean.
Counter increments verified by test.

### S6 — Reader (branches on S0)

**Under α (extend `replay_driver.cpp`)**:

- Extend `ReplayIoWorker::openOnIoThread` to detect new format
  (header bytes 8-11 = `formatVersion=1`). On match, parse
  variable header + catalog. On records: re-encode each
  signal-record back into a synthetic `RawFrame` whose payload
  carries the typed-value bytes; pipeline downstream re-decodes
  via M5. **Caveat**: this requires the file to also carry a
  schema id usable by the runtime DecoderRegistrar — which is
  what spec § 3.5's "decoder schema reference" provides.
- `replay_driver.hpp` interface unchanged (no freeze breach).
- New unit tests for the dispatch path under α.

**Under β (new `src/session/session_reader.hpp`)**:

- `SessionReader` class consumes the V1 file and pushes events to a
  `SignalValueSink`. Lives on the caller's thread (no new
  worker thread; M11 will wire a streaming variant).
- API: `open(path)` → `play(SignalValueSink&, playbackSpeed=1.0)`
  → `close()`. State machine identical in shape to M9 driver.
- New unit tests under `tests/unit/session/session_reader_*`.

**Build / test gates**: Debug + Release + debug-asan all clean.
ctest count rises ~15-20 cases. Under α, no M3 / M9 test
regression. Under β, M9 ReplayDriver tests unchanged.

### S7 — Round-trip + 7 integration tests

**Deliverables** (per spec § 2.1-12):

- `tests/integration/test_session_writer_replay_round_trip.cpp` —
  HALT-trigger #2 gate. Writes a known sequence of
  `(timestamp, signalId, SignalValue)` events through
  `SessionWriter`, reads back via the S6 reader, asserts identity.
  Includes binary payload (NUL / CRLF / 0xFF) and all 4 type
  variants (bool / int64 / double / string).
- `test_session_writer_basic_lifecycle.cpp`
- `test_session_writer_metadata.cpp` (catalog matches registry at
  start; mid-stream registers reflected via Catalog Extension
  records)
- `test_session_writer_disk_full.cpp` (uses fault-injecting
  `QIODevice` subclass)
- `test_session_writer_concurrent_access.cpp` (chart reads from
  registry while writer records, no contention; verified via
  TSan-best-effort)
- `test_session_writer_threading.cpp` (worker-thread isolation
  via instrumented logging; ASan / TSan clean)
- `test_session_writer_long_session.cpp` (10 min × 1 kHz × 60
  signals; file size + integrity)

**Build / test gates**: All 7 tests green on Debug + Release +
debug-asan. `test_session_writer_long_session` is excluded from
default `ctest`; runs via `ctest -L long`.

### S8 — `sfreplay_inspect` CLI

**Deliverables**:

- `tools/sfreplay_inspect/CMakeLists.txt` + `main.cpp` mirroring
  M5 `tools/schema_lint/` pattern.
- Reads V1 file, prints header / catalog / record histogram /
  footer in the format shown in spec § 4.8.
- `--json` flag for machine-readable output (mirrors M5 lint).
- `tests/integration/test_sfreplay_inspect.cpp` round-trips a
  writer-produced file through the inspector and validates JSON
  output schema.

**Build / test gates**: builds clean; tool runs against fixture
files generated by S4 unit tests.

### S9 — MainWindow Record toolbar

**Deliverables**:

- New `QToolBar` action: "Record" (toggles between record / stop
  states). Red dot indicator in record state.
- `QFileDialog::getSaveFileName(this, "Save session", ...,
  "SFREPLAY (*.sfreplay)")` on click → `SessionWriter::start`.
- Status bar widget shows "Recording → 12.3 MB" updated on each
  `flushed` signal (per spec § 4.7).
- App-close while recording: modal `QMessageBox` "Stop recording
  first?" with auto-stop + commit option.
- `tests/integration/test_main_window_session_record.cpp`:
  toolbar click → file dialog (mocked) → start; second click → stop.

**Build / test gates**: same as S3 / S4. Manual UI verification
documented in S11's hardware-verification protocol.

### S10 — Benchmark + 30-min memory soak

**Deliverables**:

- `tests/benchmark/bench_session_writer.cpp` per spec § 2.1-14.
  60 signals × 1 kHz × 10 s sustained recording. Reports:
  - Sustained event rate (events / sec)
  - Queue-depth p99
  - Main-thread block max (instrumented via timestamps around
    `enqueue`; fail if > 5 ms)
  - Disk write throughput (bytes / sec)
- `--soak <seconds>` mode (mirrors M9 S5s pattern in
  `bench_chart`) with 30-min memory snapshot every 60 s.
- `tests/benchmark/results/M10-baseline.md` documents results.

**HALT triggers**: HALT trigger #4 (worker can't keep up after one
optimization pass) — gated by sustained ≥ 60 k events / sec.

**Build / test gates**: bench is opt-in via
`-DSIGNALFORGE_BENCHMARKS=ON`, never run by ctest / CI per existing
bench convention.

### S11 — M10-done.md + freeze record + PR

**Deliverables**:

- `.claude/M10-done.md` mirroring M9 S11 shape: deliverables vs
  spec § 2.1, freeze record (3 sha256s — `session_writer.hpp`,
  `session_metadata.hpp`, `docs/format/sfreplay-v1.md`), PR /
  merge state, test count matrix, performance gates met,
  hand-off notes for M11 / M12 / M13, HALT trigger disposition,
  V1.5+ deferred items.
- `docs/m10-hardware-verification.md` (analogous to
  `docs/m9-hardware-verification.md`): manual record + replay +
  inspector verification protocol. Execution deferred to operator
  per the M9 pattern.
- After local commit: stop and announce per CLAUDE.md §Phase 1
  step 6. Push + PR creation deferred to per-operation
  authorization.

**Build / test gates**: docs-only commit; CLAUDE.md §Required #2
exception applies.

## 3. HALT-trigger pre-disposition

| M10 spec § 7 trigger | When measured | Where in plan |
|---|---|---|
| #1 modify M2-M9 frozen `.hpp` | Compile-time, every commit | Verified on every commit by `git diff <prev> -- 'src/{drivers,decode,buffer,frame,connection}/*.hpp'` empty check (M9 pattern) |
| #2 round-trip mismatch | S7 round-trip test | Hard gate on test pass; resolution path picked in S0 (α / β) |
| #3 main-thread block > 5 ms | S10 bench | Bench-instrumented timer; HALT if max > 5 ms after one optimization pass |
| #4 worker can't keep up at 60 k events / sec | S10 bench | HALT if sustained < 30 k events / sec |
| #5 ReplayDriver reads incomplete events from finalized file | S7 round-trip + truncation tests | Hard gate; depends on writer ordering = footer-after-records |
| #6 30-min recording memory growth > 10 % | S10 soak | Mirror of M9 S5s; gate < 10 % steady-state growth |
| #7 backpressure drops events without counter increment | S5 backpressure test | Counter assertion in test; HALT if drop count diverges from logged-WARN count |

CLAUDE.md HALT triggers (compile fail × 3, test fail × 3, new dep,
frozen-iface mod, perf-after-one-pass, arch-spec contradiction,
Qt-doc divergence, two-impl ambiguity, git fail) all apply
unchanged. Note: HALT trigger #7 (arch.md vs spec contradiction)
already pre-fired in concerns.md C2; resolution is the S0 ADR
(human-approved at Phase 4) — this is the established
parallel-track ADR pattern (M6 ADR-005, M9 C1 / C2).

## 4. Build / test cadence

Per CLAUDE.md §Required #2:

- Every code commit: Debug + Release build clean; ctest pass on
  both; debug-asan build clean (CI authoritative for ASan-runtime
  gating per host-ld.so.preload memory note).
- `clang-format --dry-run -Werror` on every changed `.cpp` /
  `.hpp`.
- Docs-only commits exempt per the existing CLAUDE.md §Required
  #2 exception.

CI run-time budget: ~10–12 min per push (matches M9 cadence).
Per-commit push authorization required per CLAUDE.md §Forbidden
#4; CC will not push autonomously.

## 5. CC-time budget per phase

| Phase | Budget |
|---|---|
| S0 (ADR + concerns) | 4 h |
| S1-S2 (scaffolding + format spec) | 8 h |
| S3-S5 (writer + queue) | 15 h |
| S6 (reader α / β) | 5 h |
| S7-S8 (tests + CLI) | 10 h |
| S9-S10 (UI + bench) | 8 h |
| S11 (closure) | 4 h |
| Slack / unforeseen | 6 h |
| **Total** | **60 h** |

Within spec § Estimated effort 5–7 person-days assuming the
person-day is ~8 h.

## 6. What stays out of M10 (deferred to V1.5+ / V2)

Per spec § 2.2:

- Compression / encryption (V1.5+)
- Replay UX (M11)
- Multi-file rotation (V1.5+)
- Concurrent multi-recorder (V1.5+)
- Edit-during-record (V1.5+)
- Auto-record-on-connect (V1.5+)
- `.sfi` index file (V2; superseded by per-file footer in V1)
- Network sync (V2)
- Plugin architecture for custom format extensions (V2)
- Auto-connect metric counters wired through M2 metrics registry
  (V1.5+; the metric **names** are stable per M9 pattern)

## 7. Phase 4 review checklist (for the human)

1. Read `.claude/M10-understanding.md` → C1 / C2 / C3.
2. Decide α vs β for C1 (round-trip path). Write it in the Phase 4
   approval message.
3. Confirm or reject S0's planned ADR-007 (format pivot).
4. Confirm S6 branch matches the C1 decision.
5. Confirm or replace estimated effort.
6. If approved: reply
   `approved, execute M10` (per CLAUDE.md authorization phrase).

If denied: CC HALTs and writes `.claude/halt/HALT-<UTC>-m10-blocked.md`.
