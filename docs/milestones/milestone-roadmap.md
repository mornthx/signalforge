# Milestone Roadmap

**Last updated**: 2026-04-24
**Current milestone**: M3 merged, M4 next
**Architecture**: v0.7 (see `docs/architecture/architecture.md`)

## Status

| ID | Name | Status | Tag |
|---|---|---|---|
| M0 | Project bootstrap | ✅ closed | v0.0.1-alpha.1 |
| M1 | Qt Quick integration spike | ✅ closed | v0.0.2-alpha.1 |
| M2 | Platform + core abstractions (interface freeze) | ✅ closed | v0.0.3-alpha.1 |
| M3 | Concrete drivers + connection manager preview | ✅ closed | v0.0.4-alpha.1 |
| M4 | Frame Pipeline (routing + backpressure glue) | 🔜 next | — |
| M5 | Decoder Layer (yaml schema + frame → signal) | — planned | — |
| M6 | Signal Buffer (time series storage + query API) | — planned | — |
| M7 | Expression Engine (exprtk + derived signals) | — planned | — |
| M8 | Real-time Chart UI | — planned | — |
| M9 | Connection Manager (full features) | — planned | — |
| M10 | Session Writer | — planned | — |
| M11 | Session Replayer (completes ReplayDriver) | — planned | — |
| M12 | Performance Optimization | — planned | — |
| M13 | Polishing + Packaging | — planned | — |

## Change Log

### 2026-04-24 — M4-M13 re-split (v2)

Previously M4-M11 covered 8 milestones. After M3 close, the scope was re-split into M4-M13 (10 milestones) for finer granularity. Rationale:

- The original "M4 Frame Pipeline" conflated routing, decoding, signal storage, and expression evaluation. Each has distinct interfaces and freeze points.
- Smaller milestones reduce CC session risk, reduce PR review burden, and allow earlier downstream milestones to start (e.g., M8 Chart UI can start once M6 Signal Buffer is frozen, even if M7 Expression is still in development).
- Total project effort estimate unchanged; duration extends by ~2-3 weeks of calendar time due to more checkpoints.

### Original roadmap (v1)

Preserved for historical reference: M4 Frame Pipeline, M5 Chart UI, M6 Real-time Chart, M7 Connection Manager, M8 Session Writer, M9 Session Replayer, M10 Performance Optimization, M11 Polishing + Packaging.

## M4 — Frame Pipeline

Route `frameReceived` signals from concrete drivers to downstream consumers. Pipeline is per-driver (decision in pre-M4 planning) with backpressure glue between stages. No decoding in M4.

**Scope**:
- `FramePipeline` class (one per driver instance)
- `FrameSink` interface (downstream consumers implement this)
- Wiring of `DriverInterface::frameReceived` → `FramePipeline::push()` → registered sinks
- Backpressure integration using M2's `WatermarkTracker`
- Lifecycle management (pipeline lifetime tied to driver lifetime)
- Unit + integration tests; benchmarks against M3 driver baselines (no regression)

**Out of scope**: Decoding (M5), signal storage (M6), expression evaluation (M7), UI (M8).

**Estimated effort**: 4-6 person-days.

## M5 — Decoder Layer

Parse `RawFrame` payload into typed `SignalValue` instances using yaml schemas.

**Scope**:
- Yaml schema format specification (fields, offsets, types, scale, offset, bit flags)
- `DecoderInterface` — accepts `RawFrame`, produces typed signals
- Schema validation (reject unsupported types, invalid offsets, circular references)
- Decoder implementation for fixed-layout byte frames (Modbus-style, custom protocols)
- Endianness handling (little/big endian, documented per schema)
- Bit flag extraction (decompose uint8 into named bools)
- Error handling (malformed frames → discard + log + stats, not exception)

**Signal data model**:
- `SignalValue` = `std::variant<bool, int64_t, double, QString>`
- Metadata: name, unit, type tag, optional scale/offset/description
- Timestamps preserved from `RawFrame::recvAt` (steady_clock nanoseconds)

**Deferred to V1.5+**: GUI schema editor. In V1 users write yaml by hand. Schema validation provides clear error messages to mitigate this.

**Estimated effort**: 8-10 person-days.

## M6 — Signal Buffer

Time-series storage for decoded signals with query API.

**Scope**:
- `SignalBuffer` — per-signal circular buffer (configurable duration / sample cap)
- Query API: time range, decimation (for chart LOD), latest N samples
- Thread-safe concurrent writer + multiple concurrent readers
- Memory budget management (global cap across buffers with LRU eviction for inactive signals)
- Persistence hooks (M10 session writer reads from here)

**Value representation**: variant-based per M5's `SignalValue`. Internal storage optimized per-type (bool packed to bits, int64 + double as 8-byte, QString as ref-counted; numeric types dominate expected workload).

**Estimated effort**: 8-10 person-days.

## M7 — Expression Engine

Derived signals computed from base signals using exprtk.

**Scope**:
- `Expression` class wrapping exprtk
- Registration of base signals as variables (`sig_a`, `sig_b`, ...)
- Compilation + validation (syntax errors → diagnostic with position)
- Re-evaluation on base signal update (event-driven, not polling)
- Result storage as a new signal in `SignalBuffer`
- Cycle detection (expression A depending on expression B depending on A → reject at registration)
- Error handling during evaluation (division by zero, domain error → publish error signal)

**Type handling**: exprtk is double-only. bool/int64 sources auto-convert to double; QString sources cannot be used in expressions (validation-time error).

**Estimated effort**: 5-7 person-days.

## M8 — Real-time Chart UI

Live-updating time-series charts for decoded signals.

**Scope**:
- Chart widget (QQuickWidget + Scene Graph per ADR-001)
- Signal selector (pick signals from `SignalBuffer` registry)
- Auto-scrolling time axis with user override (pause / zoom)
- Multi-axis support (primary + secondary, units-aware)
- Sub-second update frequency (target 30Hz render)
- Decimation for large time ranges (LOD querying from `SignalBuffer`)
- Alignment with M1 spike verdict (60 signals + 8-12 charts concurrent)

**Out of scope**: Historical chart overlays (compare live vs. replay), annotations, chart export — all later.

**Estimated effort**: 10-12 person-days.

## M9 — Connection Manager (full features)

Upgrade M3's preview Connection Manager to production-quality.

**Scope**:
- Multiple concurrent connections
- Save / load connection configurations (yaml persistence per `[Arch §8]`)
- Favorites / recent connections list
- Connection status dashboard
- Integration with M5 schema selection per connection

**Estimated effort**: 5-7 person-days.

## M10 — Session Writer

Record live signal data to disk for later replay.

**Scope**:
- Session file format specification (binary with yaml metadata sidecar or header)
- Writer: reads from `SignalBuffer`, writes to disk incrementally
- Session metadata: driver configs, schemas, ClockOrigin, duration, summary stats
- User control: start / stop / pause recording via UI
- Robust to crash (partial session recoverable up to last write point)

**Estimated effort**: 6-8 person-days.

## M11 — Session Replayer (completes ReplayDriver)

Fill in the M3 ReplayDriver skeleton with actual session file reading and frame emission.

**Scope**:
- Parse session file format from M10
- Time-accurate frame emission (respects original inter-frame intervals)
- Playback speed control (0.25x - 4x)
- Seeking within session
- Loop mode
- TODO(M9) insertion points from M3 `replay_driver.cpp` all filled (note: originally flagged as M9, renumbered to M11 in v2 roadmap)

**Estimated effort**: 5-7 person-days.

## M12 — Performance Optimization

Profile-guided optimization based on M3/M8 baselines.

**Scope**:
- Benchmark comparisons vs M3 baseline + new baselines for M4-M8 additions
- Hot-path identification (per-signal allocation, lock contention, chart render cost)
- Targeted optimization per identified hotspot
- Verification: no regression, measured improvement
- Re-evaluation of architectural assumptions (e.g., if Qt signal overhead dominates, consider direct callbacks in hot path; if so, requires ADR)

**Estimated effort**: 6-10 person-days (open-ended; depends on findings).

## M13 — Polishing + Packaging

Release preparation.

**Scope**:
- User documentation (quickstart, schema authoring guide, architecture overview)
- Desktop file + icon for Linux packaging
- .deb / .AppImage packaging
- Installer smoke tests on clean Ubuntu 24.04 VM
- Release notes for v0.1.0 (first minor release, post-alpha)

**Estimated effort**: 5-7 person-days.

## V1 Summary

- M0-M13 target: **100-130 person-days total** across all milestones
- V1 release target: **v0.1.0** (no alpha suffix) after M13
- V1 feature set: driver + pipeline + decoder + signal buffer + expression + live chart + connection management + session record/replay + packaging
- V1.5 deferred items: visual schema editor, Modbus/CAN drivers, additional chart types, multi-session comparison

## V1.5 / V2 backlog (non-committing preview)

Items deferred from V1 that have been mentioned in discussion:

- Visual yaml schema editor (replaces hand-authoring)
- Modbus/CAN driver stubs (currently arch-reserved only)
- Multi-session comparison UI
- Plugin-based decoder (alternate to yaml schema for complex protocols)
- Remote Sentry.io crash upload (backend currently local-only)
- Cross-platform builds (Windows, macOS; V1 is Ubuntu-only)
