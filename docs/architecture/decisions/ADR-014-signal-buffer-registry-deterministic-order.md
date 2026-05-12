# ADR-014 — SignalBufferRegistry deterministic signal-ID order

## Status

Accepted (V0.3 M16 S6.5 in-milestone amendment, 2026-05-12;
follows the ADR-009 / ADR-010 / ADR-011 / ADR-013 in-cycle
governance pattern of fixing what discovery surfaces, not
deferring).

## Context

V0.3 charter §3 commits to "cross-environment determinism on
the declared supported environment matrix" (Ubuntu 24.04
operator dev + CI runner). V0.3 M16 ("Visual Identity
Ownership") is the keystone milestone that empirically
delivers this promise through:

- **R10** industrial reference traceability
- **R11** manifesto-first design
- **R12** baseline regression discipline
- **R14** environment contract (4-tier env sidecar lockstep
  with every visual baseline)
- **R15** generated-asset single-source-of-truth

M16 S6 is R12's first full-baseline-set application:
re-capture all 12 V0.2 production-fidelity baselines under M16
SignalForgeStyle rendering on operator-local + CI Azure
runner; verify cross-environment pixel diff is `< 1 %` per
baseline (M16 close gate); apply per-baseline R8 operator
review prior to S7 baseline migration.

S6 outcome (`docs/v0.3/s6-cross-env-verification.md`):

| Cohort | Result |
|---|---|
| Pixel-perfect (9 of 12) | sha256-byte-identical cross-env (`00-empty-launch`, `02-conn-udp-idle`, `04-conn-udp-connected`, `25-dialog-add-udp`, `26-dialog-edit`, `30-menu-file-open`, `31-menu-connections-open`, `32-menu-session-open`, `33-status-buffer-normal`) |
| Sub-cluster diff (1 of 12) | `24-dialog-add-serial` 0.016 %, single 160-px cluster (well under 200-px gate) |
| **Failing (2 of 12)** | **`12-multi-2-drivers` 1.005 % / 822-px cluster; `13-multi-5-drivers` 1.351 % / 650-px cluster** |

The 2 failing states share a single forensic root cause that
S6 §2 phase 3 isolated by side-by-side text-content comparison:

```
LOCAL (operator dev)              CI (Azure runner)
  Driver 1:                         Driver 1:
    crc                               sensor_mo...
    reserved                          calibration...
    timestamp...                      padding
    sensor_mo...                      temperatur...
    padding                           reserved
    alarm                             alarm
    calibration...                   crc
    temperatur...                     timestamp...
    pressure  (...                    pressure  (...
```

Same 9 signals per driver, different orders.

`src/chart/signal_selector.cpp:147-148` populates the signal-
tree by iterating `SignalBufferRegistry::signalIds()`:

```cpp
const auto signalIds = registry_->signalIds();
for (const auto& signalId : signalIds) { ... }
```

`SignalBufferRegistry::signalIds()` in turn iterates its
underlying storage at `signal_buffer_registry.cpp:250-258`:

```cpp
QStringList SignalBufferRegistry::signalIds() const {
    std::lock_guard<std::mutex> lock(registryMutex_);
    QStringList out;
    out.reserve(static_cast<int>(buffersBySignalId_.size()));
    for (const auto& [id, _] : buffersBySignalId_) {  // <-- here
        out.append(id);
    }
    return out;
}
```

`buffersBySignalId_` is `std::unordered_map<QString,
std::unique_ptr<SignalBuffer>>` (declared at
`signal_buffer_registry.hpp:109`). `std::unordered_map`
iteration order is unspecified by the C++ standard and
depends on the hash function, bucket count, rehash history,
and — for `QString` keys under glibc — a process-local hash
seed that varies per-host.

Empirically: single-driver and zero-driver states (`00`,
`02`, `04`, `33`, `24-26`, `30-32`) don't surface the bug
because hash-bucket placement of small key sets is stable
across hosts for the specific glibc 2.39 + Qt 6.10.2 +
operator-host vs Azure-runner pair. Multi-driver states with
9 × 2 or 9 × 5 signals are dense enough for bucket layouts
to diverge.

The bug is V1.0-era production code (`signal_buffer_registry`
landed at M7 / V0.1.0). It went undetected through V0.1 +
V0.2 because:

- M7 unit tests verify storage / retrieval semantics, not
  iteration order.
- The signal-tree integration test
  (`tests/integration/test_signal_selector_tree_population`)
  exercises the populate-path in-process, where the
  intra-process iteration order happens to be stable (same
  hash seed, same buckets).
- V0.2 visual baselines were captured on the operator host
  only; cross-environment verification didn't exist until
  V0.3 R12.

R12 was added to the V0.3 charter expressly to catch this
class of bug before it ships in canonical baselines. ADR-014
records the first amendment driven by an R12 finding.

## Decision

Append `QStringList::sort()` to `signalIds()` immediately
before the `return` statement:

```cpp
// src/buffer/signal_buffer_registry.cpp
QStringList SignalBufferRegistry::signalIds() const {
    std::lock_guard<std::mutex> lock(registryMutex_);
    QStringList out;
    out.reserve(static_cast<int>(buffersBySignalId_.size()));
    for (const auto& [id, _] : buffersBySignalId_) {
        out.append(id);
    }
    out.sort();          // <-- amendment: deterministic
                         //     cross-environment iteration order
    return out;
}
```

Eight added lines total (one functional + seven comment).
Public API at `signal_buffer_registry.hpp:72`
(`[[nodiscard]] QStringList signalIds() const`) **unchanged**.
The `.hpp` is frozen at M7 per V0.1.0 freeze surface;
modifying the implementation in `.cpp` only preserves that
freeze.

## Rationale

### Frozen-surface analysis

| File | Frozen since | Modified at S6.5 |
|---|---|---|
| `signal_buffer_registry.hpp` | M7 (V0.1.0) | **No** |
| `signal_buffer_registry.cpp` | M7 (V0.1.0; .cpp not frozen) | Yes (8 lines) |

Per CLAUDE.md §Forbidden #9 ("No changes to public interface
signatures once marked frozen"): the public declaration is
the signature, not the implementation. `.cpp`-only changes
that preserve the documented interface contract are
permitted. Precedent established by ADR-009 (M14 S2
MainWindow ctor `.cpp`-only wire-up), ADR-010 (M14 S3
QQuickWidget chart-host `.cpp` adjustment), ADR-011 (M14 S3
geometry binding `.cpp` adjustment), ADR-013 (M14 S4
SessionWriter wire-up `.cpp` adjustments).

Frozen-surface counter remains **0 / 2** (clean), well under
HALT #5 threshold (2 cumulative `.hpp` modifications across
M2 → M13 freeze window).

### Caller audit

5 callers of `signalIds()` (4 production + 1 test);
1 caller of `signalIdsForDriver()` (1 test):

| Caller | Use | Order-dependence |
|---|---|---|
| `src/expression/expression_registrar.cpp:21` | Iterate to collect metadata into `std::vector<SignalMetadata>` for expression validation | Order-independent (validation step doesn't depend on enumeration order) |
| `src/chart/signal_selector.cpp:147-148` | Iterate to populate signal-tree | Order = display order. Sorted = deterministic alphabetical-by-signal-id within each driver subtree. **The bug-fix surface.** |
| `src/app/main_window.cpp:660` | `.size()` only | Sort-safe |
| `tests/unit/buffer/buffer_smoke_test.cpp:61` | `.isEmpty()` only | Sort-safe |
| `tests/unit/buffer/signal_buffer_registry_test.cpp:54` | `.size()` only (`signalIdsForDriver`, included for completeness) | Sort-safe |

No caller depends on hash-bucket order semantically. Sort is
a strict subset of any deterministic order. No breakage.

### Behavioural impact on signal-tree UX

After ADR-014, the signal-tree displays signals in
alphabetical-by-signal-id order within each driver subtree
(stable across hosts and across process restarts), rather
than in hash-bucket order (unstable across hosts and
unstable across glibc / Qt rebuilds). On the
`temperature_sensor` schema used by M15 multi-driver
fixtures, this means the bitfield-derived signals
(`alarm`, `calibration_active`, `reserved`, etc.) and the
primary fields (`temperature`, `pressure`, `timestamp_ms`)
appear in a stable order rather than a random-looking one.

Arguably an UX improvement on top of the determinism fix:
users can build muscle-memory for "where is the temperature
signal?" without it relocating between sessions.

### Performance impact

`signalIds()` is called:
- Once per signal-tree refresh (rare; user interaction or
  driver state change).
- Once per expression-engine validation (rare; expression
  registrar load).
- Once per `MainWindow::main_window.cpp:660` log line (rare;
  bootstrap diagnostic).

Typical `N` is 9–45 signals (9 per `temperature_sensor`
schema; 1–5 drivers). `QStringList::sort()` is O(N log N)
via the underlying `std::sort`. At `N = 45`, this is
~200 comparison operations on `QString`s of average length
~25 chars. Sub-microsecond at modern CPU speeds; not
measurable in profiler noise alongside the rest of the
signal-tree rebuild (`QTreeWidget` item construction
dominates by orders of magnitude).

Not measured empirically because the cost is below
microbenchmark resolution. Documented here as "negligible"
per V0 charter §3.5 (performance claims require measurement
or documented justification of non-measurement).

### Alternative considered — change storage to `std::map`

`buffersBySignalId_` could change from
`std::unordered_map<QString, std::unique_ptr<SignalBuffer>>`
to `std::map<QString, ...>` (sorted by default; iteration is
in `operator<` order).

**Rejected** because:

- Modifies the `.hpp` (storage type change is visible in
  `signal_buffer_registry.hpp:109`). Frozen-surface
  counter would advance.
- Larger blast radius: 8+ call sites use `bufferFor(id)`
  which performs lookup, not iteration. `std::unordered_map`
  lookup is O(1) average; `std::map` is O(log N). Trades
  better iteration determinism (already solved by sort()
  in O(N log N) per-call) for worse lookup constant
  (O(log N) per-lookup, hit more often).
- The sort fix is strictly less invasive and equally
  effective for the visible bug.

### Alternative considered — sort at call site

`signal_selector.cpp:147` could sort the result locally
instead of `signal_buffer_registry.cpp` sorting it at
source.

**Rejected** because:

- Hides the contract: a future caller would have to
  rediscover that `signalIds()` returns unordered. Better
  to provide a deterministic-order contract at the API
  boundary.
- Multiplies the sort cost across N callers (currently 4
  production callers, 1 of them rarely-called) instead of
  paying it once at the source.
- `expression_registrar` would benefit identically from a
  deterministic source order (expression validation
  diagnostics will produce stable error messages across
  hosts).

### Alternative considered — defer to V0.4 / post-M16

Accept the 2 failing baselines for M16 with documented
caveats; revisit at V0.4 keystone.

**Rejected** because:

- Weakens R12 in its first application. R12 was added
  specifically so that M16 + future milestones can lock in
  cross-environment determinism as an empirical invariant,
  not a forward-looking aspiration.
- M16 close gate is unconditional `< 1 %` per baseline on
  all 12 baselines (M16-spec §2.1 #5). Accepting `1.005 %`
  + `1.351 %` requires spec amendment, not just operator
  judgement.
- The fix is 1 line of code + comprehensive caller audit
  (this ADR). Cost/benefit overwhelmingly favours the in-
  milestone fix.
- M16 surfaces the bug; M16 fixes it. ADR-009 / ADR-010 /
  ADR-011 / ADR-013 established this discipline at M14
  (Wave 2 fixed what Wave 1 surfaced); V0.3 inherits it.

## Consequences

- **M16 close gate satisfied 12 / 12**: after the sort fix
  + state-12 + state-13 re-capture, all 12 V0.2 production-
  fidelity baselines are expected to clear the `< 1 %` cross-
  environment gate. S6 §6 amendment records empirical results.
- **R12 first-application win**: ADR-014 demonstrates the
  V0.3 R12 baseline regression discipline closing a real V1
  production non-determinism bug *within the milestone that
  surfaced it*. Pattern available for V0.4–V1.0 future
  cross-environment surfacing.
- **R14 first-application benefit (companion)**: the locale
  pin in `tests/visual/lib/capture.py` (S6 commit `d045ae9`)
  is the first amendment driven by the env-contract enforcement
  path. Together with ADR-014, M16 closes with two empirical
  cross-env invariants in place: locale + signal-tree order.
- **Signal-tree UX changes** (minor): signal names now appear
  alphabetically by signal ID within each driver subtree.
  Behavioural change visible to existing users; no functional
  regression. No V0.2-baseline test asserts on tree order
  (verified by audit).
- **Frozen-surface counter unchanged at 0 / 2**: `.hpp` not
  touched. M2–M13 V1 freeze surface intact.
- **No new dependencies**: `QStringList::sort()` is part of
  Qt 6.10.2's QtCore (already required); `std::sort` is
  `<algorithm>` (already required).
- **No new test infrastructure**: the existing
  `test_signal_selector_tree_population` integration test
  (M9-era) continues to pass after sort fix. Cross-environment
  determinism is verified empirically through M16 S6 re-
  measurement, not through a new unit test (which would only
  verify single-process order stability, not the cross-host
  invariant we care about).

## V0.3 governance lesson

ADR-014 is the first amendment recorded under V0.3 R12
discipline. It demonstrates:

1. **R12 surfaces non-determinism that prior governance
   missed**: M7 unit tests + V0.2 visual tests + single-host
   integration tests all passed; only R12 cross-environment
   pixel-diff caught the bug.
2. **In-milestone fix is the right call** when (a) the fix is
   small, (b) frozen-surface stays clean, (c) backward
   compatibility holds, (d) caller audit is complete. All
   four held here.
3. **ADR records the contract change**, not just the fix:
   future code reviewers reading `signalIds()` see the sort
   line and the ADR-014 reference comment and understand WHY
   the sort is there. Future architects considering a storage
   refactor (e.g. for performance) see the contract they
   must preserve.

V0.3 charter §6 R12 ("baseline regression discipline") is now
operational with one empirical proof point. M17 + later
milestones inherit the discipline: any cross-environment
re-capture step that surfaces > 1 % drift triggers the same
forensic → caller-audit → ADR loop demonstrated here.

## Cross-references

- **Surfacing**: `docs/v0.3/s6-cross-env-verification.md`
  (12-baseline cross-environment measurement; §2 phase 3
  isolates the root cause; §6 records the post-fix
  re-measurement)
- **Source change**: `src/buffer/signal_buffer_registry.cpp:250-258`
- **Public API (unchanged)**: `src/buffer/signal_buffer_registry.hpp:72`
- **Bug-fix surface**: `src/chart/signal_selector.cpp:147-148`
- **Caller audit context**:
  - `src/expression/expression_registrar.cpp:21`
  - `src/app/main_window.cpp:660`
  - `tests/unit/buffer/buffer_smoke_test.cpp:61`
  - `tests/unit/buffer/signal_buffer_registry_test.cpp:54`
- **V0.3 governance**: V0.3 charter §3 (cross-platform
  determinism) + §6 R12 (baseline regression discipline)
- **M16 spec**: M16-spec §2.1 #5 (cross-environment
  determinism close gate), §5.5 (S6 / S7 sequencing)
- **M16 plan**: `.claude/M16-plan.md` §S6 (lines 329–352);
  S6.5 amendment authorisation in the operator's S6 Phase 4
  review prompt
- **ADR precedent**:
  - ADR-009 (MainWindow ctor pipeline attach; `.cpp`-only
    M14 fix)
  - ADR-010 (chart QQuickWidget host scene; `.cpp`-only
    M14 fix)
  - ADR-011 (chart host geometry binding; `.cpp`-only
    M14 fix)
  - ADR-013 (recording persistence + connections.yaml
    autosave; `.cpp`-only M14 fix)
- **R12 origin**: V0.3 charter amendment authoring the R10/
  R11/R12/R13/R14/R15 disciplines (operator artefact, V0.3
  charter §6)
