# M12 — Performance

| Field | Value |
|---|---|
| Milestone ID | M12 |
| Sprint | 12 |
| Estimated effort | 5-7 person-days |
| Prerequisites | M11 closed (main at v0.0.12-alpha.1 or later) |
| Next milestone | M13 (Packaging) |
| Hard-stop type | **Performance certification** (top 3 profiled hot paths each show measurable improvement) + **Regression protection** (M0-M11 benchmarks each within 5% of M11 closure baseline) |
| Soft-HALT allowed | **No** |
| Branch | `milestone/M12` |

**Cross-reference notation**:

- `[EM §N]` — Execution Manual, section N
- `[Arch §N]` — Architecture document, section N
- `[MR]` — Milestone Roadmap
- `[CM §X]` — CLAUDE.md, section X
- `[ADR-N]` — Architecture Decision Record N
- `[M<n> §N]` — M<n> spec
- `[M<n>-baseline]` — `tests/benchmark/results/M<n>-baseline.md`

---

## 1. Goal

M12 is V1's **performance optimization milestone**. Unlike M0-M11 (feature milestones), M12 does NOT create new functional modules. Instead, M12:

1. **Profiles** the V1 codebase to identify actual bottlenecks (vs assumed)
2. **Optimizes** the top 3 hot paths discovered
3. **Verifies** each optimization improves a documented metric
4. **Protects** M0-M11 functionality from regression (every benchmark must stay within 5% of M11 closure baseline)

**No new V1 features**. M11 functional surface is the V1.0 release surface (modulo M13 packaging). M12 makes V1 fast where it matters.

After M12, V1 has documented performance characteristics suitable for **commodity hardware deployment** (per arch §3 hardware target). Future versions inherit a regression-tested baseline.

Quality philosophy:
- **Measure, don't guess**: profile first, optimize second
- **Top 3 by impact**, not "everything I can think of"
- **Reproducibility**: every optimization has a before/after benchmark documented
- **Regression discipline**: M0-M11 functional tests + benchmarks rerun every commit

---

## 2. Scope

### 2.1 Must deliver

1. **`tools/profile/`** at `tools/profile/` (mirrors `tools/sfreplay_inspect/`):
   - Profiling helper scripts + harness
   - `profile_main.cpp` runs representative workloads under perf / valgrind / Qt-built-in profiling
   - Outputs flame graphs / hotspot tables

2. **Profile report** at `tests/benchmark/results/M12-profile-report.md`:
   - Documented in S1 of M12 implementation
   - Top 10 hottest functions across V1 workloads
   - Lists top 3 candidate optimizations
   - Selection rationale (impact × confidence × scope)

3. **Top 3 optimization implementations**:
   - Each optimization: code change + before/after benchmark + rationale
   - Documented in `M12-progress.md` per S3-S5 implementation
   - Frozen-surface protection: optimizations must NOT modify M2-M11 frozen `.hpp` (HALT trigger #2). Internal `.cpp` changes OK.

4. **Optimization candidate list** (from V1 known concerns, **not prescriptive**):

   | Candidate | Source | Impact estimate | Risk |
   |---|---|---|---|
   | M11 1× timing accuracy (12% → <5% target) | M11 §5.1 | High user-visible | Medium (sleep_until + batching) |
   | M11 10× completion (2.3s → <1s) | M11 §5.1 | Medium user-visible | Medium (same as above) |
   | M11 backward seek O(N) | M11 §9 Note 2 | Medium for large files | High (in-memory index design) |
   | M6 SignalBuffer push wrapper (12-16% of overhead) | M6 baseline | Medium throughput | Low (internal optimization) |
   | M5 decoder hot path | M6 ADR-004 | Medium throughput | Low (cache strategy refinement) |
   | M8 chart redraw (LOD bin / SG node reuse) | M8 baseline | Low (already 50× margin) | Low |
   | M7 ExpressionEngine tick cost | M7 plan §8 | Unmeasured | Low |
   | SessionWriter 60-min soak memory growth | M10 §5.6 | Unknown until measured | Low (most likely no leak) |

   CC selects 3 from this list **OR** profile-discovered alternatives based on actual measured impact (decision M12.2 F).

5. **Each optimization deliverable**:
   - Implementation commit
   - Before/after benchmark numbers
   - Documented in `M12-progress.md` per subtask
   - **Acceptance gate**: optimization shows measurable improvement (decision M12.3 G — specific number required, not "feels faster")

6. **Regression protection** (decision M12.4 J):
   - **Every M12 commit** reruns M0-M11 benchmarks (M5/M6/M7/M8/M10/M11 baseline.md)
   - **No metric** may regress > 5% from M11 closure baseline
   - HALT trigger #1 fires on regression > 5%
   - Bench cycle documented in `tests/benchmark/results/M12-regression-suite.md`

7. **`tests/benchmark/results/M12-baseline.md`**:
   - Final M12 baseline (after all optimizations)
   - Comparison vs M11 closure baseline for every metric
   - Per-optimization breakdown showing source, before/after numbers, rationale

8. **`docs/architecture/decisions/ADR-008-*.md`** (conditional, decision M12.5 N):
   - Authored ONLY if any optimization requires modifying a frozen `.hpp` interface
   - Default: no ADR needed (optimizations are internal)
   - If needed: documents the architectural change, parallel to M6 ADR-005 / M10 ADR-007 patterns

9. **Integration tests** at `tests/integration/`:
   - `test_m12_no_functional_regression.cpp` — runs all M0-M11 acceptance tests against optimized code
   - `test_m12_optimization_correctness.cpp` — for each optimization, verify correctness preserved (e.g., replay output bit-equal before/after, decoder output bit-equal, signal buffer values bit-equal)

10. **Unit tests** for each optimization:
    - New unit tests for any new internal function
    - Existing unit tests must all still pass

11. **Doxygen** on any new public declaration (none expected if frozen surface unchanged)

12. **`.claude/M12-done.md`** with completion report + final M12 baseline

### 2.2 Must not do

1. **No modifications to M2-M11 frozen `.hpp`** without ADR-008. M11 SessionReader was non-frozen (per M10 explicit designation); all other modules are frozen.
2. **No new top-level dependencies**. M12 uses existing tools (perf / valgrind / Qt's profiler) + standard library.
3. **No new V1 features**. M12 is optimization-only.
4. **No premature optimization** — only optimize what profile shows is hot. CC may NOT skip the profile phase to optimize "obvious" things.
5. **No optimization without before/after benchmark**. Every change has a number.
6. **No "drive-by" refactors** unrelated to identified hot paths. Stay scope-disciplined.
7. **No removal of safety checks** to gain speed (e.g., bounds-check elision). Use compiler hints / annotations instead.
8. **No optimization touching CLAUDE.md governance flow**. M12 doesn't modify subtask discipline / commit format.
9. **No regression > 5%** on any M0-M11 benchmark.
10. **No optimization < 10% improvement** relative to its measured baseline. If profile shows 5% improvement available, that's not worth M12 budget.

---

## 3. Design Decisions (locked by this spec)

### 3.1 Hybrid spec/profile-driven (decision M12.1 Option C)

Spec lists **candidate optimizations** (§2.1-4) but does NOT prescribe specific work. CC profiles V1 first, then selects 3 optimizations based on **actual measured impact** (decision M12.2 F).

**Rationale**:
- Pure spec-driven (Option B) risks optimizing the wrong things
- Pure profile-driven (Option A) risks missing known concerns
- Hybrid balances: spec captures known-knowns; profile catches unknown-unknowns

**Process**:
1. S1: Profile V1 workloads with all major scenarios (live, replay, recording)
2. S2: Document profile findings in M12-profile-report.md
3. S2 end: CC proposes top 3 optimizations + selection rationale
4. S3-S5: Implement top 3 with regression protection

### 3.2 Profile-driven discovery (decision M12.2 Option F)

CC's top 3 may be:
- All from candidate list §2.1-4 (likely if list captures actual hot paths)
- Mix of candidate + profile-discovered
- All profile-discovered (unlikely but possible)

CC reports selection in M12-profile-report.md §"Selected optimizations" with explicit rationale per choice.

### 3.3 Per-optimization measurable improvement (decision M12.3 Option G)

Each optimization must show:
- A specific metric improvement (e.g., "M11 1× timing error reduced from 12% to 4.2%")
- Reproducible across 3 runs
- Variance < 5% across runs

**No aggregate scores** (Option H rejected) — accountability matters.

### 3.4 Per-commit regression protection (decision M12.4 Option J)

Every M12 commit triggers re-running M0-M11 benchmarks:

```
1. Apply optimization
2. Run M0-M11 regression suite (bench_decoder + bench_signal_buffer + bench_signal_buffer_e2e + bench_chart + bench_session_writer + bench_replay)
3. Compare each metric vs M11 closure baseline
4. If any metric regresses > 5%: HALT trigger #1 fires
5. If all metrics within 5%: proceed
```

**Rationale**:
- Catches subtle interactions (e.g., optimization in M6 buffer breaks M11 replay throughput)
- 5% threshold is conservative (run-to-run variance is ~2%, so 5% is "real change" floor)
- Cost: ~30 min per commit cycle. Acceptable for M12's importance.

### 3.5 Frozen-surface protection (decision M12.5 Option N)

Optimizations are **internal** by default — no `.hpp` changes. If an optimization requires modifying a frozen `.hpp`:

1. STOP — file ADR-008 in S0 (preemptively if obvious; otherwise during implementation)
2. ADR-008 documents:
   - Why frozen surface change is unavoidable
   - What the change is (additive only — new methods, new struct fields)
   - Parallel to M6 ADR-005 / M10 ADR-007 patterns
3. Wait for human Phase 4-style approval before proceeding (V1 inherent governance)

**Default expectation**: 0 ADR-008 because optimizations are internal.

### 3.6 No soft-HALT (inherits M2-M11)

### 3.7 Metric naming for new internal counters

If optimization adds new instrumentation:
- Prefix `m12_` to distinguish from production metrics
- E.g., `m12_replay_dispatch_batch_size`, `m12_decoder_cache_hit_ratio`

---

## 4. Key Implementation Details

### 4.1 Profiling tooling

`tools/profile/profile_main.cpp` runs three workload scenarios:

```cpp
// Scenario A: Live replay 60 sig × 1kHz × 30s
// Exercises: M3 driver / M4 pipeline / M5 decoder / M6 buffer / M7 expression / M8 chart

// Scenario B: Replay file 600k records × 1× speed
// Exercises: M10 SessionReader / M11 SessionPlayer / M11 PlaybackController / M5 decoder / M6 buffer / M8 chart

// Scenario C: Concurrent record + chart 60 sig × 1kHz × 30s
// Exercises: M10 SessionWriter / M5/M6 like Scenario A
```

Each scenario instrumented:
- `perf record` + `perf report` (Linux)
- Qt's `QElapsedTimer` instrumentation around major operations
- Output: hot-function table + per-module breakdown

### 4.2 Profile report format

`tests/benchmark/results/M12-profile-report.md`:

```markdown
# M12 Profile Report

## Run Environment
- Date, host, GCC, Qt, etc.

## Scenario A: Live (60×1kHz×30s)
### Hot functions (top 20 by self-time)
| Rank | Function | % of runtime | Module |
|---|---|---|---|
| 1 | LinearTypedBuffer::push | 8.4% | M6 |
| 2 | SchemaDecoder::tryDecodeFrame | 6.2% | M5 |
| ...

### Per-module breakdown
| Module | % of runtime |
|---|---|
| M6 | 22% |
| M5 | 18% |
| ...

## Scenario B: Replay (600k × 1×)
### Hot functions
...

## Scenario C: Concurrent record + chart
### Hot functions
...

## Top 3 Selected Optimizations
1. <Optimization name>
   - Source: <where it appeared>
   - Impact: <measured baseline>
   - Approach: <implementation strategy>
   - Risk: <known risks>

2. ...
3. ...

## Optimizations considered but not selected
- <Optimization name>: <why rejected>
```

### 4.3 Per-optimization implementation pattern

Each S3/S4/S5 (one per top-3 optimization) follows:

```
1. Document baseline: rerun specific benchmark, capture numbers
2. Implement optimization
3. Run specific benchmark with optimization
4. Run M0-M11 regression suite
5. Verify:
   a. Specific metric improved by ≥ 10% (decision M12.3)
   b. M0-M11 benchmarks all within 5% (decision M12.4)
6. Commit with subject: `<module>: optimize <metric> <description>`
7. Push + watch CI
8. Update M12-progress.md with before/after numbers + rationale
```

### 4.4 Regression suite mechanism

`tests/benchmark/m12_regression_suite.sh` (or CMake target):

```bash
#!/bin/bash
# Runs all M0-M11 benchmarks
# Compares results vs tests/benchmark/results/M<n>-baseline.md
# Exits non-zero if any metric regresses > 5%

cd build/release
ctest -L benchmark | tee /tmp/m12-regression-output

# Parse, compare, report
python tools/profile/check_regression.py \
  --baseline-dir tests/benchmark/results/ \
  --current /tmp/m12-regression-output \
  --threshold 5.0
```

### 4.5 Final M12 baseline

`tests/benchmark/results/M12-baseline.md`:

```markdown
# M12 Final Baseline

## Optimization 1: <Name>
- Module: <e.g., M11 SessionPlayer>
- Source: <e.g., C4 Stage B>
- Before: <metric value at M11 closure>
- After: <metric value at M12 closure>
- Improvement: <X% reduction>
- Rationale: <implementation summary>

## Optimization 2: ...

## Optimization 3: ...

## Regression Protection Results
| Benchmark | M11 baseline | M12 final | Δ | Status |
|---|---|---|---|---|
| M5 decoder throughput | X | Y | +Z% | ✅ |
| M6 SignalBuffer push (Double) | X | Y | +Z% | ✅ |
| M6 SignalBuffer e2e overhead | X | Y | +Z% | ✅ |
| M8 chart frame p99 | X | Y | +Z% | ✅ |
| M10 SessionWriter throughput | X | Y | +Z% | ✅ |
| M11 SessionPlayer 1× timing | X | Y | +Z% | ✅ |
| ...

All ≤ 5% deviation per acceptance criterion.
```

---

## 5. Performance gates

### 5.1 Optimization improvement targets

Each of the 3 selected optimizations must:

| Metric | Target | HALT |
|---|---|---|
| Specific improvement on selected metric | ≥ 10% | < 10% |
| Reproducibility across 3 runs | ✓ | variance > 5% |

If any optimization fails to improve ≥ 10%, CC may:
- Drop the optimization (revert + remove from list, leaves 2 optimizations)
- HALT and propose alternative (with profile evidence)

CC is NOT required to deliver exactly 3; 2 may be acceptable if profile shows only 2 viable candidates. Spec target is "top 3" but acceptance allows 2-3.

### 5.2 Regression protection

| Benchmark | Threshold |
|---|---|
| All M0-M11 benchmarks | within 5% of M11 closure baseline |

HALT trigger #1 fires on any regression > 5%.

### 5.3 Profile coverage

| Scenario | Required |
|---|---|
| Live (60 sig × 1kHz × 30s) | ✓ |
| Replay (600k records × 1×) | ✓ |
| Concurrent record + chart | ✓ |

If any scenario fails to profile (e.g., crash), HALT + investigate.

---

## 6. Freeze protocol

### 6.1 What freezes at M12 close

**M12 may not introduce new frozen interfaces.** M12 is optimization-only.

**Existing M0-M11 freezes remain intact**. Internal `.cpp` changes don't affect freeze.

### 6.2 What does NOT freeze

- Internal optimization implementation details (free to change in M13+)
- Profile harness in `tools/profile/` (free to extend)
- Per-optimization specific tunings

### 6.3 Freeze record format

`.claude/M12-done.md` includes:

```markdown
## Freezes established in this milestone

None — M12 is optimization-only and does not introduce frozen interfaces.

M2-M11 freezes verified intact:
| File | sha256 (unchanged) |
|---|---|
| ... |  ... |
```

If ADR-008 forces a frozen surface change, M12-done.md lists what changed.

---

## 7. M12-specific HALT triggers

Beyond CLAUDE.md §HALT:

1. **Regression > 5% on any M0-M11 benchmark** → HALT
2. **Modification to M2-M11 frozen `.hpp`** without ADR-008 → HALT
3. **Profile fails on any scenario** (crash, hang) → HALT
4. **Selected optimization improves < 10%** after one optimization pass → HALT, propose drop or alternative
5. **Optimization breaks correctness** (any unit / integration test fails) → HALT
6. **Profile-discovered hot path requires new top-level dependency** → HALT (use existing tooling)
7. **Aggregate runtime degrades** (despite individual optimizations improving, overall workload is slower) → HALT (interaction effect)

---

## 8. Acceptance criteria

### 8.1 Build and test

- [ ] Debug, Release, debug-asan all build clean under C++23
- [ ] All M0-M11 unit + integration tests pass
- [ ] Coverage maintained (no decrease > 2% relative to M11 closure)
- [ ] CI green on milestone/M12 head

### 8.2 Optimization deliverables (per §5)

- [ ] Profile report at `tests/benchmark/results/M12-profile-report.md` with top 3 selection
- [ ] 2-3 optimizations implemented (each ≥ 10% improvement on selected metric)
- [ ] Each optimization has before/after benchmark documented
- [ ] M12-progress.md tracks every optimization with rationale

### 8.3 Regression protection

- [ ] All M0-M11 benchmarks within 5% of M11 closure baseline
- [ ] Regression suite documented in `M12-regression-suite.md`
- [ ] Per-commit re-run discipline maintained

### 8.4 Frozen surface

- [ ] No M2-M11 frozen `.hpp` modified without ADR-008
- [ ] sha256 verification: M2-M11 frozen files match M11 closure values

### 8.5 Final baseline

- [ ] `tests/benchmark/results/M12-baseline.md` published
- [ ] Per-optimization breakdown
- [ ] Regression suite results table

### 8.6 Hand-off

- [ ] M12-done.md hand-off section covers:
  - For M13 Packaging: M12 baseline is the V1.0 reference; packaging includes baseline.md
  - For V1.5+ optimization: optimizations not selected in M12 are documented in profile report; future versions may revisit
  - For V2: profile harness is reusable; V2 may need new scenarios

---

## 9. Notes for CC

- **Profile FIRST, optimize SECOND**. Do not skip profiling. The first commit (S1) should be profile harness; second commit (S2) should be the report. Optimization implementation starts at S3.

- **Spec target "top 3" is flexible**: 2 is acceptable if profile shows only 2 viable. Don't pad with weak optimizations to hit 3.

- **C4 Stage B is the most likely candidate** (M11 1× / 10× timing). Plan accordingly: bench harness already exists, change is in `SessionPlayer::workerLoop()` (sleep_for → sleep_until + batched dispatch).

- **M11 backward seek O(N) is high risk**: in-memory index requires careful design (when to build, what to do with truncated files, etc.). Likely DEFER to V1.5+ unless profile shows it's high impact.

- **Don't optimize M0-M3 unless profile shows hotspot**. Most user time is M5-M11. M0-M3 baseline is solid.

- **Use existing benchmark harness**. M5-M11 baseline.md already define benchmark scenarios. Reuse don't recreate.

- **Use existing tooling**: `perf record / perf report` (Linux), `valgrind --tool=callgrind`, `QElapsedTimer`. Don't add new profiling dependencies.

- **Document every micro-optimization rationale**. Future maintainers + V1.5+/V2 must know why a specific change was made.

- **If frozen surface change becomes necessary, file ADR-008 BEFORE the change**. Do not implement → file ADR. ADR is the authorization, code is the consequence.

- **Regression suite is non-negotiable**. Every commit reruns it. The 30-min cost is the price of not breaking V1.

---

## 10. Closing note

M12 makes V1 **fast**. Combined with M0-M11 (functional V1) + M13 (packaging), V1 release is complete.

The discipline of profile-first + measurable + regression-protected is what separates "good performance" from "regressions hidden in micro-optimizations".

Quality discipline:
- Every change is **measured**: before/after numbers required
- Every change is **regression-tested**: M0-M11 stays solid
- Every choice is **documented**: profile report + done.md show why
- Every architectural exception requires **ADR**: not optional

V1.5+ may revisit optimizations not selected in M12. The profile report is the authoritative record of what's possible.

When in doubt, follow the data. Don't optimize what feels slow; optimize what profiles slow.
