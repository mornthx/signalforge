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

---

### Resuming after S2 HALT — sentry-native (2026-04-24)

**Context**: HALT resolved by human. ADR-002 selects sentry-native (option C
from the HALT report). Architecture v0.7 and M2 spec §4.5.3 amended on main
at 31f808e; merged into milestone/M2 at b26a89f for this continuation.

**Plan delta from amendment**:

- `CrashReporterConfig.handlerExecutable` → `backendHandlerPath` (spec §4.5.3
  post-amendment). Aligned `src/platform/crash_reporting.hpp` on this branch.
- `cmake/dependencies.cmake` gains a sentry-native FetchContent entry; no
  separate Crashpad shim.
- §3.4, §4.7, §5.4, §7 all scrubbed of Crashpad-specific wording on the main
  merge.
- ADR-002 Open Items: record the pinned sentry-native tag in `.claude/M2-done.md`
  at S13.

### S2 re-execution — sentry-native integration (start)

**Goal**: deliver `crash_reporting.{hpp,cpp}` against sentry-native, wire
FetchContent at a pinned stable release tag, add unit tests covering the
init/shutdown/active lifecycle.

**Approach**:

1. Add sentry-native to `cmake/dependencies.cmake` at a recent stable release
   tag, configured for the crashpad backend (default) with build options
   minimizing surface area (disable examples/tests).
2. Implement `crash_reporting.cpp` with sentry_options_t configuration:
   `sentry_options_set_database_path()` to crashDumpDirectory,
   `sentry_options_set_release()` from SIGNALFORGE_VERSION (or a M2 placeholder
   if undefined), `sentry_options_set_handler_path()` to backendHandlerPath,
   DSN intentionally unset. `sentry_init()` on positive path; return false on
   any precondition failure (paths empty, not a directory).
3. Track active state in an atomic bool. `shutdownCrashReporting()` calls
   `sentry_close()` and clears the flag. `crashReportingActive()` reads the
   flag.
4. Unit tests: init-on-valid-config, init-idempotent, init-fails-on-empty-
   backendHandlerPath, shutdown-idempotent, crashReportingActive lifecycle.
   No deliberate crashes in unit tests (per spec §3.4).
5. Build + test under all three presets; commit with pinned tag in body.

### S2 re-execution — sentry-native integration (close)

- **Backend choice**: `SENTRY_BACKEND=crashpad` (default). An initial
  attempt with `inproc` was made because the host lacked
  `libcurl4-openssl-dev` (which sentry-native's vendored Crashpad requires
  at configure time). The human installed libcurl dev headers mid-session;
  the backend was switched back to `crashpad` — the architecturally
  preferred choice matching ADR-002 and §14.3 (out-of-process handler,
  resilient to handler-thread corruption).
- **Pinned tag**: `0.7.17` (getsentry/sentry-native). Recorded in commit
  body and to be copied to `.claude/M2-done.md` at S13.
- **Implementation surfaces**:
  - `backendHandlerPath` is now **required** (non-empty) — the crashpad
    backend's `sentry_options_set_handler_path` needs a valid path to
    the `crashpad_handler` binary. sentry-native builds this binary as
    a CMake target (`crashpad_handler`); its built location is exposed
    to tests via `SIGNALFORGE_TEST_CRASHPAD_HANDLER_PATH` compile
    definition. Production consumers (M3+) will pass the deployed
    handler path.
  - Crashpad client enforces **one init per process lifetime**. After
    `shutdownCrashReporting()`, a subsequent `initCrashReporting()`
    returns false and logs a warning — it will not re-register. This
    constraint is documented in `crash_reporting.hpp` Doxygen and
    verified by the combined lifecycle test.
- **Tests**: 5 crash_reporting cases (active-default, empty-dump-dir,
  empty-handler-path, shutdown-without-init, combined lifecycle) + 19
  S1 cases + 2 smoke = 26 passing under Debug and Release. debug-asan
  build clean; runtime blocked per host preload (see
  `.claude/M2-concerns.md`).
- **Coverage**: init's validation paths (empty dump dir, empty handler
  path) and active-guard covered by dedicated tests. The combined
  lifecycle test covers sentry_init success, g_ever_initialized re-init
  guard, shutdown, and post-shutdown init. `create_directories` error
  path (permission denied) is not exercised. Estimated coverage ≥ 88%.
- **Freeze scope delivered**: `src/platform/crash_reporting.hpp` — public
  surface per spec §4.5.3 post-amendment (CrashReporterConfig with
  `backendHandlerPath`, init/shutdown/active free functions, documented
  single-init-per-process constraint).
- **Observations**: sentry-native + vendored Crashpad build adds ~2–3
  minutes to first configure/build; ~250 translation units across
  sentry-native + Crashpad + mini_chromium. Upstream Crashpad emits
  some `-Wpragmas` and `-Wclass-memaccess` warnings, but they do not
  propagate to our `-Werror` gate because sentry-native's CMake scopes
  warning flags to the Crashpad subdir. Linker emits a deprecation
  warning on `crashpad_info_note.S.o` missing `.note.GNU-stack`; it is
  a warning, not an error.

---

### S3 — frame value types (start)

**Goal**: deliver `src/frame/raw_frame.hpp` with `RawFrame`, `RxStats`,
`TxStats`, `DriverStatistics`, and type aliases `SteadyTimestamp`,
`WallTimestamp`, `SourceId`. Also `registerMetatypes()` in the .cpp to
invoke `qRegisterMetaType<RawFrame>()` at program start. Replace the
frame/ placeholder.

**Approach**: value types are plain POD structs matching spec §4.2
verbatim. Per clarification 3, concrete drivers use std::atomic
internally; the snapshot returned from statistics() is the plain struct
defined here. Per clarification 4, RawFrame::payload is
byte-as-unsigned-char at the frame-layer boundary; documented in Doxygen.
Per clarification 5, deviceAt is steady_clock only; wall-clock
translation is M4's job; documented.

Tests cover: default-init state for each struct; QByteArray implicit
sharing (copy of a RawFrame with a pre-sized payload doesn't allocate);
QVariant roundtrip via registerMetatypes.

### S3 — frame value types (close)

- **Tests**: 6 RawFrame + 5 stats = 11 new cases; 48 total across all
  test binaries in this milestone so far. All pass under Debug and
  Release. debug-asan build clean (runtime blocked per host preload).
- **Coverage**: every struct field initialized-to-default tested;
  implicit-sharing copy verified via `constData()` pointer comparison;
  metatype round-trip exercised via `QVariant::fromValue` /
  `QVariant::value`. `registerMetatypes()` idempotency validated by
  double-invocation.
- **Freeze scope delivered**: `src/frame/raw_frame.hpp` — RawFrame
  struct layout, RxStats, TxStats, DriverStatistics, type aliases.
  Per spec §6.1. Note: frame/ also hosts backpressure (S4) and
  frame_envelope is deferred (spec §2.1 lists it but §6.1 does not
  enumerate a struct for it; plan S3 scope is RawFrame+stats only).
- **clang-format**: auto-applied.

### S4 — BackpressureSignal + WatermarkTracker (close)

- **Implementation**: `observe()` returns `QueueFilling` once per
  below-high → above-high transition via CAS, `QueueRecovered` once per
  above-high → below-recover transition via CAS, and `QueueFull` on
  every observation at capacity (event, not state). Per-field
  `std::atomic` — no mutexes.
- **Caller contract** (spec §3.3 / clarification 6): receiver logs via
  `SF_LOG_WARN` and updates `queue_watermark_<queueName>` in
  MetricsRegistry. No global broker. Documented on `observe()` Doxygen.
- **Tests**: 12 new cases covering default state, below-threshold
  silence, filling-crossing, full-at-capacity, recovery below 60,
  re-entry after recovery, capacity-0 unbounded, peak tracking, reset,
  multi-producer race (8 threads, exactly 1 Filling), signal-field
  consistency, and default-init of BackpressureSignal. 49 tests pass
  across Debug and Release.
- **Coverage**: every observe() branch exercised (below-recover,
  hysteresis band, above-high, at-capacity, capacity==0). CAS
  contention path covered by the 8-thread concurrent test. Estimated
  ≥ 90%.
- **Freeze scope delivered**: `src/frame/backpressure.hpp` —
  BackpressureSignal struct, BackpressureReason enum, WatermarkTracker
  public API (spec §6.1).

### S5 — SpscRing<T> (close)

- **Implementation**: header-only template in `src/utils/spsc_ring.hpp`.
  Power-of-two internal buffer with mask-based modulo, cache-line-padded
  head / tail / peak atomics. Usable capacity reported by `capacity()`
  is one less than the allocated slot count (standard empty-vs-full
  distinction). `push(T item)` takes the argument by value per
  clarification 7; caller's object is moved-from in either return path.
  `pop()` drains the slot and nulls it so destructors run.
- **Utils library**: changed from STATIC to INTERFACE since
  `spsc_ring.hpp` is header-only. Placeholder removed. Will revisit in
  S6 when `MpscQueue` contributes a .cpp and needs STATIC.
- **Tests**: 9 new cases: empty pop, single push+pop, FIFO ordering,
  fill-to-capacity + clean fail, interleaved push/pop, power-of-2
  rounding, peak-size tracking, move-only value type (`unique_ptr`),
  and the 1M-item two-thread stress test per spec §5.2. 58 tests pass
  across Debug and Release. debug-asan build clean.
- **Coverage**: every public method exercised; push-full and pop-empty
  branches covered; peak-CAS loop implicitly exercised by stress test.
  Estimated ≥ 90%.
- **Freeze scope delivered**: `src/utils/spsc_ring.hpp` — SpscRing<T>
  public API per spec §6.1.

### S6 — MpscQueue<T> (close)

- **Implementation**: header declares `MpscQueue<T>` with `std::unique_ptr<Impl>`
  pimpl; `mpsc_queue.cpp` defines `Impl` using `moodycamel::ConcurrentQueue<T>`
  and provides explicit instantiations. This keeps `<concurrentqueue.h>` out
  of the public header. Currently instantiated: `MpscQueue<int>`. New `T`s
  (e.g., `RawFrame` in M4) add an entry at the end of the .cpp.
- **push() OOM semantics** per understanding §3.8: only `std::bad_alloc` is
  swallowed and mapped to `false`; any other exception from `T`'s move
  constructor propagates (CLAUDE.md Forbidden-7).
- **Utils library** switched back from INTERFACE to STATIC (MpscQueue's .cpp
  is the first real TU).
- **Tests**: 5 new cases: default empty pop, single-thread FIFO,
  8-producer × 100k MPSC stress, sizeApprox convergence, initial-capacity
  hint acceptance. The stress test uses a `std::set` to verify no drops
  and no duplicates across 800k items. 63 tests pass across Debug and
  Release. debug-asan build clean.
- **Coverage**: push-success, pop-empty-vs-nonempty, sizeApprox, ctor
  hints all exercised. `bad_alloc` branch not tested (hard without mocking
  allocator); estimated ≥ 85%.
- **Freeze scope delivered**: `src/utils/mpsc_queue.hpp` — MpscQueue<T>
  public API per spec §6.1.

### S7 — Snapshot<T> (close)

- **Implementation**: header-only template using
  `std::atomic<std::shared_ptr<const T>>` per spec §4.4.3 and
  clarification 8. No custom fallback. `publish(T)` wraps the value in
  `make_shared<const T>` and atomic-stores it. `read()` atomic-loads
  the shared_ptr; returned pointer keeps the value alive until the
  reader drops its copy.
- **Tests**: 6 new cases: null-on-empty, publish+read round-trip,
  old-reader-sees-old-value after republish, destructor-tracking lifetime,
  concurrent 4-reader × 100k publish stress with tearing-detection sentinel,
  and non-trivial value type (std::string). 69 tests pass across Debug
  and Release. debug-asan build clean.
- **Coverage**: every public method exercised; lifetime-semantics
  verified via destructor counter; tearing-detection via paired `{a,b}`
  invariant. ≥ 90%.
- **Freeze scope delivered**: `src/utils/snapshot.hpp` — Snapshot<T>
  public API per spec §6.1.

### S8 — MetricsRegistry (close)

- **Implementation**: `Metric` is a single-file class with lock-free
  atomic int64 value; `MetricsRegistry` is a Meyers singleton holding a
  pimpl with a mutex-guarded `std::unordered_map<std::string,
  std::unique_ptr<Metric>>`. QHash cannot hold unique_ptr (deleted copy
  ctor), so std::unordered_map is used instead; the key is the UTF-8
  serialization of the QString name. `clearForTesting()` exists for
  per-test isolation.
- **NOT frozen** per spec §6.2 (metrics API expected to evolve with the
  M5 performance panel).
- **Tests**: 10 cases: counter add, gauge set, name/kind immutability,
  getOrCreate same-ptr on repeat, different names, existing-kind wins,
  snapshot, metricNames enumeration, 8-thread × 1M add stress
  (verifies no lost increments), concurrent registration + snapshot,
  clearForTesting. 80 tests pass across Debug and Release. debug-asan
  build clean.
- **Coverage**: every public method exercised; concurrent paths tested
  under 4-producer + 1-reader load; clearForTesting exercised.
  Estimated ≥ 90%.

