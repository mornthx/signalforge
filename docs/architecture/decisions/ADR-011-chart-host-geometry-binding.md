# ADR-011 — Chart host-scene geometry binding

## Status

Accepted (V1.0 release blocker fix at M14 S2, 2026-05-09;
follows ADR-008 / ADR-009 / ADR-010 in the V1 GUI integration
audit).

## Context

ADR-010 introduced the `ChartHost.qml` host scene loaded by
`MainWindow::rebuildChartWidgets` so that
`QQuickWidget::rootObject()` returns a non-null `QQuickItem`
to which the C++ `Chart` (also a `QQuickItem`) is reparented.
The QQuickWidget is configured with
`SizeRootObjectToView`; the QML scene root has
`anchors.fill: parent`. So the **host scene** sizes itself to
match the widget — but the C++ `Chart` child does **not**
inherit that sizing.

Run-4 of the operator dogfood found the chart panel rendered
as a uniform `QQuickWidget` clear color. M14 S1's CI smoke
test reproduced and quantified the symptom:

```
M14_SMOKE_TIER_A: non_white_pixels=0 total_pixels=0
                  width=0 height=0
```

The QQuickWidget framebuffer is 0×0 because:

1. `QQuickWidget::SizeRootObjectToView` propagates the
   widget's pixel size to the root QML item.
2. `ChartHost.qml`'s `anchors.fill: parent` keeps that root
   item the same size as the widget.
3. **The Chart QQuickItem is parented into the host scene
   via `setParentItem(root)` from C++**, but
   `QQuickItem::setParentItem` is *not* a QML anchor. The
   child gets a parent in the scene-graph hierarchy but
   inherits no geometry. Chart's `width()` and `height()`
   stay at their default `0`. Chart's `updatePaintNode`
   short-circuits when its bounding rect is empty, so
   nothing renders even though all upstream wiring (ADR-008
   / 009 / 010) is correct.

Per-module unit tests (M8 chart suite) constructed Chart with
explicit sizing and never exercised the
QQuickWidget-hosted-via-`setParentItem` path. The bug only
surfaces when the production `MainWindow` instantiates Chart
inside a real QQuickWidget host scene — exactly the path
covered for the first time by the M14 S1 smoke test.

## Decision

`MainWindow::rebuildChartWidgets` explicitly binds the Chart
QQuickItem's geometry to the host scene root. The binding is
done in C++ (not QML) because Chart is a C++-instantiated
QQuickItem, not a QML-instantiated type — `anchors.fill`
isn't available from C++.

```cpp
auto* root = hostWidget->rootObject();
// ...
chart->setParentItem(root);
// Track the host scene's size. SizeRootObjectToView keeps
// `root` matched to the QQuickWidget; ChartHost.qml's
// anchors.fill keeps the QML scene matched to root. The C++
// chart child does not inherit either; bind explicitly here.
const auto syncSize = [chart, root]() {
    chart->setSize(QSizeF(root->width(), root->height()));
};
syncSize();
QObject::connect(root, &QQuickItem::widthChanged,  chart, syncSize);
QObject::connect(root, &QQuickItem::heightChanged, chart, syncSize);
chartLayout_->addWidget(hostWidget, 1);
```

The connection lifetime is bound to `chart`'s QObject
lifetime (the third `connect` argument); `chart` is owned by
`ChartManager` and outlives `root` only when the host widget
is destroyed first, in which case the connection is severed
automatically by Qt's auto-disconnect.

Initial `syncSize()` is harmless when the QQuickWidget is not
yet shown — `root->width()` returns 0 → chart sized to 0,
then the first `widthChanged` / `heightChanged` signal after
`show()` brings it to the real size. Chart's redraw timer
publishes a paint node that uses the current size at paint
time.

This keeps **all changes in non-frozen files**:

- `src/app/main_window.cpp` — not frozen (V1 integration
  point precedent through M9 / M11 / M13).
- `src/chart/chart.hpp` (M8-frozen) is **not modified**;
  Chart's existing public `setSize` / `setWidth` /
  `setHeight` (inherited from `QQuickItem`) carry the binding.
- ADR-011 documents the architectural lesson.

## Rationale

Three alternatives were considered.

### Rejected: Fix B — `qmlRegisterType<Chart>`

Register `Chart` as a QML type and instantiate it from
`ChartHost.qml`'s scene with `anchors.fill: parent`. The
QML side then carries the geometry binding natively.

**Rejected because**:
- Chart's constructor takes a `SignalBufferRegistry&` and a
  `TimeAxisManager&` — neither is QML-friendly without
  significant adapter work (QML expects default-constructible
  types or attached-property workarounds).
- Larger blast radius: changes Chart's instantiation contract
  across `ChartManager` + tests.
- M8 frozen-surface concern: registering Chart as a QML type
  is arguably an interface change on the M8 freeze.
- Higher risk during a release-prereq cycle.

Noted as a V1.5+ candidate in the *Consequences* section.

### Rejected: Override `Chart::itemChange(ItemParentHasChanged)`

Override `QQuickItem::itemChange` in `chart.cpp` and self-bind
to the parent's geometry signals.

**Rejected because**:
- Adding the override declaration to `chart.hpp` modifies the
  M8 frozen surface (counts against M14 HALT #5 budget;
  current count would go 0/2 → 1/2).
- The binding is a *V1 integration* concern, not a *Chart
  intrinsic* concern. M8's Chart was designed to work in any
  parent context where the parent supplies geometry; the
  V1 GUI host happens not to. Putting the workaround in the
  host (MainWindow) keeps the design intent of M8 intact.

### Accepted: Fix A — `MainWindow` orchestrates the binding

The smallest possible change. No frozen-`.hpp` modification.
No new ADR-required interface changes beyond ADR-011 itself.

**Accepted because**:
- Frozen-surface counter stays at 0/2.
- The orchestration concern (Chart geometry ↔ host scene)
  is an *application-layer* concern, which `MainWindow` is
  the natural home for.
- Continues the ADR-009 pattern: when a wire-up is missing
  between two M2-M12-frozen subsystems, MainWindow does the
  wiring.
- Smallest possible change for the V1.0 release.

## Known limitations (V1.0 ship-as-is)

- **No QML-native theming hook for chart geometry**. Operators
  who customize `ChartHost.qml` cannot anchor non-Chart
  children relative to the chart's geometry from QML. V1.5+
  candidate (Fix B path).
- **Initial 0×0 frame**. The chart paints a 0×0 frame on the
  first paint event before the QQuickWidget has been shown.
  Visually invisible (frame contains zero pixels). No
  observed user impact.

## Consequences

- **`src/app/main_window.cpp` gains the geometry-binding
  block** in `rebuildChartWidgets`. No `main_window.hpp`
  change. No frozen-`.hpp` change.
- **CI smoke test (M14 S1) now passes Tier A**. The
  `M14_SMOKE_TIER_A: non_white_pixels=N width=W height=H`
  log line shows W>0, H>0, N>0 after this fix lands.
  S2 commit removes the `WILL_FAIL TRUE` property on the
  smoke test.
- **Regression-protect verification** runs as part of S2
  close: revert each of ADR-009 / ADR-010 setSource /
  ADR-010 Q_INIT_RESOURCE / ADR-011 chart geometry binding,
  confirm smoke fails on each revert, re-apply.
- **V1.5+ chart-as-QML-type** remains the cleaner long-term
  design (Fix B); ADR-011 documents the V1.0 expedient.

## V1.0 governance lessons (combined ADR-008 / 009 / 010 / 011)

The same multi-milestone gap pattern recurred four times in
the M13/M14 release-prereq cycle:

1. **ADR-008** — M5 §4.6 deferred decoder schema wire-up;
   caught by M13 S7.
2. **ADR-009** — M9 deferred PipelineManager attach plumbing;
   caught by M13 S7.
3. **ADR-010** — M8/V1 chart QQuickWidget hosting + qrc
   linking; caught by M13 S8 / S8.1 / S8.2.
4. **ADR-011** — M8/V1 chart QQuickItem geometry binding to
   host scene; caught by M14 S1 (the CI smoke test
   designed to catch this class of bug).

The V1.5+ recommendation from ADR-010 stands and is now
realized: the M14 S1 CI smoke test is the regression net
that catches all four bug classes (and any future
member of this pattern). Any V1+ change to the C++↔QML
hand-off chain that breaks one of:

- decoder schema runtime registration
- PipelineManager attach
- ChartHost.qml load
- Chart geometry binding to host

…now fails CI on push, not in operator dogfood.

## Cross-references

- ADR-008 (M5 §4.6 deferred schema wire-up; CR M13 S7)
- ADR-009 (M9 deferred pipeline-attach plumbing; CR M13 S7)
- ADR-010 (M8/V1 chart QQuickWidget host scene + qrc
  registration; CR M13 S8 / S8.1 / S8.2)
- M8 chart spec (`docs/milestones/M8-chart.md`)
- M14 spec (`docs/milestones/M14-gui-audit.md`) §1, §3.6
- `.claude/M14-progress.md` §"Audit findings" F1
- `tests/integration/gui/release_binary_smoke.sh` (S1
  harness; remains green after S2 lands ADR-011)
