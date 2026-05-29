# M21 — Dashboard P0 (closure report)

Branch: `milestone/M21` (local; **not pushed** — no push/PR authorization in this session).
Baseline: `main` @ a647ecf. Implements P0 of `docs/v0.3/dashboard-interaction-design.md`
(answers DR-001). All commits local.

## What shipped

A new `signalforge_dashboard` module + MainWindow integration turning the central area from a
vertical stack of identical charts into a **dashboard of heterogeneous panels**:

| Deliverable | Where | Tests |
|---|---|---|
| `Panel` base + `PanelConfig`/`PanelType` + `suggestPanelType` | `src/dashboard/panel*.{hpp,cpp}` | panel_factory_test (4) |
| `NumericPanel` (value + unit + observed min/max), `StatePanel` (bool ●/○, string verbatim) | `numeric_panel`, `state_panel` | panels_test (4) |
| `PlotPanel` wrapping legacy `chart::Chart` in a QQuickWidget (QPointer-safe) | `plot_panel` | plot_panel_test (2) |
| `Dashboard` reflow grid + 15 Hz refresh + auto-suggest `addSignal` + plot/chart lifecycle | `dashboard` | dashboard_test (3) |
| Dashboard-aware `SignalListPanel` (checkbox state derived from dashboard) | `signal_list_panel` | signal_list_panel_test (2) |
| MainWindow: mount dashboard + signal list, route selection, `+ Plot`, repoint hooks | `src/app/main_window.*`, `main.cpp` | full ctest + GUI smoke |

Commits: `1006bae` (plan/DR) → `f889acd` S1 → `f00720a` S2 → `48a3d96` S3 → `ca78197` S4 →
`64a40fb` S5a → `148e9af` S5b → (this) S6.

## Verification

- **Debug + Release**: build green; new module + tests build under both presets.
- **ctest (Debug)**: **662/662 pass** (after rebaselining `00-empty-launch`, C3).
- **GUI release smoke (Tier A + B)**: pass — chart still renders a trace through the new
  PlotPanel path.
- **clang-format**: clean on all changed files. **clang-tidy**: dashboard module matches the
  codebase Qt-idiom baseline (`cppcoreguidelines-owning-memory` etc., same as the frozen
  `signal_selector.cpp`); the one genuine `bugprone-unchecked-optional-access` was fixed.
- **ASan/UBSan**: not runnable locally (`/etc/ld.so.preload` AppProtection) — CI-authoritative.
- **Visual (live, /tmp, not committed)**: clean empty launch (no empty grid); dashboard with a
  live Numeric card "22.490 °C / min 20.140 max 22.490" + heterogeneous State/Numeric cards.

## Deviations and concerns (see `.claude/M21-concerns.md` for detail)

- **D1** scalar auto-suggest = Numeric (not Plot) — behavioral change toward the workbench goal.
- **D2** panels are QWidgets (PlotPanel embeds the QQuickWidget).
- **D3** legacy `ChartManager` retained as PlotPanel's store.
- **D4** Dashboard 15 Hz refresh; plots keep their 30 Hz self-drive.
- **D5** new `SignalListPanel` instead of editing the frozen `SignalSelector` (left untouched,
  still tested, no longer mounted).
- **C1** M6 buffer's 100-sample publish cadence delays slow-signal display (real product gap;
  fix = the buffer's already-deferred time-based flush; out of P0 scope).
- **C2** PRE-EXISTING teardown segfault (pipeline/connection member-order UAF) — not introduced
  by M21, masked because the smoke test never asserts exit code. Out of P0 scope.
- **C3** `00-empty-launch` visual baseline rebaselined for the intended empty-state change.

No frozen interface was modified (HALT #4 avoided): `Chart`/`ChartManager`/`SignalSelector`/
`SignalMetadata` are all reused unmodified.

## Deferred (not in P0)

- **Drag-to-add** signals onto the dashboard (the design's second add path). The
  tick-a-checkbox path delivers the same one-click "add a signal → panel" capability, so this
  is a UX nicety deferred to a follow-up.
- **P1** Table panel · **P2** new readable `PlotPanel` (axes/labels/legend/per-signal-Y) ·
  **P3** Bar/Gauge — per the design's phasing.
- C1 buffer flush and C2 teardown fix — recommend dedicated follow-ups.

## Status for the human

P0 implemented and green on `milestone/M21` (local). Per the session directive, decisions were
made autonomously and logged above for a single end-review; revert any commit if a decision is
wrong. Awaiting review; not pushed.
