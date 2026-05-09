# M12 — Concerns

Six concerns surfaced during Phase 4 understanding. Each carries
an implementation-level resolution; **none currently requires
ADR-008**. C6 documents the trigger discipline that would
escalate to ADR-008 if any optimisation forces a frozen-surface
change — default expectation is **0 ADR-008 for V1**.

---

## C1 — Multi-metric optimisations

### Statement

Per spec §5.1 + §M12.3 G, each optimisation must improve a
specific metric by ≥ 10 % (reproducible across 3 runs, variance
< 5 %). Some optimisations naturally improve multiple metrics.
The most prominent example is **C4 Stage B** (M11 hand-off): the
SessionPlayer dispatch-loop rework (sleep_for → sleep_until +
batched dispatch) targets two M11-baseline gaps simultaneously:
- 1× timing accuracy (M11-baseline.md: 12 % error → spec target < 5 %)
- 10× completion (M11-baseline.md: 2.3 s → spec target ≤ 1 s)

Question: does **each** metric need to clear 10 %, or only **one**
("primary"), with the other documented as a bonus?

### Resolution (per Phase 5 clarification)

For an optimisation that affects multiple metrics:

1. **One primary metric** is named at S2 selection (in
   M12-profile-report.md §"Selected optimisations").
2. The **primary metric** must clear ≥ 10 % improvement
   (spec §5.1 / M12.3 G).
3. **Secondary metrics** that also improve are documented in
   M12-progress.md + M12-baseline.md as **bonus**, not gated.
4. If the primary metric fails to clear 10 %, the optimisation
   fails per H4 — even if a secondary metric improved.

### Application to C4 Stage B (likely S3 candidate)

- **Primary metric**: 1× timing accuracy
  - Current: 12.02 % error
  - Pass criterion: ≥ 10 % reduction → ≤ 10.82 % error
  - Stretch (spec §5.1 target): ≤ 5 % error
- **Secondary metric**: 10× completion
  - Current: 2.26 s for 10 s file
  - Bonus territory: any reduction documented; if it reaches
    spec target ≤ 1 s, that's a major V1.0 win
- The primary is the gate. The secondary is the headline.

This avoids gaming the 10 % bar across metrics and keeps each
optimisation accountable to a single number.

---

## C2 — Profile harness portability

### Statement

Spec §4.1 calls for `perf record / perf report` (Linux). The host
runs Linux but `perf` requires kernel `perf_event_paranoid` ≤ 2.
On hardened distros this defaults to 3 (no userspace event
tracing) or 4 (kernel-events-blocked entirely). If `perf` is
blocked, the harness must fall back gracefully.

### Resolution

S1 implements a tiered harness:

1. **Tier 1 — `perf record` + `perf report`** (preferred; flame
   graphs + accurate self-time).
2. **Tier 2 — `valgrind --tool=callgrind`** (works without root;
   slower; produces `callgrind.out.<pid>` consumable by
   `callgrind_annotate` / `kcachegrind`).
3. **Tier 3 — `QElapsedTimer` instrumentation** (last resort;
   coarse-grained per-region timers; always works).

`tools/profile/run_profile.sh` detects host capability:
- Try `perf stat sleep 0.1`; if succeeds → tier 1.
- Else try `valgrind --version`; if succeeds → tier 2.
- Else → tier 3 (always available).

`M12-profile-report.md` records which tier produced the report.
This concern is implementation-detail, not architectural; no ADR.

---

## C3 — Regression suite cost

### Statement

Spec §3.4 J mandates re-running M0-M11 benchmarks every commit.
Spec estimates ~30 min per cycle. M12 plan has 8 commits
(Pre-S0, S0, S1, S2, S3, S4, S5?, S6, S7). Total worst-case:
8 × 30 min = 4 hours of regression-cycle wait.

### Resolution

Strategic skipping per CLAUDE.md §Required #2 (docs-only commits
exempt from rebuild):

| Commit | Regression cycle? | Reason |
|---|---|---|
| Pre-S0 understanding+plan | no | docs-only |
| S0 concerns | no | docs-only |
| S1 profile harness | yes | new tool builds; need green CI but no opt changes |
| S2 profile report | no | docs-only |
| S3 optimisation 1 | **yes** | optimisation cycle: baseline + opt + regression |
| S4 optimisation 2 | **yes** | same |
| S5 optimisation 3 (optional) | yes if landed | same |
| S6 final baseline + integration tests | yes | full sweep |
| S7 done.md | no | docs-only |

Net regression cycles: ~4 (S1 + S3 + S4 + S5? + S6 = 4-5).
Manageable.

The CI step is per-commit anyway and runs the full ctest
(no benchmark; bench is opt-in); local regression suite is
the additional discipline gating optimisation commits.

---

## C4 — Profile may show < 2 viable optimisations

### Statement

Spec §5.1 acceptance allows **2-3** optimisations. Below 2 is a
failure mode. If S2 profile shows only 1 viable candidate (e.g.,
everything else is at the noise floor), CC must HALT per the
plan.

### Resolution — explicit decision tree at S2 close

After profile execution, CC counts viable candidates where
"viable" means:
- Estimated improvement ≥ 10 % on a measurable metric
- Implementation scope < 1 day of effort
- No frozen-surface change required (else ADR-008 path)
- No new top-level dependency required

| Viable count | Action |
|---|---|
| 0 | HALT. Profile shows V1 is already at the noise floor; M12 may need to pivot to "thorough profile + regression suite as proof of discipline" with human review. |
| 1 | HALT. Spec §5.1 minimum is 2; propose alternative (extend benchmark coverage, run soaks, or accept "1 optimisation + thorough docs"). |
| 2 | Proceed to S3+S4. Document third's "not selected" rationale in profile report. |
| 3+ | Proceed to S3+S4+S5; rank by expected impact. |

This decision tree fires at S2 close before any optimisation
code is written. CC does not "find a third" by lowering the
viability bar.

---

## C5 — M11 backward seek likely defer

### Statement

Spec §9 Note explicitly flags M11 backward seek O(N) as
**high-risk** (in-memory index design — when to build, what to
do with truncated files, where to store). Per the same note,
likely defer to V1.5+ unless profile shows it's high impact.

### Resolution

S2 profile measures backward-seek explicitly on a 600 k-record
file (Scenario B). Decision criteria:

| Backward-seek wall-time | Action |
|---|---|
| < 1 s | Document as "considered but not selected"; ship as-is |
| 1-5 s | Document; defer to V1.5+ |
| > 5 s | Escalate to optimisation candidate; weigh against ADR-008 trigger (in-memory index touches `SessionReader` which is M11-frozen) |

The third row's "weigh against ADR-008" is the realistic worst
case; per spec §9 Note, the implementation defaults to V1.5+.

---

## C6 — ADR-008 trigger discipline

### Statement

Spec §3.5 N + §6.1 + §7-2 all converge: any frozen-surface
change requires ADR-008 + human Phase-4-style approval **before**
code change. Default expectation: 0 ADR-008.

### Resolution

At each S3 / S4 / S5 implementation start, CC verifies the
optimisation does **NOT** touch any of these frozen `.hpp` files:

```
src/buffer/signal_buffer.hpp
src/buffer/signal_buffer_registry.hpp
src/chart/chart.hpp
src/chart/chart_manager.hpp
src/chart/chart_view.hpp
src/chart/signal_selector.hpp
src/chart/time_axis_manager.hpp
src/connection/connection.hpp
src/connection/connection_manager.hpp
src/decode/decoder_interface.hpp
src/decode/decoder_registrar.hpp
src/decode/schema_decoder.hpp
src/drivers/driver_interface.hpp
src/expression/expression_engine.hpp
src/frame/raw_frame.hpp
src/frame/raw_frame_value_sink.hpp
src/pipeline/frame_pipeline.hpp
src/pipeline/frame_sink.hpp
src/pipeline/pipeline_manager.hpp
src/replay/playback_controller.hpp
src/replay/replay_mode_manager.hpp
src/replay/session_player.hpp
src/session/session_metadata.hpp
src/session/session_writer.hpp
```

(M11-done.md §Freezes adds 3; M10 adds 2; etc. M11 SessionReader
**is** frozen at M11 close per M11-done.md §Freezes — explicitly
named in M11's freeze record. M10 SessionReader was non-frozen at
M10 close, but M11 froze it implicitly via the M11 close. M12
may not extend it without ADR-008.)

If a tempting optimisation requires touching any of the above,
S3/S4/S5 stops, writes ADR-008 stub, and waits for human
authorization. M2-M11 freeze sha256s verified at S6 (final
baseline) per acceptance §8.4.

---

## Summary

| ID | Resolution path | ADR? | Ships in subtask |
|---|---|---|---|
| C1 | Primary metric clears 10 %; secondaries are bonus | No | S2 selection rationale + S3-S5 progress entries |
| C2 | Tiered profile harness (perf → callgrind → QElapsedTimer) | No | S1 |
| C3 | Strategic skip: docs-only commits exempt; ~4 regression cycles total | No | S1 / S3-S6 |
| C4 | Explicit decision tree at S2 close (0/1/2/3+ viable) | No (potential HALT) | S2 |
| C5 | Backward seek measured at S2; default defer to V1.5+ | No (likely) | S2 |
| C6 | Pre-S3 freeze-list check; ADR-008 if forced | No (default) | S3-S5 + S6 sha256 verification |

**No ADR-008 expected for V1.** This file is the canonical record;
M12-done.md will reference it from §Deviations.
