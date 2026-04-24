# Milestone Roadmap

**Status**: Authoritative plan for V1 development.
**Last updated**: 2026-04-24 (v2.1 — merge of v2 M4-M13 re-split with v1 structural content)
**Architecture**: v0.7 (see `docs/architecture/architecture.md`)

This document defines the fourteen milestones (M0–M13) that make up V1. Each milestone is a unit of work CC can execute autonomously within a single session, bounded by a human hard stop.

**Companion documents**:

- `docs/architecture/architecture.md` — technical baseline
- `docs/claude-code/execution-manual.md` — CC operating rules
- `docs/milestones/M<n>-<slug>.md` — detailed per-milestone specs, produced as each milestone approaches

**Cross-reference notation**:

- `[EM §N]` — Execution Manual, section N
- `[Arch §N]` — Architecture document, section N
- `[MR M<n>]` — this Milestone Roadmap, entry for milestone `M<n>`
- `[CM §X]` — CLAUDE.md, section X

---

## 1. Overview

| ID | Name | Status | Tag | Effort (person-days) | Hard-stop type |
|---|---|---|---|---|---|
| M0 | Project Bootstrap | ✅ closed | v0.0.1-alpha.1 | 5 | Structural review |
| M1 | Qt Quick Integration Spike | ✅ closed | v0.0.2-alpha.1 | 5 | Technical decision |
| M2 | Platform + Core Abstractions | ✅ closed | v0.0.3-alpha.1 | 5 | Interface freeze |
| M3 | Concrete Drivers + Connection Manager Preview | ✅ closed | v0.0.4-alpha.1 | 12 | Implementation correctness |
| M4 | Frame Pipeline (routing + backpressure) | 🔜 next | — | 4–6 | Interface freeze |
| M5 | Decoder Layer (yaml schema → SignalValue) | — planned | — | 8–10 | Schema freeze |
| M6 | Signal Buffer (time series storage) | — planned | — | 8–10 | Interface freeze |
| M7 | Expression Engine (exprtk + derived signals) | — planned | — | 5–7 | Interface freeze |
| M8 | Real-time Chart UI | — planned | — | 10–12 | **Performance certification** |
| M9 | Connection Manager (full features) | — planned | — | 5–7 | Hardware loop |
| M10 | Session Writer | — planned | — | 6–8 | Format freeze |
| M11 | Session Replayer (completes ReplayDriver) | — planned | — | 5–7 | Equivalence verification |
| M12 | Performance Optimization | — planned | — | 6–10 | **Performance certification** |
| M13 | Polishing + Packaging | — planned | — | 5–7 | Release readiness |

Total estimate: **100–130 person-days**. With the 5-phase execution protocol and new-CC-session-per-milestone overhead, the calendar floor is 14–18 weeks. Buffer absorbs HALT resolution, human review latency, cross-milestone integration polish, and unexpected Qt version issues.

### History of this roadmap

- **v1** (pre-M3): eight milestones M4–M11 covering all post-M3 work.
- **v2** (after M3 close, 2026-04-24): re-split into M4–M13 (ten milestones) for finer granularity. The original M4 "Frame Pipeline" conflated routing, decoding, signal storage, and expression evaluation — each has distinct interfaces and freeze points and deserves its own milestone.
- **v2.1** (this revision): merged v2's re-split with v1's preserved structural content (dependency graph, transition rules, timeline analysis, closing philosophy).

---

## 2. Dependencies and Parallelism

```
M0 ──► M1 ──► M2 ──► M3 ──► M4 ──► M5 ──► M6 ──► M7 ──► M8 ──► M9 ──► M10 ──► M11 ──► M12 ──► M13
                │                                      │
                │                                      └── (M8 Chart depends on M6 Signal Buffer frozen;
                │                                           M7 Expression is parallel-capable with M8)
                │
                └── (an M1 fallback verdict forces an M8 rendering-approach rewrite;
                     ADR-001 locked QQuickWidget so this branch is now latent)
```

### Where parallelism is possible

- **M7 Expression Engine + M8 Chart UI**: M7 depends on M6 Signal Buffer frozen. M8 also depends on M6. Once M6 closes, M7 and M8 can proceed on separate branches.
- **M9 Connection Manager + M10 Session Writer**: both depend on M6 Signal Buffer. If M9 finishes first, M10 can start independently.
- **Within M8** (Chart UI): chart rendering, signal selector, and axis management can be split across engineers on the same milestone branch.
- **Within M13** (Packaging): packaging scripts and user documentation can proceed in parallel.

### Where parallelism is forbidden

- Until M1's verdict is resolved (done, Go per ADR-001), M8's rendering approach depends on the outcome. M1 closed with ADR-001 choosing QQuickWidget + Scene Graph; this constraint is now latent but historically important.
- Until M2's Driver interface is frozen (done), M3 must not start.
- Until M4's `FrameSink` and `FramePipeline` interfaces are frozen, M5 must not start.
- Until M5's decode rule schema is frozen, M6 must not start.
- Until M6's `SignalBuffer` interface is frozen, M7 and M8 must not start.
- Until M10's session file format is frozen, M11 must not start.
- Until M12's performance certification passes, M13 release packaging is not final.

These are hard gates. Human review is required to cross each of them.

---

## 3. Milestone Summaries

Each milestone has (or will have) a detailed spec at `docs/milestones/M<n>-<slug>.md`, produced as the milestone approaches. This section gives scope, deliverables, hard-stop semantics, and critical constraints.

### M0 — Project Bootstrap ✅

**Goal**: Establish a stable engineering skeleton.

**Status**: Closed. Details in `docs/milestones/M0-project-bootstrap.md`. Key outputs: CMake + 13 module skeleton, FetchContent deps, CI 3-job matrix, minimal Qt app.

---

### M1 — Qt Quick Integration Spike ✅

**Goal**: Answer "can we actually use `QQuickWidget` in production?" before committing in M8.

**Status**: Closed. Verdict per ADR-001: Go (QQuickWidget + Scene Graph). Details in `docs/milestones/M1-qtquick-integration-spike.md`.

---

### M2 — Platform + Core Abstractions ✅

**Goal**: Ship the base layer that every subsequent milestone depends on.

**Status**: Closed. Details in `docs/milestones/M2-platform-core-abstractions.md`. Freezes established: `DriverInterface`, `RawFrame`, `WatermarkTracker`, `SpscRing`, `MpscQueue`, `Snapshot`, platform utilities. ADR-002 switched crash backend from Crashpad to sentry-native mid-milestone.

**Key design constraints (frozen)**:
- `DriverInterface::open()` is non-blocking (returns Success synchronously on request acceptance; actual open completion via `stateChanged` signal).
- `RawFrame::payload` is `QByteArray` (implicit shared, O(1) copy across threads).
- `DriverStatistics` uses per-field `std::atomic`; cross-field consistency not guaranteed.
- `Snapshot<T>` uses C++20 `std::atomic<std::shared_ptr<const T>>`.
- All timestamps `std::chrono::steady_clock::time_point`; wall clock forbidden for ordering.

---

### M3 — Concrete Drivers + Connection Manager Preview ✅

**Goal**: Build four concrete drivers plus a preview UI.

**Status**: Closed. Details in `docs/milestones/M3-concrete-drivers.md`. Drivers: `SerialDriver`, `TcpDriver`, `UdpDriver`, `ReplayDriver` skeleton. Connection Manager modeless QDialog. Performance baselines committed. 179 tests across three presets.

**Key design constraints**:
- Per-driver dedicated QThread via `IoWorkerBase`.
- `write()` uniformly asynchronous across drivers.
- No Modbus/CAN (V1.5); no DriverFactory (M9 territory if needed).
- Socat-based virtual serial for loopback testing; localhost echo for TCP.

---

### M4 — Frame Pipeline 🔜

**Goal**: Route `frameReceived` from drivers to downstream sinks. No decoding in M4.

**Deliverables**:
- `FrameSink` interface (pure C++ abstract class; `onFrame`, `onError`, `onLifecycle`, `sinkName`)
- `FramePipeline` class (per-driver QThread, ingress MPSC queue, sink fanout, backpressure observation)
- `PipelineManager` (registry, attach/detach lifecycle, `pipelineAttached`/`pipelineDetached` signals)
- Integration with M3 Connection Manager (internal wiring; no UI widgets)
- Unit + integration tests; pipeline throughput benchmark (overhead ≤ 10% of M3 baseline)

**Out of scope**: decoding (M5), signal storage (M6), UI changes.

**Hard stop**: interface freeze. Human reviews:
- Is `FrameSink` surface minimal and complete for M5-M11 consumers?
- Is the pipeline threading model (one thread per driver) acceptable resource-wise?
- Are backpressure semantics clear (ingress-only in V1)?

**Key design constraints**:
- `FrameSink` is non-`QObject` abstract base — downstream sinks (decoder, recorder) are pure C++.
- Each pipeline owns exactly one QThread via `IoWorkerBase` pattern.
- One `WatermarkTracker` per pipeline, observing the ingress queue.
- Sink exception isolation: a throwing sink does not crash the pipeline.
- Pipeline lifetime tied to driver's Connection Manager Connect/Disconnect.

**Detailed spec**: `docs/milestones/M4-frame-pipeline.md`.

---

### M5 — Decoder Layer

**Goal**: Parse `RawFrame` payload into typed `SignalValue` instances using yaml schemas.

**Deliverables**:
- Yaml schema format specification for fixed-layout byte frames
- `DecoderInterface` implementing `FrameSink` (registers with a pipeline)
- Schema validator with clear error messages (line number, field name, specific issue)
- Endianness handling, bit flag extraction, scale/offset transforms
- Error handling (malformed frame → discard + log + stats, no exception)

**Signal data model**:
- `SignalValue = std::variant<bool, int64_t, double, QString>`
- Metadata: name, unit, optional scale/offset/description
- Timestamps preserved from `RawFrame::recvAt`

**Out of scope**: GUI schema editor (V1.5+); signal buffer (M6); expression evaluation (M7).

**Hard stop**: decode rule schema freeze. Human reviews:
- Does schema cover V1 field needs (fixed/variable length, endianness, bit fields, CRC, conditional matching)?
- Is there room for future extension (version field, reserved fields)?
- Are error messages specific enough? No bare "parse error".

Once approved, schema v1 is frozen. Extensions must bump to v2.

---

### M6 — Signal Buffer

**Goal**: Time-series storage for decoded signals with query API.

**Deliverables**:
- `SignalBuffer` — per-signal circular buffer, configurable duration / sample cap
- Query API: time range, decimation (for chart LOD), latest N samples
- Thread-safe concurrent writer + multiple concurrent readers
- Memory budget management (global cap; LRU eviction for inactive signals)
- Persistence hooks for M10 session writer

**Out of scope**: expression evaluation (M7); chart rendering (M8); session file I/O (M10).

**Hard stop**: `SignalBuffer` interface freeze. Human reviews:
- Is the write path efficient? M8 Chart exercises it at 30Hz with 60+ signals.
- Is the read snapshot mechanism truly lock-free? Blocking readers block chart render.
- How does memory budget enforce? LRU eviction behavior.

**Key design constraints**:
- Internal storage optimized per-variant type (bool bit-packed, int64/double as 8-byte, QString ref-counted).
- Numeric types dominate expected workload; QString is rare fallback.
- No blocking in hot read path; `Snapshot<T>` style double-buffering.

---

### M7 — Expression Engine

**Goal**: Derived signals computed from base signals using exprtk.

**Deliverables**:
- `Expression` class wrapping exprtk
- Registration of base signals as variables
- Compilation + validation (syntax error → diagnostic with position)
- Event-driven re-evaluation on base signal update
- Cycle detection at registration time
- Runtime error handling (division by zero, domain error → error signal)

**Out of scope**: scripted derivatives (V2); visual expression editor (V2).

**Hard stop**: interface freeze. Human reviews:
- Does the API handle bool/int64 → double conversion cleanly?
- Is cycle detection actually correct? (A depends on B depends on A → reject.)

**Key design constraint**:
- exprtk is double-only; `QString` source variables are rejected at registration time (type error with clear message).
- bool/int64 sources auto-convert to double for use in expressions.

---

### M8 — Real-time Chart UI

**Goal**: V1's key performance milestone. Must meet chart-related targets in `[Arch §8.4]`.

**Deliverables**:
- `ChartItem` / `ChartRenderer` using QQuickWidget + Scene Graph (per ADR-001)
- `RingDownsampler` — per-pixel min/max/avg
- `ChartInteraction` — pan and zoom
- `Chart.qml`
- `tests/perf/chart_bench.cpp` — benchmark suite
- `docs/perf/M8-baseline.md` — measurement report

**Performance gates** (must pass to close):

| Scenario | Target | Notes |
|---|---|---|
| 60 signals in one chart | 30 FPS sustained | Per M1 spike verdict |
| 8–12 charts × 5 signals each | 30 FPS sustained | Distributed across docks |
| 5k points/s total input | 30 FPS sustained | No backpressure triggered |
| 10k points/s total input | Graceful degrade to 20 FPS | Dropped frames allowed, no crashes |
| Pan / zoom interaction | Response < 100 ms | Profiler + human feel |

**Out of scope**: theme configuration (M13); tooltips (late V1); cursors (V2); historical overlays (V2).

**Hard stop**: performance certification. Any failure returns to CC for optimization, or escalates as an architecture issue that may force a Qt Quick approach change. "Close enough" is not acceptable.

---

### M9 — Connection Manager (full features)

**Goal**: Upgrade M3's preview to production-quality.

**Deliverables**:
- Multiple concurrent connections
- Save / load configurations in yaml per `[Arch §8]`
- Favorites / recent connections list
- Connection status dashboard
- Integration with M5 schema selection per connection

**Out of scope**: scripted connections (V2); visual schema editor (V1.5).

**Hard stop**: hardware loop run by the human. On real hardware:
- Three or more concurrent connections stable for 30 minutes
- Disconnect / reconnect preserves state correctly
- yaml save/restore round-trip works

---

### M10 — Session Writer

**Goal**: Record live signal data to disk for later replay.

**Deliverables**:
- Session file format specification
- Writer reading from `SignalBuffer`, writing incrementally
- Metadata: driver configs, schemas, `ClockOrigin`, duration, summary stats
- User control: start/stop/pause via UI
- Crash robustness (partial session recoverable to last write)

**Out of scope**: Replay UI (M11); alternate formats like Parquet (V2).

**Hard stop**: format freeze. Human reviews:
- Is byte-level spec unambiguous (endianness, field order, alignment stated)?
- Is there a version field positioned for extension?
- Does the dump tool output human-readable JSON?

Once frozen, only `schemaVersion` bumps allowed. No field repurposing.

---

### M11 — Session Replayer (completes ReplayDriver)

**Goal**: Fill in the M3 ReplayDriver skeleton with session file reading and frame emission.

**Deliverables**:
- Parse session file format from M10
- Time-accurate frame emission (respects original inter-frame intervals)
- Playback speed control (0.25x–4x)
- Seeking within session
- Loop mode
- All `TODO(M9)` insertion points in `replay_driver.cpp` filled (historical note: renumbered from M9 to M11 during v2 re-split)

**Out of scope**: replay export (V2); timeline thumbnails (V2).

**Hard stop**: equivalence verification. Input-through-live vs. input-through-replay must produce byte-equivalent signal output. Microsecond-level timestamp skew allowed; logical order must match.

---

### M12 — Performance Optimization

**Goal**: Hit every metric in `[Arch §8.4]`.

**Deliverables**:
- `tests/perf/full_suite.cpp` — all metrics from `[Arch §8.4]`
- Optimization commits driven by profiling, not preplanned
- `docs/perf/M12-certification.md` — full baseline report
- CI workflow for performance regression: any metric degrading ≥ 20% blocks merge

**Optimization areas** (non-exhaustive; profiling drives specifics):
- `SignalStore` snapshot path
- Chart Scene Graph node reuse
- Decode worker pool scheduling
- Session Writer I/O batching
- QML property-binding reduction
- Compiler options (LTO, PGO)

**Hard stop**: performance certification. Every `[Arch §8.4]` metric must pass. Any failure → root-cause analysis + human decision (fix / accept / downgrade).

**72-hour stability test** (QA or human):
- Continuous run for 72 hours with data flow (replay loop acceptable)
- No crashes
- RSS does not grow unboundedly (stabilizes within `[Arch §8.4]` limit)
- No flood of ERROR-level log output

---

### M13 — Polishing + Packaging

**Goal**: Make V1 ready for internal release.

**Deliverables**:
- `packaging/appimage/` — AppImage via `linuxdeployqt`
- `packaging/deb/` — `.deb` configuration
- `examples/projects/sample_serial/`, `sample_tcp/`
- `docs/user-guide/` — quickstart, main workflow screenshots
- `docs/reference/` — schema reference, command reference
- Three layout presets (default, full-screen chart, debug mode)
- Unified error messaging (error code + human text)
- Recent-projects / recent-devices persistence
- About dialog with dependencies and licenses

**Out of scope**: auto-update (V2); telemetry (V2).

**Optional**: if Qt 6.12 LTS has shipped and appears stable, insert a 1-week migration window per `[Arch §12.6]`.

**Hard stop**: release readiness review. Human checks:
- [ ] AppImage runs on clean Ubuntu 24.04 VM without installation
- [ ] `.deb` installs and uninstalls cleanly via apt
- [ ] Sample projects open, connect, show live data
- [ ] User guide is readable standalone
- [ ] SBOM and NOTICE are complete
- [ ] All `TODO`s cleared or filed as issues
- [ ] All `[WORKAROUND]` commits triaged

On pass, tag `v1.0.0-rc1` and distribute to internal testers.

---

## 4. Milestone Transition Rules

Moving from `M<n>` to `M<n+1>` requires, in order (per CLAUDE.md §Git operation protocol 5-phase flow):

1. **Phase 1** (CC): `M<n>` subtasks complete; commits pushed to `milestone/M<n>`; CI green; PR created; `.claude/M<n>-done.md` written. CC stops.
2. **Phase 2 human checkpoint A**: Human reads done report, reviews freeze record, optionally runs manual verifications. Replies `"approved, merge M<n> and begin M<n+1> bootstrap"`.
3. **Phase 3** (CC): PR merged; tagged `v0.0.<n>.1`; `milestone/M<n+1>` branch cut from main; M<n+1> spec read; `.claude/M<n+1>-understanding.md` + `M<n+1>-plan.md` produced and pushed. CC stops.
4. **Phase 4 human checkpoint B**: Human reviews understanding + plan. Replies `"approved, execute M<n+1>"`.
5. **Phase 5** (CC): M<n+1> subtasks execute.

Both checkpoints (Phase 2 and Phase 4) are mandatory. CC may not merge the two checkpoints into a single approval without explicit session-level authorization.

Additionally, before Phase 3 can proceed, the `M<n+1>` spec must already exist at `docs/milestones/M<n+1>-<slug>.md` on main. The human (or advisor-prepared CC session) commits the spec before giving the Phase 2 approval.

---

## 5. Timeline Estimate

Post-M3 remaining milestones (M4–M13): **60–85 person-days**, plus checkpoint overhead and cross-milestone integration.

| Milestone | Days | Cumulative (post-M3) |
|---|---|---|
| M4 | 4–6 | 4–6 |
| M5 | 8–10 | 12–16 |
| M6 | 8–10 | 20–26 |
| M7 | 5–7 | 25–33 |
| M8 | 10–12 | 35–45 |
| M9 | 5–7 | 40–52 |
| M10 | 6–8 | 46–60 |
| M11 | 5–7 | 51–67 |
| M12 | 6–10 | 57–77 |
| M13 | 5–7 | 62–84 |

Each milestone adds **~1 week of calendar overhead** for the 5-phase protocol (Phase 2/4 human reviews, spec preparation, context transitions). Total post-M3 calendar duration: **14–20 weeks**.

Buffer absorbs:
- HALT resolution (estimated 1–2 HALTs per milestone per M0-M3 pattern)
- Human review latency (Phase 2 and Phase 4)
- Cross-milestone integration polish
- Unexpected Qt 6.10 bugs
- Optional Qt 6.12 LTS migration, if triggered before M13

If actual progress runs ahead of plan, buffer goes toward V1.1 preparation or additional polish — not toward expanding V1 scope.

---

## 6. V1 Summary

- **M0-M13 total**: 100–130 person-days across all milestones (M0-M3 consumed ~27 days; post-M3 remaining 60–85)
- **V1 release target**: `v0.1.0` (no alpha suffix) after M13
- **V1 feature set**: drivers + pipeline + decoder + signal buffer + expression + live chart + full connection management + session record/replay + packaging
- **V1.5 deferred items** (roadmap preview only, non-committing):
  - Visual yaml schema editor (replaces hand-authoring)
  - Modbus/CAN drivers (currently architecture-reserved only)
  - Multi-session comparison UI
  - Plugin-based decoder (alternate to yaml schema for complex protocols)
  - Remote Sentry.io crash upload (backend currently local-only per ADR-002)
  - Cross-platform builds (Windows, macOS; V1 is Ubuntu-only)

---

## 7. Closing Note

These fourteen milestones are the only main-branch path for V1. Other ideas — V1.5 features, experimental modules, architectural rework — do not enter `main` before V1 ships.

Milestone granularity was chosen by balancing two pressures:

- Finer (less than 3 person-days each) causes 5-phase protocol overhead to consume CC's efficiency.
- Coarser (more than 15 person-days each) raises drift risk and dilutes the value of each hard stop.

4-12 person-days per milestone is the sweet spot for CC auto mode with the 5-phase checkpoint protocol. The v2 re-split (moving from 8 post-M3 milestones to 10) reflects real-world observation from M0-M3 that original scope-boundaries conflated independent concerns. Finer granularity trades ~2-3 weeks of calendar time for better freeze discipline, earlier downstream milestone starts (M7 Expression and M8 Chart can proceed in parallel once M6 Signal Buffer is frozen), and reduced per-session CC risk.

If a future re-split becomes necessary (e.g., M8 Chart UI proves too large), that decision follows the same ADR + roadmap-update process used here.
