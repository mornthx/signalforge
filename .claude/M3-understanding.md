# M3 — Understanding

## 1. Restatement of the M3 goal

M3 transitions SignalForge from "M2 foundation" to "first functional milestone": four concrete drivers (`SerialDriver`, `TcpDriver`, `UdpDriver`, `ReplayDriver` skeleton), a shared IO-thread lifecycle helper (`IoWorkerBase`), a preview-level Connection Manager UI, integration tests against socat / localhost peers, performance baselines, and an error-injection test suite. The M2 freeze surface (`DriverInterface`, `RawFrame`, stats, backpressure, queues, platform utilities) is strictly read-only input to M3.

The milestone's **hard-stop type** is `Implementation correctness (with freeze-alignment verification)`: drivers either satisfy the frozen contract exactly, or they do not. Soft-HALT is not permitted (spec §3.8).

The stated quality philosophy (spec §1 and §9) is **user experience first, raw throughput second**: clean disconnect handling, human-readable error messages, responsive UI, no zombie threads at shutdown. A driver that handles a USB unplug cleanly and logs a clear error is more valuable than one that shaves microseconds off write-path latency.

## 2. Observed repo state

State reconciled at the start of the Phase 3 merge/bootstrap sequence:

```
$ git log --oneline origin/main -5
5bd73b6 Merge pull request #3 from mornthx/milestone/M2
e846cdb docs: add M3 concrete drivers and connection manager preview spec
0c47a83 docs: record M2 post-close fix in done and progress reports
ad59a1b app: wire M2 integrations into main.cpp; add application smoke test
2a4feb2 ci: install libcurl4-openssl-dev for sentry-native transitive dependency

$ git tag -l
v0.0.1-alpha.1
v0.0.2-alpha.1
v0.0.3-alpha.1        # M2 close (this session)

$ git log --oneline origin/milestone/M3 -1
5bd73b6 Merge pull request #3 from mornthx/milestone/M2
```

Phase 3 actions completed:

- PR #3 (M2) merged to main (merge commit `5bd73b6`).
- Annotated tag `v0.0.3-alpha.1` created on `5bd73b6` and pushed to origin.
- `milestone/M3` branch created from main at `5bd73b6`; pushed with upstream tracking.

**Incoming from M2** (frozen, read-only):
- `src/drivers/driver_interface.hpp` — `DriverInterface`, `DriverState`, `DriverErrorCode`, `DriverError`.
- `src/frame/raw_frame.hpp` — `RawFrame`, `RxStats`, `TxStats`, `DriverStatistics`, type aliases, `registerMetatypes()`.
- `src/frame/backpressure.hpp` — `BackpressureSignal`, `BackpressureReason`, `WatermarkTracker`.
- `src/utils/spsc_ring.hpp`, `mpsc_queue.hpp`, `snapshot.hpp` — SPSC/MPSC/Snapshot primitives.
- `src/platform/*` — `time_source`, `thread_utils`, `app_paths`, `crash_reporting`.
- `src/observability/*` — `logging` (sync; see M2 deviation), `metrics`.
- `tests/mocks/mock_driver.*` — a synchronous mock used in M2 unit + integration tests; M3 may reuse or supplement it.

**Network / SSH**: the HTTPS remote path has been unstable during M2 close; SSH access via `ssh.github.com:443` has been configured by the human and verified working. Remote URL on this branch is now `git@github.com:mornthx/signalforge.git`. All Phase 3 fetch/push operations after switching worked cleanly.

## 3. Ambiguities and contradictions identified

M3 spec is substantially more prescriptive than M2's — most design decisions are pre-locked in §3. Remaining ambiguities:

### 3.1 `SerialIoWorker` / `TcpIoWorker` / `UdpIoWorker` internal detail

**Spec position**: §4.2 explicitly marks these as internal to the .cpp, pimpl-like forward-declared in the header, not part of the freeze surface. `std::unique_ptr<SerialIoWorker>` requires the complete type at destruction site — same pimpl-with-out-of-line-destructor pattern we used for `MetricsRegistry::Impl` in M2.

**Default interpretation**: Each concrete driver's `.cpp` holds the full worker class definition. The driver's destructor is defined in the .cpp (not inline in the header) so `std::unique_ptr<Worker>` destruction sees the complete type.

**Spec clarification preferred**: NO — standard Qt/C++ pimpl idiom.

### 3.2 `open()` blocking vs non-blocking on the IO thread

M2's frozen contract (spec §4.1 clarification 2): `open()` returns `Success` synchronously when the request is accepted, with completion reported via `stateChanged(Open)` / `stateChanged(Error)` async.

**Spec §4.3 (TcpDriver open sequence step 4)** says "Use `QTcpSocket::waitForConnected(connectTimeout.count())` on the worker thread; or connect to `connected` / `errorOccurred` signals."

**Ambiguity**: `waitForConnected` blocks the IO thread until connect resolves or timeout. The public `open()` on the driver's caller thread returns `Success` immediately (per M2 contract). So "blocking" refers only to IO-thread behavior, not caller-thread behavior. But the signal-based alternative ("connect to `connected`") is more idiomatic and avoids blocking the IO thread's event loop.

**Default interpretation**: Use the signal-based path (`QTcpSocket::connected` + `QTcpSocket::errorOccurred`). The IO thread's event loop is necessary anyway (for `readyRead`), so blocking it with `waitForConnected` wastes the thread. A `QTimer::singleShot(connectTimeout, ...)` handles the timeout case.

**Spec clarification preferred**: LOW — default is the cleaner of the two paths the spec explicitly permits; I will document the choice in the TcpDriver Doxygen.

### 3.3 Driver destruction in non-Idle state

M2 Doxygen says "The precondition for destruction is `state() == Idle`." M3 drivers inherit this. But the spec §4.5 `~SerialDriver()` destructor example shows `thread_->quit(); thread_->wait(500); if still running: terminate()`. It does not first call `close()`.

**Ambiguity**: If the user destroys a driver while it's Running, does the destructor need to emit `stateChanged(Closing)` → `stateChanged(Idle)` before tearing down? Or is the precondition strict (user must `close()` first; destructor only handles the IO-thread joining)?

**Default interpretation**: Follow the spec's destructor example. The destructor's job is to cleanly terminate the IO thread; it does not guarantee `close()` semantics. Users who destroy a Running driver should expect warnings in the log (destructor logs "destroyed in non-Idle state") but no crash. In practice the Connection Manager always `close()`s before releasing the driver, matching the M2 contract.

**Spec clarification preferred**: LOW — consistent with M2 and with the spec §4.5 example. Will document explicitly in each driver's destructor Doxygen.

### 3.4 Serial driver device-permission detection

Spec §4.8 error taxonomy maps "Device path exists but no permission" to `PermissionDenied`. But `QSerialPort::open` returns `QSerialPort::PermissionError` for this case plus a couple other EACCES scenarios. Distinguishing true permission-denied from udev-lock-file-present from audit-subsystem-denied is not always possible from Qt.

**Default interpretation**: Map `QSerialPort::PermissionError` → `DriverErrorCode::PermissionDenied` with a message like "Could not open /dev/ttyUSB0: Permission denied. Try adding your user to the 'dialout' group, or check udev rules." Map every other error to `IoFailure` with the Qt error string appended.

**Spec clarification preferred**: NO — default is spec §4.9's message style ("context-rich, human-readable").

### 3.5 Socat process lifecycle in tests

Spec §5.3.1 says the integration test spawns socat as a child. It doesn't specify the exact mechanism (CMake custom target, Qt `QProcess` fixture, shell wrapper in `run_tests.sh`).

**Default interpretation**: Use a Catch2 RAII fixture — `SocatVirtualPair` class with ctor that spawns socat and stores its `QProcess*`, dtor that kills it. Symlink paths are deterministic (`/tmp/sf_ttyV0_<pid>`, `/tmp/sf_ttyV1_<pid>`) to allow parallel test execution without clashes. If socat binary is missing from $PATH, the fixture constructor throws; tests using the fixture are tagged `[integration][socat]` so `ctest -LE socat` skips them cleanly on hosts without socat.

**Spec clarification preferred**: LOW — any reasonable teardown works; I will document the chosen mechanism.

### 3.6 TCP echo server implementation

Spec §5.3.2 says "Test spawns a local echo server (CMake target or Qt-based internal helper — your judgment)".

**Default interpretation**: Qt-based internal helper (`EchoServerFixture`). Simpler to integrate, no external binary dependency, runs on the test thread's event loop. It binds to 127.0.0.1 at port 0 (OS-assigned), exposes the actual port via `EchoServerFixture::port()`, accepts exactly one client at a time (sufficient for M3 tests), echoes every `readyRead` back.

**Spec clarification preferred**: NO — default matches the spec's "your judgment".

### 3.7 Benchmark build target placement

Spec §5.4 says benchmarks are separate executables "not linked into `ctest` directly". It does not specify whether they participate in the top-level CMake tree.

**Default interpretation**: `tests/benchmark/CMakeLists.txt` is added via `add_subdirectory` from `tests/CMakeLists.txt` guarded by a CMake option `SIGNALFORGE_BENCHMARKS` (default OFF). Developers explicitly enable it with `-DSIGNALFORGE_BENCHMARKS=ON` to build the benchmark executables. Benchmarks are NOT wired into `catch_discover_tests`, so `ctest` never runs them. A helper script `tests/benchmark/run_baselines.sh` builds with the option enabled, runs all three benchmark binaries sequentially, and concatenates their output into `tests/benchmark/results/M3-baseline.md`.

**Spec clarification preferred**: NO — keeps benchmarks out of default build cost while still living in the same source tree (discoverable via git-grep, shares the same `signalforge_drivers` build).

### 3.8 Benchmark run environment on the dev host

Per memory `host_asan_preload.md`, the dev host has `/etc/ld.so.preload` interference. If this affects Serial / TCP latency measurements, §5.5 "Category 5 — Host-specific" applies: document the measurement, proceed with value as baseline.

**Default interpretation**: Run the baselines on the dev host. If any threshold is missed, attribute categorically per §5.5. If category cannot be identified unambiguously, HALT per §5.5's decision rule and ask the human whether to re-run on a clean VM or waive the threshold.

**Spec clarification preferred**: NO — procedure is explicit.

### 3.9 ReplayDriver session-file header

Spec §4.7 says `open()` reads "first header bytes (TBD by M9 spec, but at minimum 16 bytes)" and verifies "basic header sanity check" — but there is no M9 spec yet to define what "sanity" means.

**Default interpretation**: For M3 skeleton, "sanity check" means: file exists, file is ≥ 16 bytes, file's first 16 bytes are not all zero. A proper magic-byte check is M9's responsibility; M3 produces a `// TODO(M9): validate magic bytes and format version` marker. The `ProtocolFailure` error case in §4.8 is reserved for real M9 validation; in M3 it's only triggered by the "empty file" path (file < 16 bytes or all-zero).

**Spec clarification preferred**: LOW — M3 does not create session files (that's M8); the test fixture manually constructs a 16-byte non-zero header for the "open valid file" path and an empty file for the `ProtocolFailure` path.

### 3.10 Connection Manager modality

Spec §4.6 says "modal or modeless — your judgment, I lean toward modeless".

**Default interpretation**: Modeless. Users may want to inspect MainWindow state while a connection is active. Matches the "preview-level integration" goal in §3.3.

**Spec clarification preferred**: NO.

### 3.11 `DriverError::at` timestamp semantics when emitted async

M2's `DriverError::at` is a steady_clock time point. The spec is silent on whether the driver stamps it at error occurrence (on the IO thread) or at signal-emit time (same thread, essentially same moment).

**Default interpretation**: Stamp at error occurrence (the moment the driver decides to emit). Functionally equivalent since the IO thread doesn't introduce meaningful delay between the two, but it matches the semantic intent of "time of error".

**Spec clarification preferred**: NO.

## 4. Thread-affinity and lifecycle strategy

Per M2 contract + M3 spec §3.1, the topology is:

```
Main thread (or caller's thread)
  ├── driver (DriverInterface subclass, QObject)
  └── QThread (owned by driver)
       └── IoWorker (QObject, moveToThread'd to QThread)
```

All `DriverInterface` signals are defined on `DriverInterface` (so `frameReceived`'s metatype is the same for all drivers; consumers connect to the base class signal). Internally, each driver routes worker signals to the base-class signals via `Qt::QueuedConnection` — so the worker's `workerFrameReceived` is the emit point, and the `DriverInterface::frameReceived` re-emission happens on the IO thread. Consumers on the main thread connect with `Qt::QueuedConnection` and the signal is delivered to them via the Qt event loop.

### Wiring pattern (all four drivers)

```cpp
thread_ = std::make_unique<QThread>();
thread_->setObjectName(...);
worker_ = std::make_unique<SerialIoWorker>(config_, thread_->objectName());
worker_->moveToThread(thread_.get());

connect(thread_.get(), &QThread::started, worker_.get(), &IoWorker::onThreadStart);
connect(thread_.get(), &QThread::finished, worker_.get(), &IoWorker::onThreadFinish);

connect(worker_.get(), &SerialIoWorker::workerFrameReceived,
        this, &DriverInterface::frameReceived, Qt::QueuedConnection);
connect(worker_.get(), &SerialIoWorker::workerErrorOccurred,
        this, &DriverInterface::errorOccurred, Qt::QueuedConnection);
connect(worker_.get(), &SerialIoWorker::workerStateChanged,
        this, &DriverInterface::stateChanged, Qt::QueuedConnection);

thread_->start();
```

### Public-method routing

`open() / close() / start() / stop() / write()` on the driver execute on the caller's thread. They:

1. Validate config / preconditions synchronously and return an error code without state change on failure.
2. On success, emit the synchronous state transition (`Opening`, `Closing`, etc.) from the caller's thread (via the `stateChanged` signal — connections to this signal are `Qt::QueuedConnection`, so consumers still see them on their own thread).
3. Post a request to the worker via `QMetaObject::invokeMethod(worker_.get(), ..., Qt::QueuedConnection)` or a dedicated signal.
4. Return `Success` synchronously.

The worker processes the request on its own thread, performs the IO, emits its `workerXxx` signal, and the driver's `workerXxx → DriverInterface::xxx` queued connection re-emits to consumers.

### Destruction

`~SerialDriver()` quits the thread and waits ≤ 500 ms. If the thread does not exit in that window, we log an error and call `terminate()` followed by `wait()`. The destructor does NOT emit `stateChanged(Idle)` — that is the Connection Manager's responsibility via `close()` before release.

### Thread-affinity verification

Each concrete driver's unit tests use `QSignalSpy` under `Qt::DirectConnection` to capture the emitter thread, asserting it matches the internal IO `QThread`. The integration tests additionally verify that `Qt::QueuedConnection` delivery ends up on the consumer's thread. These are the same patterns M2's driver_lifecycle_with_mock integration test established.

## 5. HALT risks I anticipate

Ranked by likelihood:

### Rank 1 — Performance benchmark threshold miss without clear category (spec §5.5 HALT)

Benchmarks are measured on the dev host. Several thresholds (TCP 100 MB/s, UDP 50000 datagrams/sec) are ambitious for localhost with async signal delivery through Qt's event loop. Missing a threshold with category "CC code" is a bug I fix. Missing with category "Qt framework" or "host-specific" is documented and accepted. Missing with NO clear category is a HALT.

**Estimated probability**: ~20%. Mitigation: early baseline measurement with a minimal path before writing the full driver, so category attribution is clearer.

### Rank 2 — Socat availability and virtual serial behavior

socat's virtual serial pairs have subtle quirks (flow control, open-close reliability, race when both symlinks are accessed simultaneously). The spec §7-3 HALT trigger is specifically "socat unavailable on build host at test time".

**Estimated probability**: ~15% (spec mandates socat in CI; on dev host, socat is installed per memory-noted tooling but not confirmed; need to verify and install if missing before implementing S3).

Mitigation: verify socat present at plan start; mark installation-needed in `.claude/M3-concerns.md` if missing.

### Rank 3 — Connection Manager UI blocks main thread (spec §7-5 HALT)

Any GUI path that blocks the main thread for >200 ms fails the spec's quality goal. Risks:
- Synchronous `close()` that waits on the IO thread join (up to 500 ms per §4.5's budget).
- Direct signal connection from a worker thread to a UI widget (thread-affinity violation).

**Estimated probability**: ~10%. Mitigation: `close()` from the UI dispatches via `QTimer::singleShot(0, driver, &DriverInterface::close)` or similar so the 500 ms thread-join budget is absorbed on the main event loop, not a blocking call. The UI shows an "Disconnecting..." state until `stateChanged(Idle)` fires.

### Rank 4 — Test flakiness under stress (spec §7-4 HALT)

Any intermittent failure in integration tests (race on port bind, socat startup delay, QTcpSocket `readyRead` batching) is a HALT, not a retry-until-green scenario.

**Estimated probability**: ~15%. Mitigation: each integration test uses deterministic synchronization (wait-for-signal with QSignalSpy::wait, not QTest::qWait polling). Tests retry on network "address in use" at fixture level, but never re-run assertions to mask a real race.

### Rank 5 — Qt thread cleanup leaves zombie thread (spec §7-6 HALT)

`QThread::wait(500)` returning false indicates the thread did not exit cleanly. Spec treats this as HALT.

**Estimated probability**: ~5%. Mitigation: ensure every worker connects its `onThreadFinish` slot to clean up any resources (sockets, serial ports) before the thread exits. Integration tests include a destruction-under-load scenario to catch this.

### Rank 6 — M2 freeze-surface accidental violation (spec §7-1 HALT)

If any M3 implementation needs to modify an M2-frozen header, HALT.

**Estimated probability**: ~3%. Mitigation: the M2 contract is well-specified; M3's adds are purely additive. The risk is mainly accidental (IDE auto-include reshuffle, clang-format affecting a frozen file). I will verify no .hpp under the frozen path is modified by running `git diff --name-only` against the merge base before each commit.

## 6. What I will not do in M3

Per spec §2.2 and CLAUDE.md:

1. **No modifications to any M2-frozen .hpp**: `src/drivers/driver_interface.hpp`, `src/frame/*.hpp`, `src/utils/*.hpp`, `src/platform/*.hpp`. If I think a modification is needed, HALT.
2. **No frame pipeline (decoder) wiring**: drivers emit `RawFrame`; what decodes them is M4.
3. **No multi-connection Connection Manager**: the M3 preview is single-instance.
4. **No yaml config load/save**: M7 territory.
5. **No session write/replay content**: ReplayDriver is skeleton only; writing is M8, playing real content is M9.
6. **No Modbus / CAN driver stubs**: V1.5 per architecture §3.3.
7. **No DriverFactory**: concrete drivers are constructed directly.
8. **No new top-level dependencies beyond the existing set + `socat` (runtime test tool, not a library)**.
9. **No benchmark CI integration**: benchmarks run locally or in a future dedicated workflow (spec §5.7 defers).
10. **No PR merge or tag operations** beyond the Phase 3 steps explicitly listed in CLAUDE.md §Git operation protocol. Phase 5 and later are separate sessions.

## 7. Review checkpoints expected

Per CLAUDE.md §Git operation protocol and the M3 spec's implicit phasing:

- **This session (Phase 3)**: closes with `.claude/M3-understanding.md` and `.claude/M3-plan.md` committed and pushed to `milestone/M3`. Announce "M3 understanding and plan ready for review. Awaiting execute approval."
- **Phase 4 (human checkpoint B)**: Human reviews both documents. Authorization phrase: `approved, execute M3` (or literal equivalent).
- **Phase 5 (CC autonomous)**: Implements S1 through S12 per the plan. Each subtask commits with a freeze-scope awareness note in the commit body.
- **Phase 1 of M3 close (at end of Phase 5)**: push, CI green, PR created against main, `.claude/M3-done.md` written, announce "M3 ready. Awaiting approval to merge M3 and begin M4 bootstrap."
- **Phase 2 / Phase 3**: standard milestone closure flow per CLAUDE.md.

I will not merge Phase 4 with Phase 2, nor Phase 1 with Phase 3. Each checkpoint is a distinct human review.
