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
9. **Every session's first action is state observation before planning**. Run `git status`, `git fetch origin --prune`, `git log` on relevant branches, and compare observations with the prompt's assumptions. Any divergence is reported to the human before proceeding with `understanding.md` or `plan.md`. No exceptions.

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

**Exception — additive extensions without HALT**. CC may autonomously extend the vocabulary of the report or documentation it produces, without HALT, when all three of:

- The extension is additive (new verdict category, new field, new subsection) — not changing existing semantics
- The extension does not introduce new API surface, new dependencies, or new file types
- The extension is documented in the milestone's `done.md` under "Deviations and concerns"

Typical examples: adding a `Nuanced` verdict alongside the spec's 4-state vocabulary; adding a new section to a report that the spec's template did not foresee.

Everything else follows the default HALT rule.

## Disagreement handling

If you believe a spec clause is wrong or suboptimal, record your concern in `.claude/M<n>-concerns.md` with a proposed alternative, **and then execute the spec as written**. Do not deviate from the spec without explicit human approval. Concerns are addressed at milestone review.

## Tooling

- Build: CMake + `FetchContent`. Do not switch to system packages.
- Editor config: `.editorconfig` is locked; do not modify.
- Formatter: `clang-format` with config at `.clang-format`; do not modify.
- Static analysis: `clang-tidy` with config at `.clang-tidy`. Disabled checks in the initial config may be re-enabled when you can make CI green with the enabled rule; enabling a check requires fixing all existing violations in the same commit. Relaxing an already-enabled check is forbidden.
- Build directory: `build/` only, with one subdirectory per preset.

## Environment conventions

- Qt install path: `$SIGNALFORGE_QT_PATH` (falls back to `~/Qt/6.10.2/gcc_64`).
- Log directory: `$XDG_STATE_HOME/signalforge/logs/`, defaulting to `~/.local/state/signalforge/logs/`.
- Build directory: `build/<preset>` under the repo root.
- `.claude/` is committed to git; `.claude/halt/*.inprogress` is ignored.

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
