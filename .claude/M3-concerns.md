# M3 — Concerns and deviations

## Host ASan runtime blocked by `/etc/ld.so.preload`

**When**: Local execution of the `debug-asan` ctest preset.

**Symptom**: `ctest --preset debug-asan` fails test-discovery with
`ASan runtime does not come first in initial library list; you should
either link runtime to your application or manually preload it with
LD_PRELOAD.` The host's `/etc/ld.so.preload` injects a library that
clashes with ASan's own interception order. Reproduced during S5,
S6, S7, and S8 local verification.

**Resolution**: CLAUDE.md §Required §2 accepts this by naming CI as the
authoritative ASan gate when the local host blocks the runtime. Every
M3 commit message states this explicitly. All three CI matrix jobs
(`debug`, `release`, `debug-asan`) compile and test cleanly on the
ubuntu-24.04 runner (verified by run `24882236126` on 2026-04-24).

## Qt 6.10 concurrent-writeDatagram race on loopback

**When**: Two `QUdpSocket` instances in the same process, each on a
dedicated `QThread`, issue `writeDatagram` concurrently to each other's
bound loopback port (the exact scenario exercised by
`test_udp_driver_loopback.cpp::"udp loopback: bidirectional unicast
on 127.0.0.1"`).

**Symptom**: ~30–100% of runs, exactly one of the two concurrent
`writeDatagram` calls returns `-1` with
`QAbstractSocket::NetworkError` and `errorString() == "Unable to send
a message"`. Under `strace -f` (which slows execution) the failure
rate drops to zero, confirming a timing-sensitive race inside Qt. The
underlying `sendmsg(2)` syscall returns success when traced directly
— the error is produced in Qt's own write path.

**Reproducer**: a minimal self-contained probe (two `QUdpSocket`s each
on its own `QThread`, binding to loopback, concurrent `writeDatagram`)
reproduces the race deterministically on every iteration on this host
(Qt 6.10.2, Ubuntu 24.04, kernel 6.8). Source preserved in
`.claude/notes/qt-udp-concurrent-write-probe.cpp` for future
reference.

**Mitigation**: `UdpDriver::writeOnIoThread` performs a single retry on
`QAbstractSocket::NetworkError` (logged as a warning). Post-fix the
same driver path plus the M3 integration test pass 30/30 runs locally,
against ~70% pass pre-fix. A legitimate failure (unreachable host,
routing gone, etc.) surfaces on the second attempt and is reported as
`workerTxFailure` → `txFailures_` increment → `DriverErrorCode` stays
on `health()`.

**Scope**: The retry is bounded to one attempt, with no sleep, and only
on `NetworkError`. This is recovery from a known transient Qt-layer
anomaly, not silent error swallowing (a `SF_LOG_WARN` fires on every
retry so operational incidents remain visible in logs).

**Upstream**: Not yet reported to the Qt tracker. The reproducer is
minimal enough to file; doing so is outside M3 scope but noted as a
potential post-milestone follow-up.
