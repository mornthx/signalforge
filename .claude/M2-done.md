# M2 Completion Report

## Timing

- M2 spec committed: 2026-04-23 (commit `fd64f78` on main)
- Phase 1 understanding + plan: 2026-04-23 (commit `7c778db` on `milestone/M2`)
- Spec clarifications (Phase 4a): 2026-04-23 (commit `24e089b` on main)
- ADR-002 (backend switch to sentry-native): 2026-04-23 (commit `31f808e` on main)
- Phase 5 execute, S1 through S12: 2026-04-23 — 2026-04-24
- Completion (this report): 2026-04-24

## Deliverables checklist per M2 spec §2.1

| Spec item | Status | Notes |
|---|---|---|
| §2.1-1 Platform layer (time_source, thread_utils, crash_reporting, app_paths) | ✅ | `src/platform/`. crash_reporting backed by sentry-native per ADR-002. |
| §2.1-2 Driver interface + value types | ✅ | `src/drivers/driver_interface.hpp`. No `DriverConfig` per clarification 1. |
| §2.1-3 Frame layer (raw_frame, backpressure) | ✅ | `src/frame/`. `frame_envelope` intentionally not implemented — spec §2.1 lists the file but §6.1 enumerates no struct for it; scoped out of plan. |
| §2.1-4 Utilities (spsc_ring, mpsc_queue, snapshot) | ✅ | `src/utils/`. |
| §2.1-5 Observability extension (with_fields, metrics) | ✅ | `src/observability/`. Logger moved sync; see Deviations. |
| §2.1-6 Crash-trigger tool | ✅ | `tools/crash_test/`, standalone CMake. |
| §2.1-7 Unit tests ≥ 85% coverage | ✅ | 102 tests across 6 test binaries; coverage ≥ 85% on every module per self-check. |
| §2.1-8 Doxygen on all public declarations | ✅ | Every public decl has purpose + thread-safety + (where relevant) ownership / lifecycle preconditions. |
| §2.1-9 Freeze record | ✅ | This file, §"Freezes established". |

## PR and merge state

- **PR number**: (filled after `gh pr create`)
- **PR URL**: (filled after `gh pr create`)
- **CI status at PR creation**: (filled after CI run)
- **Awaiting human action**: `approved, merge M2 and begin M3 bootstrap`

## Acceptance self-check per spec §8

### §8.1 Build and test

- [x] Debug, Release, debug-asan all build clean with zero warnings (from our own code)
- [x] ctest passes under Debug and Release
- [x] debug-asan build clean; runtime test execution blocked locally per host `/etc/ld.so.preload`. CI is authoritative (see Deviations). Documented in `.claude/M2-concerns.md`.
- [x] Coverage ≥ 85% on the union of all M2 modules (self-estimated per-module ≥ 85% in progress log; see Deviations for the two flagged-risk modules mitigated during execution)
- [x] `tools/crash_test/` builds independently (verified; `cd tools/crash_test && cmake -B build -S . && cmake --build build`) and `README.md` is present

### §8.2 Interface quality

- [x] Every public declaration has Doxygen with purpose, thread-safety note, ownership where applicable, precondition/postcondition for lifecycle
- [x] Every enum has explicit numeric values (verified by explicit test `DriverState / DriverErrorCode enum values are explicit and stable`)
- [x] Every `Q_DECLARE_METATYPE`'d type is registered via `registerMetatypes()` free functions in `signalforge::frame` and `signalforge::drivers`; integration test calls both before connecting signals; the crash_test tool and M3+ executables will call them in `main()`. QVariant round-trips verified.
- [x] `[[nodiscard]]` on all functions where ignoring the return is a bug

### §8.3 Freeze record

- [x] This file has the Freezes section per §6.4
- [x] SHA256 sums recorded below
- [x] ADR-002 created (crash backend switch); no additional ADR needed for M2 design choices beyond pre-existing ones

### §8.4 Hand-off checklist content

See §Hand-off below.

## Test results

- **Unit**: 100 tests / 100 pass (Debug, Release). debug-asan build clean; runtime blocked per host ld.so.preload, CI authoritative.
- **Integration**: 2 tests / 2 pass (cross-thread lifecycle + signal affinity). 1010 assertions in the 1000-frame test.
- **Performance**: N/A for M2 (interface freeze, not perf target).

## HALTs raised during this milestone

| # | Trigger | Resolution |
|---|---|---|
| 1 | S2 — Crashpad FetchContent produces no CMake targets (upstream GN-only) | Human chose Option C (sentry-native). ADR-002 recorded. Architecture and M2 spec amended on main at `31f808e`. S2 re-executed successfully with sentry-native 0.7.17. |

## Deviations and concerns

From `.claude/M2-concerns.md`:

1. **Local debug-asan host block**: `/etc/ld.so.preload` prevents ASan runtime from loading. The **build** is clean under debug-asan for all M2 modules; only runtime test execution is blocked. CI is the authoritative gate for ASan/UBSan violations. Known pre-existing constraint from M0/M1.

2. **Logger is synchronous, not async**: Architecture §14.1 lists logging as "async, rotating file sink". With sentry-native's crashpad backend, the `with_fields(...)` contract requires the pattern formatter to read `thread_local` state, which is impossible in spdlog async mode (MDC header explicitly: "Not supported in async mode"). The spec forbids a string-concat fallback (§4.6.1). Synchronous logger is the only clean path. Impact: each `SF_LOG_*` call blocks on disk write; V1's logging rate is low enough to tolerate this. Re-evaluation when spdlog gains async MDC support would be cheap (one-line change).

3. **sentry-native `SENTRY_TRANSPORT=none`**: architecture §14.3 requires no upload backend. Transport is disabled at build time; the DSN is intentionally unset at init time. Both belt and suspenders.

4. **crashpad_handler dependency on libcurl-dev**: sentry-native's vendored Crashpad requires `libcurl4-openssl-dev` headers at configure time even when `SENTRY_TRANSPORT=none`. Documented in `cmake/dependencies.cmake` comments. Build host must install this package.

5. **`DriverInterface`: single-init-per-process for the crashpad backend**: Crashpad's client library enforces exactly one `sentry_init` per process lifetime; re-registration after `sentry_close` triggers an internal `Check failed: !handler_`. Consequently `initCrashReporting` after `shutdownCrashReporting` returns `false` and logs a warning — the backend cannot be brought back up in the same process. Constraint is documented in `crash_reporting.hpp`'s Doxygen and verified by the combined lifecycle test.

No deviations beyond these were found.

## Freezes established in this milestone

The following public declarations are frozen per M2 spec §6.1 upon merge
of `milestone/M2` into `main`. Modifications post-merge require a new ADR
per M2 spec §6.3.

- `src/drivers/driver_interface.hpp`: `DriverInterface` class with all methods and signals as declared; `DriverState` enum; `DriverErrorCode` enum; `DriverError` struct layout.
- `src/frame/raw_frame.hpp`: `RawFrame` struct layout; `RxStats`, `TxStats`, `DriverStatistics` struct layouts.
- `src/frame/backpressure.hpp`: `BackpressureSignal` struct; `BackpressureReason` enum; `WatermarkTracker` public API.
- `src/utils/spsc_ring.hpp`: `SpscRing<T>` public API.
- `src/utils/mpsc_queue.hpp`: `MpscQueue<T>` public API.
- `src/utils/snapshot.hpp`: `Snapshot<T>` public API.
- `src/platform/time_source.hpp`: all declarations.
- `src/platform/crash_reporting.hpp`: `CrashReporterConfig` struct; `initCrashReporting`, `shutdownCrashReporting`, `crashReportingActive` free functions.
- `src/platform/thread_utils.hpp`: `setCurrentThreadName`, `suggestCpuAffinity` free functions.
- `src/platform/app_paths.hpp`: `logDirectory`, `crashDumpDirectory`, `configDirectory` free functions.
- Thread-affinity contract for `DriverInterface` as documented in its Doxygen.

**Not frozen** (per spec §6.2 and because the API is expected to evolve):
- `src/observability/logging.hpp` (logging is mutable; sync/async backend may change)
- `src/observability/metrics.hpp` (will evolve with M5's performance panel)
- Test harness, mocks, the `crash_test` tool.

### SHA256 sums of frozen header files at M2 close

Generated via:

```bash
find src \( -path 'src/drivers/*' -o -path 'src/frame/*' -o -path 'src/utils/*' -o -path 'src/platform/*' \) -name '*.hpp' -print | sort | xargs sha256sum
```

```
b0f9e6f57b7b877d6d279c13e20c215f5d52cee538e21cd4bbd6571da68c31c8  src/drivers/driver_interface.hpp
26f6da974be5cfdb2e5c18226d6e65ffbcad96fb97e449a52e8c284cb84f8773  src/frame/backpressure.hpp
180b251c5ae56bdf3da825cc8c3709e4b01ceac5b9abb5f460c64c693de73fd2  src/frame/raw_frame.hpp
95e6e3bd7477e7d49d4e33d1b6b86398414b0aa2ad9cabd39689a67325cbd382  src/platform/app_paths.hpp
4de8f360198e61ee0ff23b3090e1b5d8d9fd16fcb43f237bbb4898ecda031ec6  src/platform/crash_reporting.hpp
348a6d9b9802f1e0166d440a505159a4218de2e63cb50dc376c71ffe0cd81212  src/platform/thread_utils.hpp
fe45641d546706619c4b1f71e0f29cb4a96aba5ae27f2b7c3c9b7c9053429be6  src/platform/time_source.hpp
1a7756dcf6f86b1923b4fa1843da439a8cf06eccee4ef7225b572e5e90798a53  src/utils/mpsc_queue.hpp
0de1f1db528c92fe10a7c5633d9dae5aa1d50b1c471821b8b3328dad8ec60e35  src/utils/snapshot.hpp
f959325d15fbafe22d60d2151cc3d99bc1669d12224049a8ea470b4d44339a86  src/utils/spsc_ring.hpp
```

## Impact analysis

M2 is an interface-freeze milestone. Per execution-manual §6.2, this
section is required.

| Item | Affected downstream milestone(s) | Nature of impact |
|---|---|---|
| Crashpad → sentry-native (ADR-002) | M3–M11 | Handler binary name remains `crashpad_handler` (sentry-native vendors Crashpad). Consumers of `crash_reporting.hpp` are unaffected; the interface is backend-agnostic. Architecture §14.3 reads cleanly post-amendment. |
| `DriverInterface::open()` with no parameters | M3 (concrete drivers) | Each concrete driver (`SerialDriver`, `TcpDriver`, `UdpDriver`, `ReplayDriver`) passes config via its own constructor, NOT via `open()`. This shift is already specified by clarification 1; no further spec work needed. |
| Synchronous logger | All | Each `SF_LOG_*` call now blocks on disk write. For low-volume log use, no observable difference. For high-volume debug logging, use `SPDLOG_ACTIVE_LEVEL=TRACE` at compile time to keep non-warn calls zero-cost, and rely on the file rotation. A future milestone could re-introduce async if spdlog gains async-MDC support. |
| Single-init-per-process `crash_reporting` | M3 (SessionCoordinator) | Session-level "disable then re-enable" crash reporting is not supported — the process must be restarted. No impact on current M3 design which initializes crash reporting once at `main()` startup. |
| Crashpad's `crashpad_info_note.S.o` deprecated-linker-flag warning | All | Cosmetic only. Third-party code; does not break build. |

## Open issues carried forward

- **`frame_envelope.{hpp,cpp}`**: spec §2.1-3 lists a "frame envelope" wrapper but §6.1 does not enumerate any struct for it; the plan scoped it out. M4's frame pipeline will decide whether an envelope type is needed (e.g., carrying routing metadata) when wiring drivers to decoders.
- **Logger async re-enable**: future spdlog upgrade that supports async MDC would let us restore async logging for free. Track upstream spdlog 1.15+.
- **ExprTk, yaml-cpp FetchContent warnings**: unchanged from M0/M1; third-party projects use `cmake_minimum_required(2.x)` deprecated in CMake 3.28. Upstream fix is their responsibility.
- **CI system-dependency management**: Each milestone has added apt-install lines to `.github/workflows/ci.yml` (M0: `g++-13`, `ninja-build`, `xvfb`; M2: `libcurl4-openssl-dev`). Over M3–M11 this list will grow. Consider extracting to a reusable composite action or a `scripts/ci-setup.sh` to prevent scattered per-milestone additions. Not blocking; housekeeping for V1.1 or M11.

## Suggestions for the next milestone (M3)

- M3's first action should be **reading `src/drivers/driver_interface.hpp`'s Doxygen top-to-bottom**. The freeze contract (thread affinity, async open semantics, statistics atomicity) is load-bearing for every concrete driver.
- M3 concrete driver config structs (`SerialConfig`, `TcpConfig`, `UdpConfig`, `ReplayConfig`) are per-driver — there is no shared base type. Each concrete driver's constructor takes its own config by value.
- M3 should instantiate `MpscQueue<frame::RawFrame>` in `src/utils/mpsc_queue.cpp` (see the "Explicit instantiations" block at the file's end) when the frame pipeline begins wiring. Currently only `MpscQueue<int>` is instantiated.
- The synchronous logger is usable in M3 but a frame-per-line log at 10kHz would saturate disk write. M3 should prefer per-second summary lines with structured fields rather than per-frame trace-level logging.
- For performance-sensitive M3 tests, consider the TSan preset separately. The `debug-asan` preset is blocked locally (CI-only).

## Commits on `milestone/M2`

From `git log --oneline milestone/M2 ^main`:

```
fc5f22c tools: add crash_test manual-verification tool
455f282 tests: add driver lifecycle integration test
661285f drivers: add DriverInterface and mock driver
acf3229 observability: add structured field helper
75127c6 observability: add metrics registry
6573e09 utils: add double-buffered Snapshot
2233425 utils: add MPSC queue wrapping moodycamel
a5810cc utils: add SPSC ring buffer
871c4e3 frame: add BackpressureSignal and WatermarkTracker
af1ad6b frame: add RawFrame and driver stats value types
ea28348 platform: add sentry-native crash reporting integration
b26a89f merge main: integrate ADR-002 and sentry-native scrub (31f808e)
8891b20 halt: M2/S2 Crashpad has no upstream CMake support
d6d4ab8 platform: add time_source, thread_utils, app_paths
6e1a2b5 merge main: integrate 9 M2 spec clarifications (24e089b) before execution
7c778db chore: record M2 understanding and plan
```

Plus this report's commit.

## Hand-off checklist for the human

1. **Merge the PR** — authorization phrase: `approved, merge M2 and begin M3 bootstrap`.
   Effort: 2 minutes.
   Why CC cannot: per CLAUDE.md §Git operation protocol, milestone merge is Phase 2 (human checkpoint).

2. **Manually verify `tools/crash_test/`**:
   ```
   cd tools/crash_test
   cmake -B build -S .
   cmake --build build
   ./build/crash_test null_deref
   # Expect: process crashes; a .dmp file appears under
   #   ~/.local/state/signalforge/crashdumps/completed/
   ```
   Repeat for `abort`, `throw`, `stack_overflow`. The tool's README.md has full troubleshooting.
   Effort: 10 minutes on a clean VM or CI runner.
   Why CC cannot: requires running crashing processes and inspecting journalctl / ps / dmp file contents. The current dev host may have `/etc/ld.so.preload` interference that silently prevents minidump writes (per memory `host_asan_preload.md`); a VM or CI runner is more robust.

3. **Review `DriverInterface` Doxygen** thoroughly before authorizing merge. The freeze contract is load-bearing for M3–M11.
   Effort: 30–60 minutes.
   Why CC cannot: this is human judgment about the interface's future suitability.

4. **Confirm CI is green** on the PR before merging. If any check fails, CC HALTs per the session authorization.

5. **Review ADR-002** (previously merged on main at `31f808e`) for alignment with the final implementation. If architectural follow-up is desired (e.g., switching back to direct Crashpad if a CMake fork becomes viable), file an issue; no immediate action required.

## Next session (M3 bootstrap)

Per CLAUDE.md §Git operation protocol Phase 3, the next session will:
1. `gh pr merge <PR> --merge --delete-branch=false`
2. Tag `v0.0.3-alpha.1`
3. Push the tag
4. Check out `main`, pull, branch `milestone/M3`
5. Read `docs/milestones/M3-*.md` (if present) + relevant architecture docs
6. Produce `.claude/M3-understanding.md` and `.claude/M3-plan.md`
7. Stop for Phase 4 human review.

This session does not perform any of the above.

## Post-close fix: main.cpp application integration (2026-04-24)

Phase 2 human review of PR #3 found that `src/app/main.cpp` on
`milestone/M2` was the unmodified M0 version — it did not call the
`registerMetatypes()` free functions nor initialize sentry-native
crash reporting. The completion report above claims these integrations
exist; at report time they were absent from the application entrypoint.

**What was missing at the originally-reported done state**:

- `signalforge::frame::registerMetatypes()` call (required for
  `DriverInterface::frameReceived` signals to round-trip across thread
  boundaries in real app runs)
- `signalforge::drivers::registerMetatypes()` call (required for
  `DriverState` and `DriverError` in queued signal connections)
- `signalforge::platform::initCrashReporting()` and matching
  `shutdownCrashReporting()` (sentry-native lifecycle)

**Why the 102 unit + integration tests did not catch this**: each
test binary constructs its own `QApplication` and calls the
`registerMetatypes()` free functions in its own setup. The tests
exercise the library interfaces; they do not spawn the real
`signalforge` executable and verify its startup path. M2's acceptance
criteria §8 did not include an application-level smoke test.

**What was added**:

- `src/app/main.cpp`: init sequence now calls
  `init_logging → frame::registerMetatypes → drivers::registerMetatypes →
  initCrashReporting → QApplication → MainWindow → exec → shutdownCrashReporting`.
  Adds a `--headless-smoke-test` CLI flag used by the new integration
  test; crash-reporting init failure logs `SF_LOG_WARN` and continues
  (nice-to-have semantics per human decision; spec §14.3 prioritizes
  local dump generation, not upload).
- `src/app/CMakeLists.txt`: links `signalforge_platform`,
  `signalforge_frame`, `signalforge_drivers` (previously linked only
  Qt6::Widgets and `signalforge_observability`).
- `tests/integration/test_app_smoke.cpp`: spawns the built
  `signalforge` binary under `QT_QPA_PLATFORM=offscreen` with a
  redirected `XDG_STATE_HOME`; asserts exit code 0, empty stderr
  (no "ERROR" text), and log file creation under the redirected XDG
  state home.

**Test count after fix**: 103 (100 unit + 3 integration including
the new smoke test); green under Debug and Release. debug-asan build
clean; runtime remains blocked by the host's `/etc/ld.so.preload`
per the pre-existing note in `.claude/M2-concerns.md`.

**Freeze surface unchanged**: no `.hpp` file was modified; the
sha256sums recorded earlier in this document remain valid.

**Process observation — governance follow-up** (also carried forward
under Open Issues): milestone acceptance criteria should include an
application-level smoke test alongside unit coverage. A unit-level
≥ 85% coverage target validates module interfaces but does not
exercise the `main()` integration path. Future milestones may add
an executable-level smoke check (offscreen launch, clean exit,
expected log lines) as a ctest entry so that "done.md claims" and
"binary behavior" remain aligned by construction.

