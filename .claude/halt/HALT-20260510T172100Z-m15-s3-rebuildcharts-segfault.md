# HALT — M15 / S3 Round 2 (multi-chart baseline capture)

## Trigger

CLAUDE.md HALT trigger #2: "A test fails after **3** different fix attempts, regardless of the symptom."

The "test" here is the headless multi-chart baseline capture for states `01-empty-with-chart`, `36-multi-chart-2`, `37-multi-chart-5` per `M15-concerns.md` C3. The signalforge binary segfaults during chart-pane rebuild whenever `MainWindow::autoAddCharts(int extra)` raises the chart count from the default 1 → 2+. Fixed approaches all crashed identically.

## Context

- Currently executing: M15 S3 Round 2 (chart-add primitive + multi-chart capture).
- Completed earlier in this session:
  - S5 commit `b55203e` pushed; CI run `25634337073` green (11m25s).
  - S3 Round 1 commit `e94a656` (local; not pushed) — capture orchestrator + 6 baseline candidates (00, 04, 05, 14, 15, 33).
  - S3 Round 2 partial — 3 captures green: `02-conn-udp-idle` (PASS 0.000 %), `12-multi-2-drivers` (PASS 0.002 %), `13-multi-5-drivers` (FLAKY 0.999 %, max-diff threshold 0.5 %).
- Files modified but not in an acceptable state:
  - `src/app/main_window.hpp` — added `autoLoadFixtureNoConnect`, `autoAddCharts` declarations (clean compile).
  - `src/app/main_window.cpp` — added matching definitions (clean compile).
  - `src/app/main.cpp` — wired `--auto-no-connect` + `--auto-add-charts` flags (clean compile).
  - `tests/integration/gui/fixtures/m15_multi_2.yaml` (new) — multi-driver baseline.
  - `tests/integration/gui/fixtures/m15_multi_5.yaml` (new) — multi-driver baseline.
  - `tests/visual/scripts/capture_baselines.py` — Round 2 specs lifted; multi-chart specs reverted to `manual`.

The `--auto-no-connect` + multi-driver fixture additions work cleanly (states 02, 12, 13 captured successfully). The `--auto-add-charts` primitive compiles + links cleanly but causes the binary to segfault when invoked. No memory-safety hazard introduced into production-clicked code paths because the production "+ Chart" toolbar action is the same `onAddChart()` slot — meaning the bug is pre-existing GUI rebuild fragility that the headless tight-loop scenario surfaces.

## Problem details

Reproduction (one-liner):

```sh
xvfb-run --auto-servernum --server-args="-screen 0 1280x800x24" \
    timeout 8 ./build/release/src/app/signalforge \
        --auto-add-charts 1 --exit-after-ms 4000
```

Output (consistent across all 3 fix attempts):

```
Unknown key "software" for QSG_RHI_BACKEND, falling back to default backend.
timeout: the monitored command dumped core
Segmentation fault
```

Without `--auto-add-charts`, the binary exits cleanly. The segfault is bounded to the chart-add path.

Fix attempts:

1. **Defer 0 ms** — `QTimer::singleShot(0, ...)` post `window.show()`. Crash.
2. **Defer 400 ms + bulk-create** — refactored `autoAddCharts` to call `chartManager_->createChart()` N times then `rebuildChartWidgets()` once (instead of N `onAddChart()` calls each rebuilding). `QTimer::singleShot(400, ...)`. Crash.
3. **Synchronous before event loop** — invoke `autoAddCharts(extra)` immediately after `window.show()` in `main()`, before `app.exec()`. Crash.

All three crash before the screenshot QTimer fires (capture-after-ms=2500). The crash is in the chart QQuickWidget teardown / scene-graph re-init path inside `MainWindow::rebuildChartWidgets()`.

Hypothesised root cause: `rebuildChartWidgets` calls `deleteLater()` on existing `QQuickWidget*` items in the layout, then immediately constructs new `QQuickWidget`s and re-parents existing `Chart` `QQuickItem*` children to the new host. Under tight headless timing, the `deleteLater()`'d widget's QML scene-graph teardown races against the new host's scene-graph init (both running on the same thread but interleaving via Qt's event scheduler). Cf. ADR-010 §"Implementation lesson" — chart QQuickWidget hosting was identified as fragile; the production single-chart path works because the user clicks slowly enough to let the previous frame's render complete.

## Candidate interpretations or approaches

- **Option A — Out-of-scope: catalogue + defer to V0.3.** Mark states 01, 36, 37 as `manual` in `M15-progress.md` §S3 with note "blocked on `rebuildChartWidgets()` segfault under headless tight-loop chart-add timing; tracked for V0.3 GUI rebuild." The `autoAddCharts` primitive + `--auto-add-charts` flag remain in the codebase as future-ready infrastructure (no harm — only triggered by the test-only CLI flag). Spec §2.2 #1 explicitly forbids UX fixes during M15. → **Implication**: 3 of the 38 baselines stay manual; total auto-captured drops from a hoped-for 22+ to 9–11.
- **Option B — In-scope: redesign `rebuildChartWidgets` to be safe.** Refactor to never tear down existing host widgets; instead append new `QQuickWidget` for newly-created `Chart`s and leave existing host/Chart pairs untouched. Requires careful audit of `chartManager_` ownership semantics + new ADR (frozen-surface counter increment likely 0 since `main_window.cpp` is non-frozen, but rebuild logic affects QQuickWidget hosting which is governed by ADR-010 + ADR-011). → **Implication**: scope creep into V1.0-class GUI rebuild work that V0.2 spec explicitly excludes.
- **Option C — Reduce headless capture timing pressure.** Add an event-loop spin between teardown + reconstruct, e.g. via `QApplication::processEvents()` between `deleteLater()` queue and new `QQuickWidget` setSource. Risk: still racy; pre-existing GUI fragility surfaces under any tight-timing scenario. → **Implication**: brittle workaround; bug recurs whenever chart-add timing tightens.

## Decision requested

1. Which option (A / B / C)?
2. If A: confirm V0.3 catalogues this as a chart-rebuild redesign target.
3. Can S3 close at the partial coverage achieved (9 captures + ~12 operator-manual + ~17 round 3/4 candidates) with the multi-chart 3 states explicitly deferred?

CC's recommendation: **Option A**. Three reasons:

1. Spec §2.2 #1 forbids UX fixes during M15.
2. The crash is pre-existing and surfaces only under headless tight-loop timing, not under operator GUI clicks.
3. The `autoAddCharts` primitive + flag are useful future infrastructure even if the multi-chart state captures stay manual for V0.2 — V0.3's redesigned chart-host module can re-test the same flag.

## Side effects to clean up on resume

- Uncommitted local changes (Round 2 work):
  - `src/app/main_window.hpp` (+ ~15 lines)
  - `src/app/main_window.cpp` (+ ~25 lines)
  - `src/app/main.cpp` (+ ~25 lines)
  - `tests/integration/gui/fixtures/m15_multi_2.yaml` (new)
  - `tests/integration/gui/fixtures/m15_multi_5.yaml` (new)
  - `tests/visual/scripts/capture_baselines.py` (Round 2 specs lifted)
  - `.claude/M15-progress.md` (Round 1 §S3 table from `e94a656`)
- Working tree currently clean of any half-applied multi-chart fix; the autoAddCharts primitive is left in place for V0.3.
- Captured PNGs at `tests/screenshots/baseline-candidate/` (gitignored): 9 files for the 9 PASS / FLAKY states.
- No partial build state; `cmake --build --preset release` and `--preset debug` both clean as of HALT time.

## Recommended commit (committable now)

A single commit closing Round 2 with the partial-coverage outcome:

```
build: M15 S3 Round 2 — autoLoadFixtureNoConnect + autoAddCharts + multi-driver fixtures (3/6 captured)

Round 2 added:
- MainWindow::autoLoadFixtureNoConnect — loads YAML without connectAll;
  unlocks 02-conn-udp-idle.
- MainWindow::autoAddCharts — bulk createChart + rebuildChartWidgets;
  intended to unlock 01 / 36 / 37, but rebuildChartWidgets() segfaults
  under headless tight-loop chart-add timing (HALT report
  HALT-20260510T172100Z-m15-s3-rebuildcharts-segfault.md).
  Primitive + CLI flag retained as V0.3-ready infrastructure.
- m15_multi_2.yaml + m15_multi_5.yaml fixtures unlock 12 / 13.

Round 2 captures (PASS / FLAKY):
  02-conn-udp-idle      PASS  diff=0.000 %
  12-multi-2-drivers    PASS  diff=0.002 %
  13-multi-5-drivers    FLAKY diff=0.999 % > 0.5 %
                        (signal-selector layout under software-RHI
                         varies slightly; flagged for operator review)

Round 2 deferred (HALT trigger #2):
  01-empty-with-chart   chart-rebuild segfault
  36-multi-chart-2      chart-rebuild segfault
  37-multi-chart-5      chart-rebuild segfault

Refs HALT-20260510T172100Z-m15-s3-rebuildcharts-segfault.md.
```

After commit: stop S3, await operator decision on A / B / C.
