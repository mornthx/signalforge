# M3 — Progress log

## Session metadata

- Phase 5 execution begins 2026-04-24.
- Branch: `milestone/M3` at `5596d45` (understanding + plan from Phase 3).
- Plan: `.claude/M3-plan.md`, 12 subtasks S1–S12.
- Understanding: `.claude/M3-understanding.md`.
- Remote: `git@github.com:mornthx/signalforge.git` (SSH via `ssh.github.com:443` per user setup).

## Subtask log

Each subtask appends a start entry and a close entry. Do not overwrite.

---

### S1 — shared foundation: driver_configs + IoWorkerBase (start)

**Goal**: deliver `src/drivers/driver_configs.hpp` with the four `*Config`
structs (Serial, Tcp, Udp, Replay) per spec §4.1 verbatim, plus
`src/drivers/io_worker_base.{hpp,cpp}` as the abstract IoWorker base per
spec §4.5. Wire platform library for `setCurrentThreadName`.

**Approach**: headers are pure value types and a small abstract QObject.
Unit tests cover default-init, field population, and `IoWorkerBase`
constructor + `threadName()` accessor. Testing the `onThreadStart` hook
directly requires a concrete subclass; the test uses an in-file minimal
subclass and a `QThread::start()` + `wait()` round-trip to verify the hook
fires on the IO thread (not the caller's).

No M2 frozen .hpp touched — only additive files under `src/drivers/`.

### S1 — shared foundation (close)

- **Files delivered**: `driver_configs.hpp` (4 config structs matching
  spec §4.1 verbatim), `io_worker_base.{hpp,cpp}` (`IoWorkerBase` class
  with protected `onStarted()` hook, public `onThreadStart()` slot that
  sets OS thread name via `platform::setCurrentThreadName` then
  dispatches to `onStarted()`).
- **CMake**: `signalforge_drivers` library now also compiles
  `io_worker_base.cpp`; now links `signalforge_platform` and
  `signalforge_observability` privately (previously only Qt6::Core +
  signalforge_frame).
- **Tests**: 10 config tests (defaults + field population for all 4
  structs, copy semantics) + 2 IoWorkerBase tests (threadName accessor,
  onStarted-on-IO-thread verification via an in-file StubWorker
  subclass and a real QThread). 115 tests green under Debug + Release.
  debug-asan build clean.
- **Coverage**: every config struct field covered; both IoWorkerBase
  methods exercised (thread-affinity verification uses real QThread
  to catch any "runs on caller thread" regression).
- **Freeze scope**: no M2 frozen .hpp modified. Verified by diff.
- **Time**: ~1 h (well under the 3 h plan estimate).

### S2 — ReplayDriver skeleton (start)

**Goal**: concrete `ReplayDriver` subclass of `DriverInterface` per spec
§4.7 with a private `ReplayIoWorker`. Lifecycle is complete and correct;
frame emission is deferred to M9 with `// TODO(M9):` markers at the
insertion points. Produce `test_replay_driver_skeleton.cpp` and the
`minimal_session.sfreplay` fixture.

**Approach**: state is held as `std::atomic<DriverState>` in the driver
itself (not the worker). Driver's public methods validate and dispatch
via `QMetaObject::invokeMethod(worker, ..., Qt::QueuedConnection)` to
worker slots that run on the IO thread. The worker does the file I/O
(existence + 16-byte-non-zero check) then signals back; the driver's
slots (bound via `Qt::QueuedConnection` so they run on the caller's
thread) translate those signals into state transitions and emit the
public `stateChanged`/`errorOccurred`. `write()` returns
`NotConfigured` directly (read-only driver, no worker round-trip).

No M2 frozen .hpp touched.

### S2 — ReplayDriver skeleton (close)

- **Files delivered**:
  - `src/drivers/replay_driver.{hpp,cpp}` (ReplayIoWorker defined entirely
    in the .cpp; moc include at file bottom)
  - `tests/unit/drivers/replay_driver_test.cpp` (10 cases)
  - `tests/integration/test_replay_driver_skeleton.cpp` (3 cases)
  - `tests/integration/fixtures/minimal_session.sfreplay` (16 bytes of
    0xAB so the non-zero-header check passes)
- **Semantics**: ConfigInvalid (empty path) returned synchronously by
  open() without state transition. Nonexistent file →
  ResourceUnavailable asynchronously. File < 16 bytes or all-zero
  header → ProtocolFailure. write() always returns NotConfigured (
  read-only). Lifecycle: Idle → Opening → Open → Running → Stopping →
  Open → Closing → Idle, plus any-state → Error → (close) → Idle.
- **TODO(M9) markers** placed at `ReplayIoWorker::openOnIoThread` (full
  metadata parse), `startOnIoThread` (timer + frame emission),
  `stopOnIoThread` (timer teardown), and `onStarted` (initialization).
  Grep-discoverable: `grep -r "TODO(M9)" src/drivers/replay_driver.cpp`
  returns 4 hits.
- **Tests**: 128 green (13 new cases) under Debug and Release.
  debug-asan build clean.
- **Coverage**: every control-flow branch in `openOnIoThread` exercised
  (missing file / short / all-zero / valid); write-in-each-state tested;
  close/stop idempotency tested; statistics zero-counter property tested.
  Estimated ≥ 85% — spec §5.1 floor for ReplayDriver is 75%.
- **Freeze scope**: no M2 frozen .hpp modified.
- **Time**: ~1.5 h (under 3 h plan estimate).

### S3 — SerialDriver + unit tests (start)

**Goal**: deliver `SerialDriver` over `QSerialPort` on a dedicated IO
thread. SerialConfig validation at open(); error mapping per spec §4.8
(ResourceUnavailable for missing device / port-in-use, PermissionDenied
for EACCES, ConfigInvalid for bad baud / dataBits, IoFailure for generic
read/write errors). write() queues payload and always returns Success in
Running state (async semantics per spec §3.2); TxStats tracks actual
delivery.

**Approach**: same pattern as ReplayDriver (atomic state in driver,
worker signals mapped through driver slots). `SerialIoWorker` lives in
.cpp with a `QSerialPort*` constructed lazily on the IO thread. Read
path: `readyRead` → `readAll()` → construct RawFrame → emit
workerFrameReceived. Write path: `write()` on driver validates state
+ queues payload into an internal QByteArray deque on the worker;
emits a signal to the worker to drain; worker pops and writes to
QSerialPort.

Tests at unit level cover what's testable without a real device:
ConfigInvalid paths (empty device, invalid baud), stateless
transitions (open with bad config doesn't transition), write-state-
gates. Integration (socat) is S4.

No M2 frozen .hpp touched.

### S3 — SerialDriver + unit tests (close)

- **Files delivered**: `serial_driver.{hpp,cpp}` (SerialIoWorker lives
  in the .cpp; moc include at bottom), `tests/unit/drivers/serial_driver_test.cpp`.
- **CMake**: `signalforge_drivers` links `Qt6::SerialPort`.
- **Error mapping** per spec §4.8: DeviceNotFoundError →
  ResourceUnavailable, PermissionError → PermissionDenied, Open /
  NotOpen → ResourceUnavailable, ResourceError → ResourceLost,
  TimeoutError → Timeout, UnsupportedOperationError → ConfigInvalid,
  others → IoFailure. Error messages follow spec §4.9 style.
- **Validation** catches empty device, non-positive baud, dataBits
  outside 5–8, stopBits ≠ 1/2, unknown parity/flowControl strings —
  all ConfigInvalid without state transition.
- **Tests**: 12 new unit cases covering every validation branch plus
  async-error path (nonexistent device → Error state), write-in-wrong-
  state, idempotent close/stop, zeroed stats, config() accessor. 140
  tests green under Debug and Release. debug-asan build clean.
- **Integration (socat-based)** is S4 territory; the unit tests stay
  device-free.
- **Coverage**: every `validateConfig` branch exercised; open-error
  path exercised at async level; idempotent lifecycle covered.
  Estimated ≥ 85%.
- **Freeze scope**: no M2 frozen .hpp modified.
- **Time**: ~2 h (under 5 h plan estimate).

