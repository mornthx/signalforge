# M12 — Progress log

Per CLAUDE.md §Required #2 + plan §0, every subtask logs start +
close entries with build / test / format counts and any deviations.

---

## Pre-S0 — M12 understanding + plan (completed)

- Start: 2026-05-08T13:35Z
- Close: 2026-05-08T13:55Z
- Commit: `6032abe` "chore: record M12 understanding and plan"
- CI: pending — push triggers CI per CLAUDE.md §Required #2.
- Deliverables:
  - `.claude/M12-understanding.md` (268 lines, 6 concerns
    C1-C6 surfaced; candidate list framed as
    "informed expectation; profile arbitrates")
  - `.claude/M12-plan.md` (301 lines, S0-S7 sequenced;
    7 HALT triggers H1-H7; per-optimisation 6-step cycle)

---

## S0 — M12-concerns.md + ADR-008 decision (completed)

- Start: 2026-05-08T14:00Z

### Deliverables

- `.claude/M12-concerns.md` (~265 lines): canonical record of
  C1-C6 with subtask anchors + decision trees.
  - **C1** integrates Phase 5 clarification: primary metric
    clears 10 %; secondaries are bonus. Concrete application
    to C4 Stage B documented (1× = primary, 10× = secondary).
  - **C2** tiered profile harness (perf → callgrind →
    QElapsedTimer fallback).
  - **C3** strategic regression-cycle skip table (docs-only
    commits exempt; net ~4 cycles for the milestone).
  - **C4** explicit 0/1/2/3+ viable-count decision tree at
    S2 close.
  - **C5** backward-seek measurement → defer-to-V1.5+ default.
  - **C6** full M2-M11 frozen-`.hpp` list documented; pre-S3
    check + sha256 verification at S6.
- No ADR-008 authored. Default position holds.

### Build / test counts

- Docs-only commit. CLAUDE.md §Required #2 exception applies
  (build graph unaffected). clang-format: not applicable.
- No regression suite cycle (per C3 strategic skip).

### Deviations from plan

- Plan §S0 anticipated a conditional ADR-008 stub. Default
  position holds: no architectural divergence requiring
  ADR-008 at this point. Conditional escalation path
  documented in C6 with the full M2-M11 freeze list, so a
  future ADR-008 can be authored as a delta against this
  baseline.
- Phase 5 clarification on C1 (multi-metric optimisations:
  primary clears 10 %, secondaries are bonus) is integrated
  into the concerns doc with a worked example for C4 Stage B.

S0 commit: `ee274f0` "docs: M12 S0 — concerns C1-C6 (no ADR-008)".
Pushed after pre-S0 CI green (run 25559922777 ✓).
S0 CI: pending watch.

---

## S1 — Profile harness (completed)

- Start: 2026-05-08T14:30Z

### Deliverables

- `tools/profile/profile_main.cpp` (~210 LOC): Qt console
  app driving the 3 spec-§4.1 scenarios.
  - **Scenario A** — live-style synthetic dispatch into M6
    `SignalBufferRegistry`. Stand-in for the full M3+M4+M5+M6
    path; covers the dispatch + buffer hot-spots most likely
    to surface in profile.
  - **Scenario B** — replay: SessionPlayer driving a
    60 sig × 1 kHz × `--duration` SFREPLAY v1 fixture at 1×
    speed. Exercises M10 SessionReader + M11 SessionPlayer +
    PlaybackController + the M6 sink path.
  - **Scenario C** — concurrent record + chart: TeeSink fans
    synthetic signals to both M6 SignalBufferRegistry and
    M10 SessionWriter. Exercises both write paths
    simultaneously.
  - `--tier3` enables QElapsedTimer instrumentation (always
    works; coarse — emits `dispatch_pct` percentage of total).
- `tools/profile/CMakeLists.txt`: builds `profile_main`
  linked against the M5/M6/M10/M11 static libs.
- `tools/profile/run_profile.sh` (~75 LOC): tiered runner per
  C2 — auto-detects `perf` (tier 1 → flame-graph friendly),
  falls back to `valgrind --tool=callgrind` (tier 2 → no-root),
  finally `--tier3` QElapsedTimer (always works). Outputs to
  `tests/benchmark/results/M12-profile-<tier>-<scenario>.<fmt>`.
- `tools/profile/check_regression.py` (~125 LOC): JSONL parser
  that compares freshly-collected bench numbers vs
  hard-coded M11-closure baselines per C3. Exits non-zero on
  > threshold % regression in `--strict` mode (default
  threshold 5 % per spec §3.4 J / plan §3 H1).
- `CMakeLists.txt`: `add_subdirectory(tools/profile)` after
  `tools/sfreplay_inspect`.

### Smoke verification (S1 close)

All 3 scenarios run end-to-end with --duration 1 in well under
2 s wall-clock:

| Scenario | Output | Note |
|---|---|---|
| A | `events_per_sec=3.7M` (dispatch_pct=92 %) | tier 3 instrumented |
| B | 36 k records replayed in 1135 ms | 1× speed; backpressure-dropped |
| C | 60 k events in 29 ms (tee + writer) | concurrent path |

`check_regression.py` smoke run vs `bench_replay` JSON output
correctly catches the run-to-run variance in 1-second-test
metrics vs M11 baseline's 10-second-test numbers (+13.98 %
error_pct, +15.58 % seek_ms — both `❌` reported in
informational mode). This validates the gate semantics; S2 will
re-run with stable 10-second fixtures for the actual report.

### Build / test counts

- Debug + Release + debug-asan all build clean.
- ctest: Debug **585/585** + Release **585/585** unchanged
  (profile harness has no ctest entries; it's an opt-in tool).
- ASan local blocked by host /etc/ld.so.preload; CI authoritative.
- `clang-format -i` clean on `profile_main.cpp`.

### Deviations from plan

- Plan §S1 anticipated ~300 LOC; actual ~410 LOC (tool 210 +
  shell 75 + python 125). Within target.
- Scenario A is **synthetic** (registry.onSignal direct, no
  real schema decoder + pipeline). A full live-style scenario
  would need a UDP echo + schema fixture, which is bench
  scope-creep. The synthetic dispatch covers M5/M6 hot-paths
  per spec §4.1 — documented in profile_main.cpp comment.
  S2 may revise if the synthetic path is too thin; for now it's
  the right tier-1-ready harness.
- `tier3Instrumented` only applies to scenario A's inner loop
  (where it's most useful). Scenarios B and C don't have a
  comparable per-iteration measurement point; they emit
  scenario-level totals instead.

S1 commit: pending push.
