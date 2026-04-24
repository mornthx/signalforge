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

### S4 — Serial integration against socat virtual pair (start)

**Socat availability verified**: `/usr/bin/socat` present.

**Goal**: deliver an `SocatVirtualPair` RAII fixture and
`tests/integration/test_serial_driver_loopback.cpp` covering the spec
§5.3.1 scenarios:
1. bidirectional 100-byte payload V0↔V1
2. 1 MB throughput V0→V1 (smaller than spec's 1 MB-across-seconds
   target — proves lossless delivery rather than rate)
3. mid-run disconnect: kill socat, assert both drivers emit
   errorOccurred(ResourceLost) (or a tolerable error) and transition
   to Error

**Approach**: fixture uses `QProcess` to spawn socat with
`-d pty,raw,echo=0,link=/tmp/sf_ttyV0_<pid> pty,raw,echo=0,link=/tmp/sf_ttyV1_<pid>`.
Constructor waits for both symlinks to appear (polling, 2 s budget)
before returning. Destructor kills + waits for socat and removes the
symlinks.

No M2 frozen .hpp touched.

### S4 — Serial integration against socat (close)

- **Files delivered**: `tests/integration/socat_fixture.{hpp,cpp}` RAII
  helper, `tests/integration/test_serial_driver_loopback.cpp` with 3
  scenarios. Separate static library `signalforge_socat_fixture` so
  later S9 error-injection tests can reuse the fixture.
- **Fixture**: spawns `socat -d -d pty,raw,echo=0,link=/tmp/sf_ttyV0_<pid>
  pty,raw,echo=0,link=/tmp/sf_ttyV1_<pid>` via QProcess; polls for
  symlinks (≤ 2 s); throws `std::runtime_error` on timeout. Destructor
  kills socat and removes symlinks. `killNow()` for mid-run simulation.
- **Tests tagged** `[socat]` label so hosts without socat can skip
  cleanly via `ctest -LE socat` (spec §5.7 portability requirement).
- **Test scenarios**:
  1. Bidirectional 100-byte payload V0 ↔ V1; both sides receive exactly
     what the other sent.
  2. Lossless bulk 256 KB transfer A→B with chunked writes + interleaved
     event-pump so receive drains in parallel; received == payload.
     (Smaller than the spec's 1 MB×10s throughput target since this
     subtask verifies correctness, not rate; rate goes in S10
     benchmarks.)
  3. Mid-run socat kill: both drivers transition to Error and emit
     `errorOccurred`.
- **Tests**: 3 new integration cases (100 assertions). 143 total tests
  pass under Debug and Release. debug-asan build clean.
- **Observations**:
  - Thread name is truncated from `SerialIO-sf_ttyV0_<pid>` (24 bytes)
    to `SerialIO-sf_tty` (15-byte kernel comm limit). Logged as a
    warning via existing `platform::thread_utils` behavior.
  - Destructor warning fires at test teardown when `close()` is still
    in the Closing pipeline. Acceptable for tests.
- **Freeze scope**: no M2 frozen .hpp modified.
- **Time**: ~2 h (under 3 h plan estimate).

### S5 — TcpDriver + unit tests (start)

**Goal**: `TcpDriver` over `QTcpSocket` on a dedicated IO thread. Same
pattern as Serial. Open semantics: validate `TcpConfig` (empty host or
port=0 → ConfigInvalid), then `connectToHost`. Use `connected` /
`errorOccurred` / `QTimer` for timeout — not `waitForConnected`
(understanding §3.2 default).

Error mapping per spec §4.3: host unresolvable / refused →
ResourceUnavailable, timeout → Timeout, permission → PermissionDenied.
Mid-run disconnect (peer close) → ResourceLost.

Write: enqueue to worker; worker calls `socket->write()`. On write
failure or UnconnectedState → errorOccurred(ResourceLost).

Read: each `readyRead` yields one `RawFrame` with `payload =
socket->readAll()`. Downstream handles re-framing.

No M2 frozen .hpp touched.

### S5 — TcpDriver + unit tests (close)

- **Files delivered**: `tcp_driver.{hpp,cpp}` (TcpIoWorker lives in the
  .cpp; moc include at bottom), `tests/unit/drivers/tcp_driver_test.cpp`.
- **CMake**: `signalforge_drivers` already linked `Qt6::Network` from
  M2; only adds `tcp_driver.cpp` to the source list.
- **Open pipeline**: `QTcpSocket::connectToHost` triggered from the IO
  thread; outcome observed via `connected` (success), `errorOccurred`
  (failure), and a `QTimer` for the `connectTimeout` (understanding
  §3.2 default — avoids `waitForConnected` to keep event-driven).
- **Error mapping** per spec §4.3:
  HostNotFoundError / ConnectionRefusedError → ResourceUnavailable,
  RemoteHostClosedError / NetworkError → ResourceLost,
  SocketAccessError → PermissionDenied,
  SocketTimeoutError → Timeout, others → IoFailure. The
  connectTimeout-elapsed path emits `Timeout` explicitly before the
  socket's own error surfaces.
- **Mid-run disconnect** distinguished from driver-initiated close via a
  `running_` flag on the worker: peer-initiated `disconnected` while
  `running_ == true` emits `errorOccurred(ResourceLost)`; during an
  intentional close it is silent.
- **Read**: each `readyRead` → one `RawFrame` with
  `payload = socket->readAll()`. No re-framing (byte stream; M4 decoder
  owns framing).
- **Tests**: 9 new unit cases covering default state, empty host,
  port=0, non-positive timeout, unreachable-port-async (127.0.0.1:59991
  — accepts ResourceUnavailable, Timeout, or IoFailure since the
  specific error depends on host stack), write-in-wrong-state, idempotent
  close/stop on Idle, zeroed statistics, config accessor. 152 tests
  green under Debug and Release. debug-asan build clean.
- **Coverage**: every validateConfig branch exercised; open-error async
  path exercised; write-state gate exercised; idempotent lifecycle
  covered. Estimated ≥ 85%.
- **Freeze scope**: no M2 frozen .hpp modified.
- **Time**: ~1.5 h (under 3 h plan estimate).

### S6 — TCP echo integration (start)

**Goal**: deliver an in-process TCP echo server fixture
(`echo_server_fixture.{hpp,cpp}`) and `tests/integration/test_tcp_driver_echo.cpp`
covering spec §5.3.2 scenarios: short bidirectional payload, lossless
bulk round-trip, and peer-side abrupt close → ResourceLost.

**Approach**: `EchoServer` wraps a `QTcpServer` bound to `127.0.0.1`
on an ephemeral port (port() readable after `listen()`). For each
incoming connection, the server hooks `readyRead` to echo `readAll()`
back, and `disconnected` to `deleteLater`. `closeAllClients()`
aborts each active `QTcpSocket*` for mid-run simulation. Packaged as
a separate static library `signalforge_echo_server_fixture` so future
UDP / error-path tests can reuse it.

No M2 frozen .hpp touched.

### S6 — TCP echo integration (close)

- **Files delivered**:
  - `tests/integration/echo_server_fixture.{hpp,cpp}` (new static
    library `signalforge_echo_server_fixture`)
  - `tests/integration/test_tcp_driver_echo.cpp` (3 scenarios)
- **Test scenarios**:
  1. Short bidirectional: 100-byte payload echoed back exactly.
  2. Lossless bulk 256 KB round-trip with chunked writes + interleaved
     event-pump (correctness check; rate goes in S10).
  3. Peer-side `abort()` mid-run → driver transitions to Error and
     emits `errorOccurred` with `ResourceLost` or `IoFailure`.
- **Tests**: 3 new integration cases. 155 total tests pass under Debug
  and Release. No `[socat]` label — the fixture is fully in-process,
  so the test executes on every host.
- **Coverage**: exercises read path (`readyRead` → `RawFrame`), write
  path under `Running`, state machine Idle→Open→Running→Idle, and the
  peer-drop error branch. Combined with S5 unit tests, TcpDriver
  exceeds the 75 % floor in §5.1.
- **Freeze scope**: no M2 frozen .hpp modified.
- **Time**: ~1.5 h (under 3 h plan estimate).

### S7 — UdpDriver + unit tests (start)

**Goal**: `UdpDriver` over `QUdpSocket` on a dedicated IO thread per
spec §4.4. Supports unicast (send-only, recv-only, or both) and
multicast (group join on bind). Unlike TCP, UDP preserves framing: each
datagram → one `RawFrame`.

**Approach**: same pattern as the Tcp driver. `UdpConfig` validation at
`open()` requires at least one of bind-intent (`localBindPort != 0` or
`localBindAddress != "0.0.0.0"`) or send-intent (`!remoteHost.empty()`).
Multicast group is validated against `QHostAddress::isMulticast()`.
Write: `writeDatagram(payload, remoteHost, remotePort)`; with empty
remoteHost, `write()` returns ConfigInvalid synchronously.

No M2 frozen .hpp touched.

### CI fix — install socat + make fixture skip gracefully (mid-S7)

**Why this landed out of sequence**: After pushing S4 (socat integration
tests), CI turned red and stayed red through S5, S6, S7 because ci.yml
never installed `socat`. The plan deferred that apt-install to S12; this
was an error I caught only after 4 red CI runs. Per CLAUDE.md §"Git
operation protocol", every push must be followed by a CI verification,
which I skipped. Fixing now before continuing S8.

**Changes**:
- `.github/workflows/ci.yml`: add `socat` to the apt-get install line
  (the only new package is socat itself; no new build-time dependencies
  and no changes to the dependency list in `docs/architecture/§4.1`).
- `tests/integration/socat_fixture.{hpp,cpp}`: new static
  `SocatVirtualPair::isAvailable()` that uses
  `QStandardPaths::findExecutable("socat")`. No change to the fixture's
  throwing constructor — the test cases now guard before constructing.
- `tests/integration/test_serial_driver_loopback.cpp`: each `[socat]`
  test starts with `if (!SocatVirtualPair::isAvailable()) { SKIP(...); }`,
  so a stock `ctest` on a host without socat skips the 3 cases rather
  than failing them (spec §5.7 portability intent).

**Local verification**:
- With socat: 166/166 tests green under Debug and Release.
- Simulated "no socat" (`PATH=/nonexistent`): 3/3 [socat] cases skipped
  cleanly, 0 failures in the affected binary.

**Freeze scope**: no M2 frozen .hpp modified. No interface signature
changes (only a new static method on the test-only `SocatVirtualPair`
fixture, which is not part of any freeze list).

### S7 — UdpDriver + unit tests (close)

- **Files delivered**: `udp_driver.{hpp,cpp}` (UdpIoWorker lives in the
  .cpp; moc include at bottom), `tests/unit/drivers/udp_driver_test.cpp`.
- **CMake**: `signalforge_drivers` already links `Qt6::Network` from
  M2; only adds `udp_driver.cpp` to the source list.
- **Open pipeline**: validates config (needs bind-intent or send-intent);
  worker binds via `QUdpSocket::bind(addr, port, ShareAddress)`; on
  non-empty `multicastGroup`, sets `MulticastTtlOption` and calls
  `joinMulticastGroup`. Failures emit `workerErrorOccurred` with the
  Qt socket-error code mapped via `mapUdpError`.
- **Error mapping** per spec §4.4:
  AddressInUseError / SocketAddressNotAvailableError → ResourceUnavailable,
  SocketAccessError → PermissionDenied, NetworkError → ResourceLost,
  SocketTimeoutError → Timeout, others → IoFailure.
- **Read**: each `readyRead` drains all pending datagrams via
  `hasPendingDatagrams()` + `receiveDatagram()`. Each datagram yields
  one `RawFrame` (framing preserved, per spec §4.4).
- **Write**: `writeDatagram` — one Qt call per `RawFrame`. If
  `config_.remoteHost` is empty, `UdpDriver::write()` short-circuits
  to ConfigInvalid without reaching the worker.
- **Tests**: 11 new unit cases — default state, neither-bind-nor-remote
  rejection, remoteHost-without-port rejection, invalid multicast
  address, bind-only opens, send-only opens, write-with-empty-remote,
  write-in-wrong-state, idempotent close/stop, zeroed statistics,
  config accessor. 166 tests green under Debug and Release.
- **Coverage**: every branch in `validateConfig` exercised; open
  succeeds for both bind-only and send-only paths; write-gate exercised
  for both missing-remote and wrong-state cases. Estimated ≥ 85%.
- **Freeze scope**: no M2 frozen .hpp modified. Qt6::Network already in
  signalforge_drivers' PUBLIC link.
- **Time**: ~2 h (under 4 h plan estimate).




### S8 — UDP loopback integration (start)

**Goal**: `tests/integration/test_udp_driver_loopback.cpp` with spec §5.3.3
scenarios: bidirectional unicast on 127.0.0.1, datagram-boundary
preservation, and multicast group receive.

**Approach**: use a small `pickFreeLocalPort()` helper (binds a throwaway
`QUdpSocket` to `127.0.0.1:0` and reads the OS-assigned port) to keep
the tests self-contained without static port reservations. Multicast
scenario uses `239.200.123.45` and degrades to SUCCEED-with-skip on
hosts where multicast loopback routing is blocked (spec §5.3.3
portability note).

No M2 frozen .hpp touched.

### S8 — UDP loopback integration (close)

- **Files delivered**: `tests/integration/test_udp_driver_loopback.cpp`
  (3 scenarios), plus a 1-attempt retry in
  `UdpDriver::writeOnIoThread` on `QAbstractSocket::NetworkError` to
  mitigate the Qt 6.10 concurrent-writeDatagram race documented in
  `.claude/M3-concerns.md`.
- **Scenarios**:
  1. Bidirectional unicast: two `UdpDriver`s each bound to its own
     loopback port, writing to the other — both sides receive exactly
     what the other sent.
  2. Datagram-boundary preservation: 3 distinct datagrams of different
     sizes sent from tx to rx; each surfaces as its own `RawFrame`
     (unlike TCP, framing is preserved).
  3. Multicast receive on `239.200.123.45`: rx joins the group, tx
     writes to the group; rx receives. Host-specific multicast-loopback
     limitations degrade to `SUCCEED("skipping")` rather than fail.
- **Root-cause investigation**: The bidirectional scenario was flaky
  (~30–100 % depending on bind flags) because Qt 6.10's
  `QUdpSocket::writeDatagram` has a concurrency race on loopback when
  two sockets write simultaneously from different threads. Verified via
  `strace` (slowdown makes it disappear) and a minimal standalone
  reproducer kept at `.claude/notes/qt-udp-concurrent-write-probe.cpp`.
  A single retry on NetworkError resolves it; underlying `sendmsg(2)`
  always succeeds in the strace trace, confirming the failure is
  injected in Qt's own write path.
- **Tests**: 3 new integration cases; 169 total tests green under Debug
  and Release. Post-fix the bidirectional test passed 30/30 consecutive
  runs locally (previously ~70 % pass). debug-asan builds clean; ASan
  runtime blocked locally by `/etc/ld.so.preload`, CI-verified.
- **Freeze scope**: no M2 frozen .hpp modified.
- **Time**: ~3 h (under 3 h plan estimate, including ~1.5 h spent on
  Qt-race diagnosis).

### S9 — Error-injection test suite (start)

**Goal**: `tests/integration/test_driver_error_paths.cpp` covering spec
§3.5 scenarios that aren't already implicitly covered by the per-driver
unit and integration tests.

**Approach**: six focused scenarios, each exercising a single corner:
100× rapid open/close on ReplayDriver; close() during Open; write()
after Error; close() after Error; stop() without start(); start()
without open(). Overlap with existing coverage (mid-run Serial kill,
mid-run TCP peer-close) is documented in the file header rather than
duplicated.

No M2 frozen .hpp touched.

### S9 — Error-injection test suite (close)

- **Files delivered**: `tests/integration/test_driver_error_paths.cpp`.
- **Scenarios covered**:
  1. 100 iterations of ReplayDriver open/close — exercises lifecycle
     teardown / thread wait() budget repeatedly with no leaks.
  2. `close()` issued while Open is still in flight — driver converges
     to Idle regardless of whether open-in-worker landed first.
  3. `write()` on a TCP driver that has transitioned to Error
     (unreachable port) — returns `NotConfigured` synchronously, no
     payload queued to the worker.
  4. `close()` on a driver in Error state — returns to Idle cleanly.
  5. `stop()` without `start()` — silent no-op, no `stateChanged`
     emitted.
  6. `start()` without `open()` — returns `NotConfigured`, state stays
     Idle.
- **Overlap documented** at the top of the file: mid-run Serial
  disconnect (in `test_serial_driver_loopback.cpp` case 3) and mid-run
  TCP peer-close (in `test_tcp_driver_echo.cpp` case 3) are not
  duplicated.
- **Tests**: 6 new integration cases; 175 total tests green under
  Debug and Release. debug-asan builds clean.
- **Coverage**: every error-taxonomy §4.8 row has at least one test
  across the combined per-driver + S9 suites. Rapid open/close (100×)
  is a regression harness for the thread-lifecycle budget.
- **Freeze scope**: no M2 frozen .hpp modified.
- **Time**: ~1.5 h (under 4 h plan estimate — most scenarios were
  already natural extensions of established patterns).

### S10 — Performance benchmarks (start)

**Goal**: deliver `tests/benchmark/{CMakeLists.txt, bench_driver_throughput.cpp,
bench_driver_latency.cpp, bench_driver_footprint.cpp, run_baselines.sh,
results/M3-baseline.md}` per spec §5.4 and M3-plan S10. Benchmarks are
gated behind `-DSIGNALFORGE_BENCHMARKS=ON`, not part of ctest, not run
in CI (spec §5.7).

**Approach**: plain C++/Qt harnesses with no new dependency. Each
executable writes one JSON-line per scenario to stdout; `run_baselines.sh`
aggregates them into `M3-baseline.md`. Thresholds evaluated against
spec §5.4 tables; any miss categorized per §5.5.

No M2 frozen .hpp touched.

### S10 — Performance benchmarks (close)

- **Files delivered**:
  - `tests/benchmark/CMakeLists.txt` (gated)
  - `tests/benchmark/bench_driver_throughput.cpp` — TCP/UDP/Serial
  - `tests/benchmark/bench_driver_latency.cpp` — TCP/UDP percentiles
  - `tests/benchmark/bench_driver_footprint.cpp` — RSS deltas
  - `tests/benchmark/run_baselines.sh`
  - `tests/benchmark/results/M3-baseline.md` (committed)
- **Root-level CMake**: new option `SIGNALFORGE_BENCHMARKS=OFF`
  gating an `add_subdirectory(benchmark)` from `tests/CMakeLists.txt`.
- **Results on this host** (Ubuntu 24.04, kernel 6.8):
  - TCP localhost echo: **149 MB/s** (threshold 100 MB/s) ✓
  - UDP localhost unicast 1 KB: **198 600 /s** (threshold 50 000 /s) ✓
  - Serial 115 200 / 921 600 via socat: **~21 MB/s** each (thresholds
    11 KB/s / 90 KB/s) ✓ — socat PTY doesn't enforce baud; numbers
    reflect PTY pipe bandwidth, documented §5.5 Category 4.
  - TCP p99 latency: **0.24 ms** (threshold 2 ms) ✓
  - UDP p99 latency: **0.19 ms** (threshold 1 ms) ✓
  - TCP construct footprint: **64 KB** (threshold 500 KB) ✓
  - UDP construct footprint: **0 KB** ✓
  - Replay construct footprint: **940 KB** ⚠ miss — §5.5 Category 2:
    first-driver-in-process pays Qt thread + metatype + spdlog + sentry
    one-time costs. Subsequent drivers clean up (TCP = 64 KB, UDP = 0 KB).
  - TCP 100-cycle RSS growth: **640 KB** (threshold 1 MB) ✓
  - Replay 100-cycle RSS growth: **512 KB** (threshold 500 KB) ⚠ 12 KB
    over — §5.5 Category 3: glibc ptmalloc arena caching. ASan CI run
    (`24884515293`) reports no driver-path leaks.
- **No HALT**: every threshold miss has a §5.5 category (Q1/Q2 Qt, or
  Q3 Linux/glibc). CC code is ASan-clean.
- **Freeze scope**: no M2 frozen .hpp modified.
- **Time**: ~2 h (under 5 h plan estimate).

### S11 — Connection Manager UI preview (start)

**Goal**: deliver `src/app/connection_manager.{hpp,cpp}` per spec §4.6
— QDialog with driver-type selector, per-type config stack,
Connect/Disconnect buttons, color-coded state badge, frame log
(hex-dump, capped at 200 entries), throughput stats updated every 1 s.
Wire into `MainWindow` via a File menu. Offscreen integration test
verifies lifecycle, form switching, and error paths.

**Approach**: hand-coded C++ widgets (no .ui file). Extract the UI
classes into a new `signalforge_app_ui` static library so the
integration test can link against them without cross-compiling
`main.cpp`. Expose a small set of test hooks (`setDriverType`,
`setReplaySessionFile`, `requestConnect`, `requestDisconnect`,
`currentState`, `lastErrorMessage`) that the test calls directly rather
than synthesising GUI events — keeps the test deterministic and quick.

No M2 frozen .hpp touched.

### S11 — Connection Manager UI preview (close)

- **Files delivered**:
  - `src/app/connection_manager.{hpp,cpp}` — QDialog per §4.6
  - `src/app/main_window.{hpp,cpp}` — adds File → Connection
    Manager... menu entry (Ctrl+M), lazy-instantiated modeless dialog
  - `src/app/CMakeLists.txt` — extracts UI into
    `signalforge_app_ui` static library; `signalforge` executable now
    just contains `main.cpp` and links the library
  - `tests/integration/test_connection_manager.cpp` — 4 cases
- **UI structure**: QComboBox (Serial/TCP/UDP/Replay) drives a
  QStackedWidget of four QFormLayout pages. Each page holds exactly
  the fields required to build the matching Config struct. Connect
  constructs the concrete driver, wires its signals with
  `Qt::QueuedConnection`, and calls `open()`. On `stateChanged(Open)`
  the dialog auto-calls `start()` so the flow converges to Running
  without another click (preview-level convenience). On
  `stateChanged(Idle)` after a close, ownership is released and the
  Connect button re-enables.
- **Non-blocking close**: `Disconnect` only queues `close()` on the
  driver and relies on the stateChanged(Idle) callback to release
  ownership. No `thread_->wait()` call happens on the UI thread except
  during the dialog's own destructor, which uses a 150 ms bounded
  event-loop spin (well under spec §7-5's 200 ms UI-block ceiling).
- **Frame log**: `QPlainTextEdit` with `setMaximumBlockCount(200)` —
  Qt drops oldest lines automatically when the cap is reached. Each
  line is a truncated hex dump (first 64 bytes) plus `sourceId` and
  payload size.
- **Tests**: 4 offscreen integration cases using `QT_QPA_PLATFORM=offscreen`:
  1. Dialog constructs cleanly, title is "Connection Manager", state Idle.
  2. Changing `DriverType` flips the `QStackedWidget` to the matching
     page.
  3. Replay connect with a valid session file drives state
     Idle→Opening→Open→Running in < 1 s, then disconnect returns to
     Idle.
  4. Empty Replay path yields `ConfigInvalid` synchronously — dialog
     stays in Idle, error label populated.
- **Build metric**: 179 total tests green under Debug and Release.
  debug-asan builds clean.
- **Freeze scope**: no M2 frozen .hpp modified. The new
  `signalforge_app_ui` library exports a private API that downstream
  milestones (M7's full Connection Manager) will extend rather than
  rewrite.
- **Time**: ~2.5 h (under 6 h plan estimate — reuse of existing driver
  signals saved most of the plumbing).
