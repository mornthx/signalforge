# M8 — Plan

## 0. Execution ground rules

- Branch: `milestone/M8` (created in Phase 3 from `352f9972`;
  pushed to origin).
- Per-subtask discipline (CLAUDE.md §Required #2 + §Git operation
  protocol), identical to M5/M6/M7:
  1. Append start entry to `.claude/M8-progress.md`.
  2. Implement per plan.
  3. Build all three presets clean (Debug, Release, debug-asan).
  4. `ctest` Debug + Release clean (debug-asan host-blocked per
     `host_asan_preload`; routed to CI for authoritative gate).
  5. `clang-format --dry-run -Werror` on changed files.
  6. Append close entry to `M8-progress.md` with counts +
     deviations.
  7. Commit with `<module>: <imperative verb> <object>`; body
     states "Freeze scope: no M2-M7-frozen .hpp modified."
  8. Push `milestone/M8`.
  9. Watch CI via `gh run watch`; report result before starting
     the next subtask. No silent retries.
- No new top-level dependencies (spec §2.2-11). All Qt modules
  (Core, Gui, Quick, QuickWidgets, Widgets) already in
  `CMakeLists.txt`.
- Locked design decisions (spec §3.1-§3.7) are reflected in
  `.claude/M8-understanding.md §4` and implemented as written; not
  re-evaluated in this plan.
- Performance HALT gates (spec §7) and the 7 spec-defined HALT
  triggers are pre-encoded in §3 below with their measurement
  points.
- Strategy: **measure first, optimize only on miss** (per spec
  §9 note "Don't optimize before measuring"). The §5 gates have a
  20% margin from prototype actuals — first measurement that lands
  inside the margin is acceptable; no over-tuning.
- The M8 prototype on `prototype/m8-perf` is **reference, not
  copy** (spec §9). M8 production code reuses concepts (Chart
  QQuickItem subclass, persistent SG node map, LOD via
  `queryRange(target=pixelWidth)`) but rewrites cleanly with full
  freeze-surface + persistence + integration-test harness.
- Phase 1 closure follows the established M5/M6/M7 flow (push +
  CI green + PR creation + done.md).

## 1. Subtask sequence overview

| # | Subtask | Prereqs | Effort | Commit | Notes |
|---|---|---|---|---|---|
| S1 | `src/chart/` scaffolding + freeze-surface headers + CMake wiring + `TimeAxisManager` skeleton | — | 4 h | Yes | All four `.hpp` files match spec §4 verbatim. `chart.cpp` / `chart_manager.cpp` / `time_axis_manager.cpp` compile as ctor/dtor stubs. Top-level CMake adds `add_subdirectory(src/chart)`. |
| S2 | `TimeAxisManager` full implementation (visible_start / visible_end / live_mode / paused_at; pan / zoom / pause / resume / setRange / setPreset; `rangeChanged` / `liveModeChanged` Qt signals) + unit tests | S1 | 4 h | Yes | Pure logic class — no rendering. Test pan/zoom math in isolation; emit-counts via QSignalSpy. |
| S3 | `Chart` core (QQuickItem skeleton, SG node management, line / step / point renderers, persistent node map, `addSignal` / `removeSignal` / `setDisplayMode` / `setSignalVisible` / `config()` / `setConfig`) | S2 | 8 h | Yes | The bulk of the work. `Chart::Impl` PIMPL holds the SG-node map. `updatePaintNode` reuses nodes per signal; only vertex data updates. Auto-display-mode by `SignalMetadata::type` (spec §4.8). |
| S4 | `Chart::onTick` 30 Hz redraw timer + LOD-driven `queryRange(target=width())` + live time cursor (per spec §4.5, including subpixel-alternate fallback) | S3 | 5 h | Yes | `Qt::PreciseTimer` per spec §9 note. Cursor moves 1 px per frame in live mode; subpixel alternate when paused. |
| S5 | `ChartManager` (owns N Charts + global `TimeAxisManager`; `createChart` / `removeChart` / `chart(id)` / `chartIds()` / `activeChartId` / `setActiveChartId` / `timeAxis()`) | S3 | 3 h | Yes | Thin orchestration layer; most logic in Chart + TimeAxisManager. |
| S6 | `SignalSelector` widget (QWidget tree, group-by-driver-id + "Derived" group, checkbox toggle into active chart, filter/search line-edit, observes `SignalBufferRegistry` for registration changes) | S5 | 6 h | Yes | QTreeWidget host. Updates on `signalsRegistered`/`signalsUnregistered` Qt signals from M5/M6. |
| S7 | Pan/zoom mouse interaction + right-click context menu (pause/resume live, snap to recent) + chart configuration persistence yaml save/load (`~/.config/signalforge/charts.yaml`) | S4 + S5 | 5 h | Yes | yaml format documented in `schemas/charts_v1.yaml` (canonical example, written here). Schema freezes at M8 close per spec §6.1. |
| S8 | MainWindow integration (central ChartManager widget + side SignalSelector + top toolbar with live/paused toggle + time presets + status bar with frame-rate / dropped-frame display) + window activation handling (spec §4.6: `raise()` + `requestActivate()` + WARN log on sustained > 50 ms frame interval) | S6 + S7 | 5 h | Yes | Wires the existing M3-M7 plumbing (registry, decoders, expression engine) into the new chart UI. |
| S9 | Integration tests (7 files per spec §2.1-12) | S8 | 6 h | Yes | basic_rendering / 30hz_sustained / global_time_axis / lod_selection / signal_type_display / window_activation / signal_selector_tree_population. Mostly headless via `QTest::qWaitForWindowExposed` + offscreen QPA where possible. |
| S10 | Unit tests ≥ 80% coverage on chart modules + benchmark `bench_chart.cpp` reproducing the 4 prototype scenarios + `tests/benchmark/results/M8-baseline.md` (3 runs minimum) | S2 + S3 + S5 + S6 | 7 h | Yes | Bench is the §5 gate. HALT triggers #2 (30 Hz not sustained @ 60×1) and #5 (LOD level wrong) measure here. |
| S11 | 1-hour soak test (spec §5.6, §8.2) + `.claude/M8-done.md` + freeze record + PR against main | S10 green + CI green | 4 h | Yes | Soak runs locally + CI debug-asan; HALT trigger #6 (memory growth >10%) gates here. SHA256s for 4 hpp + 1 schema example. |

**Total estimated effort**: 57 h, comfortably within spec's
10-14 person-day (80-112 h) budget. Slack reserved for §S4 live-
cursor RHI-coalescing edge cases (spec §7 HALT trigger #4) and §S10
performance tuning if a gate trips.

## 2. Subtask details

### S1 — Scaffolding + freeze-surface headers + CMake wiring

**Deliverables**:

- `src/chart/CMakeLists.txt` adding `signalforge_chart` static lib.
  PUBLIC: Qt6::Core, Qt6::Quick, Qt6::Widgets, signalforge_buffer,
  signalforge_decoder. PRIVATE: signalforge_observability.
  AUTOMOC ON.
- `src/chart/chart.hpp` matching spec §4.1 verbatim. `Q_OBJECT`
  with `Q_DISABLE_COPY_MOVE`, ChartConfig / ChartSignalConfig /
  Chart::FrameStats / SignalDisplayMode declarations.
- `src/chart/chart_manager.hpp` matching spec §4.2.
- `src/chart/time_axis_manager.hpp` matching spec §4.3 (TimePreset
  enum included).
- `src/chart/signal_selector.hpp` — QWidget subclass; public API:
  `SignalSelector(SignalBufferRegistry&, ChartManager&, QWidget*)`;
  emits `signalToggled(QString, bool)`.
- `src/chart/{chart, chart_manager, time_axis_manager, signal_selector}.cpp`
  — ctor/dtor stubs that compile.
- Top-level `CMakeLists.txt` adds `add_subdirectory(src/chart)`.
- `tests/unit/chart/CMakeLists.txt` + a placeholder smoke test.

**Acceptance**:

- All three presets build clean.
- Doxygen on every public declaration.
- `clang-format --dry-run -Werror` clean.

### S2 — `TimeAxisManager` full implementation + unit tests

**Deliverables**:

- `time_axis_manager.cpp`: state machine for visible_start,
  visible_end, live_mode, paused_at.
  - `pan(offset)`: shift both endpoints; transition to paused mode
    if currently live.
  - `zoom(factor, referencePoint)`: scale duration around the
    reference; transition to paused if live.
  - `pause()`: capture `now()` as paused_at; live_mode = false.
  - `resume()`: live_mode = true; visible_end = now().
  - `setRange(start, end)`: explicit; transitions to paused.
  - `setPreset(preset)`: live mode, visible_end = now(),
    visible_duration = preset value (1s / 10s / 1min / 10min /
    1hr).
  - In live mode, `visibleEnd()` returns
    `std::chrono::steady_clock::now()` dynamically (not stored
    state); `visibleStart()` = end - duration.
- `rangeChanged()` Qt signal emits on every state change; debounce
  optional in V1.5+.
- `liveModeChanged(bool)` signal emits on live/paused transitions.

**Tests** at `tests/unit/chart/time_axis_manager_test.cpp`:

- Pan basic: pan(+offset) → visibleStart and visibleEnd both shift;
  rangeChanged emitted once; liveModeChanged emitted (true→false).
- Zoom around point: zoom(2.0, midpoint) → duration doubles around
  midpoint; rangeChanged emitted.
- Pause then resume: pause() → liveModeChanged(false); resume() →
  liveModeChanged(true); after resume, visibleEnd advances on
  subsequent reads.
- setPreset: 1s/10s/1min preset → visibleDuration matches; live
  mode preserved.
- setRange: explicit range → paused mode + rangeChanged.

**Acceptance**:

- ≥ 90% line coverage on time_axis_manager.cpp (it's pure logic;
  easy target).

### S3 — `Chart` core: SG node management + per-signal renderers

**Deliverables**:

- `Chart::Impl` PIMPL holding:
  - `std::unordered_map<QString, QSGGeometryNode*> signalNodes_`
  - Time cursor node + last cursor x position
  - Per-signal display-mode + color cache
- Constructor: takes `SignalBufferRegistry&`, `TimeAxisManager&`,
  optional `ChartConfig`.
- `addSignal(id, displayMode = std::nullopt)`: if displayMode
  unset, look up signal type in registry → auto-select per spec
  §4.8. Emit `signalAdded` Qt signal.
- `removeSignal(id)`: drop from config; on next `updatePaintNode`,
  the corresponding SG node is removed and deleted.
- `setDisplayMode(id, mode)`: update config; mark node dirty for
  next paint.
- `setSignalVisible(id, visible)`: toggle without removing.
- `config()` / `setConfig(...)`: roundtrip getters/setters.
- `updatePaintNode(oldNode, …)`: spec §4.4 pseudocode; for each
  visible signal, ensure a node exists, populate vertex data with
  query-range result, set DrawLineStrip / DrawPoints / step
  geometry.

Per-display-mode renderer (private functions):
- `Line`: `QSGGeometry::DrawLineStrip` over (x, y) point pairs.
- `Step`: rectangles between adjacent samples (Bool: 0/1 Y;
  Int64 step optional in V1.5+ via override).
- `Point`: `QSGGeometry::DrawPoints`; QString annotation deferred
  to V1.5+ per spec §3.4 (V1 just plots a point at the y=index
  binned position).

**Tests** at `tests/unit/chart/chart_signal_lifecycle_test.cpp`:
- addSignal / removeSignal round trip; config() reflects state.
- Auto display mode by type (Bool→Step, Double→Line, etc.).
- setDisplayMode override; setSignalVisible toggle.

### S4 — 30 Hz tick + LOD `queryRange` + live cursor

**Deliverables**:

- `Chart::onTick`: collect per-signal data via
  `bufferFor(id)->queryRange(start, end, target = width())`. Call
  `update()` to schedule a paint. Capture per-tick metrics
  (`chart_redraws_total`, `chart_frame_us_<id>`,
  `chart_dropped_frames_<id>`, `chart_lod_level_<id>`).
- Redraw timer: `QTimer` with `Qt::PreciseTimer`, 33 ms interval.
  Started in constructor; stopped in destructor.
- Live cursor (spec §4.5): vertical-line node updated every frame;
  x position = `(now - axisStart) / axisDuration * width()`;
  subpixel-alternate fallback for paused-axis edge case.
- Frame-interval tracker for the dropped-frame metric: difference
  between consecutive `frameSwapped` timestamps; > 50 ms ⇒
  dropped frame.

**Tests**: covered in S9 integration tests
(`test_chart_30hz_sustained.cpp`, `test_chart_window_activation.cpp`)
since they require an event loop.

### S5 — `ChartManager`

**Deliverables**:

- `ChartManager(SignalBufferRegistry&, QObject*)`: constructs the
  internal `TimeAxisManager` (owns it).
- `createChart(config)`: allocates a `Chart` with the registry +
  shared time axis + config; returns a generated unique chartId
  (e.g., `chart-<n>`).
- `removeChart(id)`: destroys the chart; `chartRemoved` signal.
- `chart(id)` / `chartIds()`.
- `activeChartId` / `setActiveChartId`: rotates as the user clicks
  charts; `activeChartChanged` signal fires when it changes.
- `timeAxis()`: returns `TimeAxisManager&`.
- `saveConfigToFile(path)` / `loadConfigFromFile(path)`: yaml
  roundtrip per spec §2.1-11.

**Tests** at `tests/unit/chart/chart_manager_test.cpp`:
- create/remove round trip; activeChartId tracking.
- Save/load yaml roundtrip; missing file handled gracefully;
  invalid yaml returns false + ERROR log.

### S6 — `SignalSelector` widget

**Deliverables**:

- `SignalSelector(SignalBufferRegistry&, ChartManager&, QWidget*)`:
  QTreeWidget host wrapped in a QWidget; constructs the tree
  initially from `registry.signalIds()`.
- Tree structure: top-level "Driver: <driverId>" items; each
  child is a leaf with checkbox + signal id + unit (if any).
  "Derived" group for signals with `driverId == "expression-engine"`
  (per M7's virtual driver id; verify in M7-done.md).
- Filter line-edit at top: hides non-matching items by signal id
  substring.
- Checkbox toggle: emits `signalToggled(signalId, checked)`;
  default behavior connects to
  `manager.chart(manager.activeChartId())->addSignal/removeSignal`.
- Observes registry signals: connect to
  `signalsRegistered(driverId, metas)` and
  `signalsUnregistered(driverId)` to rebuild affected branches.

**Tests** at `tests/unit/chart/signal_selector_tree_test.cpp`:
- Tree population from registry; group-by-driver-id structure.
- Checkbox toggle → activeChart->addSignal / removeSignal.
- Filter narrows visible items.
- registry add → tree updates within next event loop pass.

### S7 — Pan/zoom interaction + context menu + persistence yaml

**Deliverables**:

- Mouse handlers on `Chart` (QQuickItem mousePressEvent /
  mouseMoveEvent / wheelEvent):
  - Click+drag: `timeAxis_->pan(deltaPxToDuration(dx))`.
  - Wheel: `timeAxis_->zoom(factor, mouseTimePoint)`. factor =
    1.1^(steps); positive scroll zoom-in.
  - Right-click: context menu with "Pause live" / "Resume live" /
    "Snap to recent (1 sec)".
- yaml schema at `schemas/charts_v1.yaml` (canonical example;
  freezes at M8 close per spec §6.1):
  ```yaml
  schema_version: 1
  charts:
    - id: chart-1
      title: "Power monitoring"
      time_axis_id: ~  # null = global axis (V1)
      signals:
        - signal_id: voltage
          display_mode: line
          color: "#7AC0FF"
          visible: true
        - signal_id: power_total
          display_mode: line
          color: "#FF7A7A"
          visible: true
  ```
- Save/load wired through `ChartManager::saveConfigToFile` /
  `loadConfigFromFile`. Missing file → empty manager (graceful).
  Invalid yaml → ERROR log + start with defaults (per spec §8.4).

**Tests** at `tests/unit/chart/chart_config_persistence_test.cpp`:
- yaml roundtrip on a multi-chart config.
- Missing file: returns false, manager stays empty.
- Invalid yaml: returns false, ERROR logged.

### S8 — MainWindow integration + window activation

**Deliverables**:

- `MainWindow` central widget = a `QQuickWidget` that hosts
  `ChartManager`'s charts (one per row, or split layout per
  config). Side panel `SignalSelector` (QWidget; QDockWidget OK
  but keep it minimal).
- Top toolbar:
  - Live/paused toggle (QToolButton, checkable).
  - Time-range presets (combo box: 1 s / 10 s / 1 min / 10 min /
    1 hr).
  - "Add chart" button → `manager.createChart()`.
- Status bar:
  - Frame rate (mean over last 60 frames, computed from
    `Chart::FrameStats::lastRenderUs` × 1e6).
  - Dropped-frame counter (sum across charts).
  - Chart-window-throttled flag (lit when `chart_window_throttled`
    metric increments).
- Window activation handling (spec §4.6):
  - On `MainWindow::showEvent`, call `windowHandle()->raise()` +
    `windowHandle()->requestActivate()`.
  - QTimer::singleShot(500): if `!windowHandle()->isActive()`, log
    WARN.
  - On chart redraw, if frame interval > 50 ms sustained for 1 s,
    increment `chart_window_throttled` and log WARN.

**Tests**: covered by S9 integration tests.

### S9 — Integration tests (7 files)

Per spec §2.1-12, all in `tests/integration/`:

1. `test_chart_basic_rendering.cpp` — instantiate a Chart, attach
   5 signals, push data into the registry, run an event loop for
   1 second, verify the SG has 5 child geometry nodes + the cursor
   node.
2. `test_chart_30hz_sustained.cpp` — 60 signals × 30 Hz × 5 sec;
   FrameStats::droppedFrames == 0; total redraws ≥ 145.
3. `test_chart_global_time_axis.cpp` — 3 charts; pan one;
   timeAxis emits rangeChanged once per pan; the other 2 charts
   re-query at the new range on their next tick.
4. `test_chart_lod_selection.cpp` — push 600 k samples to a buffer;
   query at 4 zoom levels; verify chart_lod_level_<id> metric
   matches expected (0/1/2/3) per M6 §4.5.
5. `test_chart_signal_type_display.cpp` — one signal each of
   Bool / Double / Int64 / QString; verify each Chart's auto
   display mode resolves to Step / Line / Line / Point.
6. `test_chart_window_activation.cpp` — show window; verify
   `windowHandle()->isActive()` true within 500 ms; WARN logged
   when forced inactive.
7. `test_signal_selector_tree_population.cpp` — register signals
   from 2 mock drivers + 1 derived signal; tree has 3 top-level
   groups (the 2 driver IDs + "Derived") with the expected leaves.

### S10 — Unit tests ≥ 80% + `bench_chart.cpp` + `M8-baseline.md`

**Unit tests** breakdown (target ≥ 80% coverage on chart modules):
- TimeAxisManager: ≥ 90% (S2 deliverable; pure logic).
- Chart signal-lifecycle: ≥ 75% (S3 deliverable; UI paths reduce
  coverage).
- ChartManager: ≥ 85% (S5 deliverable; thin orchestration).
- SignalSelector: ≥ 75% (S6 deliverable; tree-building paths
  testable; checkbox-event paths require event-loop).

**Bench** at `tests/benchmark/bench_chart.cpp` reproduces the 4
prototype scenarios as production benchmarks:
- Scenario 1 — 60 charts × 1000 samples × 30 Hz × 1000 frames.
- Scenario 2 — 60 charts × 1 kHz updates × 30 Hz × 30 sec.
- Scenario 3 — 3 charts × 20 signals + scripted pan × 1000 frames.
- Scenario 4 — 1 chart × 600 k LOD × 4 zoom levels × 1000 frames.

Each scenario emits the same JSON line as the prototype so
results are comparable. Targets per spec §5 (prototype + 20%
margin); HALT trigger #2 fires if 30 Hz not sustained at 60×1
after one optimization pass; HALT trigger #5 fires if LOD level
selected is wrong.

**Results** in `tests/benchmark/results/M8-baseline.md` with the
3-run table + variance check (spec §5.5: < 5%).

### S11 — 1-hour soak + done.md + PR

**1-hour soak test**:
- Run S10 Scenario 2 (60 × 1 kHz × 30 Hz) for 1 hour either in a
  manually-launched session or as a CI job (debug-asan).
- Memory: tracked via `getrusage` or `/proc/self/status` snapshots
  every minute; growth < 10%.
- Dropped frames: < 50 over ~108 k frames.
- ASan / LSan clean (CI debug-asan).

**`.claude/M8-done.md`** per the M5/M6/M7 pattern:
- Deliverables checklist (vs spec §2.1)
- Acceptance self-check per spec §8
- Test count matrix
- Benchmark summary (spec §5)
- 1-hour soak result
- Freezes section with sha256 of:
  - `src/chart/chart.hpp`
  - `src/chart/chart_manager.hpp`
  - `src/chart/time_axis_manager.hpp`
  - `src/chart/signal_selector.hpp`
  - `schemas/charts_v1.yaml`
- Commit manifest
- CI verification status
- Hand-off to M9 / M10 / M11 / M12 (spec §8.6)
- HALT resolution trail (none expected)
- Deviations and concerns

**PR against `main`**, title "M8: Real-time Chart UI". Body
summarizes the §5 perf gates met + the 1-hour-soak result + the
freeze record.

**Stop and announce** per CLAUDE.md §Phase 1 step 6:
"M8 ready. Awaiting approval to merge M8 and begin M9 bootstrap."

## 3. Pre-encoded HALT statements (spec §7)

| # | Trigger | Measurement point | Action |
|---|---|---|---|
| 1 | Modification to M2-M7 frozen `.hpp` | Pre-commit `git diff` against the inherited freeze list | HALT report `.claude/halt/HALT-<ts>-frozen-modified.md`; revert change |
| 2 | 30 Hz not sustained @ 60 signals × 1 chart after one optimization pass | S10 bench Scenario 1 | HALT report; identify hotspot (SG node count, query-range cost, queryRange dispatch) |
| 3 | Window throttling not detectable | S9 `test_chart_window_activation.cpp` | HALT report; the `chart_window_throttled` metric must increment under simulated unfocused state |
| 4 | Live cursor causes RHI to drop frames | S4 manual + S9 sustained-frame test | HALT report; subpixel-alternate fallback (spec §4.5) is the prescribed fix; HALT only if fallback also fails |
| 5 | LOD level selected wrong | S9 `test_chart_lod_selection.cpp` | HALT report; trace through `queryRange(target=pixelWidth)` → M6 §4.5 selectLodLevel; M6 integration may be regressed |
| 6 | 1-hour soak memory > 10% growth | S11 measurement | HALT report; SG node leak (most likely) or M6 buffer retention misuse |
| 7 | Global time axis sync not working | S9 `test_chart_global_time_axis.cpp` | HALT report; TimeAxisManager Qt signal not emitted, or charts not connected to it |

CLAUDE.md §HALT triggers (compile error after 3 fixes, test fail
after 3 fixes, etc.) apply at every subtask.

## 4. Risk register

| Rank | Risk | Mitigation built into the plan |
|---|---|---|
| 1 | RHI frame coalescing edge cases | S4 implements the spec §4.5 subpixel-alternate fallback; S9 tests sustain-frame under live + paused axis |
| 2 | Memory growth (SG node leaks, LOD bin retention) | S11 soak; `Chart::Impl` owns SG nodes via `unique_ptr<>`-style cleanup on `removeSignal` |
| 3 | Window activation interaction with Mutter / KWin | S8 implements `requestActivate()`; S9 verifies; throttle metric documented as observable evidence |
| 4 | QTimer drift beyond ±1 ms | `Qt::PreciseTimer` (spec §9 note); S10 bench p99 frame interval verifies |
| 5 | LOD level integration regression vs M6 | S9 LOD test verifies all 4 levels; HALT trigger #5 |
| 6 | Pan-during-live race | TimeAxisManager is single-threaded (main); all changes go through Qt signals — no shared mutable state across threads |
| 7 | SignalSelector tree stale on register/unregister | S6 connects to registry's `signalsRegistered` / `signalsUnregistered` signals |

## 5. Dependencies (no new top-level)

Per spec §2.2-11, no new top-level dependencies. Existing Qt
modules already in `CMakeLists.txt::find_package`:

- Qt6::Core, Qt6::Gui, Qt6::Widgets — all M2-M7.
- Qt6::Quick, Qt6::QuickWidgets — M1 prototype + M2 RHI smoke
  test; readded for M8 production charts.
- Qt6::Test — already used in M3-M7 integration tests.

In-tree:
- `signalforge_buffer` (M6, frozen at M6 close + ADR-005 chunked
  storage on main)
- `signalforge_decoder` (M5, frozen)
- `signalforge_observability` (M2, frozen)
- `signalforge_expression` (M7, frozen — for the "Derived" group
  in SignalSelector via `expression-engine` virtual driver id;
  not a hard dep, only a metadata source)

## 6. What's deferred to V1.5+ / V2

(Per spec §2.2 + scattered §3 notes.)

V1.5+:
- Drag-and-drop signal selection.
- Per-chart "detach time axis" override (multi-axis mode).
- User-custom signal grouping.
- Annotation tools (markers, measurement cursors, regions).
- Keyboard shortcuts (vim-style or configurable).
- Print / PDF / image export.
- QML scene customization / theming.
- Touchpad gestures.
- QString-with-text-annotation render at sub-pixel alignment.

V2:
- 3D charts, surface plots, FFT, spectrogram views.
