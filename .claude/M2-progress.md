# M2 — Progress log

## Session metadata

- Phase 5 execution session begins 2026-04-23.
- Branch: `milestone/M2` (merged with `origin/main` at 6e1a2b5 to integrate the
  nine spec clarifications from commit 24e089b).
- Plan: `.claude/M2-plan.md`, 13 subtasks S1–S13.
- Understanding: `.claude/M2-understanding.md`.

## Plan delta from clarifications

The nine spec clarifications (24e089b) resolved several of the eleven
ambiguities enumerated in `.claude/M2-understanding.md §3`. Effects on the
plan:

- **§3.1 `DriverConfig` ambiguity — RESOLVED**. Clarification 1 states
  explicitly: no `DriverConfig` type in M2, each concrete driver defines its
  own config struct passed via constructor, `DriverInterface::open()` takes
  no parameters. The planned HALT in S10 is no longer needed; S10 proceeds
  without gating.
- **§3.3 `open()` async semantics — RESOLVED**. Clarification 2 locks the
  contract: `Success` = request accepted; actual open completion via
  `stateChanged(Open)` or `stateChanged(Error) + errorOccurred(...)`;
  malformed request returns `NotConfigured`/`ConfigInvalid` with no state
  transition.
- **§3.7 `with_fields(...)` lifetime — NOT directly resolved** but
  clarification 6 scope is adjacent. Default implementation still per
  understanding §3.7 Interpretation A (attached to exactly next `SF_LOG_*`).
- **§3.10 "patch 5" errata — RESOLVED**. Clarification 9 replaces the
  "patch 5" reference with "§4.6".
- Clarification 3 (Statistics atomicity model) — new constraint: per-field
  `std::atomic<uint64_t>`, no mutex-guarded stats blocks. Will be reflected
  in S3 implementation.
- Clarification 6 (BackpressureSignal caller responsibilities) — locks the
  S4 consumer contract: log warn + update `queue_watermark_<queueName>` in
  MetricsRegistry; no broker forwarding.
- Clarification 7 (SpscRing::push move semantics) — locks S5 contract:
  item unusable after push() regardless of return value.
- Clarification 8 (Snapshot implementation) — locks S7 to
  `std::atomic<std::shared_ptr<const T>>`, no fallback.

No plan-level revisions are required beyond the removal of the planned HALT
in S10. Subtask order, effort estimates, freeze-scope assignments, and test
strategies are unchanged.

## Subtask log

(Each subtask is appended here at start and close.)

---

### S1 — platform value-type utilities (start)

**Goal**: deliver `time_source`, `thread_utils`, `app_paths` as three
independent headers with implementations under `src/platform/`. Replace the
M0 placeholder. Add unit tests with a fresh `tests/unit/platform/` directory.

**Approach**: each header is a short namespace-scoped free-function set
using Qt/libstdc++ primitives only. `time_source` wraps
`std::chrono::steady_clock` and `system_clock`; `thread_utils` uses
`pthread_setname_np` (Linux) via `QThread::currentThread()->setObjectName()`
on the Qt side combined with `prctl(PR_SET_NAME)` for OS-level visibility in
htop; `app_paths` applies XDG resolution using standard env-var fallbacks
plus `std::filesystem::create_directories`. CMakeLists replaces the single
placeholder.cpp with three real .cpp files and the Catch2 subsection is
created under `tests/unit/platform/`.

### S1 — platform value-type utilities (close)

- **Tests**: 19 platform cases + 2 smoke cases = 21 passing under Debug and
  Release presets. `debug-asan` build is clean; runtime test execution is
  blocked by the host `/etc/ld.so.preload` — see `.claude/M2-concerns.md`.
- **Coverage**: every public declaration exercised. `truncateUtf8ToByteLimit`
  covered indirectly via the 26-byte input case; affinity out-of-range
  covered by `-1` and `10'000` inputs; log-directory env-var precedence
  covered by three separate cases (override, XDG, HOME fallback). Coverage
  projection ≥ 90%.
- **Freeze scope delivered**: `src/platform/time_source.hpp` (per spec §6.1
  "`src/platform/time_source.hpp`: all declarations"). `thread_utils.hpp`
  and `app_paths.hpp` are platform public surface and included in the
  freeze record for completeness per plan §1 S1 note.
- **clang-format**: clean (auto-applied on four files that had include
  ordering differences with the project style).
- **Observations**: build already configured; incremental rebuilds ~5 s.
  `tests/unit/platform/CMakeLists.txt` is a new subdir — ran `add_subdirectory`
  from `tests/unit/CMakeLists.txt` to wire it in.

---

### S2 — Crashpad integration (HALT)

**Goal**: deliver the Crashpad integration per spec §4.5.3 — `crash_reporting.
{hpp,cpp}` plus `cmake/Crashpad.cmake` (FetchContent) plus
`SIGNALFORGE_CRASHPAD_HANDLER_PATH`.

**Approach attempted**: minimal `FetchContent_Declare` +
`FetchContent_MakeAvailable` against `https://chromium.googlesource.com/
crashpad/crashpad` at `main` with `GIT_SHALLOW TRUE`.

**Outcome**: HALT per spec §7-2. Fetch succeeded (25.4 s), but the fetched
source has no top-level `CMakeLists.txt` — Crashpad upstream is GN-only.
No CMake targets produced, so `crash_reporting.cpp` cannot link against
Crashpad without adopting a community CMake fork. Which community fork to
adopt (and at what SHA, verified against Ubuntu 24.04 + GCC 13 + Qt 6.10 +
`-Werror`) requires human judgment — CLAUDE.md §HALT-trigger 9
("two plausible implementations, cannot confidently pick one") and spec
§7-2 both require HALT at this point.

**Committable artifact kept**: `src/platform/crash_reporting.hpp` matches
spec §4.5.3 verbatim in its public surface. Committed for later reuse once
the human resolves the Crashpad vendor choice.

**Experimental artifacts reverted**: `cmake/Crashpad.cmake` and the root
`CMakeLists.txt` include of it — both removed before HALT commit.

**HALT report**: `.claude/halt/HALT-2026-04-23T15-09Z-m2-s2-crashpad-no-
upstream-cmake.md`. Enumerates four resolution options (A: community fork,
B: vendor in-repo, C: sentry-native, D: defer to M2.5).

Session ends after this commit per CLAUDE.md §HALT.


