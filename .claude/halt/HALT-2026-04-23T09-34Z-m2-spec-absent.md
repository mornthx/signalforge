# HALT — Rule framework extension session / M2 spec cleanup

## Trigger

Precondition failure declared by session prompt: "If M2 spec file is absent (not yet on main), HALT — cannot modify what does not exist." Also aligns with CLAUDE.md §HALT-trigger 9 (ambiguity — cannot proceed on absent target) and the session prompt's explicit HALT-and-ask directive in the M2 SPEC CLEANUP block.

## Context

- Currently executing: state reconciliation (first action of the rule-extension session)
- Completed earlier in this session: none — HALT occurred before any patch was applied
- Files modified but not in an acceptable state: none (working tree was clean at HALT)

## Problem details

The session prompt specifies 5 patches to governance documents plus cleanup of an Appendix A in `docs/milestones/M2-platform-core-abstractions.md`. The first three patches (9, 10, 11, 12, 13 in the prompt's numbering) touch `CLAUDE.md` and `docs/claude-code/execution-manual.md` only. The M2 SPEC CLEANUP block requires editing `docs/milestones/M2-platform-core-abstractions.md`.

State observation at session start:

```
$ git log --oneline origin/main -3
2dd450a docs: propagate M0/M1 learnings into governance (8 patches)
6ef9501 Merge pull request #2 from mornthx/milestone/M1
8d9601a arch: revise §8.4 density targets per M1 spike; add ADR-001

$ ls docs/milestones/
M0-project-bootstrap.md
M1-qtquick-integration-spike.md
milestone-roadmap.md

$ ls docs/milestones/M2-platform-core-abstractions.md
ls: cannot access 'docs/milestones/M2-platform-core-abstractions.md': No such file or directory
```

The M2 spec file has not been committed to main. Only M0 and M1 specs exist. The session prompt explicitly acknowledged this possibility:

> If this file does not exist, HALT and report. The human was to commit it to main but may not have done so yet.

## Candidate interpretations or approaches

- **Option A — Proceed with the other 5 patches, defer M2 Appendix A removal**: Apply patches 9–13 (CLAUDE.md + execution-manual.md) and push, skipping only the M2 spec cleanup. Implications: partial fulfillment; advisor can run a shorter follow-up session once M2 spec lands; CLAUDE.md Forbidden-4 new wording and Git operation protocol would be in force earlier. Risk: prompt treats the M2 cleanup as part of a single atomic 5-patch-plus-cleanup unit; splitting it diverges from the declared scope.
- **Option B — Full HALT, do nothing, wait for human to commit M2 spec**: Apply no patches. Await human action to commit M2 spec to main, then rerun the rule-extension session in full. Implications: governance change delayed; no partial state; cleanly aligned with the prompt's HALT-and-ask instruction.

## Decision requested

1. Which option: A (partial apply, skip M2 cleanup) or B (full HALT, re-run later with M2 spec present)?
2. If A: is it acceptable to commit patches 9–13 to main now and handle M2 cleanup in a separate session? Push authorization in the current prompt was framed as "after all patches applied and verified" — does this cover a partial set?
3. If B: please confirm the M2 spec should be committed first, then resend the session prompt.

## Side effects to clean up on resume

- Working tree is clean. No uncommitted edits.
- No local branches created this session.
- No patches applied; `CLAUDE.md`, `docs/claude-code/execution-manual.md`, and `docs/architecture/architecture.md` are unchanged from commit `2dd450a`.
- On resume: re-verify state with `git fetch origin --prune`, re-confirm M2 spec presence, then apply patches per resolved decision.
