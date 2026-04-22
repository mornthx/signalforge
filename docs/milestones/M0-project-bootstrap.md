# M0 — Project Bootstrap

**Milestone ID**: M0
**Sprint**: 1 (week 1)
**Estimated effort**: 5 person-days
**Prerequisites**: none (starting point)
**Next milestone**: M1
**Hard-stop type**: Structural review
**Git hosting**: GitHub
**CI platform**: GitHub Actions

**Cross-reference notation**:

- `[EM §N]` — Execution Manual, section N
- `[Arch §N]` — Architecture document, section N
- `[MR M<n>]` — Milestone Roadmap, entry for milestone `M<n>`

---

## 1. Goal

Establish a C++ / Qt / CMake project skeleton on which every subsequent milestone can build, plus the governance files that encode the project's rules.

---

## 2. Scope

### 2.1 Must deliver

1. Git repository initialized and pushed to GitHub (visibility is the human's choice; private by default is recommended).
2. `main` branch protection configured on GitHub: pull request required, no force push, no direct push.
3. Top-level CMake project supporting Qt 6.10.2 + GCC 13 + Ninja.
4. `CMakePresets.json` defining `debug`, `release`, `debug-asan` build presets, with matching `testPresets`.
5. Module directory layout per `[Arch §18]`.
6. Each module directory contains:
   - `CMakeLists.txt` defining a static library target
   - A placeholder header with only `namespace` declaration and a `// TODO M<n>` marker
   - A placeholder source file (compiles to an empty object; required because some CMake generators reject header-only libraries)
7. Third-party dependencies fetched via CMake `FetchContent`:
   - spdlog
   - Catch2
   - moodycamel::ConcurrentQueue
   - ExprTk
   - yaml-cpp
   - nlohmann/json
   - All pinned to exact Git tags; see Appendix C for reference versions.
8. `src/app/main.cpp` + `src/app/main_window.{hpp,cpp}`: an empty `QMainWindow` that opens, displays a blank 1280 × 800 window titled "SignalForge", and exits cleanly on close.
9. `src/observability/logging.{hpp,cpp}`: spdlog initialization plus the `SF_LOG_*` macros. Full specification in §4.3.
10. `tests/unit/smoke_test.cpp`: a minimal Catch2 test that verifies the test harness works.
11. `.clang-format` — see Appendix A.
12. `.clang-tidy` — see Appendix B.
13. `.editorconfig` — UTF-8, LF line endings, 4-space indentation.
14. `.gitignore` — Qt, CMake, and IDE standard entries, plus `build/` and unresolved HALT files under `.claude/halt/`.
15. GitHub Actions workflow at `.github/workflows/ci.yml` with three jobs on `ubuntu-24.04`:
    - `debug-build` — configure `debug` preset, build, run `ctest`
    - `release-build` — configure `release` preset, build, run `ctest`
    - `asan-build` — configure `debug-asan` preset, build, run `ctest`
    - Each job caches the Qt installation.
16. Root-level governance files:
    - `README.md` — one paragraph describing the project, plus a three-command build example
    - `CLAUDE.md` — verbatim copy of `[EM §2]`
    - `CONTRIBUTING.md` — short; points to `CLAUDE.md`
    - `LICENSE` — placeholder only. Do not choose a license; humans decide.
17. Documents seeded into `docs/` by the human before M0 starts (CC verifies presence):
    - `docs/architecture/architecture.md`
    - `docs/milestones/milestone-roadmap.md`
    - `docs/milestones/M0-project-bootstrap.md` (this file)
    - `docs/claude-code/execution-manual.md`
18. `.claude/` directory with its own `.gitignore`. Tracked files: `M<n>-understanding.md`, `M<n>-plan.md`, `M<n>-progress.md`, `M<n>-done.md`, `M<n>-concerns.md`, `M<n>-overrides.md`, and resolved HALT reports under `.claude/halt/`. Ignored: in-progress HALT files and transient working files.

### 2.2 Must not do

1. **No business logic.** Every module is a skeleton.
2. **No Driver implementations.** Those belong to M3.
3. **No Frame / Decode / Signal implementations.** Those belong to M4 and M5.
4. **No Crashpad integration.** That belongs to M2.
5. **No UI beyond the empty main window.** Subsequent milestones add features.
6. **No dependencies outside the list in `[Arch §4.1]`.**
7. **No packaging work.** That belongs to M11.
8. **No speculative code "for future milestones".** A skeleton is a skeleton.
9. **No CI deployment steps** — no artifact upload, no release creation. Build and test only.

### 2.3 Optional (CC's discretion; does not affect acceptance)

1. `scripts/bootstrap.sh` — helper for new contributors to fetch dependencies and configure CMake.
2. `.github/ISSUE_TEMPLATE/` — basic issue templates.
3. `.github/pull_request_template.md` — PR template that references `CLAUDE.md`.
4. A pre-commit hook suggestion in `CONTRIBUTING.md`. Suggest only; do not auto-install.

---

## 3. Target Directory Structure

After M0 completes, the repository should look like this:

```
signalforge/
├── CLAUDE.md
├── CMakeLists.txt
├── CMakePresets.json
├── CONTRIBUTING.md
├── LICENSE
├── README.md
├── .clang-format
├── .clang-tidy
├── .editorconfig
├── .gitignore
├── .claude/
│   └── .gitignore
├── .github/
│   └── workflows/
│       └── ci.yml
├── cmake/
│   └── dependencies.cmake
├── docs/
│   ├── architecture/
│   │   └── architecture.md
│   ├── milestones/
│   │   ├── milestone-roadmap.md
│   │   └── M0-project-bootstrap.md
│   └── claude-code/
│       └── execution-manual.md
├── src/
│   ├── app/
│   │   ├── CMakeLists.txt
│   │   ├── main.cpp
│   │   ├── main_window.hpp
│   │   └── main_window.cpp
│   ├── platform/
│   │   ├── CMakeLists.txt
│   │   ├── placeholder.hpp
│   │   └── placeholder.cpp
│   ├── observability/
│   │   ├── CMakeLists.txt
│   │   ├── logging.hpp
│   │   └── logging.cpp
│   ├── drivers/
│   │   ├── CMakeLists.txt
│   │   ├── placeholder.hpp
│   │   └── placeholder.cpp
│   ├── frame/
│   │   ├── CMakeLists.txt
│   │   ├── placeholder.hpp
│   │   └── placeholder.cpp
│   ├── decode/
│   │   ├── CMakeLists.txt
│   │   ├── placeholder.hpp
│   │   └── placeholder.cpp
│   ├── signal/
│   │   ├── CMakeLists.txt
│   │   ├── placeholder.hpp
│   │   └── placeholder.cpp
│   ├── action/
│   │   ├── CMakeLists.txt
│   │   ├── placeholder.hpp
│   │   └── placeholder.cpp
│   ├── session/
│   │   ├── CMakeLists.txt
│   │   ├── placeholder.hpp
│   │   └── placeholder.cpp
│   ├── ui_widgets/
│   │   ├── CMakeLists.txt
│   │   ├── placeholder.hpp
│   │   └── placeholder.cpp
│   ├── ui_quick/
│   │   ├── CMakeLists.txt
│   │   ├── placeholder.hpp
│   │   └── placeholder.cpp
│   ├── models/
│   │   ├── CMakeLists.txt
│   │   ├── placeholder.hpp
│   │   └── placeholder.cpp
│   └── utils/
│       ├── CMakeLists.txt
│       ├── placeholder.hpp
│       └── placeholder.cpp
├── tests/
│   ├── CMakeLists.txt
│   └── unit/
│       ├── CMakeLists.txt
│       └── smoke_test.cpp
├── tools/
│   └── .gitkeep
├── examples/
│   └── .gitkeep
├── resources/
│   └── .gitkeep
├── packaging/
│   └── .gitkeep
└── ci/
    └── .gitkeep
```

---

## 4. Key Implementation Details

### 4.1 Top-level CMakeLists.txt

Minimum required content:

```cmake
cmake_minimum_required(VERSION 3.22)
project(SignalForge VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.10 REQUIRED COMPONENTS
    Core Widgets Quick QuickWidgets Network SerialPort Test
)
qt_standard_project_setup()

include(cmake/dependencies.cmake)

enable_testing()

add_subdirectory(src/platform)
add_subdirectory(src/observability)
add_subdirectory(src/drivers)
add_subdirectory(src/frame)
add_subdirectory(src/decode)
add_subdirectory(src/signal)
add_subdirectory(src/action)
add_subdirectory(src/session)
add_subdirectory(src/models)
add_subdirectory(src/utils)
add_subdirectory(src/ui_widgets)
add_subdirectory(src/ui_quick)
add_subdirectory(src/app)
add_subdirectory(tests)
```

All `FetchContent` logic lives in `cmake/dependencies.cmake`. The top-level `CMakeLists.txt` only includes it.

### 4.2 CMakePresets.json

Three build presets, all using Ninja and exporting `compile_commands.json`:

- `debug` — `CMAKE_BUILD_TYPE=Debug`, no sanitizers.
- `release` — `CMAKE_BUILD_TYPE=Release`, LTO enabled if GCC 13 supports it cleanly on the target environment.
- `debug-asan` — `CMAKE_BUILD_TYPE=Debug` with `-fsanitize=address,undefined` and `-fno-omit-frame-pointer`.

`CMAKE_PREFIX_PATH` defaults to the conventional Qt install location (under the user's home) and can be overridden via the `SIGNALFORGE_QT_PATH` environment variable. **Do not hardcode any absolute path.**

Matching `testPresets` entries must exist so that `ctest --preset <name>` works for each build preset.

### 4.3 logging.hpp

```cpp
// src/observability/logging.hpp
#pragma once
#include <spdlog/spdlog.h>

namespace signalforge::observability {

/// Initialize the async rotating-file logger. Idempotent.
/// Log directory: $XDG_STATE_HOME/signalforge/logs/, falling back to
/// ~/.local/state/signalforge/logs/ when XDG_STATE_HOME is unset.
/// Format: JSON lines. Fields: ts, level, thread, module, event, fields.
/// Rotation: 10 MB per file, 10 files retained.
/// Default level: info. Override via the SIGNALFORGE_LOG_LEVEL env var.
void init_logging();

}  // namespace signalforge::observability

#define SF_LOG_TRACE(...) SPDLOG_TRACE(__VA_ARGS__)
#define SF_LOG_DEBUG(...) SPDLOG_DEBUG(__VA_ARGS__)
#define SF_LOG_INFO(...)  SPDLOG_INFO(__VA_ARGS__)
#define SF_LOG_WARN(...)  SPDLOG_WARN(__VA_ARGS__)
#define SF_LOG_ERROR(...) SPDLOG_ERROR(__VA_ARGS__)
```

### 4.4 main.cpp

```cpp
// src/app/main.cpp
#include <QApplication>
#include "main_window.hpp"
#include "observability/logging.hpp"

int main(int argc, char** argv) {
    signalforge::observability::init_logging();
    SF_LOG_INFO("SignalForge starting");

    QApplication app(argc, argv);
    signalforge::app::MainWindow window;
    window.show();
    const int rc = app.exec();

    SF_LOG_INFO("SignalForge exiting, rc={}", rc);
    return rc;
}
```

`MainWindow` sets the window title to `"SignalForge"` and resizes to 1280 × 800. Nothing else.

### 4.5 smoke_test.cpp

```cpp
// tests/unit/smoke_test.cpp
#include <catch2/catch_test_macros.hpp>

TEST_CASE("smoke: Catch2 works", "[smoke]") {
    REQUIRE(1 + 1 == 2);
}

TEST_CASE("smoke: C++20 lambdas compile", "[smoke]") {
    constexpr auto sum = [](auto a, auto b) { return a + b; };
    STATIC_REQUIRE(sum(1, 2) == 3);
}
```

### 4.6 GitHub Actions workflow

The file `.github/workflows/ci.yml` has the following skeleton:

```yaml
name: CI

on:
  push:
    branches: [main, "milestone/**"]
  pull_request:
    branches: [main]

jobs:
  build:
    strategy:
      matrix:
        preset: [debug, release, debug-asan]
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4

      - name: Install GCC 13
        run: |
          sudo apt-get update
          sudo apt-get install -y g++-13 ninja-build xvfb

      - name: Install Qt 6.10.2
        uses: jurplel/install-qt-action@v4
        with:
          version: '6.10.2'
          host: 'linux'
          target: 'desktop'
          arch: 'linux_gcc_64'
          modules: 'qtserialport'
          cache: true

      - name: Configure
        env:
          CC: gcc-13
          CXX: g++-13
        run: cmake --preset ${{ matrix.preset }}

      - name: Build
        run: cmake --build --preset ${{ matrix.preset }}

      - name: Test
        run: xvfb-run --auto-servernum ctest --preset ${{ matrix.preset }} --output-on-failure
```

**Platform notes for the workflow**:

- The `ubuntu-24.04` GitHub Actions runner ships with GCC 13 by default. The workflow reinstalls `g++-13` explicitly for determinism alongside `ninja-build` and `xvfb`.
- `xvfb` is required because `QApplication` construction attempts to connect to a display even for non-visible tests.
- The `jurplel/install-qt-action` action with `cache: true` caches the Qt install across CI runs.
- `ctest --preset <name>` requires a `testPresets` section in `CMakePresets.json`. That section must be added alongside the build presets.

### 4.7 .gitignore highlights

```
# Build
build/
build-*/

# CMake
CMakeUserPresets.json
CMakeCache.txt
CMakeFiles/

# Qt
*.qmlc
*.jsc
*.autosave

# IDE
.vscode/
.idea/
*.user
.cache/
compile_commands.json

# OS
.DS_Store
Thumbs.db

# Claude Code: ignore in-progress HALT files; resolved ones are committed
.claude/halt/*.inprogress
```

### 4.8 .claude/.gitignore

```
# Ignore transient working files; keep lifecycle artifacts tracked
*.tmp
halt/*.inprogress

# Explicit allowlist for tracked files
!*-understanding.md
!*-plan.md
!*-progress.md
!*-done.md
!*-concerns.md
!*-overrides.md
!halt/HALT-*.md
```

---

## 5. Acceptance Criteria

Human reviewers use the checklists below. CC self-verifies them before writing the completion report.

### 5.1 Build and test (CC self-verification plus CI proof)

- [ ] `cmake --preset debug && cmake --build --preset debug` succeeds with zero warnings
- [ ] `cmake --preset release && cmake --build --preset release` succeeds
- [ ] `cmake --preset debug-asan && cmake --build --preset debug-asan` succeeds
- [ ] `ctest --preset debug` passes
- [ ] `ctest --preset release` passes
- [ ] `ctest --preset debug-asan` passes with no ASan or UBSan reports
- [ ] The Release build produces an executable that launches, shows a 1280 × 800 window titled "SignalForge", and exits with code 0 when closed
- [ ] A log file appears under `$XDG_STATE_HOME/signalforge/logs/` (or `~/.local/state/signalforge/logs/`) containing JSON lines
- [ ] The GitHub Actions workflow has run at least once with all three jobs green

### 5.2 Structure and constraints

- [ ] Directory layout matches §3 exactly
- [ ] `CLAUDE.md` is a verbatim copy of `[EM §2]` — no edits, no "improvements"
- [ ] Dependency list matches §2.1 item 7 exactly
- [ ] Every module has only placeholder content (no business logic has leaked in)
- [ ] `.clang-format`, `.clang-tidy`, `.editorconfig` present and reasonable
- [ ] `CMakePresets.json` contains all three build presets and all three test presets

### 5.3 Documentation

- [ ] `README.md` is readable and includes a three-command build
- [ ] `CONTRIBUTING.md` points to `CLAUDE.md`
- [ ] `docs/architecture/architecture.md` is present
- [ ] `docs/milestones/M0-project-bootstrap.md` is present (this file)
- [ ] `docs/milestones/milestone-roadmap.md` is present
- [ ] `docs/claude-code/execution-manual.md` is present

### 5.4 Process

- [ ] `.claude/M0-understanding.md`, `.claude/M0-plan.md`, `.claude/M0-done.md` exist and are substantive
- [ ] Commit history is clean: one commit per meaningful subtask, no `WIP` or `fixup` residue
- [ ] No unresolved HALT files under `.claude/halt/`; any HALTs that occurred are documented as resolved in the completion report

### 5.5 CI

- [ ] `.github/workflows/ci.yml` is syntactically valid
- [ ] Branch protection is enabled on `main`: required status checks include all three CI jobs
- [ ] At least one complete successful CI run is visible in the GitHub Actions tab

---

## 6. M0-specific HALT Triggers

These are in addition to the general HALT triggers in `CLAUDE.md`:

1. **Qt 6.10.2 cannot be found via `find_package`**. HALT and report. Do not manually patch `CMAKE_PREFIX_PATH` with an absolute path.
2. **GCC 13 is unavailable and the GCC 11.4 fallback from `[Arch §12.2]` fails to compile the required C++20 features**. HALT and await direction.
3. **Any `FetchContent` dependency fails to fetch**. HALT. Do not switch to system packages.
4. **The `debug-asan` preset reports any leak or use-after-free, including issues that appear to originate in Qt**. HALT and let the human decide on suppressions.
5. **Branch protection configuration on GitHub is refused** (permissions, repository visibility, or similar). HALT and report.

---

## 7. Session Startup (what the human does)

The first CC session on M0 follows this order:

1. The human creates the GitHub repository. Empty; private by default is recommended.
2. The human clones the repository to their local workspace.
3. The human adds the four seed documents to `docs/`:
   - `architecture/architecture.md`
   - `milestones/M0-project-bootstrap.md` (this file)
   - `milestones/milestone-roadmap.md`
   - `claude-code/execution-manual.md`
4. The human commits these seed documents to `main` and pushes.
5. The human enables branch protection on `main` (require PR, no force push).
6. The human creates `milestone/M0` locally: `git checkout -b milestone/M0`.
7. The human launches CC in the repository directory and sends the opening message from `[EM Appendix A]`, adjusting the absolute workspace path in the message to match the local checkout.
8. CC reads the documents, writes `.claude/M0-understanding.md` and `.claude/M0-plan.md`, and stops.
9. The human reviews the understanding and plan, then either approves or requests revisions.
10. On approval, CC executes M0.
11. CC writes `.claude/M0-done.md` when finished.
12. The human runs the acceptance checklist in §5.
13. On full acceptance, the human opens a pull request from `milestone/M0` to `main`, merges it, and tags `v0.0.1-alpha.1`.

M0 is now closed. Proceed to M1 setup.

---

## 8. Notes for CC

- **Do not guess build configuration.** If `find_package(Qt6)` fails, write a HALT report; do not patch `CMAKE_PREFIX_PATH` with a hardcoded path.
- **Do not collapse module `CMakeLists.txt` files into the root.** One `CMakeLists.txt` per module is an architectural requirement, not a preference.
- **Pin `FetchContent` versions by exact Git tag.** Never use `main` or `master`.
- **Copy `CLAUDE.md` verbatim** from `[EM §2]`. Resist the urge to rewrite or "improve" it. If you see what looks like a typo, record it in `.claude/M0-concerns.md`. Do not fix silently.
- **The first session writes no code.** The understanding and plan files come first. This is protocol, not a suggestion.
- **Commit after each subtask**, not after each edit. A noisy commit history is a drift signal.
- **If GitHub Actions runs fail on first push**, do not panic-patch. Read the error, identify the category (Qt install, compiler, test environment), and apply the minimum necessary fix.
- **When an M0 optional item from §2.3 is skipped**, record the decision in the completion report. "Skipped because optional" is sufficient; you do not need to justify the skip further.

---

## Appendix A — `.clang-format`

```yaml
BasedOnStyle: LLVM
Language: Cpp

# Indentation
IndentWidth: 4
TabWidth: 4
UseTab: Never
NamespaceIndentation: None
AccessModifierOffset: -4

# Line length
ColumnLimit: 120

# Braces
BreakBeforeBraces: Attach
AllowShortFunctionsOnASingleLine: Empty
AllowShortIfStatementsOnASingleLine: Never
AllowShortLoopsOnASingleLine: false
AllowShortCaseLabelsOnASingleLine: false

# Pointers and references
PointerAlignment: Left
DerivePointerAlignment: false

# Includes
IncludeBlocks: Regroup
SortIncludes: CaseSensitive

# Misc
SpacesBeforeTrailingComments: 2
FixNamespaceComments: true
```

## Appendix B — `.clang-tidy`

```yaml
Checks: >
  -*,
  bugprone-*,
  cppcoreguidelines-*,
  modernize-*,
  performance-*,
  readability-*,
  -modernize-use-trailing-return-type,
  -readability-identifier-length,
  -cppcoreguidelines-avoid-magic-numbers,
  -readability-magic-numbers,
  -cppcoreguidelines-pro-bounds-pointer-arithmetic,
  -cppcoreguidelines-pro-type-reinterpret-cast,
  -cppcoreguidelines-avoid-non-const-global-variables

WarningsAsErrors: ''
HeaderFilterRegex: 'src/.*'
FormatStyle: file
```

## Appendix C — `cmake/dependencies.cmake` reference

```cmake
include(FetchContent)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)  # option() honors normal variables

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.14.1
)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.3
)

FetchContent_Declare(
    concurrentqueue
    GIT_REPOSITORY https://github.com/cameron314/concurrentqueue.git
    GIT_TAG v1.0.4
)

FetchContent_Declare(
    exprtk
    GIT_REPOSITORY https://github.com/ArashPartow/exprtk.git
    GIT_TAG 0.0.3
)

FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG 0.8.0
)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)

set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(spdlog Catch2 concurrentqueue exprtk yaml-cpp nlohmann_json)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
```

The tags above are the reference versions known at the time of writing. CC may use newer patch versions if available; any version change must be documented in `.claude/M0-done.md` along with the reason.

## Appendix D — Example completion report

For reference when writing `.claude/M0-done.md`:

```markdown
# M0 Completion Report

## Timing
- Started: 2026-04-22T10:00:00Z
- Completed: 2026-04-23T16:00:00Z
- Active work time: ~14 hours

## Deliverables checklist
| Spec item | Status | Notes |
|---|---|---|
| Top-level CMake | ✅ | Qt6 6.10 REQUIRED, CMake 3.22 minimum |
| CMakePresets with three build presets | ✅ | Matching testPresets added |
| 13 module directories | ✅ | |
| FetchContent × 6 | ✅ | Versions recorded in cmake/dependencies.cmake |
| GitHub Actions CI | ✅ | 3-job matrix, first run passed |
| Documentation seeds present | ✅ | |
| .claude/ workspace | ✅ | |

## Acceptance self-check
§5.1–§5.5 — each item ticked; no blanks.

## HALTs raised
- HALT-2026-04-22T14-33Z-exprtk-version:
  ExprTk latest release tag is 0.0.3 (verified on GitHub).
  Spec said "latest release tag". Proposed 0.0.3.
  Resolution: human approved 0.0.3.

## Deviations and concerns
- Added `CMakeUserPresets.json` to .gitignore although not explicitly required;
  documented in concerns. Rationale: preserves per-developer overrides without
  affecting the committed preset file.

## Freezes established in this milestone
None. M0 establishes the skeleton; no interface or schema freezes occur here.

## Open issues carried forward
- LICENSE is a placeholder. Legal review pending. Not blocking.
- Optional scripts/bootstrap.sh not created; documented as intentionally skipped.

## Commits
a1b2c3d: chore: initial commit with seed docs and CLAUDE.md
d4e5f6a: build: add top-level CMakeLists and presets
7g8h9i0: build: wire up FetchContent dependencies
j1k2l3m: scaffold: create 13 module placeholders
... (approximately 11 commits)

## Suggestions for M1
None. The M1 summary in milestone-roadmap.md is clear.
```

---

## 9. Closing Note

M0 is the foundation. Every mistake here — wrong module layout, wrong dependency resolution, wrong CMake configuration — will be stepped on repeatedly across the remaining eleven milestones.

Go slow on M0. CC raising HALTs here is a positive signal: it means CC is recognizing genuine ambiguity rather than forging ahead on guesses.
