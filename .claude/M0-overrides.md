# M0 overrides

## OV-1: Branch protection deferred (2026-04-22)

**Context**: Phase B attempted to enable main branch protection via the classic Branch Protection API; refused by GitHub with 403 plan-restriction. Rulesets API also refused. Both require GitHub Pro or a public repo on this free-tier private repository.

**Decision (human)**: Accept unprotected main for now. Revisit before V1.0 release (M11 packaging) when the tradeoff can be re-evaluated with release timing in view.

**Interim discipline** (until protection is restored):
- No direct push to main by CC. CLAUDE.md §Forbidden-4 remains in effect.
- Human does not push to main directly either; all changes go through PRs from milestone/* branches.
- CI remains the de-facto gate: merges to main are only done after milestone-branch CI is green.

**Resolution paths when revisited**:
- Upgrade account to GitHub Pro ($4/mo), keep repo private.
- Make repo public (requires separate decision).
- Accept permanently and rely on process discipline.

**Scope**: This override applies only to branch protection. All other CLAUDE.md rules are in full effect.
