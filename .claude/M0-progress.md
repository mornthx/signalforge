# M0 — Progress Log

Chronological record of M0 execution. Each entry records a subtask
boundary or a material event (preflight check, HALT, defect found,
etc.). Commit hashes are recorded after the commit lands.

## 2026-04-22 — preflight (first attempt)

HALTed. See `.claude/halt/HALT-2026-04-22T14-44Z-toolchain-mismatch.md`.
Triggers: GCC 12 / `clang-format` / `clang-tidy` absent; Ubuntu 24.04
host vs Ubuntu 22.04 spec target.

Human resolution: **Option A + C**.
- A: installed `g++-12`, `clang-format`, `clang-tidy` via apt on the
  host.
- C: amended `docs/architecture/architecture.md` §12 (and related
  §§1, 3.2, 8.4, 16.1, 17.2, 18) to make Ubuntu 24.04 + GCC 13 the
  primary supported target. The arch edit is staged in the working
  tree but committed separately by the human; CC does not touch it.

## 2026-04-22 — preflight (second attempt, passed)

Re-ran the toolchain probe after Option A install:

| Check | Observed | Status |
|---|---|---|
| Host OS | Ubuntu 24.04 LTS (noble) | ✅ (now in-spec per amended `[Arch §12.1]`) |
| `g++-12 --version` | `g++-12 (Ubuntu 12.4.0-2ubuntu1~24.04.1) 12.4.0` | ✅ |
| GCC 13 (system default) | `g++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0` | ✅ (arch now recommends 13+) |
| `clang-format --version` | `Ubuntu clang-format version 18.1.3 (1ubuntu1)` | ✅ |
| `clang-tidy --version` | `Ubuntu LLVM version 18.1.3` | ✅ |
| `ninja --version` | `1.11.1.git.kitware.jobserver-1` | ✅ |
| `cmake --version` | `3.28.3` | ✅ (≥ 3.22 required) |
| Qt 6.10.2 | `~/Qt/6.10.2/gcc_64/lib/cmake/Qt6/Qt6Config.cmake` present | ✅ |
| Branch | `milestone/M0` | ✅ |
| `git remote -v` | *(empty — no remote configured)* | ⚠️ documented per Understanding §3.4; handed to human in S12 |

Preflight is PASSED. Local compiler policy: use the M0 spec §2.1-3
wording literally — CMake presets do not pin a specific compiler, so
the system default `g++` (GCC 13.3) will drive local builds, and CI
continues to install `g++-12` per M0 spec §4.6 verbatim. The arch
vs. M0-spec discrepancy on GCC version and Ubuntu runner is logged
in `.claude/M0-concerns.md`.

## Subtask ledger

| # | Subtask | Commit | Status |
|---|---|---|---|
| — | HALT record + concerns + progress | `8cab794` | done |
| S1 | Governance files (README, CONTRIBUTING, LICENSE) | `89d1907` | done |
| S2 | Editor / tooling configs | `a857e08` | done |
| S3+4+5+6 | Build skeleton + logging + main | `1b50329` | done (bundled) |
| S7 | Catch2 smoke test | `29e78f9` | done |
| S8 | Empty dir scaffolds | `26d599e` | done |
| S9 | CI workflow | `34a93e3` | done |
| S10 | Local verification pass | — (no commit; clean pass) | done |
| S11 | Completion report | — | in progress |
| S12 | Hand-off | n/a | pending |

## 2026-04-22 — S10 local verification pass

All commands run from repo root:

- `cmake --build --preset debug` → no work; all objects current.
- `ctest --preset debug` → **100% passed, 2 of 2 tests** (Catch2 smoke +
  C++20 lambda STATIC_REQUIRE). Wall time 0.02 s.
- `cmake --build --preset release` → no work.
- `ctest --preset release` → **100% passed, 2 of 2**. Wall time 0.02 s.
- `cmake --build --preset debug-asan` → no work. Build succeeds clean.
- `ctest --preset debug-asan` → **skipped locally**. Root cause logged
  in `.claude/M0-concerns.md` C2 (host `/etc/ld.so.preload` loads
  `AppProtection.so` before libasan and ASan init recurses / errors
  out). CI runs this preset on a stock ubuntu-22.04 runner with no
  such preload and will enumerate + pass the cases.
- `clang-format --dry-run -Werror` over all `src/**/*.{cpp,hpp}` and
  `tests/unit/smoke_test.cpp` → no violations.
- Release smoke-run (`QT_QPA_PLATFORM=offscreen
  ./build/release/src/app/signalforge`) → process starts, log
  directory `~/.local/state/signalforge/logs/` is created, single
  JSON-line written:
  `{"ts":"2026-04-22T23:30:23.935Z","level":"info","thread":216712,"module":"signalforge","event":"SignalForge starting","fields":{}}`.
  Process exit on SIGTERM (no graceful shutdown path implemented in
  M0, so no "exiting" line; not a regression — the "starting" line
  proves logging end-to-end).

No defects found; S10 produced no `fix:` commit.
