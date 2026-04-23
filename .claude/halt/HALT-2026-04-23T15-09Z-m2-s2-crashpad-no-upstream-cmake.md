# HALT — M2 / S2 Crashpad integration

## Trigger

M2 spec §7-2: "Crashpad FetchContent or build failure" (hard HALT, single-attempt
threshold per plan S2 HALT-primed clause).

Also matches CLAUDE.md §HALT-trigger 9 ("Two plausible implementations and
cannot confidently pick one") for the downstream fork-selection question
described below.

## Context

- Currently executing: S2 (Crashpad integration via FetchContent) per
  `.claude/M2-plan.md` §1.
- Completed earlier in this session:
  - Step 0: merged `origin/main` into `milestone/M2` (merge commit
    `6e1a2b5`) to integrate the 9 spec clarifications; pushed.
  - S1: `platform: add time_source, thread_utils, app_paths` — commit
    `d6d4ab8`; 21 tests pass under Debug and Release. Pushed.
- Files modified but not in an acceptable state:
  - `src/platform/crash_reporting.hpp` — the frozen interface per spec
    §4.5.3, written correctly and committable independent of the HALT.
    Kept in the HALT commit as "committable artifact" per CLAUDE.md §HALT
    instructions.
- Experimental files reverted:
  - `cmake/Crashpad.cmake` — removed before HALT commit
  - `CMakeLists.txt` Crashpad include line — removed before HALT commit
  - Build tree re-configured clean without Crashpad references

## Problem details

M2 spec §4.5.3 specifies:

> `crashpad_handler` is built from Crashpad's source fetched via `FetchContent`.
> The build system discovers its path and passes via
> `SIGNALFORGE_CRASHPAD_HANDLER_PATH` CMake variable.

I attempted the minimal integration: `FetchContent_Declare` and
`FetchContent_MakeAvailable` for
`https://chromium.googlesource.com/crashpad/crashpad` at the `main` tag, with
`GIT_SHALLOW TRUE`.

Observed result:

```
$ cmake --preset debug
...
-- Crashpad fetched to: /home/shuai/Music/signalforge/build/debug/_deps/crashpad-src
-- Configuring done (25.4s)
-- Generating done (0.0s)
-- Build files have been written to: /home/shuai/Music/signalforge/build/debug
```

Fetch succeeds. But inspecting the fetched source:

```
$ ls /home/shuai/Music/signalforge/build/debug/_deps/crashpad-src/
AUTHORS    build       BUILD.gn   client    codereview.settings   compat
CONTRIBUTORS  DEPS   doc   handler  infra   LICENSE   minidump   navbar.md
OWNERS   package.h   README.md   snapshot   test   third_party   tools   util

$ find /home/shuai/Music/signalforge/build/debug/_deps/crashpad-src -maxdepth 2 -name "CMakeLists.txt"
(no matches)
```

Crashpad upstream has `BUILD.gn` but **no** `CMakeLists.txt`. It is a
GN-only build. `FetchContent_MakeAvailable` clones the source but produces
no CMake targets, so `src/platform/crash_reporting.cpp` has nothing to link
against. The spec's assumption that `FetchContent` alone yields a working
Crashpad target does not hold against upstream Crashpad.

The plan's S2 risk section anticipated this:

> - **Root cause most likely**: Crashpad's canonical build is GN, not CMake;
>   available CMake forks have divergent target layouts; ...
> - **Mitigation**: 1. Use a pinned commit of `getsentry/crashpad` (known
>   CMake-ported fork) and pin the SHA in `cmake/Crashpad.cmake`.

But `getsentry/crashpad` does not expose CMake targets either — it is a
chromium mirror used by `sentry-native`, which wraps Crashpad with its own
CMake inside the sentry-native project (not as a reusable Crashpad CMake port).

`backtrace-labs/crashpad` (the other community CMake fork I know of) is
stale: its last commit predates GCC 13 and Qt 6.10; building it with
`-Werror` is not documented as working on Ubuntu 24.04.

## Candidate interpretations or approaches

- **Option A — Adopt a specific community CMake fork**:

  Pin a community-maintained CMake fork of Crashpad at a SHA that is
  verified to build on Ubuntu 24.04 + GCC 13 + `-Werror`. Candidates to
  research: `backtrace-labs/crashpad`, `oxen-io/crashpad`, a SignalForge-
  owned fork.

  Implications:
  - Requires the human to validate the fork choice (license, maintainer
    responsiveness, alignment with upstream chromium).
  - Pins SignalForge to a community fork that may drift from upstream
    chromium over V1's lifetime.
  - Adds the fork URL + SHA to a new `cmake/Crashpad.cmake`.
  - Preserves M2 scope as originally specified.

- **Option B — Vendor Crashpad inside the repository**:

  Commit a curated subset of Crashpad's sources under `third_party/crashpad/`
  with a SignalForge-owned `CMakeLists.txt` that enumerates the needed
  client + handler source files, sets platform flags, and builds the two
  targets.

  Implications:
  - Full control over the build; no network dep at configure time.
  - Increases repo size materially (~20-50 MB of C++ sources).
  - Maintenance burden: someone must sync with upstream when CVEs are
    disclosed in Crashpad.
  - Changes spec §4.5.3 wording ("fetched via FetchContent" → "vendored")
    and requires architecture §4.1 acknowledgement.

- **Option C — Switch to `sentry-native` as the crash-reporting provider**:

  Replace Crashpad with `sentry-native`, which wraps Crashpad (or Breakpad,
  or inproc) and provides a clean CMake target.

  Implications:
  - New top-level dependency. CLAUDE.md Forbidden-1 prohibits adding
    dependencies without human approval; this is a clean architecture
    amendment path.
  - `sentry-native` is actively maintained and known to build on Ubuntu
    24.04 + GCC 13.
  - Changes architecture §4.1 and M2 spec §4.5.3.
  - V1 spec §10.1 says "V1 does not operate an upload backend"; sentry-
    native can be configured to write local minidumps only without Sentry
    SaaS integration.

- **Option D — Defer Crashpad integration to a later milestone**:

  Strip `crash_reporting.{hpp,cpp}` and `tools/crash_test/` from M2 scope.
  Ship M2 without crash reporting. Add a new milestone (e.g., M2.5 or M3's
  first task) dedicated to Crashpad vendor selection.

  Implications:
  - M2 ships without item §2.1-1's `crash_reporting` and §2.1-6 (crash
    trigger tool).
  - Interface-freeze scope is reduced: `crash_reporting.hpp` is not frozen
    at M2 close.
  - Allows M2 to complete on schedule; the missing crash-reporting path
    is tracked as a follow-up.

## Decision requested

1. Which option: A (community fork — pick which), B (vendor in repo), C
   (sentry-native — requires arch amendment), or D (defer to M2.5)?
2. If A: which fork, and what SHA is known to build on the stack
   (Ubuntu 24.04 + GCC 13.3 + Qt 6.10 + `-Werror`)?
3. Whichever option: does M2's §4.5.3 wording need amendment (e.g., "fetched
   via FetchContent" may need to become "vendored" or "via
   ExternalProject_Add with GN toolchain")? If so, the architecture doc
   reference to Crashpad may need a paired amendment.

## Side effects to clean up on resume

- No uncommitted changes remain after this HALT commit. Working tree is
  clean.
- `src/platform/crash_reporting.hpp` is committed. It matches spec §4.5.3
  verbatim in its public surface (struct `CrashReporterConfig`, free
  functions `initCrashReporting`/`shutdownCrashReporting`/
  `crashReportingActive`).
- `src/platform/crash_reporting.cpp` was NOT created; there is nothing to
  delete.
- `cmake/Crashpad.cmake` was created during the attempt and removed before
  HALT. Reverted.
- Root `CMakeLists.txt` had a temporary `include(cmake/Crashpad.cmake)` line
  during the attempt; removed. Reverted.
- `.claude/M2-progress.md` has an S2 entry noting the HALT. `.claude/M2-
  concerns.md` already carries the local-asan block concern; the S2 Crashpad
  situation is referenced by this HALT report, not duplicated into concerns.
- On resume: re-verify state with `git fetch origin --prune`, read this
  HALT report, apply the human's chosen option, then continue S2 through
  S13 per the plan. If the chosen option removes S2 from M2's scope
  (Option D), the plan's S10 and S11 still proceed without modification;
  S12 (`tools/crash_test/`) becomes unnecessary because it depends on S2.

## Subtasks remaining and their state

- S3 (RawFrame, stats) — not started; independent of S2, unblocked.
- S4 (BackpressureSignal, WatermarkTracker) — not started; independent.
- S5 (SpscRing) — not started; independent.
- S6 (MpscQueue) — not started; independent.
- S7 (Snapshot) — not started; independent.
- S8 (MetricsRegistry) — not started; independent.
- S9 (with_fields) — not started; independent.
- S10 (DriverInterface) — not started; depends on S3.
- S11 (integration test) — not started; depends on S10.
- S12 (crash_test tool) — not started; depends on S2. Blocked by this
  HALT.
- S13 (completion report) — not started; depends on S1–S12.

Per CLAUDE.md §HALT: "stop all code changes, write a HALT report, commit
whatever is committable, and exit the session. Do not attempt to continue."
The session ends after the HALT commit is pushed.
