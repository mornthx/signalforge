# M14 — Progress

Live state for `milestone/M14` execution. Updated by CC after
each subtask close + on each operator audit-section pass.
Operator findings posted as numbered lists under §"Audit
findings (S3)".

Source: `.claude/M14-understanding.md` + `.claude/M14-plan.md`
+ `.claude/M14-concerns.md`. Spec:
`docs/milestones/M14-gui-audit.md`.

---

## Subtask state

| ID | Title | Status | Commits | Notes |
|---|---|---|---|---|
| S0 | Concerns C1-C6 + PR #24 closure | **in progress** | (this commit) | Includes M14-concerns.md, M14-progress.md scaffold, PR #24 close-with-supersede |
| S1 | CI release-binary smoke test (Tier A + Tier B) + framework | not started | — | Per C1: shell+Python harness invoked by Catch2 wrapper at `tests/integration/gui/` |
| S2 | Run-4 chart sizing fix | not started | — | Two plausible impls (itemChange vs qmlRegisterType); HALT #9 if can't pick |
| S3 | GUI audit (operator-paired) | not started | — | Per C2: daily ping-pong by spec §3.2 section |
| S4 | Critical bug fixes | not started | — | Per-bug commit (M14.3 P); ADR-011+ for architectural changes |
| S5 | V1.0 scope re-evaluation (Scenario A/B/C) | not started | — | Collaborative; CC drafts, human finalizes |
| S6 | 18-test HW verification re-run | not started | — | Operator-driven; 16+/18 required for Scenario A |
| S7 | M14-done.md + V1.0 release PR | not started | — | Per C3: fresh PR `milestone/M14 → main` |

## Scenario decision discipline (C5 reminder)

Per spec M14.5 X, V1.0 ships in one of three forms after
audit. **CC must NOT pre-commit to Scenario A.**

The decision is made post-audit, written collaboratively
with the human, finalized in `docs/v1.0-scope-evaluation.md`
at S5.

Triggers that should bias toward Scenario B / C:

- Architectural fix requires modifying > 2 frozen `.hpp`
  files (HALT H5)
- `> 10` Critical bugs in S3 audit (HALT H3)
- 18-test HW verification `< 12/18` (HALT H4)
- Audit reveals fundamental unfixable issues (HALT H1)

Until S5: do **not** phrase commits / progress updates /
done.md drafts as if Scenario A is the outcome. Use neutral
language ("the V1.0 scope decision in S5 will determine
…").

## Frozen-surface modifications (C6 counter)

| Counter | Limit (HALT H5) | Status |
|---|---|---|
| **0 / 2** | > 2 → HALT | OK — clean baseline |

Baseline reset at M14 S0. ADR-008's additive method on
M5-frozen `decoder_registrar.hpp` is part of `milestone/M13`
ancestry already merged into the M14 base branch and is **not
counted** against the M14 budget. Any new frozen-`.hpp`
modification by M14 commits must be appended to the table
below before commit; if appending would push count to `> 2`,
HALT #5 fires.

| # | File | sha256 pre / post | Commit | ADR |
|---|---|---|---|---|

(empty — no M14 frozen-surface modifications yet)

## Audit findings (S3)

Operator posts findings here, one section per pass. CC folds
findings into `docs/m14-gui-audit-report.md` and into S4 fix
commits as they land.

### Pass 1 — (date / section TBD)

(awaiting first operator pass; S3 begins after S1 + S2 close)

## HALT log

Track any HALT trigger fires here, with timestamp + cause +
resolution path. Empty so far.

## Cross-references

- Spec: `docs/milestones/M14-gui-audit.md`
- Understanding: `.claude/M14-understanding.md`
- Plan: `.claude/M14-plan.md`
- Concerns: `.claude/M14-concerns.md`
- M13 closure: `.claude/M13-done.md` §"M13 not release-ready
  — escalating to M14"
- Closed PR #24 — superseded by `milestone/M14`
