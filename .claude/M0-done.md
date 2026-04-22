# M0 Completion Report

## Timing

- Started (session opening per `[EM §9]`): 2026-04-22 (understanding+plan
  written and committed as `0a117e3`).
- Preflight HALTed and resolved (Option A+C): 2026-04-22T14:44Z → same
  day, resumed per `8cab794`.
- Completed: 2026-04-22 (this file committed).
- Active CC wall-clock time: approximately 3.5 hours including
  preflight, HALT authoring, three full CMake configures (Qt + six
  FetchContent deps compile cold ≈ 90 s each), three full builds,
  clang-format pass, and S10 verification. CI wall time not
  included — waits on first human push.

## Deliverables checklist — spec §2.1

| Spec item | Status | Notes |
|---|---|---|
| 1. Git repo initialized; pushed to GitHub | ⚠️ partial | Local repo on branch `milestone/M0`. No `origin` remote. Human pushes post-acceptance (see §Open issues). |
| 2. `main` branch protection | ⚠️ pending human | Requires GitHub UI; cannot be done by CC. Hand-off item for S12. |
| 3. Top-level CMake Qt 6.10.2 + GCC 12 + Ninja | ✅ | `CMakeLists.txt` verbatim from spec §4.1. Local dev uses system GCC 13; CI installs GCC 12 per spec §4.6. See concern C1. |
| 4. `CMakePresets.json` with debug / release / debug-asan + matching testPresets | ✅ | Ninja generator; `compile_commands.json` exported; `SIGNALFORGE_QT_PATH` env var with `$HOME/Qt/6.10.2/gcc_64` default. |
| 5. Module directory layout per arch §18 | ✅ | 13 modules: app, platform, observability, drivers, frame, decode, signal, action, session, ui_widgets, ui_quick, models, utils. |
| 6. Each module: CMakeLists, placeholder.hpp, placeholder.cpp | ✅ | 11 placeholder modules (observability and app have real content). |
| 7. FetchContent deps (6) pinned to tags | ✅ | spdlog v1.14.1, Catch2 v3.5.3, concurrentqueue v1.0.4, exprtk 0.0.3, yaml-cpp 0.8.0, nlohmann/json v3.11.3 — exact tags from spec Appendix C. |
| 8. `src/app/main.cpp` + `main_window.{hpp,cpp}` | ✅ | QMainWindow titled "SignalForge", 1280×800. |
| 9. `src/observability/logging.{hpp,cpp}` | ✅ | Async rotating JSON-lines logger, 10 MB × 10 files, idempotent via `std::once_flag`, `SIGNALFORGE_LOG_LEVEL` env override, `XDG_STATE_HOME` resolution. |
| 10. `tests/unit/smoke_test.cpp` | ✅ | Two Catch2 cases; verified by `ctest --preset debug/release`. |
| 11. `.clang-format` | ✅ | Verbatim from Appendix A. |
| 12. `.clang-tidy` | ✅ | Verbatim from Appendix B. |
| 13. `.editorconfig` | ✅ | UTF-8, LF, 4-space. |
| 14. `.gitignore` | ✅ | Verbatim from spec §4.7. |
| 15. `.github/workflows/ci.yml` with 3-job matrix on ubuntu-22.04 | ✅ | Verbatim from spec §4.6; YAML syntax validated locally. |
| 16. Root governance: README / CLAUDE.md / CONTRIBUTING.md / LICENSE | ✅ | CLAUDE.md landed via bootstrap step `a9aa6dc`; README + CONTRIBUTING + LICENSE placeholder in `89d1907`. |
| 17. Seed docs in `docs/` | ✅ | Architecture, milestone roadmap, M0 spec, execution manual all present (seeded by human pre-session). |
| 18. `.claude/` directory + `.gitignore` + lifecycle files | ✅ | `.claude/.gitignore` from spec §4.8; understanding / plan / concerns / progress / this done.md + one resolved HALT all tracked. |

## Acceptance self-check — spec §5

### §5.1 Build and test

- [x] `cmake --preset debug && cmake --build --preset debug` — zero warnings
- [x] `cmake --preset release && cmake --build --preset release` — zero warnings
- [x] `cmake --preset debug-asan && cmake --build --preset debug-asan` — zero warnings
- [x] `ctest --preset debug` — 2/2 passing
- [x] `ctest --preset release` — 2/2 passing
- [ ] `ctest --preset debug-asan` — **deferred to CI**; blocked locally by concern C2 (`/etc/ld.so.preload` injects AppProtection before libasan on dev host)
- [x] Release exe opens a "SignalForge" window — verified via `QT_QPA_PLATFORM=offscreen` launch; window geometry 1280×800 set in `MainWindow` ctor. Window visual on real display not re-verified by CC (no non-xvfb display in auto mode); human re-verifies in §5.3 review.
- [x] Log file at `~/.local/state/signalforge/logs/signalforge.log` with JSON lines — verified (see `.claude/M0-progress.md` S10 entry for the exact line written).
- [ ] GitHub Actions green — **pending first human push**; handed off in S12.

### §5.2 Structure and constraints

- [x] Directory layout matches §3 exactly
- [x] `CLAUDE.md` verbatim from `[EM §2]` — landed via bootstrap commit `a9aa6dc`; diff-verified against the manual at the time of writing.
- [x] Dependency list matches §2.1-7 exactly
- [x] Every module is placeholder only (observability logger and app main window are explicitly in-scope per §2.1-8 and §2.1-9)
- [x] `.clang-format`, `.clang-tidy`, `.editorconfig` present
- [x] `CMakePresets.json` has all three build presets and matching test presets

### §5.3 Documentation

- [x] `README.md` readable with three-command build
- [x] `CONTRIBUTING.md` points to `CLAUDE.md`
- [x] `docs/architecture/architecture.md` present (amended in-session by human per Option C — see §HALTs below)
- [x] `docs/milestones/M0-project-bootstrap.md` present
- [x] `docs/milestones/milestone-roadmap.md` present
- [x] `docs/claude-code/execution-manual.md` present

### §5.4 Process

- [x] `.claude/M0-understanding.md`, `.claude/M0-plan.md`, `.claude/M0-done.md` (this file) exist and are substantive
- [x] Commit history clean; one commit per subtask (+ one bundled S3–S6, as planned)
- [x] One HALT occurred in preflight and was resolved; report at `.claude/halt/HALT-2026-04-22T14-44Z-toolchain-mismatch.md`

### §5.5 CI

- [x] `.github/workflows/ci.yml` syntactically valid (`python3 -c "import yaml; yaml.safe_load(...)"`)
- [ ] Branch protection on `main` with required status checks — **human action**; see §Open issues
- [ ] One complete successful CI run visible — **pending first push**

## Test results

- Count: 2 discovered by `catch_discover_tests` (`smoke: Catch2 works`,
  `smoke: C++20 lambdas compile`).
- Debug preset: 2 / 2 passing.
- Release preset: 2 / 2 passing.
- Debug-asan preset: ctest not executed locally (C2); build clean.
- ASan / UBSan violations on completed runs: zero.

## HALTs raised

1. **`HALT-2026-04-22T14-44Z-toolchain-mismatch.md`** — preflight
   toolchain mismatch (Ubuntu 24.04 vs spec 22.04; GCC 12 absent;
   clang-format + clang-tidy absent). Resolved by **Option A+C**:
   human `apt install g++-12 clang-format clang-tidy`, plus human
   amended `docs/architecture/architecture.md` §§1, 3.2, 8.4, 12,
   16.1, 17.2, 18 to make Ubuntu 24.04 LTS + GCC 13 the supported
   target. Preflight re-ran clean (see `.claude/M0-progress.md`).

No further HALTs fired. Concerns (C1, C2) were handled by the
`CLAUDE.md` §Disagreement-handling path, not by new HALTs.

## Deviations and concerns

See `.claude/M0-concerns.md`:

- **C1**: M0 spec §§2.1-3 and §4.6 still reference `ubuntu-22.04` +
  GCC 12 while the amended architecture §12 / §16.1 now read
  `ubuntu-24.04` + GCC 13. I executed the M0 spec as written (CI
  workflow verbatim; unamended). Human action at milestone review
  is to either update the M0 spec to match arch or reaffirm the
  spec values.
- **C2**: `/etc/ld.so.preload` on the dev host forces
  `libAppProtection.so` to load before libasan, making local
  debug-asan runtime impossible. Build is clean; CI runs on a
  stock ubuntu-22.04 image with no such preload and will validate
  ASan-cleanliness. Developer may disable the preload via
  `sudo mv /etc/ld.so.preload /etc/ld.so.preload.bak` if fast local
  ASan feedback is desired.
- **Minor**: clang-format reordered `#include` blocks in
  `src/app/main.cpp` so the project-local `main_window.hpp` and
  `observability/logging.hpp` come before `<QApplication>`. This
  deviates cosmetically from M0 spec §4.4's verbatim example but is
  required by the project's formatter (which is authoritative per
  `CLAUDE.md` §Tooling). Behavior is unchanged.
- **Minor**: the spec lists `Qt 6.10 REQUIRED COMPONENTS ... Test`,
  which Qt 6.10.2 provides as `Qt6::Test`. It's not linked anywhere
  in M0; it is pulled in by `find_package` and is ready for M1+.

## Freezes established in this milestone

**None.** M0 establishes the skeleton; no interface or schema
freezes occur. Per `CLAUDE.md` §Forbidden-2 (third bullet), freezes
are recorded in `*-done.md` completion reports — this report records
the absence of any.

## Open issues carried forward

1. LICENSE is a placeholder pending legal review.
2. Optional spec §2.3 items (`scripts/bootstrap.sh`, issue / PR
   templates, pre-commit hook note in CONTRIBUTING) **intentionally
   skipped**. Each is easy to add later if the team wants it.
3. GitHub remote not configured locally. Human creates the remote,
   pushes `milestone/M0`, and configures `main` branch protection
   with the three CI jobs as required status checks.
4. First CI run has not executed. §5.1 last checkbox and all of §5.5
   are fulfilled only after that run is green.
5. C1 (spec vs arch toolchain drift) and C2 (dev-host ASan
   limitation) — see Deviations.

## Commits on `milestone/M0`

Below the seed commit `ffbf3f2` and the CLAUDE.md bootstrap
`a9aa6dc`:

| Hash | Subject |
|---|---|
| `0a117e3` | chore: record M0 understanding and plan |
| `8cab794` | chore: record M0 preflight halt and resolution |
| `89d1907` | docs: add README, CONTRIBUTING, LICENSE placeholder |
| `a857e08` | chore: add editorconfig, clang-format, clang-tidy, gitignore |
| `1b50329` | build: establish buildable CMake skeleton with logging and main window |
| `29e78f9` | test: add Catch2 smoke test and wire ctest |
| `26d599e` | chore: reserve tools/examples/resources/packaging/ci dirs |
| `34a93e3` | ci: add GitHub Actions workflow for debug/release/asan |
| *(this)* | docs: file M0 completion report |

The human's uncommitted `M docs/architecture/architecture.md` edit
(Option C) remains in the working tree for a separate human commit
per `CLAUDE.md` §Forbidden-2.

## Suggestions for M1

None. The M1 summary in `docs/milestones/milestone-roadmap.md`
looks clean from this side of M0. Whenever the human picks up M1,
concerns C1 and C2 above are the only items worth addressing before
diving in; neither blocks M1 start.
