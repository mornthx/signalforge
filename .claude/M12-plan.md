# M12 — Plan

Pairs with `.claude/M12-understanding.md`. Source of truth:
`docs/milestones/M12-performance.md` at `ae77820`. CLAUDE.md
governs.

---

## 0. Methodology

- **Profile FIRST, optimise SECOND** — S1 = harness, S2 = report
  + top-3 selection, S3-S5 = optimisations. CC may NOT skip the
  profile phase to optimise "obvious" things (M12.1 hybrid).
- One subtask = one logical commit (CLAUDE.md §Required #3).
- Each S3/S4/S5 optimisation commit:
  1. Document the specific baseline (rerun the metric's bench)
  2. Implement the optimisation (only `.cpp` changes; no frozen
     `.hpp` without ADR-008)
  3. Rerun the specific bench → verify ≥ 10 % improvement on
     the chosen metric
  4. Run **M0-M11 regression suite** → verify all metrics
     within 5 % of M11 closure baseline (M12.4 J)
  5. Commit + push + watch CI
  6. Update M12-progress.md with before/after + rationale
- Build gate before every code commit: Debug + Release + debug-asan
  build clean; ctest Debug + Release green; clang-format dry-run
  clean. ASan local-blocked per host preload note; CI authoritative.
- Documentation-only commits use the CLAUDE.md §Required #2
  exception.
- HALT triggers in §3 fire **immediately** when their watermark
  is crossed; no recovery attempts beyond CLAUDE.md's documented
  3× rule.

## 1. Subtask sequence

| ID | Title | Net LOC est. | Output | Notes |
|---|---|---:|---|---|
| S0 | M12-concerns.md (C1-C6) + ADR-008 stub decision | ~150 | docs | Default: no ADR. Conditional path in C6. |
| S1 | Profile harness (`tools/profile/`) | ~300 | tool | `profile_main.cpp`, `check_regression.py`, optional shell wrappers. Uses perf or valgrind callgrind (S0 verifies host capability). |
| S2 | Profile execution + report + top-3 selection | ~250 | docs | `M12-profile-report.md` with hot-function tables for 3 scenarios + selection rationale. CC may NOT proceed past S2 without explicit top-3 commitment. |
| S3 | Optimisation 1 (probably C4 Stage B per M11 hand-off + spec §9) | ~250 | code + tests + bench | `SessionPlayer::dispatchLoop` rework: `sleep_until` + batched-dispatch ring buffer. Targets M11 1× error 12 % → ≤ 5 % AND/OR 10× wall 2.3 s → ≤ 1.5 s (each metric independently must clear 10 % per spec §5.1). |
| S4 | Optimisation 2 (profile-selected) | ~200 | code + tests + bench | Likely M6 push wrapper or M5 decoder hot path. Confirmed at S2. |
| S5 | Optimisation 3 (profile-selected, optional) | ~150 | code + tests + bench | If profile shows only 2 viable, ship 2; document the third's "considered but not selected" rationale in profile report. |
| S6 | Final M12 baseline + integration tests | ~400 | docs + tests | `M12-baseline.md` + `test_m12_no_functional_regression.cpp` + `test_m12_optimization_correctness.cpp`. |
| S7 | M12 done.md + freeze sha256 verification | ~400 | docs | Verifies M2-M11 freeze surface intact; final test count + benchmark results. |

Total ~2 100 LOC. Compares favourably to M11's ~3 850.

## 2. Time budget

Spec target 5-7 person-days. Per-commit regression cycles add
~30 min wall-clock each (M12.4 J); 6 commits × 30 min = ~3 hours
of regression-cycle wait spread across the milestone.

## 3. HALT triggers (M12-specific, on top of CLAUDE.md §HALT)

Each trigger has a measurement point and an immediate action.

| # | Trigger | Measurement point | Action |
|---|---|---|---|
| H1 | Regression > 5 % on any M0-M11 benchmark | per-commit regression suite (S3-S7) | HALT; revert commit; investigate |
| H2 | Modification to M2-M11 frozen `.hpp` without ADR-008 | pre-commit `git diff` against M2-M11 freeze list | HALT; file ADR-008 BEFORE retry |
| H3 | Profile fails on any of 3 required scenarios | S1 / S2 execution | HALT; investigate crash/hang |
| H4 | Selected optimisation improves < 10 % after one optimisation pass | S3/S4/S5 close gates | HALT; propose drop or alternative |
| H5 | Optimisation breaks correctness (any unit / integration test fails) | per-commit `ctest` (Debug + Release) | HALT; revert; re-design |
| H6 | Profile-discovered hot path requires new top-level dependency | S2 / S3 implementation | HALT; use existing tooling |
| H7 | Aggregate runtime degrades despite individual optimisations improving | S6 final regression suite | HALT; investigate interaction effect |

Plus CLAUDE.md standard set (compile error 3×, test fail 3×,
new dep, frozen-interface mod, perf miss after 1 opt pass,
spec/arch contradiction, Qt 6.10 anomaly, two plausible impls,
unexplained git failure).

## 4. Subtask details

### S0 — Concerns + ADR-008 decision

**Inputs**: spec §3, §7, §9; this plan §3; M11-baseline.md
findings; M11-done.md hand-off line items.

**Deliverables**:
- `.claude/M12-concerns.md` documenting C1-C6 (per
  M12-understanding.md §6) with resolution paths + subtask
  anchors.
- (Conditional) `docs/architecture/decisions/ADR-008-*.md` —
  only authored if S0 review of likely optimisations reveals an
  unavoidable frozen-surface change. Default position: no ADR.

**Build / test**: docs-only. CLAUDE.md §Required #2 exception.

**Done when**: M12-concerns.md committed; if no ADR needed,
S0 explicitly states "no architectural divergence requiring
ADR-008 at this point".

### S1 — Profile harness

**Deliverables**:
- `tools/profile/CMakeLists.txt`: builds `profile_main`
  executable.
- `tools/profile/profile_main.cpp`: drives the 3 scenarios:
  - Scenario A: live replay 60 sig × 1 kHz × 30 s
    (M3 driver + M4 pipeline + M5 decoder + M6 buffer + M7
    expression + M8 chart)
  - Scenario B: replay file 600 k records × 1× speed
    (M10 SessionReader + M11 SessionPlayer + M11
    PlaybackController + M5/M6/M8)
  - Scenario C: concurrent record + chart 60 sig × 1 kHz × 30 s
    (M10 SessionWriter + M5/M6/M8)
- `tools/profile/check_regression.py`: parses bench output,
  compares to M5-M11 baseline.md, exits non-zero on > 5 %
  regression.
- `tools/profile/run_with_perf.sh` + `run_with_callgrind.sh`:
  thin wrappers that detect host capability + invoke the
  harness with the right tool. Falls back to QElapsedTimer if
  neither is available (last-resort).
- Top-level `CMakeLists.txt` `add_subdirectory(tools/profile)`.

**Build gate**: profile_main compiles + links + runs each
scenario for ~5 s without crashing. No regression cycle yet
(no optimisations land here).

**Done when**: 3 scenarios produce non-empty hot-function
output via at least one tool.

### S2 — Profile execution + report + top-3 selection

**Deliverables**:
- `tests/benchmark/results/M12-profile-report.md` with:
  - Run environment (host, tools, dates)
  - Per-scenario top-20 hot-function tables (or top-K,
    whatever the tool yields)
  - Per-module % runtime breakdown
  - Selection rationale: top 3 (or 2) optimisations + reason
    each was chosen + reason others were rejected
- `tests/benchmark/m12_regression_suite.sh`: shell wrapper that
  builds + runs all M0-M11 bench targets + invokes
  `check_regression.py`. Documented invocation in
  `M12-regression-suite.md`.
- `tests/benchmark/results/M12-regression-suite.md`: documents
  the cycle, expected runtime, exit-code semantics.

**Build gate**: docs-only commit. The regression suite shell
script is exec-bit + lints clean.

**HALT trigger evaluation at S2 close**:
- If profile shows 0 viable optimisations → HALT.
- If profile shows 1 viable → HALT, propose path forward
  (spec §5.1 allows 2-3; below 2 = HALT).
- If 2-3 viable → S3 begins.

**Done when**: report has explicit "Selected optimisations: X,
Y, Z" with rationale per selection AND per rejection. CC may
not skip this commitment.

### S3 — Optimisation 1 (likely C4 Stage B)

**Deliverables**: TBD per S2 selection. **If C4 Stage B**:
- `src/replay/session_player.cpp` rework:
  - `dispatchLoop`: `sleep_for` (chunked) → `sleep_until` against
    an absolute next-record deadline computed from
    `openTimeSteady_ + scaledNs`.
  - Optional batched dispatch: collect N records on the worker,
    direct-call sink with all N. Direct-call already lands at
    M11 S10; batching is the marginal speed-up.
  - Preserve pause-mid-sleep `pendingRecord_` semantics.
- New unit test: `tests/unit/replay/session_player_timing_v2_test.cpp`
  verifying 1× and 10× under the new dispatcher.
- Bench rerun: `bench_replay --realtime 10` + `--fast 10`.
- Acceptance per M12.3 G: 1× error 12 % → ≤ 5 % (≥ 58 %
  improvement) OR 10× wall 2.3 s → ≤ 1.5 s (≥ 35 % improvement).
  Either metric clearing the 10 % bar passes the optimisation.
- Regression suite: full M0-M11 within 5 %.
- Verify M11 unit + integration tests still pass (correctness
  preserved per M12 §2.2 #5).

### S4 — Optimisation 2 (profile-selected)

**Deliverables**: TBD per S2. Same shape as S3:
- Documented baseline (specific metric)
- Implementation in non-frozen `.cpp` only
- ≥ 10 % improvement on the metric
- Regression suite passes
- Unit + integration regression clean

### S5 — Optimisation 3 (optional)

**Deliverables**: same shape as S3 / S4, **OR** explicitly
documented as "deferred — profile shows only 2 viable
candidates" with the third's "considered but not selected"
rationale folded into M12-profile-report.md §"Optimisations
considered but not selected".

If S5 lands: same gates as S3 / S4.
If S5 deferred: spec §5.1 allows 2-3; documented in S6 baseline.

### S6 — Final M12 baseline + integration tests

**Deliverables**:
- `tests/benchmark/results/M12-baseline.md`:
  - Per-optimisation table: module, source, before, after,
    improvement %, rationale.
  - Regression results table: every M0-M11 metric, M11 baseline,
    M12 final, Δ %, ✅/❌ status.
  - All ≤ 5 % deviation per acceptance §8.5.
- `tests/integration/test_m12_no_functional_regression.cpp`:
  smoke test composing M0-M11 functional paths against
  optimised code. May simply build a 60 sig × 1 kHz × 1 s
  workload through live + replay + record and assert no
  failures + bit-equal signals.
- `tests/integration/test_m12_optimization_correctness.cpp`:
  per-optimisation correctness check (e.g., replay output
  bit-equal before/after for the C4 Stage B path).

**Build gate**: full Debug + Release + debug-asan build, full
ctest sweep, full regression suite all green.

### S7 — M12 done.md + freeze sha256 verification

**Deliverables**:
- `.claude/M12-done.md` mirroring M11-done.md shape:
  - Spec §2.1 deliverable checklist (all ✅);
  - PR + merge state placeholders;
  - **Freezes preserved** section:
    ```
    No new freezes — M12 is optimisation-only.
    M2-M11 freezes verified intact (sha256s match M11 closure).
    ```
  - Per-optimisation table (mirrors M12-baseline.md);
  - Regression suite results;
  - HALT-trigger disposition;
  - Hand-off to M13 (Packaging) + V1.5+ deferred items.
- sha256 verification: shell command in done.md showing M2-M11
  frozen file hashes match the M11-closure-recorded values.

## 5. Risk register + mitigation

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| Profile shows < 2 viable optimisations | Low | HALT (C4) | S2 explicit decision tree; alternative is "ship 1 + thorough profile" with human review |
| Regression suite catches > 5 % drop | Medium | HALT (H1) | Per-commit cycle (M12.4 J) catches early; revert + investigate |
| C4 Stage B doesn't clear 10 % on 1× metric | Medium | HALT (H4) | Two metrics in scope (1× + 10×); need only ONE to clear the bar; if neither, drop optimisation per spec §5.1 |
| Optimisation forces frozen `.hpp` change | Low | ADR-008 + delay | S0 / S3 verify pre-implementation; ADR-008 unblocks via human approval |
| `perf` blocked by host kernel `perf_event_paranoid` | Medium | S1 partial | Fallback to valgrind callgrind documented in S1 |
| 30-min regression cycle blocks rapid iteration | Medium | Time | Acceptable per M12.4 rationale; budget 6 cycles |
| Profile harness itself crashes (Qt + perf interaction) | Low | HALT (H3) | Use Release build for profiling; limit to 30-second windows; instrument with `QElapsedTimer` as fallback |

## 6. V1.5+ / V2 deferred items (mirror M12 §2.2)

V1.5+:
- Optimisations not selected in M12's top 3 (documented in
  profile report's "considered but not selected" section)
- M11 backward seek O(N) → in-memory index (high-risk per
  M11 spec §9 Note 2; profile may defer)
- 30-min memory soak as a CI gate (currently operator-run;
  inherits from M9-M11)

V2:
- Profile harness extension for new scenarios
- Cross-platform profile tooling (Windows / macOS native
  profilers)

## 7. Closeout checklist

- [ ] All S0-S7 commits landed on `milestone/M12`
- [ ] CI green on every commit (regression suite included)
- [ ] PR opened to `main`; CI green on PR
- [ ] M12-done.md published with PR # / head SHA / final
      baseline + freeze sha256 verification
- [ ] Phase 1 step 6 announce: "M12 ready. Awaiting approval
      to merge M12 and begin M13 bootstrap"
- [ ] Phase 2 follow-ups (memory soaks + hardware
      verification) explicitly carried in M12-done.md hand-off
      if not closed during M12

---

## 8. What I am NOT planning to do

- Modify any M2-M11 frozen `.hpp` (default; ADR-008 required
  if forced).
- Add a new top-level dependency. M12 uses Qt + existing
  in-tree modules + system `perf` / `valgrind`.
- Skip the profile phase to optimise "obvious" things (M12.1 C
  hybrid is the explicit guard against this failure mode).
- Deliver < 2 optimisations (HALT at S2 if profile shows that).
- Deliver any optimisation showing < 10 % improvement (HALT
  at the optimisation's S close).
- Deliver any optimisation that breaks M0-M11 functional tests
  (HALT at first failed `ctest`).

## 9. Phase 4 / 5 expectations

After this plan is approved (Phase 4 — "approved, execute M12"),
S0 begins immediately. Per-commit regression cycles run between
optimisation subtasks. At S7 Phase 1 step 6 fires.

The discipline this milestone enforces — profile-first,
measurable improvement, per-commit regression protection — is
the V1.0 quality discipline that separates "good performance"
from "regressions hidden in micro-optimisations". M12 is the
last functional milestone before M13 (packaging) closes V1.0.
