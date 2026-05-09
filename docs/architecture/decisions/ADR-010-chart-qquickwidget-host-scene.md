# ADR-010 — Chart QQuickWidget host scene

## Status

Accepted (V1.0 release blocker fix at M13 S8, 2026-05-09;
follows ADR-008 and ADR-009 in the M13 V1.0 release-prereq cycle).

## Context

`signalforge::chart::Chart` is a `QQuickItem` (M8 frozen-surface
class). `MainWindow::rebuildChartWidgets` hosts each chart in a
fresh `QQuickWidget` and reparents the `Chart` into the widget's
QML scene:

```cpp
auto* hostWidget = new QQuickWidget(chartContainer_);
hostWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
chart->setParentItem(hostWidget->rootObject());  // ← bug
chartLayout_->addWidget(hostWidget, 1);
```

`QQuickWidget::rootObject()` returns the root `QQuickItem` of the
loaded QML scene — but **without a `setSource()` call, no scene is
loaded and `rootObject()` returns nullptr**. The line above thus
calls `chart->setParentItem(nullptr)`, which orphans `Chart` from
the widget's scene graph: the QQuickItem keeps existing in memory,
the QQuickWidget keeps painting (an empty white surface), and no
chart geometry ever reaches the screen.

The defect was caught by M13 S8 GUI launch verification — the
operator opened SignalForge expecting a chart panel and saw a
blank white widget instead. M8 unit tests verified `ChartManager`
data-layer behavior; M9-M12 GUI work assumed the M8 charts
rendered correctly through `MainWindow::rebuildChartWidgets`. No
GUI integration test exercised the actual `QQuickWidget →
rootObject() → setParentItem` chain.

## Decision

Embed a minimal `ChartHost.qml` host scene as a Qt resource and
load it into each `QQuickWidget` before reparenting the chart:

```qml
// resources/qml/ChartHost.qml
import QtQuick
Item {
    anchors.fill: parent
    objectName: "chartHost"
}
```

```xml
<!-- resources/qml.qrc -->
<RCC>
  <qresource prefix="/qml">
    <!-- alias collapses the source-relative `qml/ChartHost.qml`
         to plain `ChartHost.qml`; combined with prefix="/qml"
         the resource lives at qrc:/qml/ChartHost.qml (the URL
         the C++ side passes to setSource). Without the alias
         it would register at qrc:/qml/qml/ChartHost.qml. -->
    <file alias="ChartHost.qml">qml/ChartHost.qml</file>
  </qresource>
</RCC>
```

The `alias` is **load-bearing** — the source file lives at
`resources/qml/ChartHost.qml` (one directory of nesting) and
without `alias` the qrc compiler concatenates the prefix `/qml`
with the source-relative path `qml/ChartHost.qml`, registering at
`qrc:/qml/qml/ChartHost.qml`. This was caught in S8 GUI launch
verification: the chart still rendered blank because
`setSource(qrc:/qml/ChartHost.qml)` mismatched the registered
path, the load failed silently, and the defensive `Ready`-status
guard logged the error and skipped reparenting.

```cpp
// src/app/main_window.cpp rebuildChartWidgets()
auto* hostWidget = new QQuickWidget(chartContainer_);
hostWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
hostWidget->setSource(QUrl(QStringLiteral("qrc:/qml/ChartHost.qml")));
if (hostWidget->status() != QQuickWidget::Ready) { /* log + skip */ }
auto* root = hostWidget->rootObject();
if (root == nullptr) { /* log + skip */ }
chart->setParentItem(root);
chartLayout_->addWidget(hostWidget, 1);
```

`QQuickWidget::setSource(QUrl("qrc:/..."))` is synchronous for
resource-backed URLs, so `rootObject()` is non-null on return.
The `Ready` and `nullptr` guards are defensive: if the resource
ever fails to compile (build-time error caught by AUTORCC) or to
load, we log and skip rather than re-introducing the silent
orphan bug.

CMake wire-up uses `AUTORCC ON` on the `signalforge_app_ui`
target with `${CMAKE_SOURCE_DIR}/resources/qml.qrc` listed as a
source. No new dependency; QtQuick is already linked.

## Rationale

- **Smallest possible change**. One QML file + one .qrc + one
  CMake property + one `rebuildChartWidgets` rewrite. No frozen
  `.hpp` modification (M8 `chart.hpp`, `chart_manager.hpp`, etc.
  unchanged).
- **Preserves M8 Chart QQuickItem design**. Charts remain
  `QQuickItem` instances; only the *host* gets a real QML scene.
- **Surfaces a clean theming hook for V1.5+**. The `ChartHost.qml`
  scene can be styled (background gradient, padding, dark-mode
  inheritance) without recompiling C++.
- **Synchronous load semantics**. qrc URLs load synchronously, so
  no `statusChanged` async dance is needed for V1.0.

### Rejected alternatives

- **`hostWidget->setContent(QUrl(), nullptr, chart)`**: not a
  public Qt 6 API; the old `QQuickWidget::setContent` overload
  was removed.
- **Manually create a `QQuickItem` root in C++ and assign it via
  Qt internals**: requires reaching into private Qt API; brittle
  across Qt versions.
- **Switch from `QQuickWidget` to `QQuickView::createWindowContainer`**:
  larger refactor; ADR-001 picked QQuickWidget for V1 specifically
  because of better dock behavior on Qt 6.10.

## Consequences

- **New file**: `resources/qml/ChartHost.qml`.
- **New file**: `resources/qml.qrc`.
- **Modified**: `src/app/CMakeLists.txt` — `AUTORCC ON` and qrc
  added to `signalforge_app_ui` sources.
- **Modified**: `src/app/main_window.cpp` —
  `rebuildChartWidgets()` now loads the QML scene and guards
  `rootObject()`.
- **No frozen `.hpp` modification**. M2-M12 freeze surface intact.
- **V1.0 chart rendering functional** end-to-end through the
  full live-mode chain (combined with ADR-008 + ADR-009).

## V1.0 governance lessons (combined with ADR-008 / ADR-009)

The same multi-milestone gap pattern recurred three times in M13:

1. **ADR-008**: M5 §4.6 deferred schema wire-up — caught by M13 S7.
2. **ADR-009**: M9 deferred PipelineManager attach plumbing — caught by M13 S7.
3. **ADR-010**: M8 chart QQuickWidget hosting — caught by M13 S8.

Common root cause: per-module unit tests verified the data layer
in isolation; no GUI integration test exercised the
end-to-end live-mode rendering path. M13 §3.5 V hardware
verification is the gate-of-last-resort that caught all three.

**V1.5+ recommendation**: add a mandatory GUI integration test
in CI that exercises the full chain:

- Connect a mock driver (in-process, deterministic frame source)
- Inject test frames
- Verify decoder output (signal value count > 0)
- Verify chart renders (e.g. pixel hash check on the QQuickWidget
  framebuffer, or `Chart::stats().totalRedraws > 0`)

This would have caught all three V1.0 blockers at the milestone
that introduced them, rather than at M13 release.

## Implementation lesson (post-S8.1, run-3 finding, 2026-05-09)

S8 + S8.1 fixed the qrc URL path. But the production binary
**still** failed runtime ChartHost.qml load:

```
$ nm build/release/src/app/signalforge | grep qInitResources_qml
$
```

Root cause: **static-archive linker drop**. `qrc_qml.cpp.o` lives
in `libsignalforge_app_ui.a`. `main.cpp` references no symbol
from it. `ld` pulls in only referenced objects from `.a`
archives (per System V archive semantics) — the qrc object was
silently discarded, the file-scope static initializer that would
have called `qRegisterResourceData` never ran, and the resource
never registered. `setSource(qrc:/qml/ChartHost.qml)` then
returned `Error` at runtime with no compile-time signal.

**Fix**: `Q_INIT_RESOURCE(qml)` at the top of `main()` forces
linker retention by taking the address of the generated
`qInitResources_qml` symbol:

```cpp
int main(int argc, char** argv) {
    Q_INIT_RESOURCE(qml);   // forces retention of qrc_qml.cpp.o
    // ...
}
```

Verified post-fix:

```
$ nm build/release/src/app/signalforge | grep qInitResources_qml
0000000000abc123 T qInitResources_qml
```

**CI gap**: per-test binaries each have their own link topology
and were pulling the qrc symbol correctly through Catch2's
`Catch2WithMain` reference patterns (or via direct test code
that exercised the resource). The release-binary link topology
was untested. The bug was invisible to ctest 606/606 green and
to per-job CI matrix coverage.

The S8.2 smoke test
(`tests/integration/test_m13_qrc_resources_smoke.cpp`) closes
this gap for the qrc registration: it links
`signalforge_app_ui`, calls `Q_INIT_RESOURCE(qml)` explicitly,
and asserts `QFile(":/qml/ChartHost.qml")` opens. Future
qrc-related regressions in this resource will fail this test.

**V1.5+ governance lesson** (additive to the GUI-integration
recommendation above):

- **CI smoke test against the actual release binary required** —
  not just per-test binaries. Per-test binaries' link topology
  is not the production binary's link topology.
- For each `Q_INIT_RESOURCE` / `qmlRegisterType` /
  `qRegisterMetaType` / similar registration pattern that lives
  in a static archive, add a smoke test that asserts the
  registration actually happened in the production binary
  (e.g. `nm $bin | grep $symbol` post-build, or a headless
  launch test that exercises the resource).

## Cross-references

- ADR-008 (sister fix; closes M5 §4.6 deferred schema wire-up)
- ADR-009 (sister fix; closes M9 deferred pipeline-attach plumbing)
- M8 chart spec (`docs/milestones/M8-chart.md`) — Chart QQuickItem design
- M13 spec §3.5 V hardware verification (release-prerequisite gate)
- `docs/release-notes/v1.0.0.md` "Known limitations" — V1.5+
  GUI integration-test recommendation
