# Milestone Roadmap

**Status**: Authoritative plan for V1 development.

This document defines the twelve milestones (M0–M11) that make up V1. Each milestone is a unit of work CC can execute autonomously within a single session, bounded by a human hard stop.

**Companion documents**:

- `docs/architecture/architecture.md` — technical baseline
- `docs/claude-code/execution-manual.md` — CC operating rules
- `docs/milestones/M<n>-<slug>.md` — detailed per-milestone specs, produced as each milestone approaches

**Cross-reference notation**:

- `[EM §N]` — Execution Manual, section N
- `[Arch §N]` — Architecture document, section N
- `[MR M<n>]` — this Milestone Roadmap, entry for milestone `M<n>`

---

## 1. Overview

| ID | Name | Sprint | Effort (person-days) | Hard-stop type |
|---|---|---|---|---|
| M0 | Project Bootstrap | 1 | 5 | Structural review |
| M1 | Qt Quick Integration Spike | 1–2 | 5 | Technical decision |
| M2 | Platform + Core Abstractions | 2 | 5 | Interface freeze |
| M3 | Serial / TCP / UDP Drivers | 2–3 | 10 | Hardware acceptance |
| M4 | Frame Layer + Decode Page | 3–4 | 10 | Schema freeze |
| M5 | Signal Layer + Observe Skeleton | 5 | 5 | Interface freeze |
| M6 | High-frequency Chart Component | 5–6 | 10 | **Performance certification** |
| M7 | Control Page | 7–8 | 10 | Hardware loop |
| M8 | Session Writer + File Format | 9 | 5 | Format freeze |
| M9 | Replay (Complete) | 10–11 | 10 | Equivalence verification |
| M10 | Performance Optimization | 13–14 | 10 | **Performance certification** |
| M11 | Polish and Packaging | 15–16 | 10 | Release readiness |

Total estimate: 95 person-days. With two engineers in parallel and the serialization constraints in §2, the theoretical floor is 10–12 weeks. The 16-week calendar reserves roughly 40% buffer for HALT resolution, hardware coordination, and the optional Qt 6.12 LTS migration described in `[Arch §12.6]`.

---

## 2. Dependencies and Parallelism

```
M0 ──► M1 ──► M2 ──► M3 ──► M4 ──► M5 ──► M6 ──► M7 ──► M8 ──► M9 ──► M10 ──► M11
                │
                └── (an M1 fallback verdict forces an M6 rendering-approach rewrite)
```

### Where parallelism is possible

- Within M5, the `SignalStore`, numeric cards, and signal tree can be split across two engineers on the same milestone branch.
- Within M7, the UI and the backend `ActionScheduler` can proceed in parallel.
- Within M11, packaging work and documentation can proceed in parallel.

### Where parallelism is forbidden

- Until M1 is resolved, M5 and M6 must not start. The chart approach depends on M1's verdict.
- Until M2's Driver interface is frozen, M3 must not start.
- Until M4's decode rule schema is frozen, M5 must not start.
- Until M8's `.sfr` format is frozen, M9 must not start.

These are hard gates. Human review is required to cross each of them.

---

## 3. Milestone Summaries

Each milestone has a detailed spec at `docs/milestones/M<n>-<slug>.md`, produced as the milestone approaches. This section gives scope, deliverables, hard-stop semantics, and critical constraints.

### M0 — Project Bootstrap

**Goal**: Establish a stable engineering skeleton on which all subsequent milestones can build.

**Deliverables**:

- Top-level `CMakeLists.txt` + `CMakePresets.json` (Qt 6.10.2, GCC 12, Ninja)
- Module directories per `[Arch §18]`
- Per-module `CMakeLists.txt` and placeholder files
- Third-party dependencies via `FetchContent`: spdlog, Catch2, moodycamel, ExprTk, yaml-cpp, nlohmann/json
- `.clang-format`, `.clang-tidy`, `.editorconfig`, `.gitignore`
- GitHub Actions workflow with three jobs on `ubuntu-22.04`: Debug, Release, Debug+ASan
- Minimal app: a `QMainWindow` that opens and closes cleanly
- `CLAUDE.md` at repo root, copied verbatim from `[EM §2]`
- `README.md`, `CONTRIBUTING.md`, placeholder `LICENSE`
- Docs structure: `architecture/`, `milestones/`, `claude-code/` under `docs/`
- `.claude/` working directory with its own `.gitignore`

**Out of scope**: any business logic, any Driver implementation, Crashpad integration, any UI beyond an empty window.

**Hard stop**: human structural review. Do the module boundaries match the architecture? Do the presets build cleanly on a fresh Ubuntu 22.04 + Qt 6.10.2 machine? Is CI green?

**Detailed spec**: `docs/milestones/M0-project-bootstrap.md`.

---

### M1 — Qt Quick Integration Spike

**Goal**: Answer the question "can we actually use `QQuickWidget` in production?" before committing to it in M6.

**Deliverables**:

- `tools/spike/qquick_dock_test/` — standalone program embedding three `QQuickWidget` instances in floating `QDockWidget`s
- Test coverage of all five concerns in `[Arch §8.5]`:
  - floating, re-docking, cross-monitor drag
  - HiDPI scaling (125% / 150% / 175% / 200%)
  - context-menu event propagation across the Widgets ↔ Quick boundary
  - hide / show render-thread lifecycle
  - multi-instance GPU resource usage
- `docs/spikes/M1-qtquick-integration.md` — written report with method, measurements, screenshots, and a verdict per concern

**Out of scope**: production chart component (that is M6); raw performance benchmarks (only integration correctness is measured here).

**Hard stop**: human technical decision. The report must give numbers, not prose verdicts. The human then chooses:

1. **Go** — continue with `QQuickWidget`; no change to M6 plan.
2. **Downgrade** — use `QWidget::createWindowContainer` + `QWindow`; rewrite M6 rendering plan.
3. **Aggressive downgrade** — `QPainter` + OpenGL; major M6 rewrite, schedule slip possible.

**Specific spike requirements**:

- A `QDockWidget` floats, re-docks, and moves across monitors with no visual glitches and no crashes.
- HiDPI is verified via the `QT_SCALE_FACTOR` environment variable at four scaling levels.
- A right-click on a Qt Quick element surfaces a Qt Widgets `QMenu` with correct event propagation.
- Hiding the dock for 10 seconds and showing it again produces no leaks (verified with valgrind sampling).
- With three `QQuickWidget`s active, GPU memory usage is recorded via `intel_gpu_top` or equivalent.

Numbers in the report are mandatory. Adjectives are not acceptable verdicts.

---

### M2 — Platform + Core Abstractions

**Goal**: Ship the base layer that every subsequent milestone depends on.

**Deliverables**:

- `src/platform/`:
  - `time_source.{hpp,cpp}` — `std::chrono::steady_clock` wrapper, unit-tested ≥ 90%
  - `thread_utils.{hpp,cpp}` — thread naming, CPU affinity
  - `crash_reporting.{hpp,cpp}` — Crashpad integration
- `src/drivers/driver_interface.hpp` — pure virtual base defining `open / close / start / stop / write / health / statistics`
- `src/frame/raw_frame.hpp` — `RawFrame`, `FrameEnvelope`, `RxStats`, `TxStats`, `BackpressureSignal`
- `src/utils/spsc_ring.hpp` — single-producer / single-consumer ring buffer
- `src/utils/mpsc_queue.hpp` — wrapper around `moodycamel::ConcurrentQueue`
- `src/utils/snapshot.hpp` — double-buffered snapshot utility
- Doxygen on every public declaration
- Unit tests for each component

**Out of scope**: any concrete Driver (M3); any Frame pipeline (M4).

**Hard stop**: interface freeze. The human reviews:

- Is the Driver interface signature complete? M3–M11 all depend on it.
- Are `RawFrame` fields sufficient? Changes after freeze cascade across many tests.
- Are the queue APIs ergonomic?

After freeze, subsequent milestones must not modify signatures. They may add new interfaces alongside the old ones.

**Key design constraints**:

- `DriverInterface` is asynchronous: `start()` is non-blocking; data flows via callback or Qt signal.
- `RawFrame` is movable and copyable; `payload` is `std::vector<std::byte>` or equivalent.
- `BackpressureSignal` is a value type, cheap to pass across threads.
- All timestamps are `std::chrono::nanoseconds` relative to the steady-clock origin recorded at session start.

---

### M3 — Serial / TCP / UDP Drivers

**Goal**: Build three real drivers on top of M2's interface and prove they work.

**Deliverables**:

- `src/drivers/serial_driver.{hpp,cpp}` using `QSerialPort`
- `src/drivers/tcp_driver.{hpp,cpp}`
- `src/drivers/udp_driver.{hpp,cpp}`
- `src/drivers/replay_driver.{hpp,cpp}` — skeleton only; completed in M9
- Unit tests per driver using mock data sources
- `tests/integration/drivers_loopback.cpp`:
  - Serial via `socat` PTY pair
  - TCP via an in-process test server
  - UDP via in-process test echo

**Out of scope**: Modbus, CAN (interface reserved, not implemented); any protocol-layer decoding (that is M4).

**Hard stop**: hardware acceptance by the human. CC provides socat and local-socket evidence. The human runs real hardware:

- A real USB-to-serial device is stable for 1 hour.
- A real TCP-capable device (for example, an embedded board with a TCP echo) handles connect / disconnect / reconnect correctly.

**Edge cases the integration tests must cover**:

- Sudden device disconnect (unplug serial cable)
- Connection refused
- TCP half-close
- High-rate data influx to trigger backpressure
- Invalid parameters (unsupported baud rate, port in use)

---

### M4 — Frame Layer + Decode Page

**Goal**: End-to-end — raw frames arrive, are parsed into messages, and are shown in the UI.

**Deliverables**:

- `src/frame/frame_pipeline.{hpp,cpp}` — pulls `RawFrame` from drivers, applies backpressure, hands off to decoders
- `src/decode/`:
  - `message_type.{hpp,cpp}`, `decode_rule.{hpp,cpp}`, `field_def.{hpp,cpp}`
  - `decoder.{hpp,cpp}` — rule engine
  - `rule_loader.{hpp,cpp}` — YAML loader via yaml-cpp
- `src/ui_widgets/decode_page.{hpp,cpp}` — raw-frame table, per-frame preview, field breakdown
- `schema/decode_rule_v1.yaml` — schema definition expressed in YAML
- `examples/decode_rules/simple_serial.yaml`
- Unit and integration tests

**Out of scope**: signal layer (M5); alarm bar (end of V1); visual rule editor (V1 relies on manual YAML editing).

**Hard stop**: decode rule schema freeze. The human reviews:

- Does the schema cover V1 field needs? (fixed/variable length, endianness, bit fields, CRC, conditional matching)
- Is there room for future extension? (version field, reserved fields)
- Are error messages specific enough? (No bare "parse error".)

Once approved, `schema/decode_rule_v1.yaml` is frozen. Extensions must bump to v2.

---

### M5 — Signal Layer + Observe Skeleton

**Goal**: The non-chart parts of the Observe page, plus the `SignalStore` foundation.

**Deliverables**:

- `src/signal/`:
  - `signal_store.{hpp,cpp}` — thread-safe unified signal space
  - `signal_value.{hpp,cpp}` — includes freshness and quality
  - `derived_signal.{hpp,cpp}` — ExprTk backend
  - `snapshot_provider.{hpp,cpp}` — 30 Hz tick snapshots
- `src/ui_widgets/observe_page.{hpp,cpp}` — layout skeleton
- `src/ui_widgets/value_card.{hpp,cpp}`, `status_card.{hpp,cpp}`, `signal_tree.{hpp,cpp}`, `log_stream.{hpp,cpp}`

**Out of scope**: the chart widget (M6); the alarm bar.

**Hard stop**: `SignalStore` interface freeze. The human reviews:

- Is the write / read API efficient enough? M6 will exercise it hard.
- Is the snapshot mechanism truly lock-free on the read side?
- How does `DerivedSignal` handle cycles and lifetime?

---

### M6 — High-frequency Chart Component

**Goal**: V1's key performance bet. Must meet the chart-related targets in `[Arch §8.4]`.

**Deliverables**:

- `src/ui_quick/chart_item.{hpp,cpp}` — `QQuickItem` subclass using Scene Graph
- `src/ui_quick/chart_renderer.{hpp,cpp}` — Scene Graph node management
- `src/ui_quick/ring_downsampler.{hpp,cpp}` — per-pixel min/max/avg downsampling
- `src/ui_quick/chart_interaction.{hpp,cpp}` — pan and zoom
- `src/ui_quick/Chart.qml`
- `tests/perf/chart_bench.cpp` — benchmark suite
- `docs/perf/M6-baseline.md` — measurement report

**Performance gates** (must pass to close):

| Scenario | Target | Notes |
|---|---|---|
| 100 signals in one chart | 30 FPS sustained | Single window, 1 kHz sine input each |
| 20 charts × 10 signals each | 30 FPS sustained | Distributed across docks |
| 5 k points/s total input | 30 FPS sustained | No backpressure triggered |
| 10 k points/s total input | Graceful degrade to 20 FPS | Dropped frames allowed, no crashes |
| Pan / zoom interaction | Response < 100 ms | Profiler + human feel |

**Out of scope**: theme configuration (M11); tooltips (late V1); cursors and measurement tools (V2).

**Hard stop**: performance certification. Any failure returns to CC for optimization, or escalates as an architecture issue that may force a Qt Quick approach change. "Close enough" is not acceptable.

---

### M7 — Control Page

**Goal**: The complete control path — send commands, match ACKs, run periodic tasks, execute macros.

**Deliverables**:

- `src/action/`:
  - `action_template.{hpp,cpp}`, `action_instance.{hpp,cpp}`
  - `action_scheduler.{hpp,cpp}` — periodic and trigger-driven
  - `ack_matcher.{hpp,cpp}`, `macro_action.{hpp,cpp}`
- `src/ui_widgets/control_page.{hpp,cpp}`, `command_panel.{hpp,cpp}`, `ack_status.{hpp,cpp}`
- `schema/action_v1.yaml` — command template schema
- `examples/actions/simple_cmd.yaml`
- Unit and integration tests (use `ReplayDriver` to simulate ACKs)

**Out of scope**: scripted actions (V2); visual macro editing (V1 relies on manual YAML editing).

**Hard stop**: real-hardware loop run by the human:

- On connect, auto-dispatch 3 commands; the device responds; ACK parsing is correct.
- 10 Hz periodic polling runs for 30 minutes; timeout counts look reasonable.
- Manual command dispatch shows accurate ACK status.

---

### M8 — Session Writer + File Format

**Goal**: Production implementation of `.sfr` / `.sfi` and supporting tooling.

**Deliverables**:

- `src/session/session_writer.{hpp,cpp}` and `session_reader.{hpp,cpp}`
- `src/session/sfr_format.hpp`, `sfi_format.hpp` — byte-level constants and structs
- `src/session/migration.{hpp,cpp}` — empty framework for future version upgrades
- `tools/sfr_dump/` — CLI tool that dumps `.sfr` contents as human-readable JSON
- `tools/sfr_verify/` — CLI tool that validates `.sfr` integrity
- `docs/formats/sfr_v1_spec.md` — byte-level specification
- Unit tests and round-trip tests (write → read → compare)

**Out of scope**: Replay UI (M9); Parquet or other formats.

**Hard stop**: format freeze. The human reviews:

- Is the byte-level specification unambiguous (endianness stated, field order stated, alignment stated)?
- Is the dump tool's output genuinely human-readable?
- Is the version field present and positioned to allow extension?

Once frozen, only `schemaVersion` bumps are allowed. No field repurposing.

---

### M9 — Replay (Complete)

**Goal**: Full `ReplayDriver` and Replay UI, sharing the `SignalStore` with live observation.

**Deliverables**:

- `src/drivers/replay_driver.{hpp,cpp}` — completed (the M3 version is a skeleton)
- `src/ui_widgets/replay_page.{hpp,cpp}`, `timeline.{hpp,cpp}`, `speed_control.{hpp,cpp}`, `bookmark_list.{hpp,cpp}`
- `tests/integration/replay_equivalence.cpp` — equivalence test: the same input through the live path and the replay path produces byte-equivalent signal output. Microsecond-level timestamp skew is allowed; logical order must match.

**Out of scope**: replay export (V2); timeline thumbnails (V2).

**Hard stop**: equivalence verification. If outputs differ, the discrepancy must be located, which may cascade corrections back into M4 or M8.

---

### M10 — Performance Optimization

**Goal**: Hit every metric in `[Arch §8.4]`.

**Deliverables**:

- `tests/perf/full_suite.cpp` — covers all metrics from `[Arch §8.4]`
- Optimization commits driven by profiling, not preplanned
- `docs/perf/M10-certification.md` — full baseline report
- CI workflow for performance regression: any metric degrading ≥ 20% blocks the merge

**Optimization areas** (non-exhaustive; profiling determines specifics):

- `SignalStore` snapshot path
- Chart Scene Graph node reuse
- Decode worker pool scheduling
- Session Writer I/O batching
- QML property-binding reduction
- Compiler options (LTO, PGO)

**Hard stop**: performance certification. Every metric in `[Arch §8.4]` must pass. Any failure requires a root-cause analysis and a human decision (fix, accept, or downgrade).

**72-hour stability test**, executed by QA or the human:

- Continuous run for 72 hours with ongoing data flow (replay loop acceptable)
- No crashes
- RSS does not grow unboundedly (stabilizes within the `[Arch §8.4]` limit)
- No flood of ERROR-level log output

---

### M11 — Polish and Packaging

**Goal**: Make V1 ready for internal release.

**Deliverables**:

- `packaging/appimage/` — AppImage script via `linuxdeployqt`
- `packaging/deb/` — `.deb` configuration
- `examples/projects/sample_serial/`, `sample_tcp/`
- `docs/user-guide/` — getting started, main workflow screenshots
- `docs/reference/` — command template reference, decode rule reference
- Three layout presets (default, full-screen chart, debug mode)
- Unified error messaging (error code + human text)
- Recent-projects and recent-devices persistence
- About dialog listing all dependencies and licenses

**Out of scope**: auto-update; user-behavior telemetry.

**Optional**: if Qt 6.12 LTS has shipped and appears stable, insert a 1-week migration window (see `[Arch §12.6]`).

**Hard stop**: release readiness review. The human checks:

- [ ] AppImage runs on a clean Ubuntu 22.04 VM without installation
- [ ] `.deb` installs and uninstalls cleanly via apt
- [ ] Sample projects open, connect, and show live data
- [ ] User guide is readable standalone
- [ ] SBOM and NOTICE are complete
- [ ] All `TODO`s are either cleared or filed as issues
- [ ] All `[WORKAROUND]` commits have been triaged

On pass, tag `v1.0.0-rc1` and distribute to internal testers.

---

## 4. Milestone Transition Rules

Moving from `M<n>` to `M<n+1>` requires, in order:

1. `M<n>` completion report filed
2. Human completes the `[EM §5]` acceptance checklist
3. All `M<n>` HALTs are resolved
4. Code merged to `main` via PR; tagged `v0.<n>.0-alpha.1`
5. `M<n+1>` spec reviewed by the human
6. `milestone/M<n+1>` branch cut from `main`

Only after all six conditions are satisfied does the next CC session begin.

---

## 5. Timeline Estimate

| Sprint | Milestone(s) | Person-days | Cumulative |
|---|---|---|---|
| 1 | M0 + start of M1 | 5 + 2 | 7 |
| 2 | M1 finish + M2 | 3 + 5 | 15 |
| 3 | M3 | 10 | 25 |
| 4 | M4 | 10 | 35 |
| 5 | M5 + start of M6 | 5 + 3 | 43 |
| 6 | M6 finish | 7 | 50 |
| 7 | M7 start + Checkpoint 1 | 5 + review | 55 |
| 8 | M7 finish | 5 | 60 |
| 9 | M8 | 5 | 65 |
| 10 | M9 start | 5 | 70 |
| 11 | M9 finish | 5 | 75 |
| 12 | Integration + Checkpoint 2 | 5 | 80 |
| 13 | M10 start | 5 | 85 |
| 14 | M10 finish + 72 h stability | 5 | 90 |
| 15 | M11 start (+ optional Qt 6.12 LTS migration) | 5 | 95 |
| 16 | M11 finish + packaging | 5 | 100 |

Against a 160-person-day capacity (two engineers × 16 weeks), the 95-day estimate leaves approximately 40% buffer. Buffer absorbs:

- HALT resolution (estimated 1–2 HALTs per milestone, 0.5 person-days each)
- Hardware-coordination wait time
- Human review latency
- Unexpected Qt 6.10 bugs
- Qt 6.12 LTS migration, if triggered

If actual progress runs ahead of plan, buffer goes toward V1.1 preparation or additional polish — not toward expanding V1 scope.

---

## 6. Closing Note

These twelve milestones are the only main-branch path for V1. Other ideas — V1.5 features, experimental modules, architectural rework — do not enter `main` before V1 ships.

Milestone granularity was chosen by balancing two pressures:

- Finer (less than 3 person-days each) causes review overhead to consume CC's efficiency.
- Coarser (more than 15 person-days each) raises drift risk and dilutes the value of each hard stop.

5–10 person-days per milestone is the sweet spot for CC auto mode.
