# M2 — Plan

## 1. Subtask breakdown

Thirteen ordered subtasks. All are Blocking (spec §3.7 — no Independent
category in M2; every deliverable is part of the freeze surface). Each
subtask gets its own commit unless explicitly marked as batched.

### S1 — Platform value-type utilities (time, thread_utils, app_paths)

- **Output files**:
  - `src/platform/time_source.hpp`, `.cpp`
  - `src/platform/thread_utils.hpp`, `.cpp`
  - `src/platform/app_paths.hpp`, `.cpp`
  - `src/platform/CMakeLists.txt` (replace placeholder wiring)
  - Remove `src/platform/placeholder.{hpp,cpp}`
  - `tests/unit/platform/time_source_test.cpp`
  - `tests/unit/platform/thread_utils_test.cpp`
  - `tests/unit/platform/app_paths_test.cpp`
  - `tests/unit/CMakeLists.txt` updates
- **Classification**: Blocking.
- **Prerequisites**: None (no dependencies beyond Qt Core + libstdc++).
- **Effort estimate**: 3 h.
- **Commit point**: Yes — single commit `platform: add time_source, thread_utils, app_paths`.
- **Freeze scope**: `src/platform/time_source.hpp` (spec §6.1 enumerates
  "`src/platform/time_source.hpp`: all declarations" as frozen).
  `thread_utils.hpp` and `app_paths.hpp` are NOT explicitly in §6.1 but I
  will include them in the freeze record for completeness (they are part
  of the platform public surface; adding them to freeze is additive per
  spec §3 and CLAUDE.md ambiguity exception).

### S2 — Crashpad integration via FetchContent

- **Output files**:
  - `cmake/FindCrashpad.cmake` or `cmake/Crashpad.cmake` (FetchContent wiring)
  - Root `CMakeLists.txt` addition for `SIGNALFORGE_CRASHPAD_HANDLER_PATH` and
    the Crashpad dependency discovery
  - `src/platform/crash_reporting.hpp`, `.cpp`
  - `tests/unit/platform/crash_reporting_test.cpp` (tests init/shutdown/
    `crashReportingActive` accessor only — no deliberate crash in tests)
- **Classification**: Blocking.
- **Prerequisites**: S1 (`app_paths.hpp`'s `crashDumpDirectory()` is used by
  the default config).
- **Effort estimate**: 6 h (highest-risk subtask; budget includes HALT branch).
- **Commit point**: Yes — single commit `platform: add Crashpad integration`.
- **Freeze scope**: `src/platform/crash_reporting.hpp` public surface. Not
  enumerated in spec §6.1 explicitly — I will include it in the freeze
  record (see S13).
- **HALT-primed**: If Crashpad FetchContent or build fails on Ubuntu 24.04 +
  GCC 13, HALT per spec §7-2. Do not silently disable crash reporting.

### S3 — `RawFrame`, `RxStats`, `TxStats`, `DriverStatistics` value types

- **Output files**:
  - `src/frame/raw_frame.hpp`, `.cpp` (cpp holds `qRegisterMetaType` free
    function `signalforge::frame::registerMetatypes()`)
  - `src/frame/CMakeLists.txt` (replace placeholder wiring)
  - Remove `src/frame/placeholder.{hpp,cpp}` (when the frame dir becomes
    owner of at least one real file)
  - `tests/unit/frame/raw_frame_test.cpp`
  - `tests/unit/frame/stats_test.cpp`
- **Classification**: Blocking.
- **Prerequisites**: None (value types, no cross-module deps).
- **Effort estimate**: 3 h.
- **Commit point**: Yes — commit `frame: add RawFrame and driver stats value types`.
- **Freeze scope**: `src/frame/raw_frame.hpp` public struct layouts per §6.1
  ("`RawFrame` struct layout, `RxStats`, `TxStats`, `DriverStatistics` struct
  layouts").

### S4 — `BackpressureSignal`, `BackpressureReason`, `WatermarkTracker`

- **Output files**:
  - `src/frame/backpressure.hpp`, `.cpp`
  - `tests/unit/frame/backpressure_test.cpp`
- **Classification**: Blocking.
- **Prerequisites**: None (pure independent module).
- **Effort estimate**: 3 h.
- **Commit point**: Yes — commit `frame: add BackpressureSignal and WatermarkTracker`.
- **Freeze scope**: `BackpressureSignal` struct, `BackpressureReason` enum,
  `WatermarkTracker` public API per §6.1.

### S5 — `SpscRing<T>`

- **Output files**:
  - `src/utils/spsc_ring.hpp` (header-only template; .cpp for explicit
    instantiation if needed, otherwise header-only)
  - `src/utils/CMakeLists.txt` update
  - Remove `src/utils/placeholder.{hpp,cpp}` (shared with S6, S7 — first
    to land here does it)
  - `tests/unit/utils/spsc_ring_test.cpp` (includes ThreadSanitizer
    stress test with 1M items per CLAUDE.md §Required-6)
- **Classification**: Blocking.
- **Prerequisites**: None.
- **Effort estimate**: 4 h.
- **Commit point**: Yes — commit `utils: add SPSC ring buffer`.
- **Freeze scope**: `SpscRing<T>` public API per §6.1.

### S6 — `MpscQueue<T>`

- **Output files**:
  - `src/utils/mpsc_queue.hpp` (public interface, pimpl)
  - `src/utils/mpsc_queue.cpp` (pimpl definitions; moodycamel include is
    .cpp-local so callers don't pull it in)
  - `tests/unit/utils/mpsc_queue_test.cpp` (8 producers × 100k stress test
    under TSan)
- **Classification**: Blocking.
- **Prerequisites**: None (moodycamel is FetchContent-fetched at top level;
  M0/M1 may have wired it; if not, add here).
- **Effort estimate**: 3 h.
- **Commit point**: Yes — commit `utils: add MPSC queue wrapping moodycamel`.
- **Freeze scope**: `MpscQueue<T>` public API per §6.1.

### S7 — `Snapshot<T>`

- **Output files**:
  - `src/utils/snapshot.hpp` (header-only template using
    `std::atomic<std::shared_ptr<const T>>`)
  - `tests/unit/utils/snapshot_test.cpp` (single-writer / multi-reader
    correctness + TSan test)
- **Classification**: Blocking.
- **Prerequisites**: None.
- **Effort estimate**: 3 h.
- **Commit point**: Yes — commit `utils: add double-buffered Snapshot`.
- **Freeze scope**: `Snapshot<T>` public API per §6.1.
- **HALT-primed**: If `std::atomic<std::shared_ptr<T>>` is unavailable or
  buggy on GCC 13, HALT per spec §7-3.

### S8 — `MetricsRegistry` / `Metric`

- **Output files**:
  - `src/observability/metrics.hpp`, `.cpp`
  - `src/observability/CMakeLists.txt` update (metrics.cpp added)
  - `tests/unit/observability/metrics_test.cpp` (8-thread × 1M counter
    increment stress)
- **Classification**: Blocking.
- **Prerequisites**: None.
- **Effort estimate**: 3 h.
- **Commit point**: Yes — commit `observability: add metrics registry`.
- **Freeze scope**: **Not frozen** per spec §6.2 (metrics may evolve
  through M5). Still requires full Doxygen and ≥ 85% coverage.

### S9 — Structured-field logging extension (`with_fields`)

- **Output files**:
  - `src/observability/logging.hpp` (add `with_fields(...)` declaration)
  - `src/observability/logging.cpp` (add TLS state + custom spdlog flag
    formatter; **no** changes to `init_logging` semantics, rotation, or
    SF_LOG_* macro bodies)
  - `tests/unit/observability/with_fields_test.cpp`
- **Classification**: Blocking.
- **Prerequisites**: None (additive to existing logging.cpp).
- **Effort estimate**: 3 h (risk-buffered).
- **Commit point**: Yes — commit `observability: add structured field helper`.
- **Freeze scope**: Not enumerated in §6.1 freeze list; logging is mutable.
- **HALT-primed**: Spec §4.6.1 HALT if spdlog's API cannot cleanly express
  thread-local attributes. No string-concat fallback.

### S10 — `DriverInterface` + `DriverState` + `DriverError` + mock driver + metatype registration

- **Output files**:
  - `src/drivers/driver_interface.hpp`, `.cpp`
  - `src/drivers/CMakeLists.txt` (replace placeholder wiring)
  - Remove `src/drivers/placeholder.{hpp,cpp}`
  - `tests/mocks/mock_driver.hpp`, `.cpp` (mock driver class used by both
    unit and integration tests)
  - `tests/unit/drivers/driver_interface_test.cpp` (lifecycle via mock,
    Q_DISABLE_COPY_MOVE compile-time check, metatype round-trip,
    thread-affinity of signal emission)
- **Classification**: Blocking.
- **Prerequisites**: S3 (`RawFrame` must be Q_DECLARE_METATYPE'd first).
- **Effort estimate**: 4 h.
- **Commit point**: Yes — commit `drivers: add DriverInterface and mock driver`.
- **Freeze scope**: `DriverInterface` class, signals, methods; `DriverState`
  enum, `DriverErrorCode` enum, `DriverError` struct — all per §6.1. This is
  the most consequential freeze surface in M2.
- **Planned HALT point**: Per understanding §3.1, I will HALT here and ask
  the human about `DriverConfig` before coding if §3.1 is unresolved. The
  HALT report goes to `.claude/halt/HALT-<UTC>-m2-driver-config.md`. The
  subtask resumes post-resolution.

### S11 — Integration test `driver_lifecycle_with_mock`

- **Output files**:
  - `tests/integration/driver_lifecycle_with_mock.cpp`
  - `tests/integration/CMakeLists.txt` (create if absent)
  - Root `tests/CMakeLists.txt` wire-in
- **Classification**: Blocking.
- **Prerequisites**: S10 (mock driver and DriverInterface).
- **Effort estimate**: 3 h.
- **Commit point**: Yes — commit `tests: add driver lifecycle integration test`.
- **Freeze scope**: Not frozen (test code).

### S12 — Crash-trigger tool `tools/crash_test/`

- **Output files**:
  - `tools/crash_test/CMakeLists.txt` (standalone, not `add_subdirectory`'d
    from root)
  - `tools/crash_test/main.cpp` (subcommands: null_deref, abort, throw,
    stack_overflow)
  - `tools/crash_test/README.md` (manual verification procedure +
    AppProtection troubleshooting note)
- **Classification**: Blocking.
- **Prerequisites**: S2 (`crash_reporting.hpp`).
- **Effort estimate**: 2 h.
- **Commit point**: Yes — commit `tools: add crash_test manual-verification tool`.
- **Freeze scope**: Not frozen (tool).

### S13 — Progress + completion report + freeze record

- **Output files**:
  - `.claude/M2-progress.md` (updated through each prior subtask; finalized
    here)
  - `.claude/M2-concerns.md` (contains the §3.1 DriverConfig HALT record,
    the §3.10 "patch 5" errata, local ASan block per Host ASan Preload
    memory, any other items arising during execution)
  - `.claude/M2-done.md` (completion report per execution-manual §6.2 with
    Freezes section per spec §6.4 including sha256 list)
- **Classification**: Blocking.
- **Prerequisites**: S1–S12 all green.
- **Effort estimate**: 2 h.
- **Commit point**: Yes — commit `chore: M2 completion report and freeze record`.
- **Freeze scope**: N/A — this is the freeze record itself.
- **Post-commit**: Push `milestone/M2` to origin; wait for CI green; create
  PR to main (no merge — per session authorization, PR creation will require
  a new session-prompt authorization phrase from the human). Phase 1 close.

## 2. Implementation order rationale

Order: **platform → frame → utils → observability → drivers → integration
→ tools → report**. Rationale:

1. **Simple platform utilities first** (S1). No dependencies; early wins
   establish CMake patterns (replace placeholder, add unit test harness
   directory, wire into top-level).

2. **Crashpad second** (S2). It is the single highest HALT risk in the
   milestone. Hitting that HALT early means minimum wasted investment; if
   it HALTs, the session already produced S1 as a committable artifact.
   Placing it before the rest keeps the "fail fast" discipline.

3. **Frame value types third** (S3). `RawFrame` is a dependency of
   `DriverInterface`'s signal signature and must exist with its metatype
   registration before the driver interface header can compile.

4. **Backpressure fourth** (S4). Independent, but lives in the frame
   namespace, so landing it immediately after S3 keeps commits thematically
   grouped.

5. **Utils S5–S7** (SpscRing, MpscQueue, Snapshot). No inter-dependencies;
   S5 first because it is the simplest primitive (ring), S6 next (wraps
   moodycamel), S7 last because `std::atomic<std::shared_ptr<T>>` is the
   most advanced C++20 feature and I want the simpler primitives committed
   before I touch it.

6. **Observability S8–S9** (metrics, with_fields). Metrics first because it
   is a clean new file; with_fields second because it modifies existing
   `logging.cpp` and has its own HALT risk — committing metrics first keeps
   a clean rollback point.

7. **DriverInterface S10**. Depends on S3 (`RawFrame` metatype). Planned
   HALT gate on `DriverConfig` ambiguity (§3.1). Everything else that
   composes into the interface is ready.

8. **Integration test S11**. Requires DriverInterface + mock.

9. **Crash-trigger tool S12**. Depends on S2's `crash_reporting.hpp`.
   Placed near the end because it is a manual-verification artifact, not
   part of the automated test surface.

10. **Report S13**. Aggregates everything. Final subtask.

**Rationale note on test-ability**: Every subtask's module can be tested in
isolation because M2's design is intentionally value-type and primitive-heavy.
`DriverInterface` is the only subtask that requires a consumer (the mock),
and the mock is delivered in the same subtask. No "temporarily mock X" kludges
are needed anywhere in the plan.

**Rationale note on freeze sequence**: Simple value types (time, stats,
RawFrame, BackpressureSignal) freeze first; the composite `DriverInterface`
freezes last. This matches the dependency direction and lets each freeze
record be written against fully-committed, fully-tested code.

## 3. Test strategy summary

Per-module test file inventory with coverage projection:

| Module | Test file | Categories (per spec §5.2) | Coverage projection |
|---|---|---|---|
| `time_source` | `tests/unit/platform/time_source_test.cpp` | Monotonic property, ClockOrigin capture, all three fns invoked | 95% |
| `thread_utils` | `tests/unit/platform/thread_utils_test.cpp` | Name set + truncation log, affinity no-op-safety | 85% |
| `app_paths` | `tests/unit/platform/app_paths_test.cpp` | XDG resolution (env var, fallback), mkdir semantics | 90% |
| `crash_reporting` | `tests/unit/platform/crash_reporting_test.cpp` | Init/shutdown idempotence, `crashReportingActive()` transitions. Does NOT call deliberate crashes. | 85% |
| `raw_frame` + stats | `tests/unit/frame/raw_frame_test.cpp`, `stats_test.cpp` | Default-init state, QByteArray shared-copy verification via allocator tracker, QVariant round-trip | 90% |
| `backpressure` | `tests/unit/frame/backpressure_test.cpp` | Threshold crossings (80/60), multi-producer "fires once", reset, peak tracking | 90% |
| `spsc_ring` | `tests/unit/utils/spsc_ring_test.cpp` | Single-thread ordering, two-thread 1M stress (TSan), capacity-full clean fail | 88% |
| `mpsc_queue` | `tests/unit/utils/mpsc_queue_test.cpp` | Basic push/pop, 8-producer 100k stress (TSan), sizeApprox convergence, bad_alloc path (mocked) | 85% (flagged risk) |
| `snapshot` | `tests/unit/utils/snapshot_test.cpp` | Single write/read, concurrent readers consistency (TSan), publish during read | 88% |
| `metrics` | `tests/unit/observability/metrics_test.cpp` | Counter/gauge semantics, getOrCreate stability, 8-thread 1M add stress, snapshot | 90% |
| `with_fields` | `tests/unit/observability/with_fields_test.cpp` | JSON field appears in log line, cleared after one line, per-thread isolation | 85% (flagged risk) |
| `DriverInterface` | `tests/unit/drivers/driver_interface_test.cpp` via mock | Lifecycle transitions, Q_DISABLE_COPY_MOVE compile-fail check (via SFINAE static_assert in test), metatype round-trip, signal emission thread, wrong-state errors | 90% |
| **Integration** | `tests/integration/driver_lifecycle_with_mock.cpp` | Full flow: open → start → 1000 frames via QueuedConnection → stop → close | N/A |

**Modules I flag as at-risk for the ≥ 85% threshold**:

- `mpsc_queue` — the `bad_alloc` branch is hard to exercise without mocking
  the allocator. Mitigation: use a limited-capacity custom allocator in the
  OOM test. If still below threshold, the coverage deficit will be localized
  to that one branch and reported in `.claude/M2-done.md` with justification.
- `with_fields` — complex pattern-formatter + TLS interaction. Mitigation:
  structure the TLS state so its reset and consume paths each have a
  dedicated test.

**Aggregate coverage projection**: ≥ 87% across all M2 modules, above the
85% spec floor. If aggregate falls short, HALT per spec §7-4 (coverage HALT
is automatic).

## 4. Highest-risk subtasks for HALT

The three subtasks most likely to trigger a HALT, with mitigations:

### Risk 1 — S2 (Crashpad FetchContent)

- **HALT trigger**: spec §7-2 — "Crashpad FetchContent or build failure".
- **Root cause most likely**: Crashpad's canonical build is GN, not CMake;
  available CMake forks have divergent target layouts; GCC 13 may surface
  Crashpad-internal warnings that are promoted to errors by our `-Werror`
  setting; `crashpad_handler`'s sandbox/ptrace code path may need
  `AT_SECURE` shims.
- **Mitigation**:
  1. Use a pinned commit of `getsentry/crashpad` (known CMake-ported fork)
     and pin the SHA in `cmake/Crashpad.cmake`.
  2. Wrap Crashpad targets with `target_compile_options(... PRIVATE -w)`
     **locally on Crashpad's targets only** — not on SignalForge code — so
     our `-Werror` does not fire on third-party code.
  3. If either fails, HALT with a report capturing the CMake configure log,
     the failing compiler invocation, and the Crashpad target that refused
     to build. Do not attempt a third alternative fork or skip crash
     reporting; spec §7-2 is explicit.
- **Residual risk**: Even with mitigations, probability of HALT ~20%. If
  HALT, the session's work to date (S1, nothing else) commits cleanly and
  the human resolves via ADR / vendor change.

### Risk 2 — S9 (spdlog + `with_fields`)

- **HALT trigger**: spec §4.6.1 — "If spdlog's API does not cleanly support
  this, HALT — do not implement string-concatenated fields inside the log
  message as a workaround."
- **Root cause most likely**: M0's pinned spdlog version may predate MDC
  support (spdlog added `spdlog::mdc` in 1.11). Custom pattern flag formatter
  with thread-local state is supported in older spdlog (≥ 1.4) but requires
  the TLS hook to live in our own custom flag class; this is entirely
  viable.
- **Mitigation**:
  1. First action in S9: check `vcpkg` / FetchContent manifest for pinned
     spdlog version.
  2. If ≥ 1.11: use `spdlog::mdc::put()` / `clear()` directly — the cleanest
     path.
  3. If < 1.11 but ≥ 1.4: implement a custom `custom_flag_formatter` that
     reads from our own `thread_local std::vector<std::pair<QString,QString>>`
     and emits into the JSON pattern. This is "cleanly supported" and not
     a string-concat workaround.
  4. If < 1.4: HALT (we are pinned to too-old spdlog; human decides to bump
     or to waive the structured-fields requirement).
- **Residual risk**: ~10% HALT after mitigation.

### Risk 3 — S7 (`std::atomic<std::shared_ptr<T>>`)

- **HALT trigger**: spec §7-3.
- **Root cause most likely**: On GCC 13 + libstdc++, the feature is present
  but may warn about `-Wdeprecated-declarations` on the non-template
  `atomic_load(std::shared_ptr<T>*)` free function overloads (those are
  deprecated in C++20). Our implementation must use the member functions
  `.load()`, `.store()`, `.exchange()`, `.compare_exchange_*()`.
- **Mitigation**:
  1. Verify with a 10-line standalone program before writing `Snapshot<T>`:
     ```cpp
     std::atomic<std::shared_ptr<int>> a;
     a.store(std::make_shared<int>(1));
     auto v = a.load();
     ```
     If this compiles under Debug + Debug+ASan + Release, proceed.
  2. If compile fails or warnings include ABI-related diagnostics, HALT.
- **Residual risk**: ~5%.

## 5. Freeze record preparation

Per spec §6.4, `.claude/M2-done.md` must include the Freezes section with a
sha256 enumeration. I will generate the sha256 list via a deterministic
sorted-filename command run against the worktree at commit-just-before-push:

```bash
find src \
  \( -path 'src/drivers/*' -o -path 'src/frame/*' \
     -o -path 'src/utils/*'  -o -path 'src/platform/*' \) \
  -name '*.hpp' -print \
  | sort \
  | xargs sha256sum
```

I will place this command (and its captured output) directly into
`.claude/M2-done.md`'s "Freezes established in this milestone" section,
under the §6.1 enumerated freeze items. Notes:

- I include `.hpp` only — `.cpp` files are implementation and not part of
  the freeze.
- `src/observability/metrics.hpp` and `src/observability/logging.hpp` are
  **excluded** from the sha list because neither is frozen at M2 close
  (spec §6.2).
- The command and its output are copied verbatim into `.claude/M2-done.md`
  so any future audit can re-run the exact same command against
  `v0.0.3-alpha.1` to detect post-freeze tampering.

## 6. Hand-off planning

`.claude/M2-done.md`'s handoff sections will contain:

**PR and merge state**:

- PR number and URL (created during Phase 1 close of the execution session).
- CI status at PR creation (green expected; any yellow/red is a HALT).
- Merge SHA placeholder — filled by Phase 3 after human approval.
- "Awaiting human action: `approved, merge M2 and begin M3 bootstrap`".

**Hand-off checklist for the human**:

- **Merge the PR** — authorization phrase: `approved, merge M2 and begin M3
  bootstrap`. Effort: 2 min. Why CC cannot: spec forbids self-merge.
- **Tag review**: confirm `v0.0.3-alpha.1` annotated tag is created by
  Phase 3 against the merge SHA. Effort: trivial (CC does this; human
  verifies).
- **Manually verify `tools/crash_test/`** on a host where AppProtection /
  `/etc/ld.so.preload` does not interfere (per memory
  `host_asan_preload.md`, the current dev host may block ASan; Crashpad
  minidump path is separate from ASan, but the same class of preload
  interference is possible, per spec §4.7 README troubleshooting note).
  Effort: 20 minutes on a clean VM or CI runner. Why CC cannot: requires
  running crashing processes and inspecting journalctl / `ps`, which is
  more robust with human oversight, and — per the M0/M1 memory — the
  local host may have preload-based interference that fails the tool
  silently.
- **Review `DriverInterface` Doxygen** thoroughly before signing off the
  freeze. Spec §10: "When in doubt, be precise, not fast." Effort:
  30–60 minutes. This is the load-bearing interface of V1.
- **Decide `DriverConfig` disposition** — if the planned HALT in S10 fires
  (understanding §3.1), the human answers: (a) out of scope, remove from
  §2.1; or (b) specify the type signature so M2 can freeze it. Effort:
  1 business day per execution-manual §3.4.

**Items NOT in hand-off** (go to "Open issues carried forward" instead):

- Anything CC should have done but did not (there should be none if the
  DoD is honored).
- Future-milestone follow-ups (those go to M3's session prompt).
