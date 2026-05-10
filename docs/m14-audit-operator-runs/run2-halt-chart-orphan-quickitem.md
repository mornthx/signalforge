# M13 V1.0 Hardware Verification — HALT report (run 2)

**Date**: 2026-05-09 (CST)
**Operator**: shuai
**Halt at**: T3 (M9 UDP driver), 1st test attempted of 18 (run 2)
**Acceptance bar**: 16/18 PASS
**Projected pass count if session continued**: ≤ 9/18 → HALT (same H4 trigger
per M13 plan §3)
**Prior HALT**: `M13-HALT-decoder-registrar-empty-map.md` — that defect is
**fixed** (commit `2ef60c0`, ADR-008); this is a new defect found immediately
downstream.

## Run 2 confirmed-fixed items

- ADR-008 wiring works end-to-end. Log captured at 18:51:32 / 18:51:35:
  `DecoderRegistrar: schema for driver type 'udp' set to 'examples/schemas/modbus_style.yaml'`
  followed by
  `DecoderRegistrar[udp:conn-2b51313c]: decoder attached using schema 'examples/schemas/modbus_style.yaml'`.
- SignalSelector populates: per the operator, `slave_address`, `function_code`,
  `byte_count`, `temperature_x10`, `status_flags.*` all visible after Connect.
- Checkbox toggle in SignalSelector responds (☐ → ☑ confirmed by operator).

## Trigger (run 2)

T3 Pass criteria: "UDP receive + decode + chart all work." Receive + decode are
PASS. **Chart never renders any content** — pure white surface, no axes, no
legend, no traces. Same symptom on every chart created.

## Symptom

After the fix in run 2:
1. Connect UDP with schema `modbus_style` — decoder logs confirm attach.
2. Operator sees signals in the SignalSelector left pane.
3. Operator ticks the checkbox next to `temperature_x10` — checkbox state
   updates correctly.
4. **The chart pane on the right remains pure white.** No legend label
   appears. No line is drawn. Same on a second chart added via the toolbar
   ("Add Chart").

## Evidence chain

| Layer | Observation | Verdict |
|---|---|---|
| Decoder | `DecoderRegistrar[udp:conn-...]: decoder attached` log line present | Decoder pipeline alive |
| SignalSelector | Operator confirms 5+ signals listed; checkbox toggles | Selector → ChartManager wiring alive (`signal_selector.cpp:100` → `active->addSignal(signalId)`) |
| Chart QQuickItem | Class declared at `chart.hpp:70`: `class Chart : public QQuickItem` | Chart is a Qt Quick scene-graph item, must be hosted in a QQuickWidget / QQuickView with a non-null parent |
| Hosting (`main_window.cpp:319-322`) | <pre>auto* hostWidget = new QQuickWidget(chartContainer_);<br>hostWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);<br>chart->setParentItem(hostWidget->rootObject());<br>chartLayout_->addWidget(hostWidget, 1);</pre> | **`hostWidget->rootObject()` is `nullptr`** at this point — no `setSource(...)` is ever called. Chart's parent becomes nullptr → orphan QQuickItem → never participates in any scene → never painted |
| QML inventory | `find resources/ src/ -name '*.qml' -o -name '*.qrc'` returns only the spike test (`tools/spike/qquick_dock_test/qml/DockContent.qml`) | The application does not ship a chart-host QML file |
| qmlRegisterType | `grep -rn 'qmlRegisterType.*Chart\|QML_ELEMENT' src/` returns nothing | Chart is not registered to QML — even if a QML host existed, it could not instantiate Chart from QML |

## Root cause

`MainWindow::rebuildChartWidgets()` was implemented as if `QQuickWidget` would
synthesize a default scene root for an externally-instantiated QQuickItem. It
does not. Per Qt docs, `QQuickWidget::rootObject()` returns `nullptr` until
`setSource()` loads a QML file with a top-level Item. Reparenting a
QQuickItem to `nullptr` orphans it — no painting, no input, no scene
participation. The QQuickWidget then renders only its clear color (white).

## Tests blocked by this defect

Direct fail (chart-render is in the spec's pass criteria):
- T1 Serial — "verify samples flow"
- T2 TCP — "verify signals appear in chart"
- T3 UDP — "verify decode → chart"
- T4 Replay — "verify signals appear at recorded timing"
- T13 GUI open + replay — "Charts populate as records dispatch"
- T14 Play/Pause — implicitly requires visible dispatch
- T15 Step — requires per-step visual confirmation
- T16 Timeline scrubber — "Charts re-render after each seek"
- T17 Speed combo — "10× clearly faster than 1×; 0.5× clearly slower" — purely visual

Likely still pass (lifecycle / file-level, no chart render needed):
- T5 Edit / Remove connection
- T6 Auto-connect on start
- T7 GUI round-trip recording (status-bar bytes update + sfreplay_inspect)
- T8 Replay portable across restart (file readability)
- T9 Quit-while-recording prompt
- T10 Mid-stream catalog extension (verified via sfreplay_inspect)

Optional, ambiguous:
- T11, T12, T18

Best-case: 6/18 mandatory + up to 3/18 optional = 9/18. **< 16/18 → HALT.**

## Recommended fix direction

Either:

1. **Embed a tiny QML host scene per QQuickWidget** — ship a one-line QML file
   such as `qrc:/qml/ChartHost.qml` containing `Item { anchors.fill: parent }`,
   call `hostWidget->setSource(...)` and wait for the host's `Status::Ready`,
   then `chart->setParentItem(hostWidget->rootObject())`. Smallest patch.

2. **Re-host Chart in a QML hierarchy proper** — register Chart with
   `qmlRegisterType<Chart>("SignalForge.Chart", 1, 0, "Chart");`, build a
   small `ChartHost.qml` that instantiates the Chart from QML, and let
   `ChartManager` own the data binding via QML properties. Closer to the
   QQuickItem design intent and surfaces nicer to QML theming for the
   "white-background vs system-theme" UX gap noted in run 2.

3. **Switch off Quick** — promote Chart to a `QPainter`-driven `QWidget`
   (or `QQuickPaintedItem`). Largest change but removes the QQuickWidget
   dependency that CLAUDE.md flags as risky on Qt 6.10.

In all cases an integration test should drive a UDP packet end-to-end and
assert a non-empty pixel diff on the chart canvas (or a synthetic data-flow
hook into the chart's draw pipeline).

## Secondary UX gaps observed during run 2 (not blocking, not HALT-class)

- Chart panel renders pure white regardless of system dark theme — Qt 6.10
  QQuickWidget does not propagate the host palette into the Quick scene.
  Trackable separately as theming work.
- Operator cannot delete a chart from the GUI; "Add Chart" exists, no remove.
- Default chart-line palette (`chart.cpp:54-62`) is light pastels — even when
  the rendering bug above is fixed, traces will be low-contrast on a white
  background. Pair with the theming fix.
- Connection schema field accepted (and persisted to YAML) when set to
  literal `~/Music/...` (Qt does not expand the tilde). Connect proceeds in
  Connected state with no error toast. Recommend either tilde expansion in
  the dialog or a path-existence check before allowing Connect.
- `SF_LOG_LEVEL=debug` env var has no effect on the release build's log level
  (zero debug-level lines emitted). M13 protocol §Pre-flight tells operators
  to set this for failure investigation; the variable is documented but
  non-functional in V1.0.

## Captured artefacts

- `~/.local/state/signalforge/logs/signalforge.log` (search after marker
  `>>> M13 retest run2 start 2026-05-09T18:50:29+08:00`)
- `/tmp/m13-verify-logs/M13-HALT-decoder-registrar-empty-map.md` — prior
  HALT (now resolved by ADR-008)

## Operator action taken

- M13 dogfood run 2 halted at first chart-pass criterion (T3 step 6).
- Tests T1/T2/T4/T13–T17 not attempted (all blocked by same defect).
- Lifecycle-only tests (T5, T6, T7, T8, T9, T10) deferred until chart fix
  lands, to keep the run consistent (one fix, one full re-run).
- Awaiting fix and reverification per M13 plan §3.

---

Reviewer: please route to whoever owns `src/app/main_window.cpp` `rebuildChartWidgets()`. Re-run M13 end-to-end after the chart-host fix lands.
