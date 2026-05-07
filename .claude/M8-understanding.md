# M8 — Understanding

## 1. Restatement of the M8 goal

M8 delivers the **real-time chart UI**: a Qt Quick Scene Graph
chart subsystem that visualizes base + derived signals from the
M6 `SignalBufferRegistry` at sustained 30 Hz, with global time
axis synchronization across multiple charts and signal-type-
appropriate display modes.

This is **the first user-facing UI milestone in V1**. Until M8
the app has been a headless decoder + buffer + expression-engine
pipeline. After M8: charts on screen, signals scrolling, pan/zoom
working — "SignalForge becomes a tool" (spec §10).

Hard-stop types (concurrent, per spec):

1. **Interface freeze**: `Chart`, `ChartManager`, `TimeAxisManager`,
   `SignalSelector` public APIs + `ChartConfig` / `ChartSignalConfig`
   / `Chart::FrameStats` struct layouts + `SignalDisplayMode`,
   `TimePreset` enums.
2. **Configuration schema freeze**: `~/.config/signalforge/charts.yaml`
   top-level + per-chart + per-signal keys.
3. **Performance certification**: validated against the M8
   prototype's RESULTS.md numbers + 20% margin (decision M8.5).

**Soft-HALT is not allowed** (inherits M2-M7 stance).

The performance gates in spec §5 are **measurement-derived, not
estimation-derived**. The M8 prototype on `prototype/m8-perf`
established that 60 signals × 30 Hz × 3 charts is sustainable on
this hardware (radeonsi / Ryzen 5800H); production M8 reuses the
ideas (Chart class, Scene Graph node management, LOD integration)
but rewrites cleanly per spec §9 ("prototype is reference, not
copy").

## 2. Observed repo state

```
$ git log --oneline -5 origin/main
352f9972 Merge pull request #10 from mornthx/milestone/M7
d58c61e6 Merge pull request #12 from mornthx/docs/m8-spec
6c7641e7 docs: add M8 real-time chart UI spec
7e1837a7 Merge pull request #11 from mornthx/fix/m6-publish-cadence
...
```

Phase 3 actions completed this session (report confirmed):

- PR #10 (M7 expression engine) merged at `352f99722a289b8e...`.
- Tag `v0.0.8-alpha.1` pushed, pointing at the M7 merge.
- `milestone/M8` branch created from `352f9972` and pushed to
  `origin/milestone/M8`.

`docs/milestones/M8-real-time-chart-ui.md` (836 lines) is already
on origin/main via PR #12 (merged before M7) and visible on
`milestone/M8`.

`src/chart/` does not yet exist; the M8 implementation creates it.
The M8 prototype lives on the `prototype/m8-perf` branch (still
present, not merged) — its `tools/m8_prototype/RESULTS.md` is the
reference for §5 performance gates.

## 3. Scope reminder

### Must deliver (spec §2.1)

- `Chart` (`src/chart/chart.{hpp,cpp}`): single-instance QQuickItem
  rendering N signals via Scene Graph; self-driven 30 Hz QTimer;
  per-signal display mode; live time cursor; pan/zoom interaction.
- `ChartManager` (`src/chart/chart_manager.{hpp,cpp}`): owns N
  Charts + the global TimeAxisManager; activeChartId tracking for
  the SignalSelector "active chart" model.
- `TimeAxisManager` (`src/chart/time_axis_manager.{hpp,cpp}`):
  single global time-range state; visible_start / visible_end /
  live_mode / paused_at; pan/zoom delegation; QObject signals on
  range change.
- `SignalSelector` (`src/chart/signal_selector.{hpp,cpp}`):
  QWidget-based signal tree with checkbox toggle into the active
  chart; group by driver ID + "Derived" group for the
  expression-engine virtual driver.
- `ChartRenderer` (internal to chart.cpp; not in public API):
  Scene Graph node creation per signal; vertex buffer updates;
  node reuse; LOD selection driven by `samples_per_pixel`.
- `SignalDisplayMode` enum + auto-by-type selection (Bool→Step,
  Int64/Double→Line, QString→Point).
- MainWindow integration (central ChartManager widget + side
  SignalSelector + top toolbar with live/paused toggle + time
  presets + status bar with frame-rate / dropped-frame display).
- Window activation handling (`raise()` + `requestActivate()` +
  WARN log if frame interval > 50 ms sustained).
- Live time cursor (per-frame visible change to defeat Qt 6 RHI
  frame coalescing per [Proto] Anomaly §1; subpixel-alternate
  fallback for paused-axis static-data edge case).
- Pan/zoom interaction (click+drag pan, mouse wheel zoom,
  right-click context menu).
- Chart configuration persistence (`~/.config/signalforge/charts.yaml`).
- 7 integration tests (per spec §2.1-12).
- Unit tests ≥ 80% coverage on chart modules.
- Benchmark `tests/benchmark/bench_chart.cpp` reproducing the 4
  prototype scenarios.
- Doxygen on all public declarations.
- `.claude/M8-done.md` with completion report + freeze record.

### Must NOT do (spec §2.2)

- No modification to M2-M7 frozen `.hpp`.
- No drag-and-drop signal selection (V1 uses checkbox tree;
  decision M8.2).
- No multi-time-axis mode (V1 uses single global axis;
  decision M8.3).
- No user-custom signal grouping (V1 groups by driver ID +
  "Derived").
- No 3D charts, surface plots, FFT view, spectrogram (V2).
- No annotation tools, markers, measurement cursors (V1.5+).
- No keyboard-driven navigation (V1 mouse-only).
- No print / export to image / PDF (V1.5+).
- No QML scene customization API (V1 fixed visual style).
- No dependency on M9 Connection Manager UI (charts read from
  registry; how signals get there is M3-M5 territory).
- No new top-level dependencies.
- No QML scenes (charts are pure C++ QQuickItem subclasses +
  minimal QML wrapper for window setup).

## 4. Locked design decisions (spec §3)

1. **M8.1 (Self-driven 30 Hz timer)** — each Chart has its own
   QTimer at 33.33 ms (`Qt::PreciseTimer`), independent of M7's
   ExpressionEngine. Phase relationship: chart frame N may show
   M7 tick N-1 values (≤ 33 ms latency), acceptable per M7 §3.2.
2. **M8.2 (Signal tree + checkbox UI)** — checkbox toggles signal
   into the active (last-clicked) chart. Drag-and-drop deferred
   to V1.5+.
3. **M8.3 (Global time axis)** — TimeAxisManager owns a single
   range state shared by all charts. Per-chart "detach axis" is
   V1.5+.
4. **M8.4 (Auto signal display by type)** — Bool → Step,
   Int64/Double → Line, QString → Point. User override via
   per-signal config.
5. **M8.5 (Performance certification via prototype + 20% margin)** —
   spec §5 gates are derived from prototype RESULTS.md numbers.
   No HALT for "spec gate trip" if measurement is within margin;
   ADR amendment only if architectural (>50% miss).
6. **M8.6 (No soft-HALT)** — inherits M2-M7.
7. **M8.7 (Metric naming)** — per `<module>_<metric>_<scope>`
   convention. Documented in spec §3.7.

## 5. Performance gates (spec §5; prototype + 20% margin)

| Gate | Prototype actual | M8 target | HALT |
|---|---:|---:|---:|
| p99 frame interval (vsync-bound) | 33.5 ms | < 35 ms | > 50 ms |
| p99 render-loop cost | 0.60 ms | < 1.0 ms | > 5 ms |
| Dropped frames over 1000-frame run | 0/1000 | 0/1000 | > 5/1000 |
| Visible signals/chart sustained 30 Hz | 60 | ≥ 60 | < 40 |
| Concurrent charts sustained 30 Hz | 3 | ≥ 3 | < 2 |
| Total signals across charts | 60 | ≥ 60 | < 40 |
| Soft cap signals/chart | 100 (legibility) | 100 | — |
| Single-chart query at any zoom level | < 130 µs | < 200 µs | > 500 µs |
| LOD switch latency | < 130 µs | < 200 µs | > 1 ms |
| Frame interval std dev during pan | 1.34 ms | < 2 ms | > 5 ms |
| Frame interval p99 during pan | 33.07 ms | < 35 ms | > 50 ms |
| Run-to-run variance (any metric) | — | < 5% | — |
| 1-hour soak: memory growth | — | < 10% | > 10% |
| 1-hour soak: dropped frames | — | < 50 / ~108k | > 50 |

## 6. Freeze surface (spec §6.1)

- `src/chart/chart.hpp`: `Chart` class, `ChartConfig`,
  `ChartSignalConfig`, `Chart::FrameStats`, `SignalDisplayMode`.
- `src/chart/chart_manager.hpp`: `ChartManager` class.
- `src/chart/time_axis_manager.hpp`: `TimeAxisManager` class,
  `TimePreset` enum.
- `src/chart/signal_selector.hpp`: `SignalSelector` widget public API.
- `~/.config/signalforge/charts.yaml` schema (top-level +
  per-chart + per-signal keys).

Modifications require a new ADR.

## 7. M8-specific HALT triggers (spec §7)

1. Modification to M2-M7 frozen `.hpp` → HALT.
2. 30 Hz redraw not sustained at 60 signals × 1 chart (matches
   prototype Scenario 1) → HALT after one optimization pass.
3. Window throttling not detectable (no metric increment when
   chart is unfocused under Mutter) → HALT (mitigation incomplete).
4. Live cursor causes RHI to drop frames (cursor update doesn't
   reliably keep frames distinct) → HALT.
5. LOD level wrong (chart queries LOD 0 when LOD 3 expected, or
   vice versa) → HALT (M6 integration broken).
6. Memory leak in 1-hour soak (>10% growth) → HALT.
7. Global time axis sync not working (pan one chart, others don't
   follow) → HALT.

CLAUDE.md §HALT triggers (compile error after 3 fixes, test fail
after 3 fixes, etc.) apply at every subtask.

## 8. Risk register

| Rank | Risk | Mitigation |
|---|---|---|
| 1 | Live cursor edge cases trigger RHI frame coalescing | Spec §4.5 includes a subpixel-alternate fallback for the paused-axis static-data case. Bench Scenario 4 (LOD switch through paused axis) catches it. |
| 2 | Memory growth in long-running charts (LOD bin retention, Scene Graph node leaks) | 1-hour soak test (HALT trigger #6); Chart::Impl owns SG nodes via persistent map; nodes destroyed on `removeSignal`. |
| 3 | Window activation interaction with Mutter / KWin / other compositors | `requestActivate()` is mandatory (spec §4.6); throttle detection metric `chart_window_throttled` increments; documented limitation in user-facing notes. |
| 4 | Per-chart QTimer at 33 ms drifts beyond ±1 ms | Use `Qt::PreciseTimer` per spec §9 note; verified by bench p99 frame interval. |
| 5 | LOD level selection wrong (M6 integration regression) | Bench Scenario 4 (zoom-level cycle) verifies p50/p99 per level; HALT trigger #5. |
| 6 | Pan-during-live-mode race (TimeAxisManager state during pan + tick) | TimeAxisManager is single-threaded (main); all changes go through Qt signals. |
| 7 | Signal selector tree population stale when decoders register/unregister mid-session | SignalSelector observes `SignalBufferRegistry`'s `signalsRegistered`/`signalsUnregistered` Qt signals (M5/M6 plumbing); rebuild tree on change. |

## 9. Dependencies

- M6 `SignalBufferRegistry` (frozen at M6 close + ADR-005 chunked
  storage on main); chart redraw queries `bufferFor(id)->queryRange(start, end, target)`.
- M7 `ExpressionEngine` derived signals (frozen at M7 close);
  charts treat derived signals identically to base signals — no
  chart-side awareness of which is which.
- M5 `SignalMetadata::driverId` for SignalSelector grouping;
  the M7 expression engine uses `driverId = "expression-engine"`
  which the SignalSelector renders as the "Derived" group.
- Existing Qt 6.10.2 modules: Quick, QuickWidgets, Widgets, Core.
  No new top-level dependencies (spec §2.2-11).
- exprtk (already pinned via FetchContent for M7); not used by M8.

## 10. Test strategy

- **Unit tests** at `tests/unit/chart/` (≥ 80% coverage on chart
  modules — lower than M5-M7's 85% per spec §2.1-13 because UI
  tests are inherently harder to cover in headless CI).
  Rough breakdown:
  - `chart_smoke_test.cpp`: instantiate, attach signals, verify
    accessors.
  - `time_axis_manager_test.cpp`: pan / zoom / pause / resume /
    setRange / setPreset; emits rangeChanged / liveModeChanged.
  - `chart_signal_lifecycle_test.cpp`: addSignal / removeSignal /
    setDisplayMode / setSignalVisible round-trip.
  - `chart_config_persistence_test.cpp`: yaml save/load roundtrip;
    invalid yaml; missing file.
  - `signal_selector_tree_test.cpp`: tree population from
    registry; checkbox toggle → active-chart signal change;
    group-by-driver-id structure.
- **Integration tests** at `tests/integration/` (7 files per
  spec §2.1-12):
  - `test_chart_basic_rendering.cpp`
  - `test_chart_30hz_sustained.cpp`
  - `test_chart_global_time_axis.cpp`
  - `test_chart_lod_selection.cpp`
  - `test_chart_signal_type_display.cpp`
  - `test_chart_window_activation.cpp`
  - `test_signal_selector_tree_population.cpp`
- **Benchmark** at `tests/benchmark/bench_chart.cpp` reproducing
  the 4 prototype scenarios; results in
  `tests/benchmark/results/M8-baseline.md`. 3-run minimum.
- **1-hour soak** in CI debug-asan (spec §5.6, §8.2) — gated as
  the HALT trigger #6 measurement point.

## 11. Plan link

`.claude/M8-plan.md` lays out the 11-subtask plan with effort
estimates totaling 70-90 h (within spec's 10-14 person-day
estimate of 80-112 h). Subtasks ordered to land an integrable
slice early (S1: scaffold + TimeAxisManager + bench harness) so
later subtasks can measure against a moving baseline.
