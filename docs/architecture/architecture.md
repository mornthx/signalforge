# SignalForge Architecture and First-Release Execution Plan

| Field | Value |
|---|---|
| Document version | 0.4 |
| Status | Baseline for V1 development |
| Target platform | Ubuntu 22.04 LTS (x64) |
| Qt version | 6.10.2 (planned migration to 6.12 LTS, see §12.6) |
| Language | C++20 |
| File path | `docs/architecture/architecture.md` |

This document is the authoritative technical baseline for SignalForge V1. It defines scope, architecture, data model, threading, UI structure, performance targets, testing strategy, observability, CI/CD, the 16-week sprint plan, and acceptance criteria. All downstream PRDs, interface specs, task breakdowns, and test plans treat this document as source of truth.

---

## Change Log

### v0.4 (current)

- Qt version pinned to **Qt 6.10.2** (matching the team's local environment). A planned migration to Qt 6.12 LTS is documented in §12.6.
- CMake minimum lowered to **3.22**, matching Qt 6.9+ requirements.
- Recommended compiler changed to **GCC 12+**, with GCC 11.4 documented as a fallback.
- Added §12.6: Qt version support lifetime and migration strategy.
- Added §8.6: evaluation of the Qt Graphs module and the decision to not adopt it.
- Git hosting pinned to **GitHub**; CI pinned to **GitHub Actions**.

### v0.3

- Removed Modbus from V1 (deferred to V1.5, interface reserved).
- Removed Windows and macOS support from V1 (Ubuntu-only).
- Removed auto-update from the product (deferred to V2).

### v0.2

- Removed CAN bus from V1 (deferred to V1.5, interface reserved).
- Removed Scenario Layer from the V1 codebase (design memo only).
- Locked the threading model: queue types, backpressure policies, time source, per-device IO threads.
- Locked the session format: custom binary chunk with explicit schema versioning; Parquet deferred.
- Added sections on cross-platform matrix, licensing, observability, testing, and CI/CD.

### v0.1

- Initial draft of scope, layered architecture, and 16-week plan.

---

## Table of Contents

1. Document Goals
2. Product Definition
3. V1 Scope
4. Technical Architecture
5. Threading Model
6. Data Model
7. UI Structure
8. High-Performance Chart Strategy
9. Control Path Design
10. Record and Replay
11. Project File Design
12. Platform and Environment
13. Licensing and Compliance
14. Observability Infrastructure
15. Testing Strategy
16. CI/CD, Packaging, and Distribution
17. 16-Week Execution Plan
18. Recommended Source Tree
19. First-Release Acceptance Criteria
20. Immediate Next Steps
21. Subsequent Document Backlog
22. Conclusion
23. Glossary

---

## 1. Document Goals

1. Fix the V1 product boundary and prevent scope drift.
2. Fix the Qt-based technical architecture with performance as the overriding concern.
3. Define module layering, threading model, data model, and UI structure.
4. Provide a 16-week execution plan with phase-level acceptance criteria.
5. Act as the baseline for downstream PRDs, interface specs, task breakdowns, and test plans.

---

## 2. Product Definition

### 2.1 Positioning

SignalForge is a high-performance desktop workbench for embedded development, device bring-up, test verification, and production-line testing. Its core capabilities are:

- Real-time observation
- Bidirectional control
- Record and replay
- Scenario testing (V2)

### 2.2 Product Principles

1. Performance first. The high-frequency data path and UI rendering are decoupled.
2. Prove the integration loop first; aesthetics and helper features come later.
3. Build an engineering workbench, not a presentation-oriented dashboard tool.
4. Every meaningful user action is saveable, reproducible, and replayable.
5. Users should be able to complete a first-touch integration without learning a scripting or configuration format.

### 2.3 Competitive Differentiators for V1

V1 must be credibly strong in each of these areas:

- High-frequency data throughput and stability
- Control-path capability (automatic handshake, polling, macro commands)
- Mixed-rate message handling
- Debugging-asset persistence (projects, recordings, replay)

---

## 3. V1 Scope

### 3.1 V1 Goal

Within 16 weeks, deliver a first release usable for real device bring-up, meeting:

- Connect to devices (serial / TCP / UDP)
- Observe data
- Send commands
- Record and replay
- Save the workspace

### 3.2 In Scope

**Protocols**:

- Serial / UART
- TCP
- UDP
- File-based replay (via ReplayDriver)

**Observation**:

- Raw frame inspection
- Signal tree
- Value cards
- Status cards
- High-frequency charts
- Log table
- Alarm strip

**Control**:

- Post-connect automatic commands
- Periodic polling
- Manual command dispatch
- Command templates
- Macro commands
- Response matching

**Project lifecycle**:

- Save project
- Save workspace layout
- Save decode rules
- Save command panel
- Record and replay

**Platform**: Ubuntu 22.04 LTS (x64), single platform.

### 3.3 Out of Scope for V1

The following do **not** enter V1:

- **Modbus** (deferred to V1.5; driver interface reserved)
- **CAN bus** (deferred to V1.5; driver interface reserved)
- **Windows and macOS support** (evaluated for V1.5)
- **Auto-update** (not before V2)
- BLE, MQTT
- 3D visualization, audio/FFT
- Cloud sync, multi-user collaboration
- Plugin marketplace, scripting extension system
- Automated reporting platform

### 3.4 Scenario Layer: Explicitly Deferred

The Scenario Layer described in the architecture's conceptual design is **not** implemented in V1, not even as stubbed interfaces. Its design notes live in `docs/future/scenario-design-note.md` and will be reconsidered when V2 is scoped. Preserving unused interfaces has historically led to fit problems when features are actually built, so V1 intentionally keeps zero footprint.

### 3.5 V1.5 and V2 Candidates (Reference Only)

This list does not constrain V1. It records known future directions so that V1 architecture can avoid foreclosing them.

- **V1.5**: Modbus, CAN, Windows support
- **V2**: macOS support, Scenario test execution, auto-update, cloud sync

---

## 4. Technical Architecture

### 4.1 Technology Selection

| Concern | Selection | Notes |
|---|---|---|
| Language | C++20 | Do not use C++23 features |
| Qt version | Qt 6.10.2 (non-LTS) | Local environment; migration planned, see §12.6 |
| UI shell | Qt Widgets | `QMainWindow` + Dock |
| High-frequency visualization | Qt Quick / Scene Graph | Embedded via `QQuickWidget` |
| Device I/O | `QSerialPort`, Qt Network | |
| Data model | Qt Model/View plus custom core data layer | |
| Build system | CMake 3.22+ | |
| Testing | Catch2 + Qt Test + replay-based integration tests | |
| Logging | spdlog (async, rotating file sink) | |
| Crash reporting | Crashpad (Linux) | Local minidumps, no upload backend in V1 |
| Expression engine | ExprTk (header-only) | Only for DerivedSignal |
| Config serialization | yaml-cpp for projects, nlohmann/json for layouts | |
| Lock-free queues | moodycamel::ConcurrentQueue (MPSC) + hand-rolled SPSC ring | |

### 4.2 Architecture Principles

1. The UI is not the data bus.
2. IO and parsing do not run on the main thread.
3. Chart rendering does not allocate one UI object per data point.
4. High-speed data uses ring buffers and windowed rendering.
5. All configuration persists as human-readable project files.
6. Every cross-thread queue has an explicit backpressure policy; no queue is unbounded.
7. Every persisted file has an explicit schema version; upgrades require a migration function.
8. All timestamps use a monotonic clock (`steady_clock`); human-readable time is stored separately.

### 4.3 Module Layering

The system is layered A through J. Layers are allowed to depend only on layers above them in this list; circular dependencies are forbidden.

#### A — Platform Layer

Serial, Socket, filesystem, time source (monotonic clock + wall clock), thread scheduling, process arguments, crash-reporting initialization.

#### B — Driver Layer

Unified device interface:

- `open()` / `close()` / `start()` / `stop()` / `write()` / `health()` / `statistics()`

V1 drivers:

- `SerialDriver`
- `TcpDriver`
- `UdpDriver`
- `ReplayDriver`
- **Reserved (not implemented)**: `ModbusDriver`, `CanDriver`

#### C — Frame Layer

`RawFrame`, `FrameEnvelope`, `RxStats`, `TxStats`, `BackpressureSignal`.

#### D — Decode Layer

`MessageType`, `MessageInstance`, `DecodeRule`, `FieldDef`, `DecodeError`.

#### E — Signal Layer

`SignalId`, `SignalValue`, `SignalState`, `Freshness`, `DerivedSignal` (ExprTk backend).

#### F — Action Layer

`ActionTemplate`, `ActionInstance`, `ActionSchedule`, `AckMatcher`, `MacroAction`.

#### G — Session Layer

`SessionMetadata`, `SessionChunk`, `ReplayIndex`, `Bookmark`, `SessionFormatVersion`.

#### H — Scenario Layer (not in V1)

Design memo only. See §3.4.

#### I — Presentation Layer

Split into two parts:

- **Qt Widgets** for the workbench shell
- **Qt Quick** for high-frequency visualization (embedded via `QQuickWidget`)

#### J — Observability Layer

Cross-cutting. Depended on by every other layer:

- Structured logging (spdlog)
- Runtime metrics (internal performance panel)
- Crash reporting (Crashpad)
- Tracing hook (Tracy integration reserved; off by default)

---

## 5. Threading Model

### 5.1 Thread Allocation

| Thread | Count | Responsibility | Blocking operations |
|---|---|---|---|
| Main thread | 1 | Main window, docks, Model/View | No IO; no computation > 10 ms |
| Render thread | 1 (Qt Quick internal) | Scene Graph drawing | Managed by Qt |
| IO thread | 1 per active device | Serial / TCP / UDP read+write | Blocking reads allowed |
| Decode worker pool | N (default 2, configurable) | Frame segmentation, field extraction | CPU-bound only |
| Session writer | 1 | Recording, index flushing | Blocking file IO allowed |
| Background task thread | 1 | Project save, long-running export | Blocking allowed |

`QSerialPort` and `QTcpSocket` bind to the event loop of their owning thread; they cannot migrate between threads in a pool. V1 uses **one dedicated IO thread per device**.

### 5.2 Queues and Data Flow

```
[IO Thread]  --SPSC-->  [Decode Worker Pool]  --MPSC-->  [Signal Store / Session Writer]
                                                                   |
                                                         snapshot (30 Hz ticker)
                                                                   v
                                                            [Main Thread UI]
```

| Queue | Type | Capacity | Behavior on full |
|---|---|---|---|
| IO → Decode | SPSC lock-free ring | 64 K frames per device | Emit backpressure signal; drop oldest (configurable to drop newest) |
| Decode → Signal Store | MPSC lock-free queue | 256 K messages | Emit backpressure signal; count drops |
| Signal Store → UI | Double-buffered snapshot | Latest one | UI pulls at 30 Hz; no backlog |
| Session Writer input | MPMC with blocking | 4–64 MB disk buffer (configurable) | Slow disk backpressures Decode |

### 5.3 Backpressure Policy

When a queue passes its high-water mark (default 80%), a `BackpressureSignal` is raised:

1. Logged at warn level.
2. Shown on the performance panel in red.
3. Policy applied:
   - IO layer: reduce read frequency or drop oldest frames.
   - Decode layer: skip non-critical message types (flagged by rule).
   - Session layer: degrade to raw-frame-only recording.
4. A backpressure event is written to the session as a data-quality marker.

**No queue is unbounded under any configuration.**

### 5.4 Time Source

- Internal timestamps use `std::chrono::steady_clock` at nanosecond precision.
- **Reception timestamp**: the IO thread stamps as early as possible after reading bytes.
- **Device timestamp**: if the message carries one, stored in `MessageInstance.deviceTimestamp`.
- **Wall clock** (`std::chrono::system_clock`): used only for session metadata headers, file names, and UI display. Never used for comparison.
- **Device clock drift correction**: not in V1.

Every session file header records both the `steady_clock` origin and its corresponding `system_clock` value, so human-readable time can be reconstructed later.

### 5.5 Synchronization Principles

1. The main thread consumes only snapshots.
2. Device data flows through the core data bus, never directly into UI widgets.
3. High-frequency zones refresh at a fixed tick rate (default 30 Hz, configurable to 60 Hz).
4. Low-frequency zones refresh on events.
5. Cross-thread Qt signal connections use `Qt::QueuedConnection` explicitly. No implicit direct connections across threads.

---

## 6. Data Model

### 6.1 Core Objects

`Project`, `RawFrame`, `MessageType`, `MessageInstance`, `Signal`, `DerivedSignal`, `Action`, `Session`. Field-level definitions are maintained in the module interface document (backlog item §21).

Notable fields:

- `Project.schemaVersion` — integer, bumped on every schema change
- `Session.formatVersion` — integer, bumped on every session-format change
- `Signal.quality` — enum (`GOOD`, `STALE`, `UNCERTAIN`, `BAD`); derived from Freshness and DecodeError

### 6.2 Configuration and Storage Formats

| File | Format | Purpose | Version field |
|---|---|---|---|
| `project.yaml` | YAML | Project root | `schemaVersion: 1` |
| `layouts/*.json` | JSON | Dock layout snapshots | `schemaVersion: 1` |
| `decode/*.yaml` | YAML | Decode rules | `schemaVersion: 1` |
| `actions/*.yaml` | YAML | Command templates | `schemaVersion: 1` |
| `sessions/*.sfr` | Custom binary chunk | Raw frames and messages | Magic + version in header |
| `sessions/*.sfi` | Custom binary index | Time index and bookmarks | Magic + version in header |

**Parquet is not used in V1.** Evaluation is deferred until V2 once there is a concrete analytics need.

`.sfr` file header layout (64 bytes):

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | Magic |
| 4 | 4 | Format version |
| 8 | 16 | Session UUID |
| 24 | 8 | `steady_clock` origin (ns) |
| 32 | 8 | `system_clock` origin (ns since epoch) |
| 40 | 24 | Reserved |

Chunk types: `RawFrameChunk`, `MessageInstanceChunk`, `BookmarkChunk`, `StatsChunk`, `EventChunk`.

`.sfi` is an independent index file, rebuildable from `.sfr`. It records file offsets every N seconds.

### 6.3 Schema Versioning Rules

1. Any field change bumps `schemaVersion`.
2. Older files opened by a newer build are automatically migrated to the newest version before the first write.
3. A newer file opened by an older build must fail cleanly with an explicit error. Silent field loss is forbidden.
4. Migration functions stay in the codebase for at least one year, or two minor versions, whichever is longer.

---

## 7. UI Structure

### 7.1 Main Window

`QMainWindow` plus Dock panels. Default zones:

- **Left**: device tree and signal tree
- **Center**: primary workspace (Connect / Decode / Observe / Control / Replay tabs)
- **Right**: property inspector and widget configuration
- **Bottom**: logs, raw frames, events, performance metrics

### 7.2 Primary Workspaces

**Connect**: device selection, connection configuration, recent connections, automatic-handshake setup, connection test.

**Decode**: raw frame list, per-frame preview, message recognition results, field structure, decode-rule editing, error messages.

**Observe**: value cards, status cards, charts, log stream, alarm strip.

**Control**: command panel, macros, periodic polling, manual dispatch, ACK status.

**Replay**: timeline, playback speed, bookmark navigation, signal-change review.

### 7.3 UI Principles

1. High-frequency charts and regular property panels live in strictly separated rendering layers.
2. Information density is high, but visual hierarchy is clear.
3. Avoid modal dialogs on first screen.
4. Complex configuration goes into the right-hand property inspector, not into modal editors.
5. Workspace layout presets are supported.
6. Before embedding `QQuickWidget` in Dock panels, verify floating, HiDPI scaling, and right-click behavior per §8.5.

---

## 8. High-Performance Chart Strategy

### 8.1 Goals

Charts must support:

- High refresh rate
- Multiple channels
- Pan and zoom
- Live window
- Historical replay

### 8.2 Rendering Strategy

1. Custom `QQuickItem` subclass.
2. Scene Graph nodes (`QSGGeometryNode`), not one UI object per point.
3. Per-signal ring buffer for the live window.
4. Per-pixel downsampling before draw (min / max / avg triplet).
5. Mouse interaction decoupled from drawing.

### 8.3 Data Strategy

- Live window: in-memory ring buffer, default 256 K points per signal.
- Long-term record: chunk file.
- Replay view: reconstruct window from time range; cold start uses the `.sfi` index for seek.

### 8.4 Performance Targets

Baseline hardware: a typical mid-range office laptop, roughly Intel Core i5-1240P with 16 GB RAM, running Ubuntu 22.04. Targets scale up on stronger hardware but must hold on this class.

| Metric | Target | Measurement |
|---|---|---|
| Online signal count | ≥ 100 | Observe page active |
| Live chart components | ≥ 20 | Same layout |
| Aggregate input rate | ≥ 10 k points/s total; single signal ≥ 1 kHz | Performance regression suite |
| End-to-end latency, P50 | ≤ 80 ms | IO-read to UI pixel update |
| End-to-end latency, P99 | ≤ 200 ms | Same |
| UI frame rate | ≥ 30 FPS sustained; ≥ 20 FPS in short bursts | Qt Quick profiler |
| Cold start to main window | ≤ 3 s | Empty project |
| Open 1 GB session for replay | ≤ 5 s to playable | Once index is preloaded |
| RSS ceiling, 24-hour record | ≤ 1.5 GB | `/proc/self/status` plus valgrind sampling |
| Dropped frame rate (normal load) | < 0.01% | Backpressure counters |
| Long-run stability | 72 hours with no crash or leak | LeakSanitizer + automation |

### 8.5 Qt Widgets + Qt Quick Integration Constraints

Sprint 1–2 must validate these five concerns before committing to `QQuickWidget` for the chart component:

1. `QQuickWidget` inside `QDockWidget` correctly renders during float, re-dock, and cross-monitor drag.
2. HiDPI scaling at 125%, 150%, 175%, and 200% renders text and edges cleanly, tested under Ubuntu GNOME and KDE.
3. Right-click menus traverse the Widgets / Quick boundary with correct event propagation.
4. `QQuickWidget` hidden and then re-shown stops and resumes the render thread cleanly (no leaks over 10 s dwell).
5. Multiple live `QQuickWidget` instances do not exhaust GPU resources on integrated graphics.

**Fallbacks** if any concern fails:

- Primary fallback: `QWidget::createWindowContainer` wrapping a `QWindow`.
- Aggressive fallback: QPainter + OpenGL. This would force M6 to be substantially rewritten.

Fallback decision must be made before Sprint 2 closes.

Qt 6.10's `QQuickWidget` is more stable than early Qt 6 versions, especially under the RHI path. The validation is still required, but expected risk is lower than in earlier Qt 6.

### 8.6 Evaluation of the Qt Graphs Module

Qt 6.10 ships the Qt Graphs module as the successor to Qt Charts. V1 **does not adopt Qt Graphs**, for three reasons:

1. Qt Graphs emphasizes 3D visualization and pre-built chart types. Its optimization target is presentation richness, not 10 k+ points/second throughput.
2. We need explicit control over downsampling (min/max/avg) and ring-buffer integration. The Qt Graphs series abstraction would add glue code rather than remove it.
3. Qt Graphs licensing for LGPL v3 dynamic linking requires additional review, whereas a bespoke Scene Graph implementation depends only on Qt Core and Qt Quick.

Future replacement remains possible: the high-frequency chart rendering is encapsulated behind a `ChartItem` interface, so swapping to Qt Graphs later is not architecturally blocked.

---

## 9. Control Path Design

### 9.1 Action Types

Single command, periodic command, post-connect automatic command, macro command.

### 9.2 ACK Model

Per action, configure:

- Timeout
- Expected response pattern
- Matching fields
- Success / failure state

### 9.3 UI Presentation

The Control page must display:

- Most recent transmission
- Most recent response
- Periodic task status
- Timeout count
- Success rate

---

## 10. Record and Replay

### 10.1 Recording Requirements

- Record raw frames
- Record decoded signals
- Support bookmarks
- Support exception-event tagging
- Record backpressure events as data-quality markers

### 10.2 Replay Requirements

- Original-speed playback
- Variable-speed playback (0.1×, 0.25×, 0.5×, 1×, 2×, 4×, 10×)
- Seek playback
- Replay drives the same Observe pages as live data

### 10.3 Design Principle

Replay does not re-execute the IO drivers. Instead, `ReplayDriver` implements the same driver interface and injects historical messages directly onto the core data bus. Upper layers cannot distinguish live from replay.

### 10.4 Format

V1 uses the custom binary chunk format (`.sfr`) with an independent index (`.sfi`), both carrying explicit schema versions. See §6.2.

---

## 11. Project File Design

### 11.1 Project Directory

```
project_root/
├── project.yaml            # schemaVersion required
├── layouts/
│   └── default.json
├── decode/
│   └── rules.yaml
├── actions/
│   └── default.yaml
├── sessions/
│   ├── 2026-04-22_14-03-12.sfr
│   └── 2026-04-22_14-03-12.sfi
├── assets/
└── .signalforge/           # Project cache; safe to delete
    ├── thumbnail.png
    └── recent.json
```

### 11.2 Persistence Principles

1. Configuration is human-readable.
2. Recordings are space-efficient.
3. Layouts are restorable.
4. Version upgrades are migratable (see §6.3 for migration lifetime).
5. Every file carries an explicit `schemaVersion`. There is no implicit default.

---

## 12. Platform and Environment

### 12.1 Operating System

**Only supported target**: Ubuntu 22.04 LTS (x64).

Desktop environments: GNOME (default) and KDE Plasma must launch and be usable. Other desktops (XFCE, Unity, etc.) are not actively validated but should not be deliberately broken.

### 12.2 Compiler Matrix

| Compiler | Version | Role |
|---|---|---|
| GCC | **12+ recommended**, 11.4 as fallback | Primary |
| Clang | 14+ | CI cross-check for consistency, not primary |

Ubuntu 22.04 ships GCC 11.4 by default; GCC 12 is available via `apt install g++-12`. Qt 6.10 officially supports GCC 11, but several C++20 features (notably `std::jthread`, `std::barrier`, and parts of the ranges API) are more complete on GCC 12. Upgrading via apt does not alter the base system.

### 12.3 Qt Acquisition

- Install Qt 6.10.2 through the official Qt online installer.
- **Do not** use the `qt6-base-dev` packages from the Ubuntu repository; they are out of date and incomplete.
- CI caches the Qt installation directory.
- Standard path convention: `~/Qt/6.10.2/gcc_64/`.
- `CMAKE_PREFIX_PATH` is resolved through an environment variable or CMake preset; it is never hardcoded into a `CMakeLists.txt`.

### 12.4 Hardware Access

| Type | Backend | Notes |
|---|---|---|
| Serial | `QSerialPort` | `/dev/ttyUSB*` and `/dev/ttyACM*`; requires `dialout` group membership or appropriate udev rules |
| TCP / UDP | Qt Network | Standard sockets |
| Virtual serial (test) | socat PTY pair | Required test infrastructure |

### 12.5 Non-Goals

- Windows (V1.5 candidate)
- macOS (not before V2)
- Other Linux distributions (may work, unsupported)
- Embedded ARM boards
- WebAssembly

### 12.6 Qt Version Lifetime and Migration Strategy

**Facts the team must internalize**:

- Qt 6.10 was released on October 7, 2025 as a **non-LTS release**.
- Qt's open-source support for a minor version ends when the next minor is released. Qt 6.11 has shipped (Spring 2026), so **Qt 6.10's open-source standard support has already ended**. Only commercial licensees continue to receive 6.10.x patches.
- The next LTS is Qt 6.12, expected in Autumn 2026.

**V1 Strategy (primary path)**:

1. Development (Sprints 1–16) uses Qt 6.10.2, matching the local environment. This minimizes variance during active development.
2. Internal alpha / beta releases during V1 ship against Qt 6.10.2.
3. **Before V1.0 publishes**, the project migrates to **Qt 6.12 LTS** once it is available and has at least one patch release.
4. Migration is scheduled after V1 internal-beta feedback is stable, before V1.0 publishes. Reserve at least two weeks.
5. **Forbidden**: using any Qt 6.10-only API that is not forward-compatible with Qt 6.12. Any such usage must be wrapped behind `src/platform/qt_compat.hpp` with a fallback path.

**Migration cost estimate**: low.

- Qt 6.10 → 6.12 is a minor-version transition within the same major line; API stability is strong.
- Main work: re-run the full test suite, validate `QQuickWidget` behavior for regressions, confirm Crashpad integration unchanged.
- Expected: one engineer for one week.

**Fallback plan**: if the team cannot accept a mid-project Qt migration, ship V1.0 against Qt 6.10.2. This is the fallback, not the primary path. The cost is that V1.0 users run on a version that no longer receives open-source security patches.

---

## 13. Licensing and Compliance

### 13.1 Qt License

V1 ships under **LGPL v3 with dynamic linking**:

- All Qt modules link dynamically.
- The installer includes instructions for obtaining Qt source code.
- Formal license review runs before the first release.

### 13.2 Third-Party Dependencies

| Component | License | Compatibility |
|---|---|---|
| spdlog | MIT | Compatible |
| fmt | MIT | Compatible |
| nlohmann/json | MIT | Compatible |
| yaml-cpp | MIT | Compatible |
| Catch2 | BSL-1.0 | Compatible (tests only) |
| ExprTk | MIT | Compatible |
| moodycamel::ConcurrentQueue | BSD-2 or BSL | Compatible |
| Crashpad | Apache 2.0 and associates | Compatible |

### 13.3 Compliance Checklist

- [ ] SBOM (CycloneDX) generated before every release
- [ ] `NOTICE` file contains full license text for every dependency
- [ ] About dialog lists all major third-party components and their licenses
- [ ] Export-control review: this product handles generic data; no encryption export controls apply

---

## 14. Observability Infrastructure

### 14.1 Logging

- Library: spdlog in async mode with a rotating file sink.
- Default level: info.
- Format: JSON lines. Fields: `ts`, `level`, `thread`, `module`, `event`, `fields`.
- Storage: `$XDG_STATE_HOME/signalforge/logs/`, falling back to `~/.local/state/signalforge/logs/`.
- Rotation: 10 MB per file, 10 files kept.
- Users can enable trace level in settings for diagnostics.

### 14.2 Performance Panel

An in-app performance panel (collapsed by default) shows:

- Per-device RxStats / TxStats / queue watermarks
- Decode worker throughput and latency
- Signal Store snapshot rate
- UI frame rate and dropped frames
- Current memory usage (from `/proc/self/status`)
- Backpressure event counts

### 14.3 Crash Reporting

- Library: Crashpad (Linux is well supported).
- Behavior: on crash, write a minidump to `~/.local/state/signalforge/crashdumps/`. On next launch, the user can choose to export a tarball of minidumps.
- **V1 does not operate an upload backend.** Local export only.
- systemd-coredump is a backup; not a hard dependency.

### 14.4 Tracing

Tracy integration is wired up but disabled by default, stripped from release builds. Used only for internal deep-dive profiling sessions.

---

## 15. Testing Strategy

### 15.1 Testing Pyramid

| Layer | Target | Tools |
|---|---|---|
| Unit | ≥ 70% line coverage on core modules | Catch2 |
| Module integration | Frame ↔ Decode ↔ Signal pipeline | Catch2 with mock drivers |
| Replay integration | End-to-end via pre-recorded `.sfr` | Internal test harness |
| Protocol fuzz | Malformed-frame resilience | libFuzzer |
| UI smoke | Happy paths do not error | Qt Test |
| Performance regression | §8.4 gating | Internal perf harness |
| Manual acceptance | Real-hardware loop | Human QA |

### 15.2 Hardware-in-the-Loop Substitutes

V1 does not invest in dedicated HIL hardware. Substitutes:

- Virtual serial pairs: `socat -d -d pty,raw,echo=0 pty,raw,echo=0`
- TCP/UDP mock traffic: Python scripts
- Golden session replay: QA maintains a library of `.sfr` recordings, all replayed before every release
- Real devices: QA keeps at least two USB-to-serial adapters of different brands, and one TCP-capable test device (for example, an embedded board with a TCP echo service)

### 15.3 Performance Regression Gates

Starting at Sprint 13, a performance baseline is established. Going forward:

- Every merge to `main` triggers the performance CI job.
- Any §8.4 metric degrading by ≥ 20% blocks the merge.
- Every release runs the full 72-hour stability test.

### 15.4 Fuzz Testing

Weekly scheduled fuzz runs target:

- Raw frame decoders
- Project file loader
- Session file loader

Target: zero crashes, zero uncaught exceptions, zero heap-corruption reports.

### 15.5 Memory and Thread Tooling

- Day-to-day development runs with AddressSanitizer.
- Weekly CI runs a full ThreadSanitizer pass.
- Every release runs a sampled valgrind memcheck.

---

## 16. CI/CD, Packaging, and Distribution

### 16.1 Code Hosting and CI

- **Code hosting**: GitHub. The `main` branch is protected: required PR, required status checks, no force push, no direct push.
- **CI**: GitHub Actions.
- **Build environment**: Ubuntu 22.04 + GCC 12 + Qt 6.10.2 + CMake 3.22+. No build matrix.
- **Merge gate**: build passes; tests pass; net new code has ≥ 70% coverage; no AddressSanitizer violations.

### 16.2 Versioning

- Semantic versioning: `MAJOR.MINOR.PATCH`. V1's first production release is `1.0.0`.
- Each completed sprint produces an alpha tag, e.g., `0.3.0-alpha.1` at the end of Sprint 3.
- Alpha builds may link against Qt 6.10.2. The final 1.0.0 release may link against Qt 6.12 LTS depending on the §12.6 decision.

### 16.3 Packaging

| Format | Notes |
|---|---|
| AppImage | Single-file install-free distribution; the primary channel |
| `.deb` | For internal corporate distribution via apt |

Qt runtime is bundled via `linuxdeployqt` into the AppImage. The `.deb` package declares dynamic dependencies with explicit Qt version constraints.

**No code signing is required.** Linux does not mandate it for desktop applications, and removing signing eliminates DevOps overhead.

### 16.4 Installer Capabilities

- AppImage runs without root and without installation.
- `.deb` follows standard apt semantics for install / remove / upgrade. `postinst` registers file associations for `.sfproj` and `.sfr`.
- **No auto-update.** Not before V2.
- Uninstall preserves user project directories (for example, `~/signalforge/projects/`).

---

## 17. 16-Week Execution Plan

### 17.1 Team Assumptions

Baseline staffing for this plan:

- **2 full-time C++ engineers** (one on core data + charts, one on drivers + UI application)
- **1 QA** joining from Sprint 3, preparing test fixtures in advance
- **0.2 DevOps** for CI bootstrap and occasional maintenance
- **0.5 product / interaction** for requirements and interaction review

With 3 C++ engineers, the plan has generous buffer for experimental optimization during the performance and polish phases. With fewer than 2 C++ engineers, Modbus-equivalent features or replay scope must be cut at the Sprint 2 checkpoint.

### 17.2 Sprint Breakdown

#### Sprint 1–2: Project bootstrap and Qt Quick integration spike

- CMake project and module layout (`cmake_minimum_required 3.22`, `CMAKE_CXX_STANDARD 20`)
- Main window and Dock frame
- spdlog integration and embryonic performance panel
- Crashpad integration validation
- Driver interface, basic `SerialDriver` and `TcpDriver`
- **Qt Quick integration spike report on Qt 6.10.2 (required)**, covering §8.5 in full
- Confirm usage boundary for Qt 6.10-only APIs (§12.6)

**Acceptance**: cold-start to main window ≤ 3 s; serial or TCP connection shows raw data; Qt Quick integration verdict is final.

#### Sprint 3–4: Frame layer and Decode page

`RawFrame` pipeline, Decode model, raw-frame table, frame inspector, basic rule configuration, `UdpDriver`.

**Acceptance**: frames visible; basic field slicing works; decode rules save correctly; backpressure path is testable.

#### Sprint 5–6: Signal layer and Observe page

Signal Store, value cards, status cards, high-frequency chart v1, signal tree, DerivedSignal (ExprTk).

**Acceptance**: multiple signals refresh live; chart scrolls smoothly; view layouts save correctly; basic DerivedSignal expressions work.

#### Sprint 7: Control page (first half) + **Checkpoint 1**

Command templates, manual dispatch, auto-handshake, initial ACK matching.

**Checkpoint 1 (Sprint 7 close)**:

- Review actual performance against §8.4 targets
- Review realized velocity vs plan
- Decide: keep original scope, trim DerivedSignal capability, or trim macros
- Check for any known Qt 6.10 issues that would force V1.0 off 6.10 (trigger the Qt 6.12 LTS evaluation if yes)

#### Sprint 8: Control page complete

Periodic commands, ACK timeout and retry, macros.

**Acceptance**: post-connect auto-commands work; periodic polling is stable; ACK status is clear.

#### Sprint 9: Session Writer and file format

`.sfr` / `.sfi` format implementation, independent verification tool, recording pipeline closed loop.

**Acceptance**: recordings produce valid files; the standalone dump tool can parse and verify them; format spec document is complete.

#### Sprint 10: Replay page foundation

Replay UI skeleton, timeline, original-speed and basic variable-speed playback, `ReplayDriver` feeding the data bus.

**Acceptance**: loading a `.sfr` drives the Observe page.

#### Sprint 11: Replay full capability

Seek, bookmarks, full variable-speed ladder (0.1× through 10×), UI responsiveness consistency during replay.

#### Sprint 12: Integration polish + **Checkpoint 2**

End-to-end integration walkthrough, unified error messaging, recent-projects and recent-devices, one sample project.

**Checkpoint 2 (Sprint 12 close)**:

- Review that the end-to-end integration loop works
- Freeze V1 scope: only bug fixes going forward
- Decide on performance and polish priorities for the final four sprints
- **Qt 6.12 LTS status check**: if released and stable, evaluate inserting a migration window in Sprint 14–15

#### Sprint 13–14: Performance optimization

Ring buffer optimization, chart downsampling, UI tick refresh tuning, performance panel completion, performance regression baseline.

**Acceptance**: all §8.4 metrics pass; 72-hour stability test passes.

#### Sprint 15–16: Polish and freeze

Layout presets, documentation, packaging (AppImage and `.deb`), regression test pass, first production installer.

**Optional**, pending Checkpoint 2: insert Qt 6.12 LTS migration, approximately one week.

**Acceptance**: deliverable internal-beta release; both AppImage and `.deb` run on a clean Ubuntu 22.04.

### 17.3 Buffer Rationale

No scope change relative to v0.3. Internal rhythm:

- Sprint 9: pure Session Writer
- Sprint 10–11: full Replay pipeline
- Sprint 12: integration polish + Checkpoint 2
- Sprint 13–16: four continuous sprints for performance and polish

If progress runs ahead, Sprint 15 can accommodate the Qt 6.12 LTS migration. If progress runs tight, the internal-beta ships on Qt 6.10.2 and LTS migration slips to a post-V1.0 window.

### 17.4 Risk Register

| Sprint | Risk | Mitigation |
|---|---|---|
| 1–2 | Qt Quick integration falls short | Have QPainter + OpenGL fallback prepared |
| 1–2 | Team Qt environments drift apart | Lock the Qt online installer version; commit `CMakePresets.json` |
| 3–4 | Decode rule expressiveness insufficient | Validate against real device frames, not synthetic |
| 5–6 | Charts miss 30 FPS | Surface numbers at Checkpoint 1, not Sprint 13 |
| 7–8 | ACK matching mis-matches under multiplexing | Require source device ID and sequence number in matchers |
| 9 | `.sfr` format design inadequate | Build the dump tool early to find field gaps |
| 10–11 | Replay and live paths diverge | Replay and live must share the `SignalStore` interface and share integration tests |
| 13–14 | Performance tuning regresses memory | AddressSanitizer + LeakSanitizer routine; valgrind per release |
| 15–16 | Qt 6.12 LTS migration surfaces regressions | Run full test suite pre-migration; this work is optional, skippable under time pressure |
| 15–16 | AppImage incompatible with other distributions | V1 only commits to Ubuntu 22.04 |

---

## 18. Recommended Source Tree

```
src/
├── app/                  # Entry point and main window
├── platform/             # Platform abstraction, time source, threading, crash reporting
├── observability/        # Logging, performance panel, tracing
├── drivers/              # serial / tcp / udp / replay (modbus/can interfaces reserved)
├── frame/                # RawFrame pipeline and backpressure
├── decode/               # Rule engine and error handling
├── signal/               # SignalStore and DerivedSignal (ExprTk)
├── action/               # ActionScheduler and AckMatcher
├── session/              # .sfr / .sfi read/write and migration
├── ui_widgets/           # QMainWindow, Docks, tab shells
├── ui_quick/             # High-frequency chart QQuickItems
├── models/               # Qt Model/View adapters
└── utils/
tests/
├── unit/
├── integration/
├── fuzz/
├── perf/
└── fixtures/             # Golden .sfr samples
resources/
examples/
├── example_project_serial/
└── example_project_tcp/
docs/
└── future/               # scenario-design-note.md and other future memos
ci/
└── github/               # GitHub Actions workflows
packaging/
├── appimage/
└── deb/
CMakePresets.json         # Qt 6.10.2 paths, GCC 12, Ninja bindings
```

---

## 19. First-Release Acceptance Criteria

### 19.1 Functional

- Serial, TCP, and UDP drivers work
- File-based replay works
- Raw frames are visible
- Signals display live
- Commands can be sent
- Periodic polling works
- Record and replay work
- Projects save
- Crash reporting pipeline is exercised (manual crash test)

### 19.2 Performance

All §8.4 metrics pass. In particular:

- End-to-end P99 ≤ 200 ms
- 24-hour recording RSS ≤ 1.5 GB
- 72-hour stability test passes

### 19.3 Quality

- Unit test coverage ≥ 70% on core modules
- Fuzz testing produces no crashes
- AddressSanitizer clean on the full test suite
- AppImage and `.deb` run on a clean Ubuntu 22.04
- SBOM generated; `NOTICE` complete

### 19.4 Experience

- New-project flow is walkable
- Connect-to-chart steps are clear
- Failures localize to the right component (logs plus error messages)
- First screen does not pop dialogs

---

## 20. Immediate Next Steps

1. Lock the source tree and module boundaries.
2. Stand up the `QMainWindow` + Dock shell on Qt 6.10.2.
3. Run the Qt Quick integration spike; reach a verdict before Sprint 2 closes.
4. Wire up a minimum spdlog + Crashpad path.
5. Define the Driver interface and `RawFrame` data structure.
6. Build the first version of the Decode page model and a synthetic data flow.
7. Build a proof-of-concept for the high-frequency chart component.
8. Provision CI on Ubuntu 22.04 + GCC 12 + Qt 6.10.2 + CMake 3.22.
9. Commit `CMakePresets.json` and standardize Qt install paths.
10. File a Sprint-12 tracking task to review Qt 6.12 LTS migration readiness.

---

## 21. Subsequent Document Backlog

1. PRD v0.1
2. Page and interaction specification v0.1
3. Core class diagram and module interface document v0.1
4. Chart rendering technical note v0.1
5. Record/replay format specification v0.1 (`.sfr` / `.sfi` byte-level)
6. Test plan v0.1
7. License and third-party dependency manifest v0.1
8. Runtime log field dictionary v0.1
9. Performance regression baseline definition v0.1
10. Qt 6.12 LTS migration assessment report (output at Sprint 12)

---

## 22. Conclusion

V1 of SignalForge holds to:

- A Qt-native workbench, developed on Qt 6.10.2, with V1.0 migration to Qt 6.12 LTS assessed before release
- Performance first
- Observation and control treated as equals
- Data flow decoupled from UI
- Debugging assets that persist across sessions
- Backpressure, time source, and schema versioning decided in Sprint 1; not revisited later
- Single platform (Ubuntu 22.04) and single protocol family (Serial / TCP / UDP), so that all engineering effort concentrates on core-path quality

This document is the v0.4 baseline. Downstream requirements, architecture iterations, and task breakdowns originate here.

---

## 23. Glossary

Terms used consistently across this document and downstream specs. When implementing, CC should treat these as precise definitions, not loose synonyms.

- **Driver** — a module implementing the Driver Layer interface (§4.3.B). Responsible for moving bytes between a physical or virtual device and the `Frame` layer. Does not parse protocol semantics.
- **RawFrame** — a unit of unparsed bytes received from a Driver, plus reception metadata (source ID, timestamps, protocol type). See §4.3.C.
- **MessageType** — a decode rule describing one logical message, including its framing pattern, field definitions, and timing expectations. See §4.3.D.
- **MessageInstance** — one concrete decoded result produced by applying a `MessageType` to a `RawFrame`. Carries typed fields, a device timestamp if available, and any decode errors.
- **Signal** — a stable, named variable view. Multiple `MessageType`s can produce writes to the same `Signal`. See §4.3.E.
- **DerivedSignal** — a `Signal` computed from other `Signal`s via an ExprTk expression. See §4.3.E.
- **Action** — an outbound operation toward a device: a single command, a periodic poll, a post-connect sequence, or a macro. See §4.3.F.
- **AckMatcher** — the rule that pairs an `Action` with a `MessageInstance` to determine success, failure, or timeout. See §4.3.F.
- **Session** — a contiguous record of raw frames, decoded messages, bookmarks, and events, stored as one `.sfr` plus one `.sfi`. See §4.3.G.
- **Bookmark** — a named position in a `Session`, manually set by the user or automatically by exception detection.
- **BackpressureSignal** — a value-type event emitted when any queue crosses its high-water mark. See §5.3.
- **Snapshot** — a consistent, read-only view of the `SignalStore` at one instant, double-buffered so that readers never block writers. See §5.5.
- **Schema version** — a monotonically increasing integer attached to every persisted file or interface. Changes require a migration function. See §6.3.
- **Workbench** — the top-level user interface: `QMainWindow` plus Docks plus the five primary tabs (Connect, Decode, Observe, Control, Replay).
- **Workspace layout** — the saved arrangement of Docks and tabs within the Workbench for one project.
