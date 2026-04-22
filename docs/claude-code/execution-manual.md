# Claude Code Execution Manual

**Status**: Operating contract for Claude Code (hereafter "CC") on the SignalForge project.

**Purpose**: Convert the architecture document into a workflow that CC can execute safely in auto mode, with clear human checkpoints to prevent drift.

**Core premise**: CC supplies engineering volume. Humans set and correct direction.

**Companion documents**:

- `docs/architecture/architecture.md` — technical baseline
- `docs/milestones/milestone-roadmap.md` — overview of all milestones
- `docs/milestones/M<n>-<slug>.md` — per-milestone detailed specs

**Cross-reference notation used across project documents**:

- `[EM §N]` — this Execution Manual, section N
- `[Arch §N]` — Architecture document, section N
- `[MR M<n>]` — Milestone Roadmap, entry for milestone `M<n>`

---

## 1. Roles and Responsibilities

### 1.1 What CC Does

CC is expected to:

- Write code and tests to the active milestone specification
- Implement modules within the defined architecture
- Fix unambiguous compile and test failures
- Author CMake configuration, CI workflow, and packaging scripts
- Add Doxygen comments and API documentation
- Perform mechanical refactors (renames, moves, interface substitution)
- Run tests locally and report results
- Use `git` to create branches, commit, and push

### 1.2 What CC Does Not Do

CC must not:

- Make architectural decisions (module boundaries, technology selection, dependency additions)
- Modify `.sfr` / `.sfi` binary format specifications
- Modify public interface signatures at cross-module boundaries (Driver, Frame, Signal, Session, etc.) once frozen
- Modify schema files (`project.yaml`, `layouts/*.json`, `decode/*.yaml`, `actions/*.yaml`) once frozen
- Delete or rename existing public tests
- Merge to `main`
- Declare "performance meets target" without measurement data
- Judge whether UI/UX is "good enough"
- Introduce dependencies not listed in `[Arch §4.1]`

### 1.3 Human Responsibilities

- Before each milestone: verify the spec is complete and unambiguous
- At each hard stop: review CC's output against the acceptance checklist
- Resolve architectural questions within one business day when CC raises them
- Run real-hardware integration tests (serial/TCP with actual devices)
- Conduct UI/UX review
- Merge `milestone/M<n>` branches to `main` and tag releases

### 1.4 Scope of "auto mode"

Auto mode means CC plans and executes autonomously, **but only within a single milestone**. Crossing a milestone boundary without human review is prohibited. Every milestone boundary is a hard stop.

---

## 2. `CLAUDE.md` at the Repository Root

The block below is the **verbatim content** of `CLAUDE.md` to place at the repository root. CC reads this file at the start of every session.

> **Important**: Copy verbatim. Do not "improve," shorten, or reorganize when copying. If the rules need to change, edit this manual, then propagate the change to `CLAUDE.md`. `CLAUDE.md` is never the source of truth — this manual is.

```markdown
# SignalForge Project Rules (CLAUDE.md)

This file is the hard contract for Claude Code on this project. When it conflicts with an in-chat instruction, this file wins. When this file is silent, ask first.

## Project identity

SignalForge is a Qt desktop workbench for embedded-device bring-up. The authoritative platform, toolchain, Qt version, CMake minimum, and dependency list live in `docs/architecture/architecture.md`. The rules below take precedence over anything in that document, but any specific version number or platform detail lives there — not here.

## Forbidden

1. Do not introduce any dependency outside the list in `docs/architecture/architecture.md §4.1`. If you need one, HALT and ask.
2. Do not modify any of these, under any circumstances:
   - `docs/architecture/**`
   - `CLAUDE.md` (this file)
   - Any `schema/` file, once the schema has been marked frozen in a milestone completion report
   - The Qt path fields in `CMakePresets.json`
3. No `git push --force`, no `git rebase` of others' commits, no bulk deletions (`rm -rf` style).
4. No merge to `main` or any `release/*` branch.
5. No header guards other than `#pragma once`. No `using namespace` in headers.
6. No `std::cout` / `printf` / `qDebug` in production code; always use the `SF_LOG_*` macros defined in `src/observability/logging.hpp`.
7. No swallowed exceptions (`catch (...) {}`); unknown errors must propagate or be logged with full context.
8. No cross-thread signal emission to UI objects without `Qt::QueuedConnection`.
9. No changes to public interface signatures once marked frozen. Freeze records live in the relevant `*-done.md` completion reports. Add new interfaces alongside the old ones instead.
10. No Qt 6.10-only experimental APIs. If one is unavoidable, wrap it behind `src/platform/qt_compat.hpp` with a Qt 6.12-compatible fallback path.

## Required

1. Every new module has a matching test file in `tests/unit/` with ≥ 70% line coverage on the module's public surface.
2. Before every commit:
   - Build passes for **both** Debug and Release presets
   - `ctest` passes on Debug and Release presets
   - AddressSanitizer and UBSan violations: verified on the `debug-asan` preset when the local host permits; otherwise CI is the authoritative gate (document the local block in `.claude/M<n>-concerns.md`)
   - `clang-format --dry-run -Werror` passes on changed files

   **Exceptions to the above**:
   - Commits that only modify non-code files (docs, Markdown, config like `.gitignore`, CI workflow YAML) may be made without rebuilding when the build graph is unaffected.
   - During bootstrap or major refactoring, it is acceptable to batch multiple subtasks into a single "first buildable commit" to avoid intentionally-red intermediate states. State this intent in the plan before executing.
3. Commit message format: `<module>: <imperative verb> <object>`. Example: `frame: add backpressure hook`. Subject line ≤ 72 characters.
4. Each PR or merge is ≤ **800 net lines added**, excluding generated files and test fixtures. Larger changes must be split.
5. Any change touching a performance-sensitive path includes before/after benchmark numbers in the commit body.
6. Any cross-thread code comes with a corresponding ThreadSanitizer test.
7. Prefer modern C++: `std::jthread` over `std::thread`, `std::string_view` / `std::span` where appropriate, `std::optional` / `std::variant` over sentinel values.
8. Smart pointers: `std::unique_ptr` is the default; `std::shared_ptr` only when ownership is genuinely shared.

## HALT triggers — stop immediately when any of these fires

You **must** stop all code changes, write a HALT report to `.claude/halt/HALT-<UTC-timestamp>-<slug>.md`, commit whatever is committable, and exit the session. Do **not** attempt to continue. Do **not** try alternative approaches beyond the attempt limits below.

1. A compile error persists after **3** attempts to fix it
2. A test fails after **3** different fix attempts, regardless of the symptom
3. You need to introduce a new dependency
4. You need to modify any public interface signature or any frozen schema
5. You need to modify any file in the Forbidden list above
6. A performance benchmark fails its target after **one** optimization pass
7. You find a contradiction between the milestone spec and `docs/architecture/architecture.md`
8. A Qt 6.10 API behaves differently than its documentation indicates, and resolving it requires external research
9. You have two plausible implementations and cannot confidently pick one
10. Any `git` operation fails for a reason you cannot explain

HALT report format: see `docs/claude-code/execution-manual.md §6.1`.

## Definition of Done (per task)

A task is "done" only when **all** of the following hold:

1. Code is written and compiles cleanly under Debug, Release, and Debug+ASan presets
2. All relevant tests pass under all three presets
3. Coverage target is met (≥ 70% on the module's public surface)
4. Doxygen comments exist on all public declarations
5. `clang-format` and `clang-tidy` produce no new warnings
6. The change is committed with a conforming message
7. `.claude/M<n>-progress.md` is updated to reflect the new state

If any of these is not met, the task is **not done**. Either keep working, or HALT.

## Ambiguity handling

When the spec is ambiguous, the default action is **do not proceed**. Write your two best interpretations into `.claude/M<n>-concerns.md` with their implementation differences, then HALT if the ambiguity blocks progress, or continue with other tasks if it does not. Never guess silently.

## Disagreement handling

If you believe a spec clause is wrong or suboptimal, record your concern in `.claude/M<n>-concerns.md` with a proposed alternative, **and then execute the spec as written**. Do not deviate from the spec without explicit human approval. Concerns are addressed at milestone review.

## Tooling

- Build: CMake + `FetchContent`. Do not switch to system packages.
- Editor config: `.editorconfig` is locked; do not modify.
- Formatter: `clang-format` with config at `.clang-format`; do not modify.
- Static analysis: `clang-tidy` with config at `.clang-tidy`; you may extend rules, you may not relax them.
- Build directory: `build/` only, with one subdirectory per preset.

## Qt 6.10.2 notes

- Qt 6.10 is non-LTS. V1.0 may migrate to Qt 6.12 LTS. **Do not use Qt 6.10-only APIs.** If unavoidable, wrap behind `src/platform/qt_compat.hpp`.
- `QQuickWidget` + RHI is more stable on Qt 6.10 than on early Qt 6 releases, but floating dock panels remain risky. Until M1's integration report concludes, keep `QQuickWidget` usage minimal.
- Prefer C++-driven Qt Quick patterns; QML for presentation only.

## Anti-patterns to avoid

- Over-abstraction before a second use case exists
- Helper functions with exactly one caller
- Silent error swallowing disguised as "graceful degradation"
- Edit loops: modify, revert, re-modify — if stuck, HALT
- "Fixing" tests by loosening assertions
- Adding `// TODO` without an issue reference or HALT
- Premature optimization without benchmark numbers
- Renaming files or symbols outside the declared scope of the current task

## What you may decide on your own

- Private function names
- Internal data structure choices, so long as interface contracts hold
- Test organization, so long as coverage targets are met
- Comment wording, so long as Doxygen tags are correct
- Which standard-library facility to use when multiple are viable
- Internal tool implementation details
```

---

## 3. Session Protocol

### 3.1 Session granularity

**One CC session equals one milestone.** Never span milestones within a single session.

Within a milestone, CC may break work into multiple "tasks" (for example, in M3, `SerialDriver`, `TcpDriver`, and `UdpDriver` are separate tasks). Task boundaries must be clear, and each task ends with an update to `.claude/M<n>-progress.md`.

### 3.2 Session startup inputs

At the start of a milestone session, the human provides CC with three inputs:

1. The milestone spec path (e.g., `docs/milestones/M0-project-bootstrap.md`)
2. The completion or HALT report of the previous milestone, if any
3. The explicit task scope for this session (e.g., "all of M0" or "the `SerialDriver` portion of M3")

### 3.3 Required CC workflow inside a session

CC executes in this order:

1. Read `CLAUDE.md`, the current milestone spec, and the relevant sections of `docs/architecture/architecture.md`.
2. Produce `.claude/M<n>-understanding.md`: restatement of the spec, ambiguities identified, risks anticipated.
3. Produce `.claude/M<n>-plan.md`: ordered subtask breakdown with outputs and estimates.
4. **Stop and wait for human approval of the understanding and plan.**
5. After approval, execute subtasks in order. Commit after each subtask.
6. Update `.claude/M<n>-progress.md` after each commit.
7. On any HALT trigger, stop immediately, write `.claude/halt/HALT-<UTC-timestamp>-<slug>.md`, and exit.
8. When all subtasks are done, produce `.claude/M<n>-done.md` and exit.

Step 4 is not optional. The 15 minutes of human review at step 4 is the primary anti-drift mechanism.

### 3.4 Human review cadence

| Review point | Action | Time budget |
|---|---|---|
| Before session start | Confirm the spec is clear, prior milestone is closed | Before handing off to CC |
| Understanding + plan received | Scan for drift from the spec; approve or request revision | 30 minutes |
| Each HALT report | Resolve the HALT: amend spec, make decision, or unblock | 1 business day |
| Completion report received | Run the acceptance checklist (§5) | 2–4 hours |
| Merge to `main` | Human opens PR, reviews, merges | Immediately after acceptance |

### 3.5 Branch strategy

- `main` is protected. Only humans merge into it.
- `milestone/M<n>` is cut from `main` at the start of each milestone. CC commits here.
- CC commits directly to `milestone/M<n>`; no sub-branches are required.
- On acceptance, the human opens a PR from `milestone/M<n>` to `main`, merges, and tags `v0.<n>.0-alpha.1`.

Commits on `milestone/M<n>` should be meaningful: one commit per subtask, not per edit. If CC finds itself churning, HALT.

---

## 4. Interruption Matrix

These are the hard stops where human intervention is required. CC cannot proceed past any of them.

| # | Trigger | CC produces | Human decides |
|---|---|---|---|
| 1 | End of any milestone | Completion report | Accept via §5 checklist, or reject |
| 2 | Any HALT condition inside a milestone | HALT report | Resolve HALT: update spec or give decision |
| 3 | M1 — Qt Quick integration conclusion | Spike report with numbers | Choose: QQuickWidget / QWindow container / QPainter + OpenGL fallback |
| 4 | M2 — Driver interface finalized | Header + docs | Review and freeze the interface |
| 5 | M3 — Real-hardware acceptance | socat and local-socket evidence | Run real-hardware loop |
| 6 | M4 — Decode rule schema finalized | YAML schema + sample | Freeze schema |
| 7 | M6 — Chart performance acceptance | Benchmark report | Pass against `[Arch §8.4]` |
| 8 | M7 — Control loop hardware acceptance | Mock-loop evidence | Run real-hardware loop |
| 9 | M8 — `.sfr` format finalized | Byte-level spec + dump tool | Freeze format |
| 10 | M10 — Performance certification | Full benchmark report | Pass against `[Arch §8.4]` in full |
| 11 | M11 — Release readiness | Release checklist + installers | Go / no-go on internal release |

### 4.1 Meaning of "freeze"

A frozen interface or schema evolves only in **backward-compatible** ways: add fields, bump `schemaVersion`, deprecate (never delete). CC cannot unfreeze a frozen artifact; only a human can, and only by explicitly updating the relevant completion report and the architecture document.

---

## 5. Milestone Acceptance Checklist

Every completion report covers these items. Humans tick each item during review.

### 5.1 Code

- [ ] All promised modules and files exist
- [ ] Debug and Release presets build clean; there are no warnings
- [ ] Unit tests pass under all configured presets; coverage ≥ 70% on core modules
- [ ] AddressSanitizer is clean under the `debug-asan` preset
- [ ] `clang-format` and `clang-tidy` pass
- [ ] No `TODO` residue (any that remain are listed and justified in the completion report)
- [ ] No commented-out code blocks
- [ ] No stray debug output (`std::cout`, `qDebug`, `printf`)

### 5.2 Tests

- [ ] Unit tests cover every happy path in the spec
- [ ] Unit tests cover every failure path in the spec
- [ ] Integration tests exist where the spec requires them
- [ ] Performance tests exist where the spec requires them
- [ ] All tests runnable via `ctest` from the relevant build directory

### 5.3 Documentation

- [ ] Public declarations have Doxygen comments
- [ ] Non-trivial public interfaces carry a usage example
- [ ] Spec-required documents exist under `docs/`

### 5.4 Process

- [ ] Commit history is clean and grouped by subtask (no `WIP` leftovers)
- [ ] Commit messages conform to `<module>: <verb> <object>`
- [ ] No unresolved merge conflicts
- [ ] All HALT reports, if any, are resolved

### 5.5 Spec conformance

- [ ] Every "must deliver" item in the spec is done
- [ ] Every "must not do" item is verifiably absent
- [ ] Every acceptance criterion in the spec is met
- [ ] Deviations and concerns from `.claude/M<n>-concerns.md` are triaged

---

## 6. Report Templates

### 6.1 HALT report — `.claude/halt/HALT-<UTC>-<slug>.md`

```markdown
# HALT — M<n> / <subtask>

## Trigger

(Pick one from the HALT list in CLAUDE.md, or describe if the trigger is outside the list.)

## Context

- Currently executing: <subtask>
- Completed earlier in this session: <list>
- Files modified but not in an acceptable state: <list>

## Problem details

<Exact error output, file paths, reproduction steps.>

## Candidate interpretations or approaches

- **Option A**: <description> → implications: <list>
- **Option B**: <description> → implications: <list>
- (Option C, if applicable)

## Decision requested

1. Which option (A / B / C / other)?
2. Should the spec be amended?
3. Can this item be skipped for this milestone?

## Side effects to clean up on resume

- Uncommitted changes at: <paths>
- Half-created files: <paths>
- Partial build state or other state to reset
```

### 6.2 Completion report — `.claude/M<n>-done.md`

```markdown
# M<n> Completion Report

## Timing

- Started: <UTC>
- Completed: <UTC>
- Active work time: <hours>

## Deliverables checklist

| Spec item | Status | Notes |
|---|---|---|
| ... | ✅ / ❌ / ⚠️ | ... |

## Acceptance self-check (§5)

(All items from §5.1–§5.5, each marked ✅ or explained.)

## Test results

- Unit: <pass>/<total>, coverage <percent>
- Integration: <pass>/<total>
- Performance (if applicable): <key numbers>

## HALTs raised during this milestone

| # | Trigger | Resolution |
|---|---|---|
| 1 | ... | ... |

## Deviations and concerns

List items from `.claude/M<n>-concerns.md` and how each was treated.

If no deviations, say so explicitly — and recheck once; "zero deviations" is suspicious.

## Freezes established in this milestone

List any interface, schema, or format that was frozen during this milestone. Subsequent milestones cannot change these except additively.

## Open issues carried forward

Small TODOs, known limitations, items needing future work.

## Suggestions for the next milestone

Anything learned that would change the next milestone's spec.

## Commits in this milestone

<Commit hash list, one line each.>
```

---

## 7. Anti-Drift Mechanisms

Auto-mode CC's main failure mode is slow drift: not single-point errors, but small "close enough" deviations accumulating into a wrong direction. These mechanisms counter it.

### 7.1 Spec restatement

The first action of every milestone session is the understanding file. CC restates the spec in its own words and lists ambiguities. The human confirms the reading within 15 minutes before code is written. **This step is never skipped.** A day of rework costs more than 15 minutes of review.

### 7.2 Deviation reports are mandatory

Every completion report includes a deviations section. If CC writes "zero deviations, fully compliant," treat that as a warning sign — real specs always have edge cases. Ask CC to re-review its concerns file.

### 7.3 Random spot checks

Every three milestones, pick one completed milestone and spot-check one submodule. Look for things a spec-compliance review might miss: implicit global state, error paths without tests, overly permissive comparisons.

### 7.4 No quiet workarounds

If CC applies a workaround rather than a fix, the commit subject must begin with `[WORKAROUND]` and the completion report must list it. Workarounds are triaged at milestone acceptance: accept as permanent, schedule for a later fix, or reject now.

### 7.5 Edit-loop detection

Three failed fix attempts on the same symptom is a HALT, not a fourth attempt. CC HALTs even if the next attempt "feels close."

---

## 8. Inter-Milestone Transitions

Moving from `M<n>` to `M<n+1>` requires, in order:

1. `M<n>` completion report filed and reviewed
2. §5 acceptance checklist passed by human
3. All `M<n>` HALTs resolved
4. `milestone/M<n>` merged to `main` via PR; tagged `v0.<n>.0-alpha.1`
5. `M<n+1>` spec has been reviewed by human
6. `milestone/M<n+1>` branch cut from `main`

Only then does the next CC session begin. No exceptions.

---

## 9. Bootstrap Sequence

Run once, to set up the project:

1. Create an empty GitHub repository. Enable branch protection on `main`: required reviews, no force push, no direct push.
2. `git clone` locally.
3. Populate `docs/architecture/architecture.md`.
4. Populate `docs/milestones/milestone-roadmap.md`.
5. Populate `docs/milestones/M0-project-bootstrap.md`.
6. Populate `docs/claude-code/execution-manual.md` (this file).
7. Commit seed docs to `main` and push.
8. (CC handles this as preflight.) At the start of the M0 session, CC copies §2 verbatim to `/CLAUDE.md` and commits it. The human does not need to create this file manually.
9. Create `milestone/M0` branch from `main`.
10. Hand the session off to CC using the opening message in Appendix A.
11. Wait for understanding + plan files; review and approve.
12. Monitor for HALTs or completion.

Repeat steps 5, 9, 10, 11, 12 for each subsequent milestone, substituting the next milestone's spec and branch name.

---

## 10. Conflict Resolution

When this manual and an in-chat instruction to CC disagree, this manual wins.

When a human wants to relax a rule ("just this once, allow dependency X"), the correct response is to **edit this manual or the relevant milestone spec**, not to issue a verbal override. Reason: CC re-reads `CLAUDE.md` and spec files throughout a session but does not reliably retain ad-hoc chat overrides.

If a relaxation is truly one-off and time-sensitive, record it in `.claude/M<n>-overrides.md` with human sign-off, and revisit it at milestone acceptance.

---

## Appendix A: First-session opening message



```
You are Claude Code, working on the SignalForge project on branch milestone/M0.

Your first task is to bootstrap the project skeleton. Follow the spec strictly.

Required reading, in this order:
1. CLAUDE.md
2. docs/claude-code/execution-manual.md — sections 1 through 7
3. docs/architecture/architecture.md — sections 3, 4, 12, 18
4. docs/milestones/M0-project-bootstrap.md

Do NOT write code yet. Produce only these two files, then stop:

Step 1 — write .claude/M0-understanding.md:
- Restate the M0 goal in your own words (3–5 sentences).
- List ambiguities or contradictions you identified, with your default interpretation for each.
- List the HALT risks you anticipate.

Step 2 — write .claude/M0-plan.md:
- Break M0 into 8 to 15 ordered subtasks.
- For each subtask: output files, rough effort, prerequisites.
- Mark the commit points.

After both files exist, reply "M0 understanding and plan ready for review" and stop.
Do not proceed to code until I say "approved, begin M0 execution".
```

---

## Appendix B: Quick reference for humans

| What you want to do | Where to look |
|---|---|
| Start a new milestone | §3.2, §9 (steps 5 and 9–12) |
| Review CC's understanding | §3.3 step 4, §7.1 |
| Handle a HALT | §6.1, §4 |
| Accept a completed milestone | §5 |
| Merge and tag | §3.5, §8 |
| Change a project rule | §10 — edit this manual, then propagate to `CLAUDE.md` |
| Freeze an interface or schema | §4.1; record freeze in the milestone completion report |

---

## Appendix C: Directory map for `.claude/`

All CC-authored operational artifacts live under `.claude/`:

```
.claude/
├── M0-understanding.md
├── M0-plan.md
├── M0-progress.md
├── M0-done.md
├── M0-concerns.md           # ambiguities, disagreements
├── M0-overrides.md          # one-off human-approved rule relaxations, if any
├── halt/
│   └── HALT-2026-04-22T14-33Z-cmake-qt-not-found.md
└── M1-...                   # next milestone's artifacts
```

This directory is committed to git. The one exception is unresolved HALT files mid-session, which are committed at resolution time rather than as they are written.
