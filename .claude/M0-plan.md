# M0 — Execution Plan

## Conventions

- Each entry is a **subtask**, not an edit. Subtasks may touch many
  files. A subtask ends with a single commit when it reaches a state
  that satisfies `CLAUDE.md` §Required-2 (build + tests + ASan +
  `clang-format` all green). Subtasks that only touch markdown / YAML
  / dotfiles (no effect on the build graph) commit freely.
- Effort estimates are wall-clock CC time, not human review time.
- "Prereq" refers to other plan subtasks, not to already-seeded docs.

## §9 step 8 — already executed (out of plan)

Commit `a9aa6dc docs: add CLAUDE.md verbatim from execution manual §2`
is `CLAUDE.md` at the repo root, byte-exact against
`docs/claude-code/execution-manual.md` lines 74–185 (verified via
`diff`). This was a missing bootstrap step per EM §9 step 8, not an
M0 deliverable; S1 below is adjusted accordingly.

## Preflight — executes once, before subtask 1

Before writing any file, I verify these local conditions:

| Check | Command | Action on failure |
|---|---|---|
| Qt 6.10.2 installed | `ls ~/Qt/6.10.2/gcc_64/lib/cmake/Qt6/Qt6Config.cmake` (or via `SIGNALFORGE_QT_PATH`) | HALT M0 §6-1 |
| GCC 12 installed | `g++-12 --version` | HALT M0 §6-2 |
| Ninja installed | `ninja --version` | Install via apt if sudo is permitted without password; else HALT (document as general HALT "git operation / tooling unavailable") |
| `clang-format` installed | `clang-format --version` | Same as Ninja |
| `clang-tidy` installed | `clang-tidy --version` | Same as Ninja |
| Working branch | `git rev-parse --abbrev-ref HEAD` == `milestone/M0` | Stop, tell human |
| Remote origin | `git remote -v` | Not a failure — documented per Understanding §3.4 |

Preflight findings are logged to `.claude/M0-progress.md`. No commit
yet.

## Subtasks

Twelve ordered subtasks. All 8–15 per Execution Manual §3.3.

### S1 — Repo governance files (no-build commit)

`CLAUDE.md` was landed separately per §9 step 8 (see preamble);
S1 covers the remaining governance files only.

**Files created**:
- `README.md` — one paragraph (project one-liner, platform pin),
  plus a three-command build:
  ```
  cmake --preset debug
  cmake --build --preset debug
  ctest --preset debug
  ```
- `CONTRIBUTING.md` — short; "see `CLAUDE.md` for project rules"
  plus a pointer to `docs/claude-code/execution-manual.md`.
- `LICENSE` — placeholder. Single line noting legal review pending.
  Per spec §2.1-16 I do not choose a license.

**Prereq**: preflight passed.
**Effort**: ~15 min.
**Commit point**: 🟢 yes — markdown-only, no build effect.
Message: `docs: add CLAUDE.md and root governance files`.

### S2 — Editor / tooling configs (no-build commit)

**Files created**:
- `.editorconfig` — UTF-8, LF, 4 spaces. (Spec §2.1-13.)
- `.clang-format` — verbatim from spec Appendix A.
- `.clang-tidy` — verbatim from spec Appendix B.
- `.gitignore` — spec §4.7 content.
- `.claude/.gitignore` — spec §4.8 content.

**Prereq**: S1.
**Effort**: ~10 min.
**Commit point**: 🟢 yes — dotfiles only. `clang-format --dry-run`
on changed files is vacuous (no C++ files touched).
Message: `chore: add editorconfig, clang-format, clang-tidy, gitignore`.

### S3 — Top-level CMake + dependencies + presets

**Files created**:
- `CMakeLists.txt` — the minimum content from spec §4.1 verbatim
  (including `add_subdirectory` lines for every `src/<module>` and
  for `tests`).
- `cmake/dependencies.cmake` — the `FetchContent` block from spec
  Appendix C verbatim, with the tag versions as-specified. If the
  `FetchContent` configure step later fails, HALT per M0 §6-3.
- `CMakePresets.json` — three build presets + three test presets:
  - `debug`: `Debug`, no sanitizers.
  - `release`: `Release`, LTO gated behind `check_ipo_supported`.
  - `debug-asan`: `Debug` + `-fsanitize=address,undefined
    -fno-omit-frame-pointer`.
  - All presets use Ninja and export `compile_commands.json`.
  - `cacheVariables.CMAKE_PREFIX_PATH` uses
    `$env{SIGNALFORGE_QT_PATH}` with a sensible default
    (`$env{HOME}/Qt/6.10.2/gcc_64`) — no hardcoded absolute path.

**Prereq**: S2.
**Effort**: ~45 min.
**Commit point**: 🔴 no — not yet buildable (modules don't exist).
Deferred to S6.

### S4 — Module scaffolding (12 placeholder modules)

**Files created** (per spec §3 tree):

For each of `platform, drivers, frame, decode, signal, action,
session, ui_widgets, ui_quick, models, utils` (11 modules):
- `src/<mod>/CMakeLists.txt` — defines `signalforge_<mod>` as a
  static library with `placeholder.cpp`, linking any Qt components
  it will eventually need (kept minimal here: Qt::Core).
- `src/<mod>/placeholder.hpp` — `#pragma once`, `namespace
  signalforge::<mod> { /* TODO M<n> */ }` where `<n>` is the
  milestone that first fleshes out the module. `ui_quick` additionally
  links `Qt::QuickWidgets` because that was the point of finding it.
- `src/<mod>/placeholder.cpp` — empty TU referencing the header so the
  compiler emits an object file. A single no-op inline translation
  unit satisfies "some generators reject header-only libraries".

The `observability` module is scaffolded with `CMakeLists.txt` only
at this point (its `.hpp`/`.cpp` come from S5). The `app` module's
`CMakeLists.txt` + sources come from S6.

**Prereq**: S3.
**Effort**: ~30 min.
**Commit point**: 🔴 no — top-level `add_subdirectory(src/app)` still
dangles. Deferred to S6.

### S5 — Observability (logging) implementation

**Files created**:
- `src/observability/CMakeLists.txt` — `signalforge_observability`
  static library, linked against `spdlog::spdlog` from `FetchContent`.
- `src/observability/logging.hpp` — verbatim from spec §4.3.
- `src/observability/logging.cpp` — `init_logging()` implementation:
  - Resolve log dir: `$XDG_STATE_HOME/signalforge/logs/` if set,
    else `$HOME/.local/state/signalforge/logs/`.
  - Create directory if absent (`std::filesystem::create_directories`).
  - `spdlog::init_thread_pool(8192, 1)` for async.
  - Rotating file sink at 10 MB × 10 files; JSON-lines formatter with
    fields `ts, level, thread, module, event, fields`.
  - Default level: `info`; overridden by `SIGNALFORGE_LOG_LEVEL` env
    var if set (`trace|debug|info|warn|error`).
  - Idempotent: guarded by a `std::once_flag`.
  - No `std::cout` / `printf` / `qDebug` anywhere.

**Prereq**: S4.
**Effort**: ~40 min.
**Commit point**: 🔴 no — `main.cpp` consumes this; still not a
full build. Deferred to S6.

### S6 — App entry and main window (**first buildable state**)

**Files created**:
- `src/app/CMakeLists.txt` — defines the `signalforge` **executable**
  (see Understanding §3.9 for the app-is-exe interpretation). Links
  against `Qt6::Widgets`, `signalforge_observability`. `WIN32_EXECUTABLE`
  not applicable on Linux.
- `src/app/main.cpp` — verbatim from spec §4.4.
- `src/app/main_window.hpp` — `class MainWindow : public QMainWindow`
  in `namespace signalforge::app`. Header contains only the
  declaration.
- `src/app/main_window.cpp` — constructor sets the title to
  `"SignalForge"`, `resize(1280, 800)`. Nothing else.

**Prereq**: S3, S4, S5.
**Effort**: ~30 min.
**Commit point**: 🟢 yes — **this is the first commit where all
`CLAUDE.md` §Required-2 preconditions are verifiable**. Before
committing, I run:
```
cmake --preset debug && cmake --build --preset debug
ctest --preset debug --output-on-failure   # no tests yet; should pass trivially with ctest's "no tests" path; if ctest errors on empty set, add a placeholder test here instead of in S7
cmake --preset release && cmake --build --preset release
cmake --preset debug-asan && cmake --build --preset debug-asan
xvfb-run --auto-servernum ./build/debug-asan/src/app/signalforge &
sleep 2; kill %1
clang-format --dry-run -Werror <changed C++ files>
```
All must pass. Commits S3 + S4 + S5 + S6 collapse into one
"build: establish buildable CMake skeleton with logging and main
window" commit.

### S7 — Test harness (Catch2 smoke test)

**Files created**:
- `tests/CMakeLists.txt` — `add_subdirectory(unit)`.
- `tests/unit/CMakeLists.txt` — defines `smoke_test` executable,
  links `Catch2::Catch2WithMain` and any library smoke needs;
  `catch_discover_tests(smoke_test)`.
- `tests/unit/smoke_test.cpp` — verbatim from spec §4.5.

**Prereq**: S6.
**Effort**: ~15 min.
**Commit point**: 🟢 yes. All three `ctest --preset <name>` must now
report ≥ 2 passing tests.
Message: `test: add Catch2 smoke test and wire ctest`.

### S8 — Empty directory scaffolds

**Files created**:
- `tools/.gitkeep`
- `examples/.gitkeep`
- `resources/.gitkeep`
- `packaging/.gitkeep`
- `ci/.gitkeep`

**Prereq**: S7.
**Effort**: ~5 min.
**Commit point**: 🟢 yes — no build effect.
Message: `chore: reserve tools/examples/resources/packaging/ci dirs`.

### S9 — GitHub Actions CI workflow

**Files created**:
- `.github/workflows/ci.yml` — verbatim from spec §4.6.

I will run a local YAML-syntax check (`python -c "import yaml;
yaml.safe_load(open('.github/workflows/ci.yml'))"` or equivalent)
before committing, since the spec §5.5 acceptance asks for syntactic
validity.

**Prereq**: S8.
**Effort**: ~10 min.
**Commit point**: 🟢 yes.
Message: `ci: add GitHub Actions workflow for debug/release/asan`.

### S10 — Local verification pass

No file writes. Action:
1. `cmake --preset debug && cmake --build --preset debug && ctest --preset debug --output-on-failure`
2. Same for `release` and `debug-asan`. ASan output scanned for any
   leak / use-after-free — any report triggers M0 §6-4 HALT.
3. `xvfb-run ./build/release/src/app/signalforge` — window opens,
   verify title and geometry via `xdotool` if available (nice to have;
   fall back to "launches without crashing and exits 0 on close").
4. Log file inspection: confirm a JSON-lines file appears at the
   resolved log directory containing the two expected `INFO` lines
   ("SignalForge starting" and "SignalForge exiting, rc=0").
5. `clang-format --dry-run -Werror` on all `src/**/*.{cpp,hpp}`.

All results recorded in `.claude/M0-progress.md` with command output
snippets.

**Prereq**: S9.
**Effort**: ~30 min (includes build time for three presets).
**Commit point**: 🔴 typically no — only if a defect is found and
fixed, committing as its own `fix:` commit. A clean pass produces no
commit.

### S11 — Completion report

**Files created**:
- `.claude/M0-done.md` — follows `[EM §6.2]` template plus the
  customization shown in spec Appendix D.

Covers:
- Timing (start / end in UTC).
- Deliverables checklist against spec §2.1.
- Self-check against `[EM §5]` acceptance items.
- Test results (count, pass, ASan-clean confirmation).
- HALTs raised and resolved (expected: zero if nothing went wrong).
- Deviations and concerns — anything that ended up in
  `.claude/M0-concerns.md`, including any ambiguity from the
  Understanding doc that needed confirmation during execution.
- Freezes: **none**. M0 freezes nothing.
- Open issues: LICENSE is a placeholder; optional §2.3 items
  intentionally skipped; branch protection CI checks for the human
  to add after first green run.
- Commit hash list.
- Suggestions for M1 (should be empty unless something surfaced).

**Prereq**: S10 clean.
**Effort**: ~30 min.
**Commit point**: 🟢 yes — markdown-only.
Message: `docs: file M0 completion report`.

### S12 — Hand-off checklist to the human (no commit)

Report via chat:
- "M0 complete; acceptance ready for review."
- Git state: branch `milestone/M0`, N commits ahead of `main`.
- Remote state: present / absent (see Understanding §3.4). If
  absent, list the commands the human should run to push.
- CI state: workflow file present but not yet run (runs on first push).
- Outstanding human actions:
  1. Push `milestone/M0` to GitHub (if remote was absent).
  2. Wait for first CI run; confirm all three jobs green.
  3. Add `build (debug)`, `build (release)`, `build (debug-asan)`
     as required status checks on `main` branch protection.
  4. Run `[EM §5]` acceptance checklist.
  5. Open PR `milestone/M0 → main`, merge, tag `v0.0.1-alpha.1`.

**Prereq**: S11.
**Effort**: ~5 min.
**Commit point**: none.

## Commit layout summary

Expected commit history on `milestone/M0` after M0 closes (below
the existing `ffbf3f2 docs: seed baseline architecture and milestone
specs`):

1. `docs: add CLAUDE.md and root governance files` (S1)
2. `chore: add editorconfig, clang-format, clang-tidy, gitignore` (S2)
3. `build: establish buildable CMake skeleton with logging and main window` (S3+S4+S5+S6 bundled — see Understanding §3.1)
4. `test: add Catch2 smoke test and wire ctest` (S7)
5. `chore: reserve tools/examples/resources/packaging/ci dirs` (S8)
6. `ci: add GitHub Actions workflow for debug/release/asan` (S9)
7. `docs: file M0 completion report` (S11)
   *(plus `.claude/M0-understanding.md`, `.claude/M0-plan.md`,
   `.claude/M0-progress.md` — included in the first commit after
   approval or as a standalone `chore: record M0 understanding and
   plan` commit, whichever the human prefers; default is standalone
   immediately after approval.)*

That is **seven** net commits plus optionally an eighth for the
`.claude/` workspace bootstrap. If S10 surfaces a defect, an
extra `fix: ...` commit lands between S9 and S11.

## Total effort estimate

~4 hours of CC wall-clock time including builds, plus human review
gates. The spec's 5 person-day estimate absorbs the understanding /
review overhead and any HALT handling.

## Out-of-band interventions I may request

- Qt installation (if preflight fails).
- GCC 12 / Ninja / clang-format / clang-tidy install (same).
- GitHub remote creation and branch-protection policy (§3.3, §3.4).
- Resolution of any HALT that fires during S3 (FetchContent tag
  issues for ExprTk, see Understanding §3.6).
