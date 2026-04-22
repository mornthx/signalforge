# M0 — Understanding

## 1. Restatement of the M0 goal

M0 lays down a buildable, CI-tested C++20 / Qt 6.10.2 / CMake 3.22+ skeleton
for SignalForge, plus the governance files (`CLAUDE.md`, editor configs,
`.gitignore`, `.clang-format`, `.clang-tidy`) that encode the project's
operating rules. The skeleton must match the module layering in
`[Arch §4.3]` and the source tree in `[Arch §18]`: thirteen module
directories under `src/` (app, platform, observability, drivers, frame,
decode, signal, action, session, ui_widgets, ui_quick, models, utils),
each with its own `CMakeLists.txt` and placeholder sources, plus a
`tests/` tree with a Catch2 smoke test. The application entry point
must open a 1280×800 window titled `"SignalForge"` and exit cleanly;
`src/observability/logging.{hpp,cpp}` must initialise spdlog and expose
the `SF_LOG_*` macros. Third-party dependencies
(spdlog, Catch2, moodycamel, ExprTk, yaml-cpp, nlohmann/json) are
pulled via `FetchContent` pinned to exact Git tags. A GitHub Actions
workflow runs Debug / Release / Debug+ASan builds and `ctest` on every
push to `milestone/**` or PR to `main`. **No business logic lands here** —
M1 through M11 build on top of this skeleton, so everything beyond the
empty window and the logging stub is out of scope.

## 2. Scope delta from the spec — none

I am treating the M0 spec (`docs/milestones/M0-project-bootstrap.md`)
as the source of truth for deliverables. I am not proposing to add,
remove, or reinterpret any "Must deliver" item, and I will skip none
of them. The "Optional" items in §2.3 are declined for this session
(see §6 below) and will be recorded as intentional skips in the
completion report.

## 3. Ambiguities and contradictions identified

For each, I state my default interpretation. If the human reviewer
disagrees, please correct during plan review — not mid-execution.

### 3.1 Commit-time preconditions vs. incremental scaffolding

`CLAUDE.md` §Required-2 says **every** commit must pass Debug and
Release builds, `ctest`, AddressSanitizer, and `clang-format`. M0 starts
from an empty repo state where nothing is buildable yet: the first few
subtasks (writing `CMakeLists.txt`, fetching dependencies, adding
module scaffolding, adding the logging library, adding `main.cpp`)
cannot individually produce a green build.

**Default interpretation**: the commit-time preconditions apply once
the tree reaches a buildable state. I will plan commits such that:
- Commits 1–2 (markdown / config files only, no C++) are commit-safe
  because they cannot affect the build graph.
- Commits 3+ (anything touching `CMakeLists.txt` or `.cpp/.hpp` files)
  are only made after `cmake --preset debug && cmake --build --preset debug`
  and `ctest --preset debug` both succeed from the tree state at that
  commit.
- This forces the CMake + module scaffolding + logging + main +
  smoke-test work to land in one consolidated "first buildable commit"
  rather than split across several red commits.

If the human instead wants strict literal enforcement (no commit until
the whole tree builds), I will squash the entire code landing into
one big commit — tell me.

### 3.2 Coverage target on placeholder modules

`CLAUDE.md` §Required-1 requires ≥ 70% line coverage on each module's
public surface. M0 modules (except `observability`) expose **no** public
surface — they contain only a placeholder namespace declaration and a
no-op source file.

**Default interpretation**: the coverage requirement applies once a
module acquires a real public surface. For M0, "zero public API ⇒
coverage vacuously satisfied" is how I will read it. I will *not*
stub fake API just to generate coverage numbers.

### 3.3 Branch protection on `main`

Spec §2.1 item 2 lists branch protection as a deliverable. Spec §7
step 5 assigns **initial** branch protection to the human pre-M0, and
§5.5 acceptance asks for "required status checks include all three CI
jobs" — which is only possible after the CI workflow has run at least
once on a PR.

**Default interpretation**: CC does not touch GitHub repository
settings. I will:
- Ensure the CI workflow exists and is syntactically valid.
- In the completion report, flag "add the three CI job names as
  required status checks on `main`" as the human's remaining step
  after the first green CI run, and name the checks as they will
  appear (see §3.7 below).

### 3.4 No git remote is configured yet

The working tree has one commit (`ffbf3f2 docs: seed baseline
architecture and milestone specs`) on `milestone/M0` and no remote.

**Default interpretation**: I will not run `git push`, `gh repo
create`, or similar — that is a human bootstrap step per §7 steps 1–5.
If by the end of execution no remote is configured, I will flag it in
the progress log so the human can `git remote add origin <url>` and
push `milestone/M0` before opening the M0 → `main` PR. The CI workflow
will simply not run until the push happens.

### 3.5 `CLAUDE.md` content source

No `CLAUDE.md` exists at the repo root yet. The spec's Appendix A
opening message lists `CLAUDE.md` as required reading *before* I
create it. The literal source for the file is `[EM §2]`, and that
is what I have read.

**Default interpretation**: for this session, `[EM §2]` **is** the
effective `CLAUDE.md`. One of my early subtasks (before any code
commit) copies it verbatim to `/CLAUDE.md`, exactly as in
`docs/claude-code/execution-manual.md` between the opening
```markdown fence and the closing ``` fence. No edits, no "improvements".

### 3.6 `exprtk` Git tag `0.0.3`

Spec Appendix C pins ExprTk to `GIT_TAG 0.0.3`. The Appendix D example
completion report even calls out that a HALT was raised and resolved
for this exact tag. ExprTk historically releases via Git commit, not
semver tags, so the tag may or may not exist upstream.

**Default interpretation**: I will use `GIT_TAG 0.0.3` exactly as
specified, and verify the tag resolves during the `FetchContent`
`--preset debug` configure step. If it fails, that is HALT condition
§6-3 of the M0 spec (`Any FetchContent dependency fails to fetch`) —
I will not silently substitute a commit hash. If the human already
knows this tag is invalid and wants a specific commit pinned, override
in the plan review and I will adjust.

### 3.7 CI job names vs. §5.5 "three CI jobs"

Spec §2.1 item 15 names three jobs: `debug-build`, `release-build`,
`asan-build`. The sample workflow in spec §4.6 uses
`strategy.matrix.preset: [debug, release, debug-asan]` under a single
job named `build`, which GitHub surfaces as `build (debug)`,
`build (release)`, `build (debug-asan)`.

**Default interpretation**: I will use the matrix form verbatim from
spec §4.6 — that is the more authoritative of the two, being explicit
YAML. When documenting "required status checks" for the human, I will
report the names as GitHub actually surfaces them
(`build (debug) / build (release) / build (debug-asan)`).

### 3.8 Release preset LTO

Spec §4.2 says "LTO enabled if GCC 12 supports it cleanly on the
target environment". This is a soft qualifier.

**Default interpretation**: I will enable
`CMAKE_INTERPROCEDURAL_OPTIMIZATION` in the `release` preset only
after `check_ipo_supported()` returns success for the current compiler;
otherwise it stays off with a status message. No tuning flags beyond
what CMake's built-in IPO support provides.

### 3.9 `src/app/` produces an executable, not a static library

Spec §2.1 item 6 states "each module directory contains a
`CMakeLists.txt` defining a static library target". But `src/app/`
has `main.cpp` and the spec's run-time acceptance requires an
executable. This is a minor wording conflict.

**Default interpretation**: every `src/<module>/CMakeLists.txt`
defines a static library target **except** `src/app/CMakeLists.txt`,
which defines the `signalforge` executable (via `qt_add_executable`
if Qt's helper is preferred, or `add_executable`). This matches the
intent of §4.1's top-level `CMakeLists.txt`.

### 3.10 Optional §2.3 items

All four optional items (`scripts/bootstrap.sh`, issue templates, PR
template, pre-commit hook suggestion) are declined for this M0. I will
list them as intentional skips in the completion report, per spec §8
note "Skipped because optional is sufficient".

## 4. HALT risks I anticipate

Ranked by likelihood, not severity.

1. **`find_package(Qt6 6.10 REQUIRED)` cannot locate Qt**. Ubuntu
   system Qt is too old and the spec forbids it anyway. I need Qt
   6.10.2 under `~/Qt/6.10.2/gcc_64/` or the `SIGNALFORGE_QT_PATH`
   env var pointing there. If neither resolves, this is M0-specific
   HALT §6-1. I will check early (before writing any CMake) by
   running `ls ~/Qt/6.10.2/gcc_64/lib/cmake/Qt6 2>&1` — if missing,
   HALT immediately and ask the human to install Qt.
2. **GCC 12 not installed on this host**. Default Ubuntu 22.04 ships
   GCC 11. I will check `which g++-12` early. If missing, this is
   M0-specific HALT §6-2; I will not fall back to GCC 11 silently.
3. **`FetchContent` network failure** (including `exprtk 0.0.3` tag
   resolution, see §3.6). M0-specific HALT §6-3.
4. **ASan noise from Qt**. Qt's internal code occasionally triggers
   ASan warnings on early window show/close. The `debug-asan` preset
   enables UBSan too. M0-specific HALT §6-4: any leak / use-after-free
   — including Qt-originated — is a HALT, not an auto-applied
   suppression. I will run `xvfb-run ./signalforge &` under ASan
   briefly to catch this before committing.
5. **`clang-format --dry-run -Werror` fails on the first commit**
   because of style drift between what I write and Appendix A's
   LLVM-based config. Not a HALT per se — I will re-format once
   and re-run. Three such rounds on the same file trigger general
   HALT rule "edit-loop detection".
6. **Branch protection / GitHub push not configured**. Not a HALT;
   see §3.3 — this is a human post-M0 step.
7. **xvfb not installed locally for the main-window smoke test**.
   CI installs it via apt; the local machine may not have it. I will
   check and, if missing, skip the local run-the-binary check and
   rely on CI — *not* a HALT, but documented in the progress log so
   the human can run the binary manually.

## 5. Things I am explicitly **not** doing in M0

- No Driver implementations (reserved for M3)
- No Frame / Decode / Signal / Action / Session / Chart code
- No Crashpad integration (M2)
- No Qt Quick usage beyond the `QuickWidgets` find_package dependency
  (first real usage arrives in M1's spike, under `tools/spike/`, which
  M0 does not create)
- No packaging scripts (M11)
- No performance work
- No `docs/future/scenario-design-note.md` or other future docs

## 6. Review checkpoints I expect

- After this understanding + plan are approved, I execute the plan
  (§3.3 of the Execution Manual).
- During execution, any of the M0-specific HALT triggers (§6 of the
  M0 spec) or general HALT triggers (`CLAUDE.md`) stops me
  immediately; I write `.claude/halt/HALT-<UTC>-<slug>.md`, commit
  what's safe, and exit.
- At the end, I write `.claude/M0-done.md` and reply with a
  completion summary. The human runs the acceptance checklist
  (§5 of the M0 spec and `[EM §5]`).
