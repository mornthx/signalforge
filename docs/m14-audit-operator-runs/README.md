# M14 Audit — Operator-Run Reports

Detailed run-by-run reports from the M13 V1.0 hardware-verification
dogfood that triggered M14 (the V1.0 GUI integration audit). One
file per operator session; chronological. The high-level summary
deliverable for M14 lives in `../m14-gui-audit-report.md`; this
directory holds the source evidence those rows are filled from.

## Run index

| Run | Date | Outcome | Headline finding | Fix landed in |
|---|---|---|---|---|
| 1 | 2026-05-09 ~16:50 | HALT at T3 | `DecoderRegistrar` constructed with empty `driverTypeToSchemaPath`; per-connection `decoderSchemaId` not wired to runtime | commit `2ef60c0` (S7, ADR-008) — verified working in run 2+ |
| 2 | 2026-05-09 ~18:55 | HALT at T3 | `QQuickWidget::rootObject()` is `nullptr` because `setSource()` is never called → orphan Chart QQuickItem | commit `f285503` + `9005ec2` (S8 / S8.1, ADR-010) — verified at source level in run 3 |
| 3 | 2026-05-09 ~20:50 | HALT at T3 | `qrc_qml.cpp.o` stripped from binary at link time (static-archive rule), `qInitResources_qml` symbol absent → resource never registers | commit `aa100c9` (S8.2, `Q_INIT_RESOURCE(qml)`) — verified in run 4 |
| 4 | 2026-05-09 ~21:25 | HALT at T3 | Chart QQuickItem stays 0×0 after `setParentItem`; nothing sets width/height | commit `4038191` (S2 chart geometry) — fix present at source, but render still white in run 5 (F4 unresolved at runtime) |
| 5 | 2026-05-09 → 05-10 | Pivot to non-chart audit | F4 chart still broken at runtime even with S2 fix; **+ 14 new findings F5–F18** spanning recording / persistence / replay UX / observability / resource budgets | (next M14 fix wave) |

## Cross-cutting observations

1. **Same symptom, four root causes**: runs 1–4 all halted at T3 step 6
   ("verify decoder → chart") with the same operator-visible behavior
   (white chart). Each prior fix correctly resolved its layer but
   exposed the next downstream barrier. This pattern is what motivated
   the M14 milestone (audit + smoke harness) per `M14-S1` infrastructure.
   Run 5 confirms the chart hand-off chain still has at least one more
   layer (F4 at the geometry-or-paint level even with `chart->setSize`
   wired).

2. **Multi-module systemic deficits, not a single module bug**: run 5's
   non-chart audit found 14 distinct deviations across `Recording`,
   `Persistence`, `Replay UX`, `Observability`, and resource budgeting.
   The chart pipeline is not the only V1 area failing operator-level
   verification.

3. **Best-case M13 acceptance projection** (with all proposed fixes
   landed): see `run5-non-chart-audit.md §Updated M13 18-test
   acceptance projection`. Even after F4 is fixed, **T5 + T10 = 2/18**
   without F6/F11/F15/F17/F12/F18 also landing.

## How to use these files

- The `m14-gui-audit-report.md` skeleton's table cells are filled from
  these run reports. When marking a row ✓/⚠/✗, cite the specific
  finding ID (F1–F18) and the run that observed it.
- A new operator run should be added as `runN-<headline-slug>.md` and
  rolled into the index above.
- All `Severity` calls in the run reports are operator-side
  recommendations; the milestone owner makes the final ship-or-patch
  call.

## Finding ID legend

- F1–F4 surfaced during runs 1–4 and each got their own HALT report
  named `runN-halt-<slug>.md`. Cross-references:
  - F1 → `run1-halt-decoder-registrar-empty-map.md`
  - F2 → `run2-halt-chart-orphan-quickitem.md`
  - F3 → `run3-halt-qrc-static-lib-stripped.md`
  - F4 → `run4-halt-chart-zero-size.md` (still un-fixed at runtime)
- F5–F18 surfaced during run 5 and are documented in
  `run5-non-chart-audit.md`. F5 (right-click menu) is the lowest
  severity; F6 (recording silent drop), F15 (buffer budget exhaustion)
  and F17 (persistence completely broken) are the critical items.
