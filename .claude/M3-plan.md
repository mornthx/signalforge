# M3 — Plan

## 1. Subtask breakdown

Twelve ordered subtasks. All are Blocking (per M2 precedent — no partial delivery for an implementation-correctness milestone). Each subtask gets its own commit unless explicitly marked as batched.

### S1 — Shared foundation: `driver_configs.hpp` + `IoWorkerBase`

- **Output files**:
  - `src/drivers/driver_configs.hpp` — the four `*Config` structs per spec §4.1
  - `src/drivers/io_worker_base.hpp`, `.cpp` — abstract IoWorker base per spec §4.5
  - `src/drivers/CMakeLists.txt` update (link `signalforge_platform` for `setCurrentThreadName`)
  - `tests/unit/drivers/driver_configs_test.cpp` — default-init + field population
  - `tests/unit/drivers/io_worker_base_test.cpp` — constructor, threadName()
- **Classification**: Blocking.
- **Prerequisites**: None (uses only Qt + M2 frozen surface).
- **Effort estimate**: 3 h.
- **Commit point**: Yes — single commit `drivers: add driver_configs + IoWorkerBase foundation`.
- **Freeze scope**: NOT part of freeze; both files are M3 additions that may evolve in M4+ (no external caller references them beyond the concrete drivers).

### S2 — ReplayDriver skeleton

- **Output files**:
  - `src/drivers/replay_driver.hpp`, `.cpp` with `ReplayIoWorker` in the .cpp
  - `tests/unit/drivers/replay_driver_test.cpp`
  - `tests/integration/test_replay_driver_skeleton.cpp` per spec §5.3.4
  - `tests/integration/fixtures/minimal_session.sfreplay` (16-byte non-zero header)
- **Classification**: Blocking.
- **Prerequisites**: S1 (uses `IoWorkerBase`, `ReplayConfig`).
- **Effort estimate**: 3 h.
- **Commit point**: Yes — commit `drivers: add ReplayDriver skeleton with lifecycle-only worker`.
- **Freeze scope**: not frozen; ReplayDriver body is TODO(M9) beyond lifecycle.
- **Rationale**: Simplest concrete driver (no real IO). Validates the `IoWorkerBase` pattern before the heavier Serial/TCP/UDP drivers. If the pattern is wrong, we find out cheapest here.

### S3 — SerialDriver + unit tests

- **Output files**:
  - `src/drivers/serial_driver.hpp`, `.cpp` with `SerialIoWorker` in the .cpp
  - `src/drivers/CMakeLists.txt` update (link `Qt6::SerialPort`)
  - `tests/unit/drivers/serial_driver_test.cpp` — lifecycle, write-state-gates, stats, error-taxonomy-subset testable without a real device
- **Classification**: Blocking.
- **Prerequisites**: S1.
- **Effort estimate**: 5 h.
- **Commit point**: Yes — commit `drivers: add SerialDriver over QSerialPort`.
- **Freeze scope**: not frozen (concrete drivers' public API is implementation surface, not the DriverInterface freeze).

### S4 — Serial integration test against socat virtual pair

- **Output files**:
  - `tests/integration/socat_fixture.hpp`, `.cpp` — RAII helper that spawns/kills socat
  - `tests/integration/test_serial_driver_loopback.cpp` — scenarios per spec §5.3.1
- **Classification**: Blocking.
- **Prerequisites**: S3.
- **Effort estimate**: 3 h.
- **Commit point**: Yes — commit `tests: add Serial loopback integration against socat`.
- **HALT-primed**: spec §7-3 — socat unavailable. Will verify socat on PATH before starting S4; if missing, record in `.claude/M3-concerns.md` and HALT with clear remediation.

### S5 — TcpDriver + unit tests

- **Output files**:
  - `src/drivers/tcp_driver.hpp`, `.cpp` with `TcpIoWorker` in the .cpp
  - `src/drivers/CMakeLists.txt` update (link `Qt6::Network`)
  - `tests/unit/drivers/tcp_driver_test.cpp`
- **Classification**: Blocking.
- **Prerequisites**: S1.
- **Effort estimate**: 5 h.
- **Commit point**: Yes — commit `drivers: add TcpDriver over QTcpSocket`.

### S6 — TCP integration test against in-process echo server

- **Output files**:
  - `tests/integration/echo_server_fixture.hpp`, `.cpp` — Qt-based 127.0.0.1 echo server
  - `tests/integration/test_tcp_driver_echo.cpp`
- **Classification**: Blocking.
- **Prerequisites**: S5.
- **Effort estimate**: 3 h.
- **Commit point**: Yes — commit `tests: add TCP echo integration`.

### S7 — UdpDriver + unit tests

- **Output files**:
  - `src/drivers/udp_driver.hpp`, `.cpp` with `UdpIoWorker` in the .cpp
  - `tests/unit/drivers/udp_driver_test.cpp`
- **Classification**: Blocking.
- **Prerequisites**: S1.
- **Effort estimate**: 4 h.
- **Commit point**: Yes — commit `drivers: add UdpDriver over QUdpSocket`.

### S8 — UDP integration test

- **Output files**:
  - `tests/integration/test_udp_driver_loopback.cpp` — bidirectional 127.0.0.1, multicast loopback
- **Classification**: Blocking.
- **Prerequisites**: S7.
- **Effort estimate**: 3 h.
- **Commit point**: Yes — commit `tests: add UDP loopback integration`.

### S9 — Error-injection test suite

- **Output files**:
  - `tests/integration/test_driver_error_paths.cpp` covering the extended set per spec §3.5 + §4.8:
    - Mid-run Serial disconnect (kill socat)
    - Mid-run TCP disconnect (close echo server)
    - Rapid open/close cycles (100 iterations, no leaks)
    - open() → close() before Open reached (race)
    - start() without open() → NotConfigured
    - Writing to a driver whose state just became Error
- **Classification**: Blocking.
- **Prerequisites**: S4, S6, S8.
- **Effort estimate**: 4 h.
- **Commit point**: Yes — commit `tests: add driver error-injection extended set`.

### S10 — Performance benchmarks

- **Output files**:
  - `tests/benchmark/CMakeLists.txt` (gated by `-DSIGNALFORGE_BENCHMARKS=ON`)
  - `tests/benchmark/bench_driver_throughput.cpp`
  - `tests/benchmark/bench_driver_latency.cpp`
  - `tests/benchmark/bench_driver_footprint.cpp`
  - `tests/benchmark/run_baselines.sh` — driver script writing `tests/benchmark/results/M3-baseline.md`
  - `tests/benchmark/results/M3-baseline.md` — the committed baseline, with category classification per spec §5.5 for any threshold miss
- **Classification**: Blocking.
- **Prerequisites**: S4, S6, S8 (benchmarks use the socat / echo-server fixtures).
- **Effort estimate**: 5 h (including measurement runtime).
- **Commit point**: Yes — commit `bench: add driver throughput/latency/footprint baselines`.
- **HALT-primed**: per spec §5.5, threshold miss without clear category → HALT.

### S11 — Connection Manager UI preview

- **Output files**:
  - `src/app/connection_manager.hpp`, `.cpp` (hand-coded QDialog; no .ui file)
  - `src/app/CMakeLists.txt` update
  - `src/app/main_window.cpp` (+ `main_window.hpp`) — add `File → Connection Manager...` menu item. The menu wiring is a small incremental change; the menu bar setup is an M0 artifact we extend.
  - `tests/integration/test_connection_manager.cpp` — offscreen UI test: dialog opens, driver type selector works, Connect/Disconnect state badge transitions correctly using a mock driver or replay driver
- **Classification**: Blocking.
- **Prerequisites**: S2 (needs at least one constructable driver for live-UI smoke).
- **Effort estimate**: 6 h.
- **Commit point**: Yes — commit `app: add Connection Manager dialog (preview)`.
- **HALT-primed**: spec §7-5 — any UI path blocking main thread >200 ms.

### S12 — CI workflow + M3 completion report

- **Output files**:
  - `.github/workflows/ci.yml` — add `socat` to apt-install step
  - `.claude/M3-done.md` — completion report per execution-manual §6.2 with benchmark summary inline, error-injection coverage matrix, M4 hand-off notes per spec §6
  - `.claude/M3-concerns.md` — any deviations surfaced during S1–S11
  - `.claude/M3-progress.md` — finalize
- **Classification**: Blocking.
- **Prerequisites**: S1–S11 green.
- **Effort estimate**: 3 h.
- **Commit point**: Yes — commit `chore: M3 completion report and CI socat wiring`.
- **Post-commit**: push `milestone/M3`; wait CI green; create PR to main (per session authorization — the Phase 5 session will include this authorization).

## 2. Implementation order rationale

**Order**: shared foundation → simplest driver (replay) → full driver matrix (Serial → TCP → UDP) with integration tests interleaved → error injection → benchmarks → UI preview → close.

Rationale:

1. **S1 first** establishes `IoWorkerBase` and the config structs. Every subsequent subtask depends on these.

2. **S2 (Replay) second, not last**, because it's the simplest real driver. Landing it early validates the IoWorkerBase pattern with minimal surface; errors are localized. The spec lists ReplayDriver last, but the plan is ordered by dependency risk, not deliverable-list order.

3. **S3 → S4 (Serial + its integration test) before S5–S6**: Serial has the most edge cases (permission, udev, device hot-unplug). Resolving it early reduces late-milestone surprise.

4. **S5 → S6 (TCP + echo)**: simpler failure model than Serial; socket errors are more uniform across OSes.

5. **S7 → S8 (UDP)**: datagram semantics differ enough from TCP that UDP warrants its own block; having TCP done first lets S7 reuse the IoWorker pattern variant more confidently.

6. **S9 (error injection)** composes the fixtures from S4/S6/S8. Running it before benchmarks catches bugs that would skew measurements.

7. **S10 (benchmarks)** last of the implementation subtasks for a reason: measurements of broken code are noise. Drivers must be correct before we measure them.

8. **S11 (UI)** near the end: the UI exercises drivers end-to-end through a real event loop. Any surprises here (blocking operations, thread-affinity bugs) are caught just before close, where the fix is still reasonable in scope.

9. **S12 (report + CI)** formalizes the closure.

**Alternative considered**: Implementing all four drivers first, then all integration tests, then benchmarks. Rejected because: (a) a long stretch of driver work with no end-to-end verification lets class-of-bugs accumulate, (b) integration tests per-driver mean each concrete driver has a test that exercises its public contract before we move on.

## 3. Test strategy summary

Per spec §5, layered test surface:

| Layer | Binary | Scope |
|---|---|---|
| Unit | `drivers_test` (extended in S3/S5/S7) | Per-driver lifecycle, state machine, statistics snapshots, compile-time `Q_DISABLE_COPY_MOVE`, metatype round-trip |
| Unit | `drivers_foundation_test` (new in S1) | `IoWorkerBase`, `*Config` structs |
| Unit | `replay_driver_test` (new in S2) | ReplayDriver skeleton lifecycle + write() → NotConfigured |
| Integration | `test_serial_driver_loopback` (S4) | socat pair, bidirectional IO, throughput sanity, mid-run disconnect |
| Integration | `test_tcp_driver_echo` (S6) | Echo server, bidirectional IO, graceful + RST close, timeout path |
| Integration | `test_udp_driver_loopback` (S8) | Unicast both ways, multicast loopback, datagram boundary preservation |
| Integration | `test_replay_driver_skeleton` (S2) | Lifecycle, nonexistent/empty file error paths |
| Integration | `test_driver_error_paths` (S9) | Extended error-injection set |
| Integration | `test_connection_manager` (S11) | Offscreen UI smoke via `QT_QPA_PLATFORM=offscreen` |
| Benchmark | `bench_driver_throughput`, `bench_driver_latency`, `bench_driver_footprint` (S10) | Gated by CMake option; NOT run in ctest |

Per-module coverage projection:

| Module | Projection |
|---|---|
| `io_worker_base.cpp` | 75% (per spec §5.1 baseline; abstract base) |
| `serial_driver.cpp` | 88% |
| `tcp_driver.cpp` | 88% |
| `udp_driver.cpp` | 85% |
| `replay_driver.cpp` | 75% (skeleton, most of M9's code is TODO) |
| `connection_manager.cpp` | 65% (UI; compensate with integration test) |

Modules at risk for threshold: `replay_driver` (75% spec floor but skeleton makes higher hard to achieve) and `connection_manager` (60% spec floor per §5.1). Both have explicit lower floors per spec; no deviation expected.

## 4. Highest-risk subtasks for HALT

### Risk 1 — S10 benchmarks threshold miss without clear category

Several thresholds are ambitious:
- Serial 921600 baud ≥ 90 KB/s: socat virtual serial's actual ceiling depends on the kernel tty layer; may or may not reach 90 KB/s.
- TCP localhost ≥ 100 MB/s: QTcpSocket with signal/slot dispatch has known overhead; hitting 100 MB/s depends on batching behavior.
- UDP ≥ 50000 datagrams/sec: Qt's `QUdpSocket::readyRead` emission pattern may bottleneck before the kernel does.

**Mitigation**: before running formal benchmarks, do a 30-second sanity measurement per driver; if numbers are wildly off, investigate path before declaring threshold miss.

**Residual HALT probability**: ~15%.

### Risk 2 — S4 socat environment missing or misconfigured

**Mitigation**: verify socat at S4 start (first action of the subtask: `which socat && socat -V | head -1`). If missing, add to `.claude/M3-concerns.md` and HALT with remediation ("apt-get install -y socat").

**Residual HALT probability**: ~5%.

### Risk 3 — S11 UI blocks main thread

The UI's "Connect" button calls `driver->open()` which is synchronous-accepts but state transitions are async via `stateChanged`. The UI should not call `waitForConnected` or any other blocking API.

**Mitigation**: all UI-to-driver dispatch uses Qt signals + state observation; no `wait*` APIs in the UI path. `close()` is dispatched via `QTimer::singleShot(0, driver, [driver]{ driver->close(); })` so the ≤ 500 ms thread-join budget (M3 spec §4.5) absorbs on the event loop, not on a blocking UI call.

**Residual HALT probability**: ~5%.

### Risk 4 — Mid-run Serial unplug / TCP RST timing-sensitive

Error-injection tests that simulate disconnect (kill socat, close echo server) must emit `errorOccurred(ResourceLost)` within a bounded window. If Qt's signal delivery is batched across the event loop, the test may observe the error later than expected.

**Mitigation**: tests use `QSignalSpy::wait(timeout)` not `QTest::qWait(fixed)`. Timeouts are generous (5 s) so jitter doesn't flake.

**Residual HALT probability**: ~10%.

## 5. Performance benchmark execution plan

Per spec §5.4 + §5.5:

1. **Sanity pass** (pre-S10): one-off rapid measurement per driver (30 s each). If any measurement is >50% below threshold or shows obvious bugs (throughput collapsing under load, memory growing unbounded), HALT at S10 start to investigate.

2. **Formal baseline run** (during S10):
   - Throughput: 10-second sustained measurement per driver configuration
   - Latency: 100 seconds × 4 drivers = 400 seconds of measurement
   - Footprint: 100 open/close cycles per driver for RSS growth

3. **Results output**:
   - Script `tests/benchmark/run_baselines.sh` writes `tests/benchmark/results/M3-baseline.md` with a table per benchmark, each row including: driver, configuration, measurement, threshold, pass/miss, and (on miss) category classification per §5.5.

4. **Categorization rule on miss**:
   - Category 1 (CC code) → fix and retry; no category 1 miss should make it into the committed baseline.
   - Categories 2–5 → document and accept as the category-attributed baseline.
   - No clear category → HALT per §5.5 decision rule.

5. **Environmental note**: the dev host has `/etc/ld.so.preload` interference per memory `host_asan_preload.md`. If benchmark timing is visibly affected, attribute category 5 (host-specific) and document.

## 6. Hand-off preparation

`.claude/M3-done.md` will include:

**Completion sections** (standard):
- Timing
- Deliverables checklist vs spec §2.1
- Acceptance self-check per spec §8
- PR and merge state (filled after PR created)
- HALTs raised during milestone
- Deviations and concerns (pointer to `.claude/M3-concerns.md`)
- Test result summary (unit/integration counts per preset)
- Benchmark results summary (inline, with category attribution)
- Error-injection coverage matrix (one row per spec §4.8 error-taxonomy entry)

**Hand-off to M4** (spec §6):
- Drivers emit `frameReceived`; M4 connects decoders to this signal
- ReplayDriver skeleton available for pipeline end-to-end testing
- Performance baseline numbers are M4's no-regression target
- Connection Manager UI optionally integrates decoded values in M4 or M5

**Hand-off to human**:
- Manual UI verification steps (Connect via Serial with real device, if available; UI responsiveness spot-check)
- Real-hardware tests deferred (M3 uses only virtual peers)
- Note on benchmark environment (host-specific quirks if any)

**Impact analysis**:
- DriverInterface freeze compatibility: all four concrete drivers satisfy the frozen contract verbatim
- No `.hpp` under `src/drivers/driver_interface.hpp`, `src/frame/**`, `src/utils/**`, `src/platform/**` is modified
- Freeze sha256sum verification command from `.claude/M2-done.md` re-runs cleanly against milestone/M3 tip

## 7. Open questions carried into Phase 5

None that block execution. Items documented in `.claude/M3-understanding.md §3` are all resolved by default interpretation; any that surface real issues during implementation will be added to `.claude/M3-concerns.md` with the usual "document + proceed" or "HALT and ask" disposition per CLAUDE.md §Ambiguity handling.
