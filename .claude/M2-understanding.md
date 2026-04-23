# M2 — Understanding

## 1. Restatement of the M2 goal

M2 delivers the foundation layer of SignalForge: platform utilities, the
`DriverInterface` base class, the `RawFrame`/stats value types, bounded-queue and
snapshot primitives, and the observability extensions (structured log fields and a
metrics registry). The milestone's hard-stop type is **interface freeze**: at
close, a concrete enumerated list of headers and their public declarations
(spec §6.1, §6.4) becomes immutable for the rest of V1 — M3 through M11 may add
new types alongside them but cannot rename, renumber, resign, or re-semantic any
signature. The downstream consequence is large: every concrete driver in M3
(`SerialDriver`, `TcpDriver`, `UdpDriver`, `ReplayDriver`) inherits verbatim from
the `DriverInterface` defined here; M4's decode layer consumes `RawFrame` by
value; M5's performance panel queries `MetricsRegistry::snapshot()`; M8's `.sfr`
format embeds `steady_clock` origins from `platform::captureOrigin()`. Any
ambiguity left in M2 compounds across all subsequent milestones — per spec §10,
three days of extra review here saves weeks downstream. Precision over speed.

## 2. Observed repo state

State reconciliation (run at Phase 1 start):

```
$ git fetch origin --prune
$ git status
On branch main
Your branch is up to date with 'origin/main'.
nothing to commit, working tree clean

$ git log --oneline origin/main -6
448146c docs: remove M2 spec Appendix A (session opening template)
fd64f78 docs: add M2 platform and core abstractions spec
ce209aa docs: establish git operation protocol; extend conflict resolution
c061ecc halt: M2 spec absent, rule-extension session cannot proceed
2dd450a docs: propagate M0/M1 learnings into governance (8 patches)
6ef9501 Merge pull request #2 from mornthx/milestone/M1

$ git branch -a
  main
  milestone/M0
  milestone/M1
* milestone/M2                          (created this session)
  remotes/origin/main
  remotes/origin/milestone/M0
  remotes/origin/milestone/M1
  remotes/origin/milestone/M2           (pushed this session)

$ gh repo view --json defaultBranchRef -q .defaultBranchRef.name
main

$ gh auth status
  ✓ Logged in to github.com account mornthx (keyring, active)
```

Required files present: `CLAUDE.md`, `docs/claude-code/execution-manual.md`,
`docs/architecture/architecture.md`,
`docs/architecture/decisions/ADR-001-rendering-approach.md`,
`docs/milestones/M2-platform-core-abstractions.md`.

**No divergence from the prompt's expected state.** Top three commits match
(`448146c`, `fd64f78`, `ce209aa`). `milestone/M2` did not exist prior to this
session; it was created locally from `main` at `448146c` and pushed to origin
with upstream tracking (see Phase 1 git reports).

The existing module directories under `src/` (`drivers/`, `frame/`, `platform/`,
`utils/`) each contain placeholder `.cpp`/`.hpp` and a `CMakeLists.txt` from M0
bootstrap; these placeholders will be replaced by M2 code. `src/observability/`
already contains real `logging.cpp`/`logging.hpp` from M0 with working
`SF_LOG_*` macros (confirmed by file read during session startup). `tools/`
contains only M1's `spike/` subdirectory.

## 3. Ambiguities and contradictions identified

M2 is an interface-freeze milestone. The spec has been written with care but
still has edges. Each ambiguity below lists my default interpretation, its
downstream blast radius, and whether I would prefer a human clarification before
executing.

### 3.1 `DriverConfig` is promised as a deliverable but never defined

Spec §2.1 item 2 lists `DriverConfig` as an associated value type, annotated
"(tag-dispatched variant or base class)". Spec §4.1 (the authoritative
declaration of `DriverInterface`) shows **no** `configure()` method and no
`DriverConfig` type definition, yet `DriverErrorCode::NotConfigured` exists
(line 172) — implying a configure step that has no representation in the
interface.

- **Default interpretation**: I will NOT invent `DriverConfig` or a
  `configure()` method, because spec §9 Notes-for-CC and spec §4 both say
  "types listed in §4 are exhaustive for M2" and "If you think you need a new
  public type, HALT and ask." I will HALT at the start of the driver-interface
  subtask (S11 in the plan) and ask the human whether: (a) `DriverConfig` is
  out of scope for M2 and the §2.1 mention is stale, in which case
  `NotConfigured` stays as a purely lifecycle-state indicator; or (b)
  `DriverConfig` needs an amendment to §4.1 before I can proceed.
- **Downstream effect**: M3's concrete drivers need a way to receive baud
  rate / host-port / etc. If the answer is (a), M3 will pass config via
  each concrete driver's constructor (no `DriverInterface`-level contract),
  which is viable. If (b), freezing `configure()` now is cheaper than adding
  it post-freeze.
- **Spec clarification preferred**: YES. This is a freeze milestone and the
  discrepancy is too large to guess.

### 3.2 Thread model for `DriverInterface` itself is under-specified

Spec §4.1 Doxygen says signals are emitted "from the driver's IO thread" and
lifecycle methods (`open`/`close`/`start`/`stop`/`write`) "must be called from
the thread that owns the driver". These two statements are only consistent
under one of two architectures:

- **Architecture A (driver-as-QObject-on-main, owns worker on IO thread)**:
  `DriverInterface` lives on the caller's thread; it internally forwards
  work to a worker `QObject` that has `moveToThread()`'d onto a dedicated
  `QThread`. Signals are re-emitted from the `DriverInterface` on the IO
  thread via `QMetaObject::invokeMethod(Qt::DirectConnection)` from the worker.
  But that would violate Qt's "signals are emitted from the QObject's thread"
  invariant unless we make the emitter live on the IO thread.

- **Architecture B (driver-itself-moveToThread'd to IO)**: `DriverInterface`
  is `moveToThread()`'d to its IO `QThread` at `open()`. Then all methods
  must be called via `QMetaObject::invokeMethod(..., Qt::BlockingQueuedConnection)`
  from the caller's thread — which contradicts "must be called from the
  thread that owns the driver" unless "owns" means "the IO thread itself"
  post-move.

- **Default interpretation**: M2 defines the **contract only**; concrete thread
  topology is M3's choice per driver. M2's `DriverInterface` remains on whatever
  thread the concrete driver places it, and the Doxygen will say "IO thread
  = whatever thread the concrete driver uses for its read loop; see the
  concrete driver's class doc for its specific placement." The mock driver
  used in M2's tests will pick Architecture B (move the mock to a dedicated
  `QThread`), which is sufficient to verify the signal/slot mechanics. I do
  NOT ship a prescribed topology for all drivers.
- **Downstream effect**: M3 per-driver decides. `SerialDriver` likely goes
  Architecture A (because `QSerialPort` binds to event loop); `TcpDriver`
  similarly. If this interpretation is wrong and the human wants a prescribed
  topology now, M3's drivers will need to be retrofitted.
- **Spec clarification preferred**: DESIRABLE. I can proceed with the default
  (contract-only, per-driver choice) without HALT if the human does not
  clarify, but I would flag this explicitly in M2-concerns.md and in the
  Doxygen of `DriverInterface`.

### 3.3 `open()`/`start()` synchronous-return vs. async-completion semantics

Spec §4.1 line 213 Doxygen says `open()` is "Non-blocking; emits stateChanged()
transitions. Returns Success immediately on accepted request; actual open may
complete async." This is an unusual contract: a `DriverErrorCode` return
implies synchronous answer, but the "actual open may complete async" implies
the real outcome arrives later via `stateChanged(Open)` or `errorOccurred(...)`.

- **Default interpretation**: `open()` returns synchronously with one of:
  `Success` (request accepted, watch state transitions), `NotConfigured`
  (precondition violated synchronously), `ConfigInvalid` (validation failed
  synchronously). All resource-level failures (`ResourceUnavailable`,
  `PermissionDenied`, `IoFailure`) arrive **asynchronously** via
  `errorOccurred()` followed by `stateChanged(Idle | Error)`. Same for
  `start()`. `close()` and `stop()` return `void` because they are requests
  with no possible synchronous failure. `write()` is concrete-driver-specific
  per the Doxygen — some drivers' `write` may block briefly (Serial) and some
  may queue asynchronously (TCP).
- **Downstream effect**: M3 concrete drivers must not return
  `ResourceUnavailable` synchronously from `open()`. Consumers must not
  assume `open() == Success` means the device is ready — they must wait for
  `stateChanged(DriverState::Open)`. Tests must encode this expectation.
- **Spec clarification preferred**: LOW priority — the default is the only
  interpretation that reconciles the Doxygen with the return type. I will
  document it in Doxygen explicitly and flag in M2-concerns.md.

### 3.4 Thread-safety of `state()`, `health()`, `statistics()` is asymmetric in the Doxygen

Spec §4.1 says `state()` is "Safe to call from any thread" (line 234) and
`statistics()` is "Safe to call from any thread" (line 240). `health()`
(line 238) says nothing about thread safety.

- **Default interpretation**: `health()` is also safe to call from any thread,
  because it is `[[nodiscard]] const` and every const accessor on this
  interface is meant to be thread-safe. I will add the thread-safety note to
  `health()`'s Doxygen explicitly and implement accordingly in the mock.
- **Downstream effect**: M3 concrete drivers must implement `health()` with
  an atomic snapshot (same pattern as `state()`).
- **Spec clarification preferred**: NO — default is the minimum-surprise
  interpretation consistent with the other accessors.

### 3.5 Move/copy semantics and destruction ordering of `DriverInterface`

`DriverInterface` carries `Q_DISABLE_COPY_MOVE(DriverInterface)` — good. But
spec §5.2 "For all types: Construction, move/copy semantics where applicable"
implies we should test both. For `DriverInterface` the test is "verify copy
and move are compile-time deleted." That's fine. More subtle: destruction
ordering. If a driver in state `Running` is destroyed, what happens?

- **Default interpretation**: The **precondition for destruction** is
  `state() == DriverState::Idle`. Destroying a non-Idle driver is undefined
  behaviour (documented explicitly in Doxygen; enforced via debug-build
  `Q_ASSERT` in the base class destructor). Concrete drivers in M3 will
  enforce this by calling `close()` from their own destructors, but
  `DriverInterface` itself does not — the base class has no resources.
- **Downstream effect**: M3 concrete driver destructors must `close()` first.
  Every consumer must similarly call `close()` before `delete`. RAII wrappers
  in M3+ are encouraged but not forced by the base class.
- **Spec clarification preferred**: NO — default is conventional for Qt
  device code.

### 3.6 `Q_DECLARE_METATYPE`'d types must be registered at "program start" — but M2 has no program

Spec §8.2 requires every `Q_DECLARE_METATYPE`'d type (`DriverState`,
`DriverError`, `RawFrame`) to be "registered at program start (document
where)". M2 has no `main()` — there is `src/app/` but no executable is yet
wired. The test harness has a `main()` (Catch2-provided). The crash_test tool
has a `main()` (tool-owned).

- **Default interpretation**: I will expose free functions
  `signalforge::drivers::registerMetatypes()` and
  `signalforge::frame::registerMetatypes()` that callers invoke once at
  program startup. M2's test harness calls them in a Catch2 listener
  (`CATCH_EVENT_LISTENER_SINGLETON` or equivalent) so tests exercise them.
  Crash_test tool calls them in its `main()` before `initCrashReporting`.
  When M3+ builds a real application executable, its `main()` is responsible
  for calling both. This is documented in each header's top-level Doxygen.
- **Downstream effect**: M3 app-bootstrap code must explicitly invoke both
  `registerMetatypes()` functions before any driver is constructed.
- **Spec clarification preferred**: NO — default is standard Qt practice.

### 3.7 `with_fields(...)` thread-local lifetime and `SF_LOG_*` integration

Spec §4.6.1 prescribes a thread-local `with_fields(...)` helper "cleared
after use." Two behaviours are both consistent with "cleared after use":

- **Interpretation A**: Cleared after the **very next** `SF_LOG_*` line, even
  if that line is in a different function up the call stack. Risk: caller
  forgets and the fields leak to an unintended log line, or caller expects
  multiple lines to share fields and is surprised.
- **Interpretation B**: Cleared immediately at `with_fields()` call site after
  writing into the TLS, meaning fields persist until explicitly cleared or a
  subsequent `with_fields()` overwrites. Risk: fields leak between logical
  operations.

- **Default interpretation**: I will implement Interpretation A: fields are
  attached to exactly the next `SF_LOG_*` line on the same thread, then
  cleared. This matches the spec's example "with_fields(...); SF_LOG_WARN(...)"
  pattern. The implementation uses spdlog's custom formatter hooking into
  thread-local storage; the formatter consumes and resets.
- **Downstream effect**: If M3+ wants sustained fields across multiple lines,
  they will need a scoped-fields RAII helper — not promised here, can be
  added post-freeze because `metrics.hpp` and logging additions are **not**
  frozen per spec §6.2.
- **Spec clarification preferred**: LOW — default is the one the example in
  the spec illustrates.

### 3.8 `MpscQueue::push()` OOM semantics vs. moodycamel's `enqueue` exception behavior

Spec §4.4.2 says `push()` "Always succeeds (queue grows). Returns false only if
out of memory." But moodycamel's `ConcurrentQueue::enqueue` documents that OOM
results in `std::bad_alloc` being thrown (or returns `false` for the
`try_enqueue` family with pre-allocated producer tokens).

- **Default interpretation**: Implement `push()` as `try { return q_.enqueue(std::move(item)); } catch (const std::bad_alloc&) { return false; }`.
  Any other exception (e.g., from `T`'s move constructor) propagates — per
  CLAUDE.md Forbidden-7 "no swallowed exceptions". Document this in Doxygen.
- **Downstream effect**: Callers of `push()` treat `false` strictly as OOM.
  This matches the spec wording exactly.
- **Spec clarification preferred**: NO.

### 3.9 `Snapshot<T>` shows two private members in the sketch — not binding, but worth flagging

Spec §4.4.3 sketch contains both `std::shared_ptr<const T> current_` AND
`mutable std::atomic<std::shared_ptr<const T>*> currentPtr_{nullptr};`. Spec
§6.2 explicitly says "Internal implementation details (private methods,
member variable layouts, .cpp-file helpers)" are NOT frozen.

- **Default interpretation**: I will implement `Snapshot<T>` with a single
  `std::atomic<std::shared_ptr<const T>>` member (C++20 feature, GCC 13
  supports it — per spec §7-3, if GCC 13 does not support it cleanly, HALT).
  The two-field sketch is illustrative, not prescriptive; the private layout
  is not frozen.
- **Downstream effect**: None. Public API is what freezes.
- **Spec clarification preferred**: NO.

### 3.10 Spec §2.2-4 references "patch 5 of this spec" which does not exist

Spec §2.2 item 4: "Do not restructure `src/observability/logging.cpp` beyond
what patch 5 of this spec asks." The spec document has no numbered "patches" —
this reads as leftover language from an earlier draft.

- **Default interpretation**: I will restrict logging.cpp changes to exactly
  what §4.6.1 and §4.6.2 require: (a) add a custom formatter or thread-local
  fields hook for the `with_fields(...)` mechanism, (b) keep `init_logging()`
  signature and behavior unchanged, (c) do not touch rotation, async-sink
  thread-pool size, or the SF_LOG_* macro definitions.
- **Downstream effect**: Low — I am being conservative.
- **Spec clarification preferred**: NICE-TO-HAVE, not blocking. I will note
  the dangling "patch 5" reference in `.claude/M2-concerns.md` as an errata
  for the human to address in a future spec edit.

### 3.11 Crashpad integration details deferred to implementation — build-system touchpoint is partially specified

Spec §4.5.3 says "`crashpad_handler` is built from Crashpad's source fetched
via `FetchContent`. The build system discovers its path and passes via
`SIGNALFORGE_CRASHPAD_HANDLER_PATH` CMake variable." Crashpad builds via GN
(Google's meta-build) by default, not CMake, and its CMake wrapping varies
across forks.

- **Default interpretation**: Use a known Crashpad CMake fork (e.g.,
  `getsentry/crashpad` or `backtrace-labs/crashpad`) via `FetchContent`,
  build the handler, expose `SIGNALFORGE_CRASHPAD_HANDLER_PATH` as a
  CMake variable propagated into the Crashpad init code. If no such fork
  builds cleanly on Ubuntu 24.04 + GCC 13, HALT per spec §7-2 (this is an
  explicit hard HALT trigger).
- **Downstream effect**: If Crashpad is unavailable, M2 cannot reach its
  "Must deliver" §2.1-1 obligation. The whole milestone blocks.
- **Spec clarification preferred**: NO — spec is explicit that this is a
  hard HALT if the build fails.

## 4. Thread-affinity verification strategy

Spec requires `DriverInterface` signals emitted from the driver's IO thread
and consumers on other threads must use `Qt::QueuedConnection`. Verification:

**In unit tests (via `tests/mocks/mock_driver.hpp`)**:

- `MockDriver` creates its own `QThread`, does `moveToThread()` on an internal
  worker QObject, and emits `frameReceived`/`errorOccurred`/`stateChanged`
  from that worker. The unit test captures `QThread::currentThread()` inside
  a directly-connected lambda slot (`Qt::DirectConnection`) — this slot runs
  in the **emitter's** thread — and asserts it equals the mock's IO thread
  pointer. This verifies emission-thread identity for each signal type.
- A second test uses `Qt::QueuedConnection` and verifies the receiver runs on
  `QThread::currentThread() == QApplication::instance()->thread()` (the test
  main thread), proving queued delivery works.

**In the integration test (`tests/integration/driver_lifecycle_with_mock.cpp`)**:

- Construct mock on main thread. Verify `mock->thread() == QThread::currentThread()`
  at construction. After `open()`, verify that the mock's internal worker is
  on a different `QThread`. Connect a consumer slot via `Qt::QueuedConnection`
  and verify frames arrive on the test main thread (`QCoreApplication::exec()`-driven)
  with exactly the expected count and no reordering.

**What cannot be verified, must be trusted by design review**:

- That **every future consumer** (M3 SerialDriver's internal dispatch,
  M4 decoder wiring, M5 UI) uses `Qt::QueuedConnection` on cross-thread slot
  connections. We cannot enforce this in M2's tests because consumers don't
  exist yet. Review gate: CLAUDE.md Forbidden-8 is a project-wide rule; code
  review at each future PR enforces it.
- That `RawFrame::payload`'s implicit sharing actually produces O(1) copies
  under real memory pressure. We can test it under controlled allocator
  instrumentation; we cannot prove the QByteArray internal heuristics don't
  switch to deep-copy under some obscure condition. Mitigation: document the
  expectation; if a future profile shows allocation spikes in signal
  delivery, that's an M10 performance concern, not an M2 freeze concern.

## 5. HALT risks I anticipate

Ranked by likelihood, highest first:

### Rank 1 — Crashpad FetchContent or build failure (spec §7-2 hard HALT)

Crashpad is complex, Google-owned, defaults to GN, and its CMake ports vary.
Ubuntu 24.04 + GCC 13 + linking Crashpad's `client` library and
`crashpad_handler` executable is the most fragile touchpoint in the milestone.
Mitigation: implement it **second** (after simple platform value types), so
that if it HALTs, we HALT with minimal wasted work. Estimated probability of
hitting the HALT: ~25-35%. Known mitigations: pinning a commit SHA known to
build; using a CMake-ported fork; documenting the exact fork choice in
M2-concerns.md.

### Rank 2 — `with_fields(...)` / spdlog custom formatter integration (spec §4.6.1 hard HALT)

spdlog's pattern-formatter supports custom flags but not thread-local
attributes naturally. Some integrations use spdlog's `mdc` (mapped diagnostic
context, spdlog ≥ 1.11). If M0's pinned spdlog version is < 1.11, the
MDC path is unavailable. Mitigation: verify spdlog version in S10 before
writing code; if the version lacks clean support, HALT rather than fall back
to string concatenation (spec forbids that fallback). Estimated probability:
~15-20%.

### Rank 3 — Coverage threshold (≥ 85%) on `WatermarkTracker` and `Snapshot<T>`

`WatermarkTracker` is small (maybe 30 lines of impl). At 85% line coverage,
every branch of `observe()` and every reset path must be exercised. Doable
but requires deliberate test enumeration. `Snapshot<T>`'s generic nature means
we need a `Snapshot<int>` test and a `Snapshot<some-non-trivial-type>` test
plus a ThreadSanitizer test. Estimated probability of missing the threshold
on first pass: ~15%. Mitigation: write tests with coverage in mind (table-
driven threshold crossings).

### Rank 4 — `std::atomic<std::shared_ptr<T>>` GCC 13 availability (spec §7-3 hard HALT)

GCC 13 ships `std::atomic<std::shared_ptr<T>>` as part of C++20. libstdc++
implementation has had correctness fixes through 13.2+. Ubuntu 24.04's
default GCC is 13.3. Low probability (~5%) that it's unavailable or subtly
buggy for our usage. If triggered, HALT per spec.

### Rank 5 — MpscQueue 8×100k stress test flakiness (spec §7-5 any flake = HALT)

Not due to moodycamel's correctness (it's battle-tested), but due to test
timing harness on a CI runner under ThreadSanitizer. TSan slows execution
~10×. If the test depends on wall-clock convergence (e.g., "poll
`sizeApprox()` until zero"), it can flake under TSan. Mitigation: use
deterministic synchronization (barriers, `QFuture` / `std::latch`) instead of
polling; ensure every producer completes before consumer counts total.
Estimated probability of hitting: ~10%.

### Rank 6 — `DriverConfig` ambiguity (my §3.1 above)

I have committed to HALTing at the driver-interface subtask until the human
resolves §3.1. Not strictly a HALT **risk** — it's a planned HALT. Listed
here so I am transparent that S11 will stop and wait.

## 6. What I will not do in M2

Explicit out-of-scope list per spec §2.2:

1. **No concrete driver implementations**. `SerialDriver`, `TcpDriver`,
   `UdpDriver`, `ReplayDriver` — all are M3.
2. **No frame pipeline wiring**. I define `RawFrame` and
   `WatermarkTracker`; I do not wire driver → decoder. That's M4.
3. **No UI code**. No `QQuickWidget`, no performance panel (M5), no
   workbench shell.
4. **No main application executable**. `src/app/` is untouched beyond what
   M0 may have placed there. `crash_test/` is the only new executable, and
   it is a standalone CMake project.
5. **No new top-level dependencies**. I stay within
   `docs/architecture/architecture.md §4.1`. If I need one I HALT.
6. **No reorganization of `src/observability/logging.cpp`** beyond adding the
   structured-fields helper and minimal formatter wiring per §4.6.1.
7. **No modification of forbidden files**: `docs/architecture/**`,
   `CLAUDE.md`, any schema file, `CMakePresets.json` Qt path fields, the M2
   spec itself.
8. **No deliberate-crash tests in `tests/unit/`**. Crash_test tool only, per
   spec §3.4.
9. **No merging to main, no PR creation, no tagging** — those are Phase 3
   operations, not authorized this session nor in the planned M2 execution
   session (Phase 5 authorizations end at push to `milestone/M2`).

## 7. Review checkpoints I expect

Per CLAUDE.md §Git operation protocol and the M2 session prompt's closure
model:

- **This session ends at Phase 1 close**, immediately after `git push origin
  milestone/M2` commits `.claude/M2-understanding.md` and `.claude/M2-plan.md`.
- **Phase 4 (human checkpoint B)** is the mandatory execute-approval gate.
  After the human reads both documents, the literal approval phrase
  `approved, execute M2` (case-insensitive, whitespace-tolerant) authorizes
  Phase 5 execution.
- **Phase 5 (CC autonomous)** — executing M2 subtasks — happens in a
  **separate future session**. I will not begin any implementation, any
  modification to `src/`, any creation of `tools/crash_test/`, or any commit
  beyond the understanding/plan artifacts in this session.
- **Phase 1 (next time)** at end of M2 execution: CI green → PR created →
  `.claude/M2-done.md` written with the Freezes section and sha256 record →
  announce "M2 ready. Awaiting approval to merge M2 and begin M3 bootstrap".
- **Phase 2** (human checkpoint A) then gates merge authorization and M3
  bootstrap.

I will not merge Phases 1 and 2 into a single approval, nor collapse Phase 4
into Phase 2's approval. Each checkpoint is mandatory per CLAUDE.md §Git
operation protocol.
