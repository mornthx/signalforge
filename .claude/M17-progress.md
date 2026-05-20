# M17 Progress Log

Running progress log for M17 execution. Updated after each subtask.

## Status table

| Subtask | Status | Commit | Build | ctest | clang-format | Notes |
|---|---|---|---|---|---|---|
| S0 (Bootstrap) | completed | 8aa7225 | n/a (docs only) | n/a | n/a | spec + understanding + plan + concerns drafted; pushed milestone/M17 to origin |
| S1 (StatusWidget class) | completed | 0fbc6c0 | debug ✓ + release ✓ | 621/621 + 6/6 M17 S1 | ✓ | aggregate-state enum + class property; classes status-idle/connecting/connected/error per precedence rule §6.1 |
| S2 (ListWidget rows + header) | completed | 70b9c06 | debug ✓ + release ✓ | 625/625 + 4/4 M17 S2 | ✓ | QFrame#panelHeader title row + per-row QColor from tokens::light status accessors. C2 resolved (no mirror needed; direct consumer) |
| S3 (SignalSelector header + count + order) | completed | c5a3e59 | debug ✓ + release ✓ | 627/627 + 6/6 M17 S3 | ✓ | panelHeader "Signals" + filter-count label (class="caption") + std::map storage + sorted top-level groups (ADR-014 consumer closure) |
| S4 (MainWindow objectName audit) | completed | eb9a059 | debug ✓ + release ✓ | 18/18 M17 + 1 visual fail (expected per C3) | ✓ | objectNames on dock/status-bar surfaces; visual regression confirmed at 00-empty-launch (3.135% / 15906px cluster) per C3 — resolved at S5 |
| S5 (Visual baselines) | completed | a104296 | debug ✓ + release ✓ | 633/633 debug + 633/633 release ✓ | n/a (binary baselines) | All 12 M16 baselines R8-re-accepted; 1 new M17 baseline (34-conn-replay-error) for red status state. C3 resolved. |
| S6 (Closure) | completed | pending | n/a (docs only) | n/a (no source changes) | n/a | M17-done.md + widget-styling-guide §12 + PR + tag |

## S0 progress log

2026-05-20 — Bootstrap commit-cluster.

- `git checkout -b milestone/M17` from main `9674261` (M16 merge SHA).
- `docs/milestones/M17-widget-rebuild.md` — drafted (10 sections + 6
  subtask breakdown + risk register).
- `.claude/M17-understanding.md` — drafted (8 sections).
- `.claude/M17-plan.md` — drafted (S0-S6 sequencing + diff budget +
  build/CI strategy + roll-back plan).
- `.claude/M17-concerns.md` — drafted (C0 carry-forward + C1-C3
  open).
- `.claude/M17-progress.md` — this file.

Next: commit + push branch to origin; begin S1 (StatusWidget).

## Concerns reference

See `.claude/M17-concerns.md`.

## S1 progress log

2026-05-20 — ConnectionStatusWidget aggregate state + QSS class.

- Added `AggregateState` enum (Idle / Connecting / Connected / Error) to
  `connection_status_widget.hpp` + `aggregateState()` test accessor.
- Implemented aggregate computation in `refresh()` honouring spec §6.1
  precedence (Error > Connecting > Connected > Idle).
- Wired `setProperty("class", …)` + `style()->unpolish/polish/update`
  so the M16 `tokens.qss` `QLabel[class="status-*"]` rules activate.
- Added `objectName="connectionStatusLabel"` for visual-test targeting.
- Added 6 unit tests (one per aggregate state + objectName assertion);
  all pass in debug and release.
- Full ctest suite: 621/621 green in debug, 6/6 M17 S1 green in
  release.
- clang-format clean (auto-formatted on commit).

C1 status unchanged (precedence is CC-authored; awaits PR review).

## S2 progress log

2026-05-21 — ConnectionListWidget panel-header + state-coloured rows.

- Added `QFrame* header_` with `objectName="panelHeader"` and a "Connections"
  title `QLabel` (with `class="heading"`) at the top of the widget's layout.
  Header automatically picks up M16's `QFrame#panelHeader` QSS rule (M16
  tokens.qss line 56 — background `#f5f5f4`, border-bottom `#d6d6d4`,
  4 px / 8 px padding).
- Added static `colorForState(Connection::State) → QColor` consuming
  `signalforge::tokens::light::status*()` directly. No C++ mirror needed
  (concern C2 resolved — see concerns log).
- Per-row `Qt::ForegroundRole` set on `QListWidgetItem` creation (in
  `rebuild()`, `updateRow()`, and `onConnectionAdded()` paths) so the
  list item renders in the state-matched colour.
- Added test-surface accessors: `panelHeader()` (returns `QFrame*`) and
  public static `colorForState()`.
- Added 4 new unit tests (panel-header objectName + per-state colour
  parity + initial-state colour + transition-to-error colour); all pass
  in debug and release.
- Full ctest suite: 625/625 green in debug.
- clang-format clean (auto-formatted on commit).

## S3 progress log

2026-05-21 — SignalSelector panel header + count + deterministic order.

- Added `QFrame* header_` with `objectName="panelHeader"` and a "Signals"
  title label (`class="heading"`) at the top of the layout.
- Hidden the `QTreeWidget::header()` since the panelHeader now provides
  the title.
- Added `QLabel* countLabel_` (`objectName="signalSelectorCount"`,
  `class="caption"`) below the filter line edit. Text format:
    - `No signals available` when total == 0
    - `N signals` when filter is empty
    - `N / M signals` when filter is non-empty
- Switched `Impl::groups` and `Impl::leaves` from `std::unordered_map`
  to `std::map<QString, ...>` — deterministic alphabetical iteration
  closes the ADR-014 consumer-side anti-pattern (widget-styling-guide
  §9 row 6) in SignalSelector.
- Added explicit sort of `signalIds()` before tree population +
  post-population re-sort of top-level items so QTreeWidget's
  insertion-ordered storage doesn't override the deterministic group
  order.
- Added 6 new unit tests covering:
    - panel header objectName + label text + class property
    - count label class + empty-state text
    - count label transitions (unfiltered → filtered → unfiltered)
    - reverse-alphabetical insertion → alphabetical group order
    - non-alphabetical leaf insertion → alphabetical leaf order
    - tree header hidden assertion
- Full ctest suite: 627/627 green in debug.
- clang-format clean (auto-formatted on commit).

## S4 progress log

2026-05-21 — MainWindow objectName audit + outer-widget naming.

- Set `objectName` on both `SignalSelector` (`signalSelectorPanel`) and
  `ConnectionListWidget` (`connectionListPanel`) inside their respective
  constructors so the outer widgets have stable identifiers regardless
  of where they're embedded.
- In `MainWindow::buildChartUi()` added objectNames to status-bar
  labels: `fpsLabel`, `droppedLabel`, `throttledLabel`,
  `bufferBudgetLabel`, plus `mainStatusBar` on `QStatusBar*` itself.
- In `MainWindow::buildSessionUi()` and the replay-UI setup, added
  `recordingStatusLabel` and `replayStatusLabel`.
- Added 2 new unit tests asserting the outer-panel objectNames; both
  pass in debug and release.
- C3 confirmed: visual ctest `M15-visual-test_states_empty` fails after
  the build with `00-empty-launch` diff 3.135 % / max cluster 15 906 px.
  This is the SignalSelector + ConnectionListWidget panelHeader chrome
  shift from S2/S3 making contact with a baseline that pre-dates the
  chrome. All other visual tests still pass (their pytest harnesses
  cache `tests/screenshots/<state>.png` and skip re-capture when the
  file is present — those captures still reflect the pre-S2 build).
  Full impact assessment + R8 acceptance handled at S5.
- clang-format clean (auto-formatted on commit).

## S5 progress log

2026-05-21 — Visual-baseline refresh + 1 new M17 baseline.

- Cleared `tests/screenshots/*.png` + env-sidecars so visual ctests
  re-captured against the new M17 build.
- Full visual ctest pre-acceptance: 12/12 M16 baselines failed under
  the visual-diff contract (1.3–6.5 % diff, 12–16 k px clusters) due
  to the S2/S3 panelHeader chrome and the S1/S2 status colour changes.
- Ran `scripts/accept-baseline.sh <state> ""` for all 12 affected
  states. R8 acceptance recorded per-state in `.claude/M17-concerns.md`
  C3 with the pre-acceptance diff metric. Single universal R8 rationale:
  "M17 chrome shift — new QFrame#panelHeader rows above SignalSelector
  + ConnectionListWidget docks (28 px each) and per-state status colours
  on ConnectionStatusWidget + ConnectionListWidget rows".
- Added new fixture `tests/integration/gui/fixtures/m17_replay_error.yaml`
  — one Replay driver pointing at `/tmp/m17-nonexistent-session.sfreplay`.
  ConnectAll() drives it to Error → red status label + row.
- Added new state `34-conn-replay-error` to
  `tests/visual/tests/test_states_production_fidelity.py` SPECS list.
- Captured + accepted the new baseline.
- Final visual ctest: 13 / 13 baselines green on debug, 13 / 13 on
  release. Full ctest: 633 / 633 debug + 633 / 633 release.
- Scope reduction: spec §3.2 mentioned 2 new M17 baselines; the
  "all-connected" case is now implicitly captured by the re-accepted
  `04-conn-udp-connected` baseline (which renders the new M17 green
  status label). Only `34-conn-replay-error` was a genuine new state.

## Build / CI history

| Subtask | Debug build | Debug ctest | Release build | Release ctest | clang-format |
|---|---|---|---|---|---|
| S0 | n/a (docs only) | n/a | n/a | n/a | n/a |
| S1 | ✓ | 621/621 ✓ | ✓ | 6/6 M17 S1 ✓ | ✓ |
| S2 | ✓ | 625/625 ✓ | ✓ | 10/10 M17 S1+S2 ✓ | ✓ |
| S3 | ✓ | 627/627 ✓ | ✓ | 16/16 M17 S1+S2+S3 ✓ | ✓ |
| S4 | ✓ | 1 visual fail (C3) + 18/18 M17 unit ✓ | ✓ | 18/18 M17 unit ✓ | ✓ |
| S5 | ✓ | 633/633 ✓ | ✓ | 633/633 ✓ | n/a (no source changes) |
| S6 | n/a (docs only) | n/a | n/a | n/a | n/a |
