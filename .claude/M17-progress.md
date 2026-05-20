# M17 Progress Log

Running progress log for M17 execution. Updated after each subtask.

## Status table

| Subtask | Status | Commit | Build | ctest | clang-format | Notes |
|---|---|---|---|---|---|---|
| S0 (Bootstrap) | completed | 8aa7225 | n/a (docs only) | n/a | n/a | spec + understanding + plan + concerns drafted; pushed milestone/M17 to origin |
| S1 (StatusWidget class) | completed | pending | debug ✓ + release ✓ | 621/621 + 6/6 M17 S1 | ✓ | aggregate-state enum + class property; classes status-idle/connecting/connected/error per precedence rule §6.1 |
| S2 (ListWidget rows + header) | pending | — | — | — | — | — |
| S3 (SignalSelector header + count + order) | pending | — | — | — | — | — |
| S4 (MainWindow objectName audit) | pending | — | — | — | — | — |
| S5 (Visual baselines) | pending | — | — | — | — | — |
| S6 (Closure) | pending | — | — | — | — | — |

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

## Build / CI history

| Subtask | Debug build | Debug ctest | Release build | Release ctest | clang-format |
|---|---|---|---|---|---|
| S0 | n/a (docs only) | n/a | n/a | n/a | n/a |
| S1 | ✓ | 621/621 ✓ | ✓ | 6/6 M17 S1 ✓ | ✓ |
