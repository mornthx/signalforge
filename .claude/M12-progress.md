# M12 — Progress log

Per CLAUDE.md §Required #2 + plan §0, every subtask logs start +
close entries with build / test / format counts and any deviations.

---

## Pre-S0 — M12 understanding + plan (completed)

- Start: 2026-05-08T13:35Z
- Close: 2026-05-08T13:55Z
- Commit: `6032abe` "chore: record M12 understanding and plan"
- CI: pending — push triggers CI per CLAUDE.md §Required #2.
- Deliverables:
  - `.claude/M12-understanding.md` (268 lines, 6 concerns
    C1-C6 surfaced; candidate list framed as
    "informed expectation; profile arbitrates")
  - `.claude/M12-plan.md` (301 lines, S0-S7 sequenced;
    7 HALT triggers H1-H7; per-optimisation 6-step cycle)

---

## S0 — M12-concerns.md + ADR-008 decision (completed)

- Start: 2026-05-08T14:00Z

### Deliverables

- `.claude/M12-concerns.md` (~265 lines): canonical record of
  C1-C6 with subtask anchors + decision trees.
  - **C1** integrates Phase 5 clarification: primary metric
    clears 10 %; secondaries are bonus. Concrete application
    to C4 Stage B documented (1× = primary, 10× = secondary).
  - **C2** tiered profile harness (perf → callgrind →
    QElapsedTimer fallback).
  - **C3** strategic regression-cycle skip table (docs-only
    commits exempt; net ~4 cycles for the milestone).
  - **C4** explicit 0/1/2/3+ viable-count decision tree at
    S2 close.
  - **C5** backward-seek measurement → defer-to-V1.5+ default.
  - **C6** full M2-M11 frozen-`.hpp` list documented; pre-S3
    check + sha256 verification at S6.
- No ADR-008 authored. Default position holds.

### Build / test counts

- Docs-only commit. CLAUDE.md §Required #2 exception applies
  (build graph unaffected). clang-format: not applicable.
- No regression suite cycle (per C3 strategic skip).

### Deviations from plan

- Plan §S0 anticipated a conditional ADR-008 stub. Default
  position holds: no architectural divergence requiring
  ADR-008 at this point. Conditional escalation path
  documented in C6 with the full M2-M11 freeze list, so a
  future ADR-008 can be authored as a delta against this
  baseline.
- Phase 5 clarification on C1 (multi-metric optimisations:
  primary clears 10 %, secondaries are bonus) is integrated
  into the concerns doc with a worked example for C4 Stage B.

S0 commit: pending push.
