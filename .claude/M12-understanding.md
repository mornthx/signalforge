# M12 — Understanding

Source of truth: `docs/milestones/M12-performance.md` (511 lines,
merged to `main` at `ae77820` via PR #21). Architectural prereqs:
all M2-M11 freezes + M5-M11 baseline.md files at
`tests/benchmark/results/`. M11 closure carried 3 follow-ups (30-min
soaks + combined hardware verification + replay perf) — M12 is the
natural home for the perf one if profile says so.

Cross-reference notation matches the spec (`[M12 §X]`, `[M11-baseline]`,
`[ADR-N]`, `[CM §Z]`).

---

## 1. Goal in one paragraph

M12 makes V1 **fast** without changing what it does. Profile the
V1 codebase against three representative workloads (live, replay,
concurrent record + chart), select 2-3 hot paths from the
candidate list (or profile discovery), implement each as an
internal optimisation that yields ≥ 10 % improvement on a
documented metric, and verify nothing else regresses by more than
5 % vs the M11 closure baseline. M12 closes V1 with a
performance-tested, regression-protected codebase ready for M13
packaging.

## 2. What ships (per spec §2.1)

1. `tools/profile/` — profiling helper scripts + `profile_main.cpp`
   harness wiring `perf` / `valgrind` / `QElapsedTimer` around the
   three required scenarios.
2. `tests/benchmark/results/M12-profile-report.md` — top-10
   hottest functions per scenario + the top-3 selected
   optimisations with rationale.
3. **2-3 optimisation implementations**, one per subtask, each
   backed by:
   - A specific, measured before/after benchmark
   - ≥ 10 % improvement (M12.3 G)
   - 3-run reproducibility, variance < 5 %
   - All M0-M11 benchmarks within 5 % of M11 closure baseline
4. `tests/benchmark/m12_regression_suite.sh` (or CMake target) —
   reruns M0-M11 benchmarks every commit, exits non-zero on > 5 %
   regression.
5. `tests/benchmark/results/M12-regression-suite.md` — documents
   the cycle.
6. Optional `docs/architecture/decisions/ADR-008-*.md` — only if
   an optimisation forces a frozen-surface change. Default: no
   ADR (M12.5 N).
7. `tests/integration/test_m12_no_functional_regression.cpp` +
   `tests/integration/test_m12_optimization_correctness.cpp`.
8. Unit tests for any new internal helper.
9. `tests/benchmark/results/M12-baseline.md` — final V1.0
   performance reference, per-optimisation breakdown +
   regression results table.
10. `.claude/M12-done.md` — completion report + sha256 verification
    that M2-M11 freeze surface is intact.

## 3. Out of scope (per spec §2.2)

- New V1 features (M11 functional surface = V1.0 release surface).
- Modifications to M2-M11 frozen `.hpp` without ADR-008. M11's
  M10 SessionReader extension was permitted only because
  `session_reader.hpp:33-36` + M10-done.md §Freezes explicitly
  excluded the reader from M10's freeze; **no other module has
  that out**.
- New top-level dependencies (use `perf` / `valgrind` / Qt's
  built-in profiler).
- Premature optimisation — the spec explicitly forbids skipping
  the profile phase to optimise "obvious" things (M12.1 hybrid).
- Drive-by refactors unrelated to identified hot paths.
- Removing safety checks for speed (use compiler hints, not
  bounds-check elision).
- Optimisations < 10 % improvement (under that bar, not worth M12
  budget).

## 4. Locked design decisions (spec §3)

| ID | Decision | Implication |
|---|---|---|
| M12.1 C | Hybrid spec/profile-driven | Spec lists candidates, profile selects |
| M12.2 F | Profile-driven discovery permitted | Top 3 may include profile-found alternatives, not just §2.1-4 list |
| M12.3 G | Per-optimisation measurable improvement | Aggregate scores rejected; each opt has its own number |
| M12.4 J | Per-commit regression protection | M0-M11 benchmarks rerun every commit; > 5 % drop = HALT #1 |
| M12.5 N | Frozen-surface protection via ADR-008 | Default is no ADR; if forced, ADR + human approval BEFORE code change |
| M12.6 | No soft-HALT (inherits M2-M11) | Same as prior milestones |
| M12.7 | Metric naming `m12_<...>` for new internal counters | E.g., `m12_replay_dispatch_batch_size` |

## 5. Candidate optimisations (spec §2.1-4 — reference, not prescription)

The spec lists 8 candidates harvested from M0-M11 known concerns.
S2 (profile execution) determines which 2-3 actually rank in the
top by impact × confidence × scope. Listed here for reference,
**not** as a commitment:

| # | Candidate | Source | M11-baseline marker | Risk |
|---|---|---|---|---|
| 1 | M11 1× timing accuracy (12 % → < 5 %) | M11-baseline.md §Findings | confirmed gap; live data | Medium (sleep_until + batching, C4 Stage B) |
| 2 | M11 10× completion (2.3 s → < 1 s) | M11-baseline.md §Findings | confirmed gap; live data | Medium (same as #1) |
| 3 | M11 backward seek O(N) | M11 spec §9 Note 2 | medium-impact for large files | High (in-memory index design) — likely defer to V1.5+ per spec §9 Note |
| 4 | M6 SignalBuffer push wrapper (12-16 % overhead) | M6-baseline.md | medium throughput | Low (internal) |
| 5 | M5 decoder hot path | M6 ADR-004 | medium throughput | Low (cache strategy) |
| 6 | M8 chart redraw (LOD bin / SG node reuse) | M8-baseline.md | already 50× margin | Low |
| 7 | M7 ExpressionEngine tick cost | M7 plan §8 | unmeasured | Low |
| 8 | SessionWriter 60-min memory growth | M10 §5.6 | unknown until soak run | Low |

**Most likely top 3** (informed expectation; profile arbitrates):
1. **C4 Stage B** — M11 1× timing + 10× completion. Strong
   evidence (M11-baseline.md), clear implementation
   (`SessionPlayer::dispatchLoop` rework: `sleep_for` →
   `sleep_until` + batched-dispatch ring buffer). Spec §9 Note
   explicitly calls this out as "the most likely candidate".
2. **M6 push wrapper** — backed by M6 baseline showing wrapper
   accounts for 12-16 % of per-event overhead in live mode.
   Well-scoped internal change.
3. **TBD by profile** — slot reserved for whatever scenario A
   (live) profile shows. Likely a specific decoder hot-path
   refinement (#5) or chart redraw (#6) depending on scenario.

## 6. Concerns surfaced (recorded canonically in `.claude/M12-concerns.md`)

### C1 — Optimisation budget pressure on top 3

Spec §5.1 acceptance allows **2-3** optimisations; spec target
"top 3" is flexible. C4 Stage B is two interrelated improvements
(1× and 10× timing) implemented in the same code path. Treating
them as **one optimisation** (counts as 1 of 3) is the natural
reading; the 10 % improvement bar applies to each *metric* (1×
error, 10× wall-time) separately, both metrics must clear 10 %.

**Resolution proposal**: count C4 Stage B as a single
optimisation (1 commit, 1 progress entry) covering 2 metrics.
Each metric independently must clear the 10 % bar. Documented in
M12-progress.md S3.

### C2 — Profile harness portability

Spec §4.1 calls for `perf record / perf report` (Linux). The host
runs Linux but `perf` requires kernel `perf_event_paranoid` ≤ 2
which may be locked down. Fallback: `valgrind --tool=callgrind`
+ `kcachegrind` (heavier but works without root). Plan §S1
documents both, S1 verifies which works on the host.

**Resolution**: S1 detects host capability + writes the harness
to use whichever is available. M12-profile-report.md records
which tool produced the report.

### C3 — Regression suite cost

Spec §3.4 mandates re-running M0-M11 benchmarks every commit.
Spec estimates ~30 min per cycle. M12 has 6+ commits (S1 profile,
S2 report, S3-S5 optimisations, S6 baseline, S7 done.md). Total
regression-cycle time: 6 × 30 = 180 min wall-clock. CI also
re-runs on every push.

**Resolution**: keep S0-S2 docs-only (no regression cycles
needed). S3-S5 optimisations each pay one regression cycle. Final
S6 + S7 run the full suite once each. Total: ~3-4 regression
cycles, manageable.

### C4 — Profile may show no clear top 3

If profile shows only 1 viable optimisation (e.g., everything is
already at the noise floor except C4 Stage B), spec §5.1 allows
2-3 deliverables. Going below 2 would be a HALT.

**Resolution**: at S2 end, if profile shows < 2 viable
optimisations, HALT and propose alternative direction (e.g.,
benchmark coverage extension, soak run, or accept "M12 ships
1 optimisation + thorough profile + regression suite as proof of
discipline"). Document path in C4 with explicit decision tree.

### C5 — M11 backward seek likely defer

Spec §9 Note explicitly flags M11 backward seek O(N) as
**high-risk** (in-memory index design). Per the same note,
likely defer to V1.5+ unless profile shows it's high impact.

**Resolution**: profile (S2) measures backward-seek explicitly
on a 600 k-record file. If wall-time > 5 s, escalate to
optimisation candidate. Otherwise document as V1.5+ deferred.

### C6 — ADR-008 trigger discipline

Spec §3.5 N + §6.1 + §7-2 all converge: any frozen-surface change
requires ADR-008 + human Phase-4-style approval **before** code.
Default expectation: 0 ADR-008.

**Resolution**: at S3-S5 implementation start, CC verifies the
optimisation does NOT touch any frozen `.hpp`. If a tempting
optimisation requires touching a frozen surface, HALT and write
ADR-008 stub before any code change. M2-M11 freeze sha256s
verified at S6 (final baseline) per acceptance §8.4.

## 7. Integration surfaces I will touch

| File / area | Status | Why I touch it |
|---|---|---|
| `tools/profile/*` (new) | new tool dir | S1 harness |
| `tests/benchmark/results/M12-profile-report.md` (new) | docs | S2 report |
| `tests/benchmark/results/M12-baseline.md` (new) | docs | S6 final baseline |
| `tests/benchmark/results/M12-regression-suite.md` (new) | docs | regression cycle log |
| `tests/benchmark/m12_regression_suite.sh` (new) | tool | S2 regression infra |
| `src/replay/session_player.cpp` (likely) | M11 non-frozen .cpp | C4 Stage B (probable optimisation 1) |
| `src/buffer/*.cpp` (possible) | M6 non-frozen .cpp | M6 push wrapper (possible optimisation) |
| `tests/integration/test_m12_*` (new) | new tests | regression + correctness |
| `docs/architecture/decisions/ADR-008-*.md` (conditional) | conditional | only if frozen `.hpp` touched |
| `.claude/M12-done.md`, `.claude/M12-concerns.md`, `.claude/M12-progress.md` | new docs | governance |

## 8. Definition of done (per spec §8 + CLAUDE.md)

A task is done when **all** of these hold (any miss → keep working
or HALT):

1. Code compiles cleanly under Debug / Release / debug-asan.
2. ctest green on Debug + Release; debug-asan green on CI.
3. All M0-M11 benchmarks within 5 % of M11 closure baseline.
4. Each optimisation has ≥ 10 % improvement on its specific metric,
   reproducible across 3 runs with < 5 % variance.
5. `clang-format -i` clean; `clang-tidy` no new warnings.
6. M12-progress.md tracks every subtask with before/after numbers.
7. M2-M11 freeze sha256s verified intact (no ADR-008 by default).
8. Profile report + final baseline + regression suite docs all
   published in `tests/benchmark/results/`.
9. PR opened to `main`, CI green, awaiting Phase 2 approval.

## 9. Effort sketch

Spec target: 5-7 person-days. M12's discipline (profile-first,
per-commit regression cycles) makes it heavier than M11 in
governance overhead but lighter in code surface (no new modules).

Likely subtask sizes:
- S0 concerns: ~150 LOC docs
- S1 profile harness: ~300 LOC tool + scripts
- S2 profile report: ~250 LOC docs
- S3-S5 optimisations: ~150 LOC code each + ~100 LOC tests each
- S6 final baseline: ~200 LOC docs
- S7 done.md: ~400 LOC docs

Total ~2 500 LOC, mostly docs + small targeted optimisation
patches + tests. Well within the M11 closure footprint.

## 10. Phase 2 follow-ups inherited (from M11)

These are tracked but **non-blocking** for M12 progression:

1. M10 30-min memory soak via `bench_session_writer --soak 1800`.
   May fold into M12 S2 profile if memory growth is a concern.
2. M11 30-min memory soak via `bench_replay --memory-soak 1800`.
   Same — may fold in.
3. Combined M9 + M10 + M11 manual hardware verification (18
   tests). Operator-run; not in M12 scope.
4. M11 1× / 10× perf optimisation. **Likely top candidate** for
   M12 S3 — profile arbitrates.

These items are **hand-off line items**, not deliverables of M12
itself. Items #1, #2, #4 may close inside M12 if profile-driven;
#3 stays for the V1.0 dogfood session.

---

## 11. Critical execution reminder

Per spec §9 + your Phase 4 review: **Profile FIRST, optimise
SECOND**. S1 = harness; S2 = report + selection; S3+ = code.
CC may NOT skip the profile phase. If the spec's candidate list
"feels obvious", that's not enough — measure first. The hybrid
M12.1 C decision exists exactly to prevent this failure mode.
