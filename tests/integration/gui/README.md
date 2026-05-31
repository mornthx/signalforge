# `tests/integration/gui/` — V1+ GUI integration test framework

CI release-binary GUI smoke + the M14 S6 mechanical 18-test
automation scaffold. Born from the M13/M14 release-prereq
audit cycle (ADR-008 → ADR-013, run 1 → run 6).

## What's here

| File | Purpose |
|---|---|
| `release_binary_smoke.sh` | M14 S1 — release-binary GUI smoke. Headless launch + UDP fixture + Tier A pixel diff + Tier B log grep. The canonical scene-graph + qrc + chart-host regression net. |
| `test_release_binary_smoke.cpp` | Catch2 wrapper that invokes the shell harness, so the smoke is discoverable by ctest. |
| `run_mechanical_18.sh` | M14 S6 — mechanical 18-test automation scaffold. Wraps `release_binary_smoke.sh` for T3; T1/T2/T5/T7/T8/T10/T11 are documented V1.0.1 automation candidates (see "Future automation" below). |
| `fixtures/m14_smoke.yaml` | UDP fixture connection at `127.0.0.1:9998` with `decoderSchemaId=temperature_sensor`. Reused by both harnesses. |
| `helpers/udp_fixture_sender.py` | 50 Hz `temperature_sensor`-shaped frame sender (Python stdlib only). |
| `helpers/udp_high_rate_feeder.py` | High-rate (5 kHz default) feeder for T11 backpressure smoke; reserved for V1.0.1 once the timing race below is resolved. |

## Mechanical 18-test mapping

The M13 18-test HW verification protocol
(`docs/m13-hardware-verification.md`) splits into:

| Test | Mode |
|---|---|
| T3 UDP driver connect + decode + chart paint | **CC-automated via `release_binary_smoke.sh`** (M14 S1) |
| T4 Replay file picker | Operator-visual (file-open dialog) |
| T6 Auto-connect on startup | Unit-covered through `ConnectionManager::connectStartupConnections`; GUI smoke can reuse persisted `autoConnectOnStartup=true` fixtures when needed |
| T9 Quit-while-recording prompt | Operator-visual (confirm dialog) |
| T13–T17 Replay UI (charts populate, Play/Pause, Step, Scrubber, Speed) | Operator-visual (chart rendering + button feedback) |
| T18 Live ↔ Replay 3-option dialog | Operator-visual |
| T12 Disk-full | Skip (requires sudo + tmpfs; not worth automating for V1.0) |
| **T1, T2, T5, T7, T8, T10, T11** | **V1.0.1 follow-up automation** (see below) |

## V1.0.1 follow-up automation

Why these aren't automated for V1.0:

- **T1 Serial / T2 TCP**: same fixture-pattern as T3 once
  driver-specific feeders (socat PTY, Python TCP server) are
  added. Ready-but-deferred for V1.0 to keep the harness
  scope tight.

- **T5 Edit/Remove**: yaml-roundtrip test. Direct
  `ConnectionManager` API tests in
  `tests/integration/test_connection_lifecycle_full_stack.cpp`
  + ADR-013 F17 persistence smoke would be the cleanest path
  rather than driving via the GUI binary.

- **T7 Recording GUI round-trip / T8 Across-restart / T11
  Backpressure**: blocked on a headless-environment race in
  `UdpDriver::onReadyRead`'s level-vs-edge semantics under
  `Q_QPA_PLATFORM=offscreen` + `xvfb-run` + Qt's
  QueuedConnection scheduling. When the harness backgrounds a
  UDP feeder before the driver's IO worker thread sets
  `running_=true`, kernel-buffered datagrams accumulate
  (`Recv-Q > 0`) but `QUdpSocket::readyRead` does not always
  re-fire after `running_` flips true. Operator's natural
  real-X11 dogfood (run-6) does NOT trigger this — the
  manual click sequence (Click Connect → driver fully
  Running → start feeder) avoids the race entirely. F6 + F17
  + F19 + F9 are operator-validated end-to-end (run-6:
  13,761 records natural Connect → Record).

  V1.0.1 automation candidates:
  - Add `UdpIoWorker::startOnIoThread` post-`running_=true`
    logic that explicitly drains pending datagrams (calls
    `onReadyRead()` once after setting the flag). Likely
    one-line fix; needs a bench-soak round-trip to confirm
    no perf regression at line rate.
  - OR: switch to a QTest framework GUI test that uses the
    real X server (no offscreen quirks).
  - OR: integrate `xdotool` to drive a real signalforge GUI
    instance.

- **T10 Mid-stream catalog**: needs two driver fixtures
  with staggered Connect timing. Builds naturally on T2 + T7.

## Running locally

```
# CI smoke (works without changes; PASSES Tier A + Tier B):
bash tests/integration/gui/release_binary_smoke.sh \
    --binary build/release/src/app/signalforge \
    --repo-root $(pwd)

# Mechanical 18-subset (currently routes only T3 to release_binary_smoke):
bash tests/integration/gui/run_mechanical_18.sh \
    --binary build/release/src/app/signalforge \
    --repo-root $(pwd)
```

The `release_binary_smoke.sh` test is also wired into ctest
under target `test_release_binary_smoke` (Catch2-discovered).
The mechanical-18 harness runs as a standalone script — ctest
integration is V1.0.1 work alongside the wider mechanical
subset.

## Headless environment

Both harnesses set:

- `xvfb-run --auto-servernum --server-args="-screen 0 1280x800x24"`
- `QSG_RHI_BACKEND=software` — software RHI for portability
- `SF_F4_DIAG=1` — env-gated diagnostic in `Chart::updatePaintNode`
  appends an orange `QSGSimpleRectNode` to the QSG root each
  paint pass. The rect is a solid fill, rasterizes under
  software-RHI (1-px line strips do NOT reliably rasterize),
  and proves: scene-graph submission + chart paint hooks +
  QQuickWindow render pass + QQuickWidget framebuffer capture
  are all alive.

Production users do **not** set `SF_F4_DIAG`; the rect is
invisible in normal operator sessions. Real-X11 chart line
verification stays operator-driven (run-5 + run-6).

## V1+ governance role

This directory is the V1+ permanent regression net for the
C++ ↔ QML hand-off chain that produced ADR-008 / ADR-009 /
ADR-010 / ADR-011 / ADR-013 during M13/M14 release-prereq
work. Every V1.5+ patch must keep `release_binary_smoke.sh`
green. The persistence + recording smoke extensions
(documented above as V1.0.1 candidates) close the remaining
audit gaps.
