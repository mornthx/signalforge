# M13 V1.0 Hardware Verification — HALT report (run 4)

**Date**: 2026-05-09 (CST)
**Operator**: shuai
**Halt at**: T3 (M9 UDP driver), 1st test attempted of 18 (run 4)
**Acceptance bar**: 16/18 PASS
**Projected pass count**: same as run 2/3 — best-case 9/18 (chart-render still
broken; same dependent-test list blocked)

## Prior HALTs and their fixes

| Run | Root cause | Fix commit | Status |
|---|---|---|---|
| 1 | Empty `driverTypeToSchemaPath` in `DecoderRegistrar` | `2ef60c0` (S7, ADR-008) | ✅ Verified working in runs 2/3/4 |
| 2 | `QQuickWidget` never given a QML source → `rootObject()` null → orphan Chart | `f285503` + `9005ec2` (S8, S8.1, ADR-010) | ✅ Source-level fix verified; chart QML now parses |
| 3 | `qrc_qml.cpp.o` stripped from binary by static-archive linker | `aa100c9` (S8.2 `Q_INIT_RESOURCE(qml)`) | ✅ Resource symbol now present (`nm \| grep qInitResources_qml` → 1) |

## Trigger (run 4)

Run 4 confirmed via runtime log:

```
{"ts":"2026-05-09T21:21:28.067Z","event":"MainWindow: created chart chart-1"}
{"ts":"2026-05-09T21:22:30.814Z","event":"DecoderRegistrar: schema for driver type 'udp' set to 'examples/schemas/modbus_style.yaml'"}
{"ts":"2026-05-09T21:22:34.526Z","event":"DecoderRegistrar[udp:conn-2a5f772e]: decoder attached using schema 'examples/schemas/modbus_style.yaml'"}
```

**Crucially absent** (compared to run 3):
- No `MainWindow: ChartHost.qml failed to load (status=...)` error
- No `MainWindow: ChartHost.qml loaded but rootObject() is null` error

So the QML host scene is loading, status=Ready, rootObject() returns a real
Item. Yet the chart pane is still pure white. Operator confirms multiple
charts created (chart-1 through chart-5 in the log) — none renders.

## Evidence chain

| Layer | Observation | Verdict |
|---|---|---|
| QML loading | No load-error log lines (run 3 had them; runs 1/2 didn't reach this stage) | `setSource` succeeded |
| QML root | No "rootObject is null" log line | `rootObject()` returns valid `Item` (the `ChartHost.qml` `Item { anchors.fill: parent; objectName: "chartHost" }`) |
| Parent linkage | `chart->setParentItem(root)` is called unconditionally if both upper checks pass (`main_window.cpp:336`) | Chart is in the QML scene |
| **Chart geometry** | `grep -nE 'setSize\|setWidth\|setHeight\|geometryChange\|itemChange' src/chart/chart.cpp` → **empty**; same grep on `src/app/main_window.cpp` → **empty** | **Chart QQuickItem is never given a non-zero size** |
| Chart constructor (`chart.cpp:135-156`) | Sets `ItemHasContents`, `acceptedMouseButtons`, starts a 30 Hz redraw timer; no width/height set; no anchor binding | Chart's effective `width = 0`, `height = 0` after `setParentItem` |
| Decoder pipeline | "decoder attached" present + frames feeding (UDP feeder confirmed sending 5 Hz) | Data is flowing into SignalBufferRegistry; the chart simply has no canvas to draw on |

## Root cause

`QQuickItem::setParentItem(parent)` only changes the scene-graph parent. It
**does not** size the child to the parent. Without an explicit
`chart->setWidth(parent->width())` / `setHeight(parent->height())` (or QML
anchors, which are not used here because Chart is instantiated from C++),
the Chart item stays at its default 0×0 geometry. With `ItemHasContents`
true, Qt asks for a paint node, but the paint area is 0 pixels — nothing
is rendered. The QQuickWidget shows its default clear color (white).

This is the fourth manifestation of the same symptom (white chart). Each
prior fix correctly resolved the layer it targeted but failed to address
the next downstream barrier:

```
decoder data → [run 1: pipeline empty]
             → [run 2: no QML host]
             → [run 3: qrc stripped at link]
             → [run 4: Chart sized 0×0]   ← here
             → ... (more layers possible) ...
             → user sees a real waveform
```

## Tests blocked

Same set as runs 2/3:

- Direct fail: T1 / T2 / T3 / T4 / T13 / T14 / T15 / T16 / T17 (9 tests)
- Likely still pass without chart: T5 / T6 / T7 / T8 / T9 / T10
- Optional ambiguous: T11 / T12 / T18

Best-case 9/18 < 16. **HALT (H4 trigger).**

## Recommended fix direction

Smallest patch (preferred):

In `src/app/main_window.cpp::rebuildChartWidgets()`, after the existing
`chart->setParentItem(root);` line, add four lines:

```cpp
chart->setWidth(root->width());
chart->setHeight(root->height());
QObject::connect(root, &QQuickItem::widthChanged, chart, [chart, root]() { chart->setWidth(root->width()); });
QObject::connect(root, &QQuickItem::heightChanged, chart, [chart, root]() { chart->setHeight(root->height()); });
```

Trivially testable: with the QML host's `anchors.fill: parent` the root's
size tracks the QQuickWidget. Once Chart's width/height bind to the root,
the redraw timer's `update()` calls will produce non-zero geometry on the
scene graph.

Cleaner alternatives:

1. Override `Chart::itemChange(ItemParentHasChanged, ...)` and connect to
   the new parent's `widthChanged` / `heightChanged` inside Chart itself.
   Self-contained; no caller knowledge required.

2. Register Chart with `qmlRegisterType<Chart>("SignalForge.Chart", 1, 0, "Chart")`
   and instantiate from QML inside `ChartHost.qml`:
   ```qml
   import SignalForge.Chart 1.0
   Item {
       anchors.fill: parent
       Chart { anchors.fill: parent }
   }
   ```
   This is the QML-native way and was probably the original architectural
   intent. Largest change because Chart needs a default-constructible /
   QML-friendly factory + ChartManager handoff.

## CI-side observation worth flagging (compounding from run 3)

Three of the four HALTs (run 2, run 3, run 4) are different symptoms of
"the C++ → QML scene-graph hand-off is incompletely wired." A targeted CI
test that:

1. Builds the release `signalforge` executable.
2. Launches it with `Q_QPA_PLATFORM=offscreen`.
3. Drives a known UDP frame.
4. Reads back a chart-pane pixel near the centre of the rendered chart.
5. Asserts the pixel is **not** the QQuickWidget clear color.

…would have caught all three of these in CI. Without something equivalent,
runs 5/6/… are at risk of catching yet another layer in the same hand-off
chain. Recommend prioritising this CI add before accepting the run-4 fix.

## Captured artefacts

- `~/.local/state/signalforge/logs/signalforge.log` — search after marker
  `>>> M13 retest run4 start 2026-05-09T21:21:15+08:00`
- `/tmp/m13-verify-logs/M13-HALT-decoder-registrar-empty-map.md` (run 1)
- `/tmp/m13-verify-logs/M13-HALT-chart-orphan-quickitem.md` (run 2)
- `/tmp/m13-verify-logs/M13-HALT-qrc-static-lib-stripped.md` (run 3)
- This report (run 4)

## Operator action taken

- M13 dogfood run 4 halted at first chart-pass criterion (T3 step 6).
- Tests T1/T2/T4/T13–T17 not attempted — same defect would block them.
- Lifecycle-only tests (T5/T6/T7/T8/T9/T10) deferred to keep the run
  consistent (one fix, one full re-run).
- Awaiting Chart-sizing fix (and ideally the CI smoke test above) per M13
  plan §3.

---

Reviewer: this is the fourth halt, same symptom, fourth root cause in the
same hand-off chain. Strongly recommend not authorising run 5 without:

1. The size-binding fix above (or QML re-registration), AND
2. A release-binary smoke test that fails CI when the chart pixel is the
   clear color, so we stop discovering this layer-by-layer in operator
   sessions.
