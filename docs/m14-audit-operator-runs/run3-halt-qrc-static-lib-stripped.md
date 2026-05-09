# M13 V1.0 Hardware Verification — HALT report (run 3)

**Date**: 2026-05-09 (CST)
**Operator**: shuai
**Halt at**: T3 (M9 UDP driver), 1st test attempted of 18 (run 3)
**Acceptance bar**: 16/18 PASS
**Projected pass count**: same as run 2 — best-case 9/18 (chart-render still
broken at runtime; same dependent-test list blocked)

## Prior HALTs

- Run 1: empty `driverTypeToSchemaPath` in `DecoderRegistrar` → fixed in
  commit `2ef60c0` (M13 S7, ADR-008). **Verified working in run 2 and run 3.**
- Run 2: orphan QQuickItem (`QQuickWidget` had no QML source) → fixed *at
  the source level* in commits `f285503` (M13 S8, ADR-010) and `9005ec2`
  (S8.1 qrc-prefix follow-up).

## Trigger (run 3)

The S8/S8.1 fix is present in the source on `milestone/M13` HEAD and the
`build/release/src/app/signalforge` binary was rebuilt against it, but the
chart still renders pure white. Log captured at session start:

```
{"ts":"2026-05-09T20:43:37.305Z","level":"error","thread":3172224,"module":"signalforge",
 "event":"MainWindow: ChartHost.qml failed to load (status=3)","fields":{}}
```

`status=3` is `QQuickWidget::Status::Error`. The QML resource is failing to
load — meaning `setSource()` is being called (good — that part of S8/S8.1 is
in) but the resource itself is not present at runtime.

## Evidence chain

| Layer | Observation | Verdict |
|---|---|---|
| Source: `src/app/main_window.cpp:325` | `hostWidget->setSource(QUrl(QStringLiteral("qrc:/qml/ChartHost.qml")));` | Source-level fix present |
| Source: `resources/qml/ChartHost.qml` | Valid Qt Quick `Item { anchors.fill: parent; objectName: "chartHost" }` | QML asset present |
| Source: `resources/qml.qrc` | `<qresource prefix="/qml"><file alias="ChartHost.qml">qml/ChartHost.qml</file></qresource>` | qrc prefix matches setSource URL (S8.1 fixed double-prefix) |
| Build: `src/app/CMakeLists.txt:4,26` | `qml.qrc` listed in sources; `AUTORCC ON` | CMake-side wiring present |
| Build artefact: `…/qrc_qml.cpp.o` | Contains `qInitResources_qml` and `_GLOBAL__sub_I_qrc_qml.cpp` static initializer | AUTORCC ran correctly; resource compiled into static lib |
| Static lib: `libsignalforge_app_ui.a` | `ar t` lists `qrc_qml.cpp.o` | Object file is in the archive |
| **Final binary: `signalforge`** | `nm signalforge \| grep qInitResources_qml` → **empty** | **Linker dropped the qrc object — its symbols are unreferenced from the executable, and `.a` archives only pull in objects whose symbols are needed** |
| Source: `src/app/main.cpp` | `grep -rn 'Q_INIT_RESOURCE' src/` returns nothing | No explicit reference to `qml` resource → linker has no reason to keep it |

## Root cause

Classic static-archive + static-initializer interaction. The Qt resource
system relies on a global static initializer (`_GLOBAL__sub_I_qrc_qml.cpp`)
to call `qRegisterResourceData()` at process start. When the resource is in
a static library (`.a`) and the executable's main `.o` does not reference
any symbol from `qrc_qml.cpp.o`, the linker — by design — does **not** pull
that object file into the final binary. The static initializer never runs;
the resource never registers; `setSource("qrc:/qml/ChartHost.qml")` fails.

S8/S8.1 fixed the C++/QML/qrc-path triplet but did not bridge the
static-lib boundary. CI integration tests likely passed because tests live
in their own executables with their own main()s and a different link
topology that didn't trip this trap — exactly the kind of CI/release-build
divergence that 18-test dogfood is supposed to catch.

## Tests blocked

Same set as run 2 (chart-render is the criterion that fails):

- Direct fail: T1 / T2 / T3 / T4 / T13 / T14 / T15 / T16 / T17 (9 tests)
- Likely still pass without chart: T5 / T6 / T7 / T8 / T9 / T10
- Optional ambiguous: T11 / T12 / T18

Best-case 9/18 < 16. **HALT (H4 trigger).**

## Recommended fix direction

Smallest patch (recommended): add **one line** to `src/app/main.cpp` near
the top of `main()`:

```cpp
Q_INIT_RESOURCE(qml);
```

This forces the linker to keep `qrc_qml.cpp.o` in the executable and
guarantees the resource registration runs at startup. Trivially testable:
re-run `nm build/release/src/app/signalforge | grep qInitResources_qml` —
the symbol must appear.

Alternatives if Q_INIT_RESOURCE is undesirable:

1. Promote `signalforge_app_ui` from `add_library(... STATIC ...)` to
   `add_library(... OBJECT ...)`. Object libraries always pull all objects
   into dependents.
2. Apply `--whole-archive` to the link of `signalforge_app_ui` into
   `signalforge` (e.g. via
   `target_link_options(signalforge PRIVATE "-Wl,--whole-archive,$<TARGET_FILE:signalforge_app_ui>,--no-whole-archive")`).
3. Move `main_window.cpp` (which references the resource via setSource) and
   `qrc_qml.cpp` into the `signalforge` executable directly rather than
   the `signalforge_app_ui` static lib. Largest reorg; only sensible if
   architecturally clean.

## CI-side observation worth flagging

The fact that S8/S8.1 *unit/integration tests* passed but the actual `signalforge`
binary still fails the very thing the fix targeted indicates the verification
harness for ADR-010 didn't exercise the same link topology as the release
binary. Consider adding a smoke test that:

1. Builds `signalforge` for the `release` preset.
2. Launches it headlessly with `Q_QPA_PLATFORM=offscreen`.
3. Greps the runtime log for `MainWindow: ChartHost.qml failed to load` and
   fails the test if found.

Without that, every future `qml.qrc` change is at risk of silently
regressing in the release binary while CI stays green.

## Captured artefacts

- `~/.local/state/signalforge/logs/signalforge.log` — search after marker
  `>>> M13 retest run3 start 2026-05-09T20:43:23+08:00`
- `/tmp/m13-verify-logs/M13-HALT-decoder-registrar-empty-map.md` (run 1, fixed)
- `/tmp/m13-verify-logs/M13-HALT-chart-orphan-quickitem.md` (run 2, partly
  fixed in source, not in binary)
- This report (run 3)

## Operator action taken

- M13 dogfood run 3 halted at first chart-pass criterion (T3 step 6).
- Tests T1/T2/T4/T13–T17 not attempted — same defect would block them.
- Lifecycle-only tests (T5/T6/T7/T8/T9/T10) deferred to keep the run
  consistent (one fix, one full re-run).
- Awaiting `Q_INIT_RESOURCE(qml)` (or equivalent) and reverification per M13
  plan §3.

---

Reviewer: this is the third halt in the same protocol; consider tightening
release-build verification before run 4 so we don't burn another operator
session on a binary-only defect.
